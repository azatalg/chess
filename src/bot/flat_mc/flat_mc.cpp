#pragma once  
#include <bits/stdc++.h>
#include "flat_mc.h"
#include "utils/utils.h"


Moves Flat_mc::flat_mc(){
    std::vector<Moves> moves = board.generate_moves(color); // TODO

    i32 best_score = -INF, arg_best_score = 0;
    for(int i = 0; i < moves.size(); i++){
        const Moves &move = moves[i];
        i32 score = 0;

        Timer timer;
        while(timer.elapsed_time() < timer.M_TIME / moves.size()){
            for(int j = 0; j < 20; j++){
                make_board_copy();
                board_copy.make_move(move); // TODO
                color_copy = opposite(color);

                score += simulate_till_end() == color ? 1 : -1; 
            }
        }

        if(score > best_score){
            best_score = score;
            arg_best_score = i;
        }
    }

    return moves[best_score];
}

Flat_mc::Flat_mc(Board board, Color color)
    : Bot(board, color) {}