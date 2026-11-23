#pragma once

#include "../bot/bot.h"

namespace chess::ai {

class FlatMonteCarlo final : public Bot {
public:
    FlatMonteCarlo(const Board& board, Color color) : Bot(board, color) {}
    Move choose_move(i32 time_budget_ms = 100);
};

using Flat_mc [[deprecated("Use FlatMonteCarlo")]] = FlatMonteCarlo;

} // namespace chess::ai
