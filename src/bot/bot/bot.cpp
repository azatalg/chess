#include "bot.h"

namespace chess::ai {

Bot::Bot(const Board& board, Color color) : board_(board), color_(color) {}

int Bot::simulate(Board position, Color perspective, const Timer& timer, i32 deadline_ms) {
    // The cap prevents pathological random games from running forever.
    for (int ply = 0; ply < 512; ++ply) {
        if (timer.elapsed_milliseconds() >= deadline_ms) return 0;
        MoveList moves;
        position.generate_legal(moves);
        if (moves.empty()) {
            if (!position.in_check(position.side_to_move())) return 0;
            return position.side_to_move() == perspective ? -1 : 1;
        }
        if (position.halfmove_clock() >= 100 || position.insufficient_material()) return 0;
        position.make_move(moves[random_.next() % moves.size()]);
    }
    return 0;
}

} // namespace chess::ai
