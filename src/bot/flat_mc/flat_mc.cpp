#include "flat_mc.h"
#include <EASTL/fixed_vector.h>
#include <limits>

namespace chess::ai {

Move FlatMonteCarlo::choose_move(i32 time_budget_ms) {
    MoveList moves;
    board_.generate_legal(moves);
    if (moves.empty()) return Move{};

    eastl::fixed_vector<i32, 256, false> scores(moves.size(), 0);
    eastl::fixed_vector<i32, 256, false> visits(moves.size(), 0);
    Timer timer;
    std::size_t cursor = 0;
    while (timer.elapsed_milliseconds() < time_budget_ms) {
        Board position = board_;
        position.make_move(moves[cursor]);
        scores[cursor] += simulate(position, color_, timer, time_budget_ms);
        ++visits[cursor];
        cursor = (cursor + 1) % moves.size();
    }

    std::size_t best = random_.next() % moves.size();
    for (std::size_t i = 0; i < moves.size(); ++i) {
        if (!visits[i]) continue;
        if (!visits[best] || scores[i] * visits[best] > scores[best] * visits[i]) best = i;
    }
    return moves[best];
}

} // namespace chess::ai
