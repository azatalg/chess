#pragma once
#include <bits/stdc++.h>
#include "bot.h"
#include "utils/utils.cpp"

Bot::Bot(Board board, Color color)
    :board(board), color(color) {}

Color Bot::simulate_till_end() {
    while(!board_copy.end_game()) { //TODO
        std::vector<Moves> moves = board_copy.generate_move(color_copy); //TODO
        const Moves &move = moves[Random::random() % moves.size()];

        board_copy.make_move(move);
        color_copy = opposite(color_copy);
    }

    return opposite(color_copy); // return winner of the simulate
}

void Bot::make_board_copy(){
    board_copy = board;
}