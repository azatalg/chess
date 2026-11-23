#pragma once

#include "board.h"
#include <EASTL/vector.h>

namespace chess {

enum class GameResult { Ongoing, WhiteWins, BlackWins, Stalemate, FiftyMoveDraw,
                        RepetitionDraw, InsufficientMaterialDraw };

class Game {
public:
    Game();

    Board& board() { return board_; }
    const Board& board() const { return board_; }
    Color side_to_move() const { return board_.side_to_move(); }
    bool in_check() const { return board_.in_check(board_.side_to_move()); }

    void legal_moves(MoveList& moves) { board_.generate_legal(moves); }
    void play(Move move);
    GameResult result();

private:
    Board board_;
    eastl::vector<u64> position_history_;

    bool threefold_repetition() const;
};

} // namespace chess
