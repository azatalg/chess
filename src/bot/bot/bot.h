#pragma once
#include "../config.h";
#include "../game/board.h"
#include "../game/moves.h"

class Bot {
protected:
    // data
    Board board;
    Color color;

    Board board_copy;
    Color color_copy;

    // heuristic evaluation methods
    Color simulate_till_end();

    // utils
    void make_board_copy();

public:
    // public methods
    Bot(Board board, Color color);
};