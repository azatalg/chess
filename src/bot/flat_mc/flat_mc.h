#pragma once 
#include "../bot/bot.h"
#include "../config.h";
#include "../../game/board.h"
#include "../../game/moves.h"

class Flat_mc : Bot {
public:
    Moves flat_mc();
    Flat_mc(Board board, Color color);
};