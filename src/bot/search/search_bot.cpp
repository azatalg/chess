#include "search_bot.h"
#include <algorithm>
#include <cstdlib>
#include <limits>

namespace chess::ai {
namespace {

constexpr i32 infinity = 1'000'000;
constexpr i32 mate_score = 900'000;
constexpr i32 piece_value[7] = {100, 320, 330, 500, 900, 0, 0};

i32 positional_bonus(Piece piece, Color color, u8 square) {
    const int file = square & 7;
    const int rank = color == Color::White ? square >> 3 : 7 - (square >> 3);
    const int center_distance = std::abs(2 * file - 7) + std::abs(2 * rank - 7);
    switch (piece) {
        case Piece::Pawn: return rank * 8 - std::abs(2 * file - 7) * 2;
        case Piece::Knight: return 42 - center_distance * 6;
        case Piece::Bishop: return 28 - center_distance * 3;
        case Piece::Rook: return rank == 6 ? 20 : 0;
        case Piece::Queen: return 14 - center_distance;
        case Piece::King: return rank < 2 ? -center_distance : 0;
        default: return 0;
    }
}

u8 pop_lsb(u64& bits) {
    const u8 square = static_cast<u8>(__builtin_ctzll(bits));
    bits &= bits - 1;
    return square;
}

} // namespace

SearchBot::SearchBot(const Board& board, Color color)
    : Bot(board, color), table_(1U << 18) {}

bool SearchBot::time_expired() {
    if ((++nodes_ & 1023ULL) == 0 && timer_.elapsed_milliseconds() >= deadline_ms_)
        stopped_ = true;
    return stopped_;
}

i32 SearchBot::evaluate(const Board& board) const {
    i32 white_score = 0;
    i32 phase = 0;
    for (Color color : {Color::White, Color::Black}) {
        phase += int(__builtin_popcountll(board.pieces(color, Piece::Knight)));
        phase += int(__builtin_popcountll(board.pieces(color, Piece::Bishop)));
        phase += 2 * int(__builtin_popcountll(board.pieces(color, Piece::Rook)));
        phase += 4 * int(__builtin_popcountll(board.pieces(color, Piece::Queen)));
    }
    for (Color color : {Color::White, Color::Black}) {
        i32 score = 0;
        for (u8 p = 0; p < 6; ++p) {
            const Piece piece = Piece(p);
            u64 pieces = board.pieces(color, piece);
            while (pieces) {
                const u8 square = pop_lsb(pieces);
                score += piece_value[p] + positional_bonus(piece, color, square);
            }
        }

        const u64 bishops = board.pieces(color, Piece::Bishop);
        if (__builtin_popcountll(bishops) >= 2) score += 35;

        const u64 pawns = board.pieces(color, Piece::Pawn);
        for (int file = 0; file < 8; ++file) {
            const u64 file_mask = 0x0101010101010101ULL << file;
            const int count = __builtin_popcountll(pawns & file_mask);
            if (count > 1) score -= 14 * (count - 1);
            if (count && !(pawns & ((file ? file_mask >> 1 : 0) |
                                    (file < 7 ? file_mask << 1 : 0)))) score -= 10;
        }

        u64 pawn_scan = pawns;
        const u64 enemy_pawns = board.pieces(opposite(color), Piece::Pawn);
        while (pawn_scan) {
            const u8 square = pop_lsb(pawn_scan);
            const int file = square & 7;
            const int rank = square >> 3;
            u64 files = 0x0101010101010101ULL << file;
            if (file) files |= 0x0101010101010101ULL << (file - 1);
            if (file < 7) files |= 0x0101010101010101ULL << (file + 1);
            const u64 ahead = color == Color::White
                ? (~0ULL << ((rank + 1) * 8)) : (rank ? ((1ULL << (rank * 8)) - 1) : 0);
            if (!(enemy_pawns & files & ahead))
                score += 12 + 8 * (color == Color::White ? rank : 7 - rank);
        }

        const u8 king = static_cast<u8>(__builtin_ctzll(board.pieces(color, Piece::King)));
        const int king_file = king & 7;
        const int king_rank = king >> 3;
        const int home_rank = color == Color::White ? 1 : 6;
        if (phase > 12) {
            for (int df = -1; df <= 1; ++df) {
                const int f = king_file + df;
                if (f >= 0 && f < 8 && (pawns & square_bit(u8(home_rank * 8 + f)))) score += 12;
            }
        } else {
            score += 24 - 4 * (std::abs(2 * king_file - 7) + std::abs(2 * king_rank - 7));
        }
        white_score += color == Color::White ? score : -score;
    }
    return board.side_to_move() == Color::White ? white_score : -white_score;
}

i32 SearchBot::move_score(Move move, Move tt_move, Color side, i32 ply) const {
    if (move == tt_move) return 2'000'000;
    i32 score = 0;
    if (move.is_capture())
        score += 1'000'000 + 16 * piece_value[index(move.captured())] - piece_value[index(move.piece())];
    if (move.is_promotion()) score += 800'000 + piece_value[index(move.promotion())];
    if (!move.is_capture()) {
        if (ply < 128 && move == killers_[ply][0]) score += 700'000;
        else if (ply < 128 && move == killers_[ply][1]) score += 690'000;
        score += history_[index(side)][move.from()][move.to()];
    }
    return score;
}

void SearchBot::order_moves(MoveList& moves, Move tt_move, Color side, i32 ply) const {
    for (u16 i = 1; i < moves.size(); ++i) {
        const Move move = moves[i];
        const i32 score = move_score(move, tt_move, side, ply);
        u16 j = i;
        while (j && move_score(moves[j - 1], tt_move, side, ply) < score) {
            moves[j] = moves[j - 1];
            --j;
        }
        moves[j] = move;
    }
}

bool SearchBot::repeated(u64 key, i32 ply) const {
    for (i32 previous = ply - 2; previous >= 0; previous -= 2)
        if (repetition_[previous] == key) return true;
    return false;
}

i32 SearchBot::quiescence(Board& board, i32 alpha, i32 beta, i32 ply) {
    if (time_expired()) return 0;
    MoveList moves;
    const bool checked = board.in_check(board.side_to_move());
    if (checked) {
        board.generate_legal(moves);
        if (moves.empty()) return -mate_score + ply;
    } else {
        const i32 stand_pat = evaluate(board);
        if (stand_pat >= beta) return beta;
        if (stand_pat > alpha) alpha = stand_pat;
        board.generate(moves, Generation::Captures);
    }
    order_moves(moves, Move{}, board.side_to_move(), ply);
    for (Move move : moves) {
        StateInfo state;
        board.make_move(move, state);
        const i32 score = -quiescence(board, -beta, -alpha, ply + 1);
        board.unmake_move(move, state);
        if (stopped_) return 0;
        if (score >= beta) return beta;
        if (score > alpha) alpha = score;
    }
    return alpha;
}

i32 SearchBot::negamax(Board& board, i32 depth, i32 alpha, i32 beta, i32 ply) {
    if (time_expired()) return 0;
    if (ply >= 127) return evaluate(board);
    if (repeated(board.hash(), ply)) return 0;
    repetition_[ply] = board.hash();
    if (board.halfmove_clock() >= 100 || board.insufficient_material()) return 0;
    if (depth <= 0 && !board.in_check(board.side_to_move()))
        return quiescence(board, alpha, beta, ply);
    if (depth <= 0) depth = 1;

    const bool checked = board.in_check(board.side_to_move());
    const u64 non_pawns = board.pieces(board.side_to_move(), Piece::Knight) |
                          board.pieces(board.side_to_move(), Piece::Bishop) |
                          board.pieces(board.side_to_move(), Piece::Rook) |
                          board.pieces(board.side_to_move(), Piece::Queen);
    if (depth >= 3 && !checked && non_pawns && beta < mate_score - 128) {
        StateInfo null_state;
        board.make_null_move(null_state);
        repetition_[ply + 1] = board.hash();
        const i32 score = -negamax(board, depth - 3, -beta, -beta + 1, ply + 1);
        board.unmake_null_move(null_state);
        if (stopped_) return 0;
        if (score >= beta) return beta;
    }

    TTEntry& entry = table_[board.hash() & (table_.size() - 1)];
    const i32 original_alpha = alpha;
    if (entry.key == board.hash() && entry.depth >= depth) {
        if (entry.bound == Bound::Exact) return entry.score;
        if (entry.bound == Bound::Lower && entry.score >= beta) return entry.score;
        if (entry.bound == Bound::Upper && entry.score <= alpha) return entry.score;
    }

    MoveList moves;
    board.generate_legal(moves);
    if (moves.empty()) return board.in_check(board.side_to_move()) ? -mate_score + ply : 0;
    order_moves(moves, entry.key == board.hash() ? entry.best : Move{}, board.side_to_move(), ply);

    i32 best_score = -infinity;
    Move best_move{};
    i32 move_index = 0;
    for (Move move : moves) {
        StateInfo state;
        board.make_move(move, state);
        i32 score;
        if (move_index == 0) {
            score = -negamax(board, depth - 1, -beta, -alpha, ply + 1);
        } else {
            i32 reduction = 0;
            if (depth >= 3 && move_index >= 4 && !checked && !move.is_capture() &&
                !move.is_promotion()) reduction = 1 + (depth >= 6 && move_index >= 10);
            score = -negamax(board, depth - 1 - reduction, -alpha - 1, -alpha, ply + 1);
            if (score > alpha && reduction)
                score = -negamax(board, depth - 1, -alpha - 1, -alpha, ply + 1);
            if (score > alpha && score < beta)
                score = -negamax(board, depth - 1, -beta, -alpha, ply + 1);
        }
        board.unmake_move(move, state);
        if (stopped_) return 0;
        if (score > best_score) { best_score = score; best_move = move; }
        if (score > alpha) alpha = score;
        if (alpha >= beta) {
            if (!move.is_capture() && ply < 128) {
                if (!(move == killers_[ply][0])) {
                    killers_[ply][1] = killers_[ply][0];
                    killers_[ply][0] = move;
                }
                i32& history = history_[index(board.side_to_move())][move.from()][move.to()];
                history = std::min(200'000, history + depth * depth * 16);
            }
            break;
        }
        ++move_index;
    }

    entry.key = board.hash();
    entry.best = best_move;
    entry.score = best_score;
    entry.depth = static_cast<i16>(depth);
    entry.bound = best_score <= original_alpha ? Bound::Upper
                : best_score >= beta ? Bound::Lower : Bound::Exact;
    return best_score;
}

Move SearchBot::choose_move(i32 time_budget_ms) {
    MoveList root_moves;
    board_.generate_legal(root_moves);
    if (root_moves.empty()) return Move{};

    deadline_ms_ = std::max<i32>(1, time_budget_ms);
    timer_ = Timer{};
    nodes_ = 0;
    stopped_ = false;
    Move best = root_moves[0];
    i32 previous_score = 0;
    repetition_[0] = board_.hash();

    for (i32 depth = 1; depth <= 64; ++depth) {
        i32 delta = depth >= 3 ? 40 : infinity;
        i32 window_alpha = depth >= 3 ? previous_score - delta : -infinity;
        i32 window_beta = depth >= 3 ? previous_score + delta : infinity;
        Move iteration_best = best;
        i32 iteration_score = previous_score;

        for (;;) {
            order_moves(root_moves, best, board_.side_to_move(), 0);
            i32 alpha = window_alpha;
            i32 best_score = -infinity;
            Move candidate = best;
            for (Move move : root_moves) {
                StateInfo state;
                board_.make_move(move, state);
                const i32 score = -negamax(board_, depth - 1, -window_beta, -alpha, 1);
                board_.unmake_move(move, state);
                if (stopped_) break;
                if (score > best_score) { best_score = score; candidate = move; }
                if (score > alpha) alpha = score;
                if (alpha >= window_beta) break;
            }
            if (stopped_) break;
            if (best_score <= window_alpha) {
                delta *= 2;
                window_alpha = std::max(-infinity, best_score - delta);
                continue;
            }
            if (best_score >= window_beta) {
                delta *= 2;
                window_beta = std::min(infinity, best_score + delta);
                continue;
            }
            iteration_best = candidate;
            iteration_score = best_score;
            break;
        }
        if (stopped_) break;
        best = iteration_best;
        previous_score = iteration_score;
        TTEntry& root_entry = table_[board_.hash() & (table_.size() - 1)];
        root_entry.key = board_.hash();
        root_entry.best = best;
        root_entry.score = previous_score;
        root_entry.depth = static_cast<i16>(depth);
        root_entry.bound = Bound::Exact;
    }

    return best;
}

} // namespace chess::ai
