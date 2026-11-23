#include "board.h"
#include "attacks.h"
#include "magic_bitboards.h"

namespace chess {
namespace {

u8 pop_lsb(u64& bits) {
    const u8 square = static_cast<u8>(__builtin_ctzll(bits));
    bits &= bits - 1;
    return square;
}

u8 nearest_square(u64 bits, int direction) {
    const bool increasing = direction == 0 || direction == 1 ||
                            direction == 4 || direction == 7;
    return increasing ? static_cast<u8>(__builtin_ctzll(bits))
                      : static_cast<u8>(63 - __builtin_clzll(bits));
}

constexpr u64 splitmix64(u64 value) {
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
}

constexpr auto make_piece_keys() {
    eastl::array<eastl::array<eastl::array<u64, 64>, 6>, 2> keys{};
    for (u8 color = 0; color < 2; ++color)
        for (u8 piece = 0; piece < 6; ++piece)
            for (u8 square = 0; square < 64; ++square)
                keys[color][piece][square] = splitmix64(1 + square + 64 * piece + 384 * color);
    return keys;
}

inline constexpr auto piece_keys = make_piece_keys();
inline constexpr eastl::array<u64, 16> castling_keys = [] {
    eastl::array<u64, 16> keys{};
    for (u8 rights = 0; rights < 16; ++rights) keys[rights] = splitmix64(800 + rights);
    return keys;
}();
inline constexpr eastl::array<u64, 8> en_passant_keys = [] {
    eastl::array<u64, 8> keys{};
    for (u8 file = 0; file < 8; ++file) keys[file] = splitmix64(900 + file);
    return keys;
}();
inline constexpr u64 side_key = splitmix64(1000);
inline constexpr auto castling_update = [] {
    eastl::array<u8, 64> masks{};
    for (u8& mask : masks) mask = 0x0f;
    masks[0] &= u8(~white_queen);
    masks[4] &= u8(~(white_king | white_queen));
    masks[7] &= u8(~white_king);
    masks[56] &= u8(~black_queen);
    masks[60] &= u8(~(black_king | black_queen));
    masks[63] &= u8(~black_king);
    return masks;
}();

constexpr u64 piece_key(Color color, Piece piece, u8 square) {
    return piece_keys[index(color)][index(piece)][square];
}

} // namespace

Board::Board() {
    pieces_[index(Color::White)] = {
        0x000000000000ff00ULL, 0x0000000000000042ULL,
        0x0000000000000024ULL, 0x0000000000000081ULL,
        0x0000000000000008ULL, 0x0000000000000010ULL
    };
    pieces_[index(Color::Black)] = {
        0x00ff000000000000ULL, 0x4200000000000000ULL,
        0x2400000000000000ULL, 0x8100000000000000ULL,
        0x0800000000000000ULL, 0x1000000000000000ULL
    };
    occupancy_[0] = 0x000000000000ffffULL;
    occupancy_[1] = 0xffff000000000000ULL;
    for (u8 color = 0; color < 2; ++color)
        for (u8 piece = 0; piece < 6; ++piece) {
            u64 bits = pieces_[color][piece];
            while (bits) mailbox_[pop_lsb(bits)] = u8(1 + piece + 6 * color);
        }
    hash_ = compute_hash();
}

Board Board::from_fen(const char* fen) {
    Board board;
    board.pieces_ = {};
    board.occupancy_ = {};
    board.mailbox_ = {};
    board.king_square_ = {{no_square, no_square}};
    board.castling_rights_ = 0;
    board.en_passant_ = no_square;
    board.halfmove_clock_ = 0;
    board.fullmove_number_ = 1;

    int rank = 7, file = 0;
    while (*fen && *fen != ' ') {
        const char c = *fen++;
        if (c == '/') { --rank; file = 0; continue; }
        if (c >= '1' && c <= '8') { file += c - '0'; continue; }
        const Color color = c >= 'A' && c <= 'Z' ? Color::White : Color::Black;
        const char lower = c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : c;
        Piece piece = Piece::None;
        if (lower == 'p') piece = Piece::Pawn;
        else if (lower == 'n') piece = Piece::Knight;
        else if (lower == 'b') piece = Piece::Bishop;
        else if (lower == 'r') piece = Piece::Rook;
        else if (lower == 'q') piece = Piece::Queen;
        else if (lower == 'k') piece = Piece::King;
        if (piece != Piece::None) board.add_piece(color, piece, u8(rank * 8 + file++));
    }
    if (*fen == ' ') ++fen;
    board.side_to_move_ = *fen == 'b' ? Color::Black : Color::White;
    while (*fen && *fen++ != ' ') {}
    while (*fen && *fen != ' ') {
        if (*fen == 'K') board.castling_rights_ |= white_king;
        else if (*fen == 'Q') board.castling_rights_ |= white_queen;
        else if (*fen == 'k') board.castling_rights_ |= black_king;
        else if (*fen == 'q') board.castling_rights_ |= black_queen;
        ++fen;
    }
    if (*fen == ' ') ++fen;
    if (*fen >= 'a' && *fen <= 'h') {
        board.en_passant_ = u8((fen[1] - '1') * 8 + fen[0] - 'a');
        fen += 2;
    } else if (*fen == '-') ++fen;
    if (*fen == ' ') ++fen;
    u16 halfmoves = 0;
    while (*fen >= '0' && *fen <= '9') halfmoves = u16(halfmoves * 10 + *fen++ - '0');
    board.halfmove_clock_ = halfmoves;
    if (*fen == ' ') ++fen;
    u16 fullmoves = 0;
    while (*fen >= '0' && *fen <= '9') fullmoves = u16(fullmoves * 10 + *fen++ - '0');
    if (fullmoves) board.fullmove_number_ = fullmoves;
    board.hash_ = board.compute_hash();
    return board;
}

void Board::add_piece(Color color, Piece piece, u8 square) {
    pieces_[index(color)][index(piece)] |= square_bit(square);
    occupancy_[index(color)] |= square_bit(square);
    mailbox_[square] = u8(1 + index(piece) + 6 * index(color));
    if (piece == Piece::King) king_square_[index(color)] = square;
}

void Board::remove_piece(Color color, Piece piece, u8 square) {
    pieces_[index(color)][index(piece)] &= ~square_bit(square);
    occupancy_[index(color)] &= ~square_bit(square);
    mailbox_[square] = 0;
}

Piece Board::piece_on(u8 square, Color color) const {
    const u8 encoded = mailbox_[square];
    if (!encoded || static_cast<std::size_t>((encoded - 1) / 6) != index(color)) return Piece::None;
    return Piece((encoded - 1) % 6);
}

bool Board::piece_at(u8 square, Piece& piece, Color& color) const {
    const u8 encoded = mailbox_[square];
    if (!encoded) return false;
    piece = Piece((encoded - 1) % 6);
    color = Color((encoded - 1) / 6);
    return true;
}

bool Board::square_attacked(u8 square, Color by) const {
    return square_attacked(square, by, occupancy());
}

bool Board::square_attacked(u8 square, Color by, u64 occupied, u8 ignored_enemy) const {
    const auto side = index(by);
    const u64 ignored = ignored_enemy == no_square ? 0 : square_bit(ignored_enemy);
    if ((pieces_[side][index(Piece::Pawn)] & ~ignored) &
        attacks::pawn[index(opposite(by))][square]) return true;
    if ((pieces_[side][index(Piece::Knight)] & ~ignored) & attacks::knight[square]) return true;
    if ((pieces_[side][index(Piece::King)] & ~ignored) & attacks::king[square]) return true;

    const u64 queens = pieces_[side][index(Piece::Queen)] & ~ignored;
    if (magic::rook_attacks(square, occupied) &
        ((pieces_[side][index(Piece::Rook)] & ~ignored) | queens)) return true;
    if (magic::bishop_attacks(square, occupied) &
        ((pieces_[side][index(Piece::Bishop)] & ~ignored) | queens)) return true;
    return false;
}

bool Board::in_check(Color color) const {
    return square_attacked(king_square(color), opposite(color));
}

void Board::generate_pawns(MoveList& moves, Color side) const {
    u64 pawns = pieces_[index(side)][index(Piece::Pawn)];
    const int step = side == Color::White ? 8 : -8;
    const int start_rank = side == Color::White ? 1 : 6;
    const int promotion_rank = side == Color::White ? 7 : 0;
    const Color enemy = opposite(side);

    while (pawns) {
        const u8 from = pop_lsb(pawns);
        const int rank = from >> 3;
        const int one = int(from) + step;
        if (one >= 0 && one < 64 && !(occupancy() & square_bit(u8(one)))) {
            if ((one >> 3) == promotion_rank) {
                for (Piece p : {Piece::Queen, Piece::Rook, Piece::Bishop, Piece::Knight})
                    moves.emplace_back(from, u8(one), MoveFlag::promotion, Piece::Pawn, p);
            } else {
                moves.emplace_back(from, u8(one), MoveFlag::quiet, Piece::Pawn);
                const int two = int(from) + 2 * step;
                if (rank == start_rank && !(occupancy() & square_bit(u8(two))))
                    moves.emplace_back(from, u8(two), MoveFlag::double_push, Piece::Pawn);
            }
        }

        u64 captures = attacks::pawn[index(side)][from] & occupancy(enemy);
        while (captures) {
            const u8 to = pop_lsb(captures);
            if ((to >> 3) == promotion_rank) {
                for (Piece p : {Piece::Queen, Piece::Rook, Piece::Bishop, Piece::Knight})
                    moves.emplace_back(from, to, MoveFlag::promotion_capture, Piece::Pawn, p,
                                       piece_on(to, enemy));
            } else moves.emplace_back(from, to, MoveFlag::capture, Piece::Pawn, Piece::Queen,
                                      piece_on(to, enemy));
        }
        if (en_passant_ != no_square &&
            (attacks::pawn[index(side)][from] & square_bit(en_passant_)))
            moves.emplace_back(from, en_passant_, MoveFlag::en_passant, Piece::Pawn,
                               Piece::Queen, Piece::Pawn);
    }
}

void Board::generate_leapers(MoveList& moves, Color side, Piece piece) const {
    u64 sources = pieces_[index(side)][index(piece)];
    while (sources) {
        const u8 from = pop_lsb(sources);
        u64 targets = (piece == Piece::Knight ? attacks::knight[from] : attacks::king[from]) &
                      ~occupancy(side);
        while (targets) {
            const u8 to = pop_lsb(targets);
            const bool capture = occupancy(opposite(side)) & square_bit(to);
            moves.emplace_back(from, to, capture ? MoveFlag::capture : MoveFlag::quiet,
                               piece, Piece::Queen,
                               capture ? piece_on(to, opposite(side)) : Piece::None);
        }
    }
}

void Board::generate_sliders(MoveList& moves, Color side, Piece piece) const {
    u64 sources = pieces_[index(side)][index(piece)];
    while (sources) {
        const u8 from = pop_lsb(sources);
        u64 targets = 0;
        if (piece == Piece::Rook || piece == Piece::Queen)
            targets |= magic::rook_attacks(from, occupancy());
        if (piece == Piece::Bishop || piece == Piece::Queen)
            targets |= magic::bishop_attacks(from, occupancy());
        targets &= ~occupancy(side);
        while (targets) {
            const u8 to = pop_lsb(targets);
            const bool capture = (occupancy(opposite(side)) & square_bit(to)) != 0;
            moves.emplace_back(from, to, capture ? MoveFlag::capture : MoveFlag::quiet, piece,
                               Piece::Queen, capture ? piece_on(to, opposite(side)) : Piece::None);
        }
    }
}

void Board::generate_castling(MoveList& moves, Color side) const {
    if (in_check(side)) return;
    const bool white = side == Color::White;
    const u8 king = white ? 4 : 60;
    const u8 rights_k = white ? white_king : black_king;
    const u8 rights_q = white ? white_queen : black_queen;
    const u64 without_king = occupancy() & ~square_bit(king);
    if ((castling_rights_ & rights_k) &&
        (pieces_[index(side)][index(Piece::Rook)] & square_bit(king+3)) &&
        !(occupancy() & (square_bit(king+1)|square_bit(king+2))) &&
        !square_attacked(king+1, opposite(side), without_king | square_bit(king+1)) &&
        !square_attacked(king+2, opposite(side), without_king | square_bit(king+2)))
        moves.emplace_back(king, u8(king+2), MoveFlag::castle, Piece::King);
    if ((castling_rights_ & rights_q) &&
        (pieces_[index(side)][index(Piece::Rook)] & square_bit(king-4)) &&
        !(occupancy() & (square_bit(king-1)|square_bit(king-2)|square_bit(king-3))) &&
        !square_attacked(king-1, opposite(side), without_king | square_bit(king-1)) &&
        !square_attacked(king-2, opposite(side), without_king | square_bit(king-2)))
        moves.emplace_back(king, u8(king-2), MoveFlag::castle, Piece::King);
}

void Board::generate_pseudolegal(MoveList& moves) const {
    moves.clear();
    const Color side = side_to_move_;
    generate_pawns(moves, side);
    generate_leapers(moves, side, Piece::Knight);
    generate_sliders(moves, side, Piece::Bishop);
    generate_sliders(moves, side, Piece::Rook);
    generate_sliders(moves, side, Piece::Queen);
    generate_leapers(moves, side, Piece::King);
    generate_castling(moves, side);
}

void Board::generate_legal(MoveList& moves) {
    generate(moves, Generation::All);
}

void Board::generate(MoveList& moves, Generation mode) {
    if (side_to_move_ == Color::White) {
        if (mode == Generation::Captures) generate_legal_for<Color::White, Generation::Captures>(moves);
        else if (mode == Generation::Quiets) generate_legal_for<Color::White, Generation::Quiets>(moves);
        else generate_legal_for<Color::White, Generation::All>(moves);
    } else {
        if (mode == Generation::Captures) generate_legal_for<Color::Black, Generation::Captures>(moves);
        else if (mode == Generation::Quiets) generate_legal_for<Color::Black, Generation::Quiets>(moves);
        else generate_legal_for<Color::Black, Generation::All>(moves);
    }
}

template<Color Side, Generation Mode>
void Board::generate_legal_for(MoveList& moves) {
    constexpr Color Enemy = opposite(Side);
    constexpr int pawn_step = Side == Color::White ? 8 : -8;
    constexpr int pawn_start_rank = Side == Color::White ? 1 : 6;
    constexpr int promotion_rank = Side == Color::White ? 7 : 0;
    moves.clear();
    const LegalContext context = legal_context(Side);
    const u8 king = king_square_[index(Side)];
    const u64 own = occupancy_[index(Side)];
    const u64 enemy = occupancy_[index(Enemy)];
    const u64 occupied = own | enemy;

    auto allowed = [&](u8 from, u8 to) {
        const u64 destination = square_bit(to);
        if (!(context.evasion & destination)) return false;
        return !(context.pinned & square_bit(from)) ||
               (attacks::line[king][from] & destination);
    };

    // Pawns. En passant is the only non-king move requiring a final occupancy test.
    if (context.checker_count < 2) {
        u64 pawns = pieces_[index(Side)][index(Piece::Pawn)];
        while (pawns) {
            const u8 from = pop_lsb(pawns);
            if constexpr (Mode != Generation::Captures) {
                const int one = int(from) + pawn_step;
                if (!(occupied & square_bit(u8(one)))) {
                    if (allowed(from, u8(one))) {
                        if ((one >> 3) == promotion_rank) {
                            for (Piece promotion : {Piece::Queen, Piece::Rook, Piece::Bishop, Piece::Knight})
                                moves.emplace_back(from, u8(one), MoveFlag::promotion, Piece::Pawn, promotion);
                        } else {
                            moves.emplace_back(from, u8(one), MoveFlag::quiet, Piece::Pawn);
                        }
                    }
                    const int two = int(from) + 2 * pawn_step;
                    if ((from >> 3) == pawn_start_rank && !(occupied & square_bit(u8(two))) &&
                        allowed(from, u8(two)))
                        moves.emplace_back(from, u8(two), MoveFlag::double_push, Piece::Pawn);
                }
            }
            if constexpr (Mode != Generation::Quiets) {
                u64 targets = attacks::pawn[index(Side)][from] & enemy;
                while (targets) {
                    const u8 to = pop_lsb(targets);
                    if (!allowed(from, to)) continue;
                    if ((to >> 3) == promotion_rank) {
                        for (Piece promotion : {Piece::Queen, Piece::Rook, Piece::Bishop, Piece::Knight})
                            moves.emplace_back(from, to, MoveFlag::promotion_capture, Piece::Pawn,
                                               promotion, piece_on(to, Enemy));
                    } else moves.emplace_back(from, to, MoveFlag::capture, Piece::Pawn,
                                              Piece::Queen, piece_on(to, Enemy));
                }
                if (en_passant_ != no_square &&
                    (attacks::pawn[index(Side)][from] & square_bit(en_passant_))) {
                    Move move(from, en_passant_, MoveFlag::en_passant, Piece::Pawn,
                              Piece::Queen, Piece::Pawn);
                    if (legal_after(move, Side)) moves.push_back(move);
                }
            }
        }

        // Knights cannot move at all when pinned.
        u64 knights = pieces_[index(Side)][index(Piece::Knight)] & ~context.pinned;
        while (knights) {
            const u8 from = pop_lsb(knights);
            u64 targets = attacks::knight[from] & context.evasion & ~own;
            if constexpr (Mode == Generation::Captures) targets &= enemy;
            if constexpr (Mode == Generation::Quiets) targets &= ~enemy;
            while (targets) {
                const u8 to = pop_lsb(targets);
                const bool capture = enemy & square_bit(to);
                moves.emplace_back(from, to, capture ? MoveFlag::capture : MoveFlag::quiet,
                                   Piece::Knight, Piece::Queen,
                                   capture ? piece_on(to, Enemy) : Piece::None);
            }
        }

        // Bishops, rooks and queens use one or two magic lookups respectively.
        for (Piece piece : {Piece::Bishop, Piece::Rook, Piece::Queen}) {
            u64 sources = pieces_[index(Side)][index(piece)];
            while (sources) {
                const u8 from = pop_lsb(sources);
                u64 targets = 0;
                if (piece != Piece::Bishop) targets |= magic::rook_attacks(from, occupied);
                if (piece != Piece::Rook) targets |= magic::bishop_attacks(from, occupied);
                targets &= context.evasion & ~own;
                if (context.pinned & square_bit(from)) targets &= attacks::line[king][from];
                if constexpr (Mode == Generation::Captures) targets &= enemy;
                if constexpr (Mode == Generation::Quiets) targets &= ~enemy;
                while (targets) {
                    const u8 to = pop_lsb(targets);
                    const bool capture = enemy & square_bit(to);
                    moves.emplace_back(from, to, capture ? MoveFlag::capture : MoveFlag::quiet,
                                       piece, Piece::Queen,
                                       capture ? piece_on(to, Enemy) : Piece::None);
                }
            }
        }
    }

    // King destinations need the resulting occupancy because the king itself may unblock a ray.
    u64 king_targets = attacks::king[king] & ~own;
    if constexpr (Mode == Generation::Captures) king_targets &= enemy;
    if constexpr (Mode == Generation::Quiets) king_targets &= ~enemy;
    while (king_targets) {
        const u8 to = pop_lsb(king_targets);
        const bool capture = enemy & square_bit(to);
        Move move(king, to, capture ? MoveFlag::capture : MoveFlag::quiet, Piece::King,
                  Piece::Queen, capture ? piece_on(to, Enemy) : Piece::None);
        if (legal_after(move, Side)) moves.push_back(move);
    }
    if constexpr (Mode != Generation::Captures)
        if (context.checker_count == 0) generate_castling(moves, Side);
}

Board::LegalContext Board::legal_context(Color side) const {
    LegalContext context;
    const Color enemy = opposite(side);
    const auto enemy_index = index(enemy);
    const u8 king = king_square(side);
    const u64 occupied = occupancy();

    context.checkers |= pieces_[enemy_index][index(Piece::Pawn)] &
                        attacks::pawn[index(side)][king];
    context.checkers |= pieces_[enemy_index][index(Piece::Knight)] & attacks::knight[king];
    context.checkers |= pieces_[enemy_index][index(Piece::King)] & attacks::king[king];
    context.checkers |= magic::rook_attacks(king, occupied) &
        (pieces_[enemy_index][index(Piece::Rook)] | pieces_[enemy_index][index(Piece::Queen)]);
    context.checkers |= magic::bishop_attacks(king, occupied) &
        (pieces_[enemy_index][index(Piece::Bishop)] | pieces_[enemy_index][index(Piece::Queen)]);
    context.checker_count = static_cast<u8>(__builtin_popcountll(context.checkers));

    if (context.checker_count == 1) {
        const u8 checker = static_cast<u8>(__builtin_ctzll(context.checkers));
        context.evasion = square_bit(checker) | attacks::between[king][checker];
    } else if (context.checker_count > 1) {
        context.evasion = 0;
    }

    for (int direction = 0; direction < 8; ++direction) {
        u64 blockers = attacks::ray[direction][king] & occupied;
        if (!blockers) continue;
        const u8 candidate = nearest_square(blockers, direction);
        if (!(occupancy(side) & square_bit(candidate))) continue;
        blockers = attacks::ray[direction][candidate] & occupied;
        if (!blockers) continue;
        const u8 pinner = nearest_square(blockers, direction);
        const Piece pinner_piece = piece_on(pinner, enemy);
        const bool valid_slider = pinner_piece == Piece::Queen ||
            (direction < 4 && pinner_piece == Piece::Rook) ||
            (direction >= 4 && pinner_piece == Piece::Bishop);
        if (!valid_slider) continue;
        context.pinned |= square_bit(candidate);
    }
    return context;
}

bool Board::legal_after(Move move, Color side) const {
    const Piece moving = move.piece();
    u64 occupied = occupancy() & ~square_bit(move.from());
    u8 captured = no_square;

    if (move.flag() == MoveFlag::en_passant) {
        captured = u8(int(move.to()) + (side == Color::White ? -8 : 8));
        occupied &= ~square_bit(captured);
    } else if (move.is_capture()) {
        captured = move.to();
        occupied &= ~square_bit(captured);
    }
    occupied |= square_bit(move.to());

    // Castling transit and destination were checked while generating it.
    if (move.flag() == MoveFlag::castle) return true;
    const u8 king = moving == Piece::King ? move.to() : king_square(side);
    return !square_attacked(king, opposite(side), occupied, captured);
}

namespace {
constexpr u64 pack_state(u8 castling, u8 en_passant, u16 halfmove, u16 fullmove) {
    return u64(castling) | (u64(en_passant) << 4) | (u64(halfmove) << 11) |
           (u64(fullmove) << 27);
}
constexpr u8 state_castling(u64 state) { return u8(state & 0x0f); }
constexpr u8 state_en_passant(u64 state) { return u8((state >> 4) & 0x7f); }
constexpr u16 state_halfmove(u64 state) { return u16((state >> 11) & 0xffff); }
constexpr u16 state_fullmove(u64 state) { return u16((state >> 27) & 0xffff); }
}

void Board::make_null_move(StateInfo& state) {
    state.hash = hash_;
    state.metadata = pack_state(castling_rights_, en_passant_, halfmove_clock_, fullmove_number_);
    if (en_passant_ != no_square) hash_ ^= en_passant_keys[en_passant_ & 7];
    en_passant_ = no_square;
    ++halfmove_clock_;
    if (side_to_move_ == Color::Black) ++fullmove_number_;
    side_to_move_ = opposite(side_to_move_);
    hash_ ^= side_key;
}

void Board::unmake_null_move(const StateInfo& state) {
    side_to_move_ = opposite(side_to_move_);
    castling_rights_ = state_castling(state.metadata);
    en_passant_ = state_en_passant(state.metadata);
    halfmove_clock_ = state_halfmove(state.metadata);
    fullmove_number_ = state_fullmove(state.metadata);
    hash_ = state.hash;
}

template<MoveFlag Flag>
void Board::make_move_impl(Move move, StateInfo& state) {
    constexpr bool capture = Flag == MoveFlag::capture || Flag == MoveFlag::en_passant ||
                             Flag == MoveFlag::promotion_capture;
    constexpr bool promotion = Flag == MoveFlag::promotion ||
                               Flag == MoveFlag::promotion_capture;
    constexpr bool common = Flag == MoveFlag::quiet || Flag == MoveFlag::capture ||
                            Flag == MoveFlag::double_push;
    const Color side = side_to_move_, enemy = opposite(side);
    state.hash = hash_;
    state.metadata = pack_state(castling_rights_, en_passant_, halfmove_clock_, fullmove_number_);
    const Piece moving = move.piece();
    const u8 from = move.from(), to = move.to();
    const u64 from_bit = square_bit(from), to_bit = square_bit(to);
    const u64 move_mask = from_bit | to_bit;
    u64 next_hash = hash_ ^ piece_key(side, moving, from) ^ castling_keys[castling_rights_];
    if (en_passant_ != no_square) next_hash ^= en_passant_keys[en_passant_ & 7];

    if constexpr (common) {
        if constexpr (Flag == MoveFlag::capture) {
            pieces_[index(enemy)][index(move.captured())] ^= to_bit;
            occupancy_[index(enemy)] ^= to_bit;
            next_hash ^= piece_key(enemy, move.captured(), to);
        }
        pieces_[index(side)][index(moving)] ^= move_mask;
        occupancy_[index(side)] ^= move_mask;
        mailbox_[from] = 0;
        mailbox_[to] = u8(1 + index(moving) + 6 * index(side));
        if (moving == Piece::King) king_square_[index(side)] = to;
        next_hash ^= piece_key(side, moving, to);
    } else {
        u8 captured_square = to;
        if constexpr (Flag == MoveFlag::en_passant)
            captured_square = u8(int(to) + (side == Color::White ? -8 : 8));
        if constexpr (capture) {
            next_hash ^= piece_key(enemy, move.captured(), captured_square);
            remove_piece(enemy, move.captured(), captured_square);
        }
        remove_piece(side, moving, from);
        const Piece placed = promotion ? move.promotion() : moving;
        add_piece(side, placed, to);
        next_hash ^= piece_key(side, placed, to);
    }

    if constexpr (Flag == MoveFlag::castle) {
        const bool king_side = to > from;
        const u8 rook_from = u8(from + (king_side ? 3 : -4));
        const u8 rook_to = u8(from + (king_side ? 1 : -1));
        remove_piece(side, Piece::Rook, rook_from);
        add_piece(side, Piece::Rook, rook_to);
        next_hash ^= piece_key(side, Piece::Rook, rook_from) ^ piece_key(side, Piece::Rook, rook_to);
    }

    castling_rights_ &= castling_update[from] & castling_update[to];
    en_passant_ = Flag == MoveFlag::double_push ? u8((int(from) + int(to)) / 2) : no_square;
    halfmove_clock_ = (moving == Piece::Pawn || capture) ? 0 : u16(halfmove_clock_ + 1);
    if (side == Color::Black) ++fullmove_number_;
    side_to_move_ = enemy;
    next_hash ^= castling_keys[castling_rights_];
    if (en_passant_ != no_square) next_hash ^= en_passant_keys[en_passant_ & 7];
    hash_ = next_hash ^ side_key;
}

template<MoveFlag Flag>
void Board::unmake_move_impl(Move move, const StateInfo& state) {
    constexpr bool capture = Flag == MoveFlag::capture || Flag == MoveFlag::en_passant ||
                             Flag == MoveFlag::promotion_capture;
    constexpr bool promotion = Flag == MoveFlag::promotion ||
                               Flag == MoveFlag::promotion_capture;
    constexpr bool common = Flag == MoveFlag::quiet || Flag == MoveFlag::capture ||
                            Flag == MoveFlag::double_push;
    side_to_move_ = opposite(side_to_move_);
    const Color side = side_to_move_, enemy = opposite(side);
    const u8 from = move.from(), to = move.to();
    const u64 from_bit = square_bit(from), to_bit = square_bit(to);
    const u64 move_mask = from_bit | to_bit;

    if constexpr (common) {
        pieces_[index(side)][index(move.piece())] ^= move_mask;
        occupancy_[index(side)] ^= move_mask;
        mailbox_[from] = u8(1 + index(move.piece()) + 6 * index(side));
        mailbox_[to] = 0;
        if (move.piece() == Piece::King) king_square_[index(side)] = from;
        if constexpr (Flag == MoveFlag::capture) {
            pieces_[index(enemy)][index(move.captured())] ^= to_bit;
            occupancy_[index(enemy)] ^= to_bit;
            mailbox_[to] = u8(1 + index(move.captured()) + 6 * index(enemy));
        }
    } else {
        const Piece moved = promotion ? move.promotion() : move.piece();
        remove_piece(side, moved, to);
        add_piece(side, move.piece(), from);
        if constexpr (capture) {
            const u8 captured_square = Flag == MoveFlag::en_passant
                ? u8(int(to) + (side == Color::White ? -8 : 8)) : to;
            add_piece(enemy, move.captured(), captured_square);
        }
    }
    if constexpr (Flag == MoveFlag::castle) {
        const bool king_side = to > from;
        const u8 rook_from = u8(from + (king_side ? 3 : -4));
        const u8 rook_to = u8(from + (king_side ? 1 : -1));
        remove_piece(side, Piece::Rook, rook_to);
        add_piece(side, Piece::Rook, rook_from);
    }
    castling_rights_ = state_castling(state.metadata);
    en_passant_ = state_en_passant(state.metadata);
    halfmove_clock_ = state_halfmove(state.metadata);
    fullmove_number_ = state_fullmove(state.metadata);
    hash_ = state.hash;
}

void Board::make_move(Move move, StateInfo& state) {
    switch (move.flag()) {
        case MoveFlag::quiet: make_move_impl<MoveFlag::quiet>(move, state); break;
        case MoveFlag::capture: make_move_impl<MoveFlag::capture>(move, state); break;
        case MoveFlag::double_push: make_move_impl<MoveFlag::double_push>(move, state); break;
        case MoveFlag::en_passant: make_move_impl<MoveFlag::en_passant>(move, state); break;
        case MoveFlag::castle: make_move_impl<MoveFlag::castle>(move, state); break;
        case MoveFlag::promotion: make_move_impl<MoveFlag::promotion>(move, state); break;
        case MoveFlag::promotion_capture: make_move_impl<MoveFlag::promotion_capture>(move, state); break;
    }
}

UndoState Board::make_move(Move move) {
    StateInfo state;
    make_move(move, state);
    return state;
}

void Board::unmake_move(Move move, const UndoState& state) {
    switch (move.flag()) {
        case MoveFlag::quiet: unmake_move_impl<MoveFlag::quiet>(move, state); break;
        case MoveFlag::capture: unmake_move_impl<MoveFlag::capture>(move, state); break;
        case MoveFlag::double_push: unmake_move_impl<MoveFlag::double_push>(move, state); break;
        case MoveFlag::en_passant: unmake_move_impl<MoveFlag::en_passant>(move, state); break;
        case MoveFlag::castle: unmake_move_impl<MoveFlag::castle>(move, state); break;
        case MoveFlag::promotion: unmake_move_impl<MoveFlag::promotion>(move, state); break;
        case MoveFlag::promotion_capture: unmake_move_impl<MoveFlag::promotion_capture>(move, state); break;
    }
}

bool Board::insufficient_material() const {
    if (pieces_[0][index(Piece::Pawn)] || pieces_[1][index(Piece::Pawn)] ||
        pieces_[0][index(Piece::Rook)] || pieces_[1][index(Piece::Rook)] ||
        pieces_[0][index(Piece::Queen)] || pieces_[1][index(Piece::Queen)]) return false;
    const u64 knights = pieces_[0][index(Piece::Knight)] | pieces_[1][index(Piece::Knight)];
    const u64 bishops = pieces_[0][index(Piece::Bishop)] | pieces_[1][index(Piece::Bishop)];
    if (__builtin_popcountll(knights | bishops) <= 1) return true;
    if (!knights && bishops) {
        constexpr u64 dark = 0xaa55aa55aa55aa55ULL;
        return !(bishops & dark) || !(bishops & ~dark);
    }
    return false;
}

u64 Board::compute_hash() const {
    u64 result = 0;
    for (u8 color = 0; color < 2; ++color)
        for (u8 piece = 0; piece < 6; ++piece) {
            u64 bits = pieces_[color][piece];
            while (bits) result ^= piece_key(Color(color), Piece(piece), pop_lsb(bits));
        }
    result ^= castling_keys[castling_rights_];
    if (en_passant_ != no_square) result ^= en_passant_keys[en_passant_ & 7];
    if (side_to_move_ == Color::Black) result ^= side_key;
    return result;
}

} // namespace chess
