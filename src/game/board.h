#pragma once

#include "./moves.h"
#include "./pieces_shitfs.h"
#include <vector>
#include <cmath>
using std::vector;

class Board {
i8 get_bit(i8 x, i8 y) const {
	return y*8 + x;
};
bool is_blocked(int x, int y, Color color) const {
	i8 bit = get_bit(x, y);
	return (board[U64(color)] & (1 << U64(bit))) != 0;
};

public:
    u64 board[3];
	// board[0] → all white pieces
	// board[1] → all black pieces
	// board[2] → all pieces (white | black)
    u64 pieces[6][2];
	u8 king_position[2];
	bool castle_rights[2];
    Board() {
        /*  WHITE  -------------------------------------------------- */
        pieces[U64(Piece::PAWN)]   [U64(Color::WHITE)] = 0x000000000000FF00ULL;   // 8‑15
        pieces[U64(Piece::KNIGHT)] [U64(Color::WHITE)] = 0x0000000000000042ULL;   // b1, g1
        pieces[U64(Piece::BISHOP)] [U64(Color::WHITE)] = 0x0000000000000024ULL;   // c1, f1
        pieces[U64(Piece::ROOK)]   [U64(Color::WHITE)] = 0x0000000000000081ULL;   // a1, h1
        pieces[U64(Piece::QUEEN)]  [U64(Color::WHITE)] = 0x0000000000000008ULL;   // d1
        pieces[U64(Piece::KING)]   [U64(Color::WHITE)] = 0x0000000000000010ULL;   // e1

        /*  BLACK  -------------------------------------------------- */
        pieces[U64(Piece::PAWN)]   [U64(Color::BLACK)] = 0x00FF000000000000ULL;   // 48‑55
        pieces[U64(Piece::KNIGHT)] [U64(Color::BLACK)] = 0x4200000000000000ULL;   // b8, g8
        pieces[U64(Piece::BISHOP)] [U64(Color::BLACK)] = 0x2400000000000000ULL;   // c8, f8
        pieces[U64(Piece::ROOK)]   [U64(Color::BLACK)] = 0x8100000000000000ULL;   // a8, h8
        pieces[U64(Piece::QUEEN)]  [U64(Color::BLACK)] = 0x0800000000000000ULL;   // d8
        pieces[U64(Piece::KING)]   [U64(Color::BLACK)] = 0x1000000000000000ULL;   // e8
		
		board[U64(Color::WHITE)] = 0x000000000000FFFFULL;  // white: ranks 1&2
        board[U64(Color::BLACK)] = 0xFFFF000000000000ULL,  // black: ranks 7&8
        board[2] = 0xFFFF00000000FFFFULL;				   // all: white + black
		king_position[U64(Color::WHITE)] = 3;
		king_position[U64(Color::BLACK)] = 59;
		castle_rights[U64(Color::WHITE)] = true;
		castle_rights[U64(Color::BLACK)] = true;
    }
	Piece get_type(u8 bit, Color color);
	bool is_king_attacked(u64 bo, Color color, Color turn);
	vector<Moves> get_piece_legal_move(u8 place, Piece type, Color color, bool canCastle);
	vector<Moves> get_all_legal_moves(Color color);
	void updateBoard(Moves& move, Color color) {
		u8 from = move.getStart();
		u8 to = move.getEnd();
		u8 fromCastle = move.getStartCast();
		u8 toCastle = move.getEndCast();
		Piece piece = move.getPiece();
		board[U8(color)] ^= 1<<from;
		board[U8(color)] |= 1<<to;
		board[2] ^= 1<<from;
		board[2] |= 1<<to;
		if(piece == Piece::KING && castle_rights[U8(color)] && std::abs(I8(to)-I8(from)) ==  2) {
			board[U8(color)] ^= fromCastle;
			board[U8(color)] |= toCastle;
			board[2] ^= fromCastle;
			board[2] |= toCastle;
		}
	}
};

