#pragma once
#include "./board.h"

class Game {
	Board board;
	std::vector<Moves> moves;
	Color curPlayer;
	bool canCastle[2];
public:
	bool is_played;
	Game() {
		curPlayer = Color::WHITE;
		is_played = true;
	}

	void moveExe(Moves& move) {
		moves.push_back(move);
		board.updateBoard(move, curPlayer);
		curPlayer = opposite(curPlayer);
	}

	bool isCheck() {
		return board.is_king_attacked(board.board[2], curPlayer, curPlayer);	
	}
	bool isCheckmate() {
		if(!isCheck()) return false;
		vector<Moves> moves = board.get_all_legal_moves(curPlayer);
		if(moves.size() != 0) return false;
		is_played = false;
		return true;
	}
	bool isStelmate() {
		if(isCheck()) return false;
		vector<Moves> moves = board.get_all_legal_moves(curPlayer);
		if(moves.size() != 0) return false;
		is_played = false;
		return true;
	}

	vector<Moves> getMoves() {
		return board.get_all_legal_moves(curPlayer);
	}
};
