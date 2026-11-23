#pragma once

#include "move.h"
#include <EASTL/array.h>

namespace chess {

enum CastlingRight : u8 { white_king = 1, white_queen = 2, black_king = 4, black_queen = 8 };

struct StateInfo {
    u64 hash = 0;
    u64 metadata = 0;
};
using UndoState = StateInfo;
static_assert(sizeof(StateInfo) == 16, "StateInfo must remain two machine words");

class Board {
public:
    Board();
    static Board from_fen(const char* fen);

    Color side_to_move() const { return side_to_move_; }
    u64 occupancy() const { return occupancy_[0] | occupancy_[1]; }
    u64 occupancy(Color color) const { return occupancy_[index(color)]; }
    u64 pieces(Color color, Piece piece) const { return pieces_[index(color)][index(piece)]; }
    u8 en_passant_square() const { return en_passant_; }
    u8 castling_rights() const { return castling_rights_; }
    u16 halfmove_clock() const { return halfmove_clock_; }
    u64 hash() const { return hash_; }

    bool piece_at(u8 square, Piece& piece, Color& color) const;
    bool square_attacked(u8 square, Color by) const;
    bool in_check(Color color) const;
    bool insufficient_material() const;

    void generate_pseudolegal(MoveList& moves) const;
    void generate_legal(MoveList& moves);
    void generate(MoveList& moves, Generation mode);
    UndoState make_move(Move move);
    void make_move(Move move, StateInfo& state);
    void unmake_move(Move move, const UndoState& undo);
    void make_null_move(StateInfo& state);
    void unmake_null_move(const StateInfo& state);

private:
    struct LegalContext {
        u64 checkers = 0;
        u64 evasion = ~0ULL;
        u64 pinned = 0;
        u8 checker_count = 0;
    };

    // Piece bitboards and occupancies form the hot attack-generation block.
    eastl::array<eastl::array<u64, 6>, 2> pieces_{};
    eastl::array<u64, 2> occupancy_{};
    eastl::array<u8, 2> king_square_{{4, 60}};
    // 0 means empty; otherwise 1 + piece + 6 * color. O(1) piece lookup.
    eastl::array<u8, 64> mailbox_{};
    Color side_to_move_ = Color::White;
    u8 castling_rights_ = white_king | white_queen | black_king | black_queen;
    u8 en_passant_ = no_square;
    u16 halfmove_clock_ = 0;
    u16 fullmove_number_ = 1;
    u64 hash_ = 0;

    void add_piece(Color color, Piece piece, u8 square);
    void remove_piece(Color color, Piece piece, u8 square);
    Piece piece_on(u8 square, Color color) const;
    u8 king_square(Color color) const { return king_square_[index(color)]; }
    u64 compute_hash() const;
    bool square_attacked(u8 square, Color by, u64 occupied,
                         u8 ignored_enemy = no_square) const;
    bool legal_after(Move move, Color side) const;
    LegalContext legal_context(Color side) const;
    template<Color Side, Generation Mode>
    void generate_legal_for(MoveList& moves);
    template<MoveFlag Flag>
    void make_move_impl(Move move, StateInfo& state);
    template<MoveFlag Flag>
    void unmake_move_impl(Move move, const StateInfo& state);
    void generate_pawns(MoveList& moves, Color side) const;
    void generate_leapers(MoveList& moves, Color side, Piece piece) const;
    void generate_sliders(MoveList& moves, Color side, Piece piece) const;
    void generate_castling(MoveList& moves, Color side) const;
};

} // namespace chess
