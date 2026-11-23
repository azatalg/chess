#pragma once

#include "../../game/board.h"
#include "../utils/utils.h"

namespace chess::ai {

class Bot {
public:
    explicit Bot(const Board& board, Color color);
    virtual ~Bot() = default;

protected:
    Board board_;
    Color color_;
    Random random_;

    int simulate(Board position, Color perspective, const Timer& timer, i32 deadline_ms);
};

} // namespace chess::ai
