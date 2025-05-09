#include "./board.h"
#include <cmath>
#include <iostream>
Piece Board::get_type(u8 pos, Color color) {
	for(u8 i=0; i<6; i++) {
		if(pieces[i][U8(color)] & (1<<pos)) {
			return static_cast<Piece>(i);
		}
	}
	return Piece::PAWN;
}

// true if both squares lie on the same rank (row 0..7)
inline bool checkRow(u8 from, u8 to) {
    return (from >> 3) == (to >> 3);
}

// true if both squares lie on the same file (column a..h)
inline bool checkColumn(u8 from, u8 to) {
    return (from & 7) == (to & 7);
}

// true if both squares lie on the “a1–h8” diagonal (↗ or ↙)
inline bool checkDiagRight(u8 from, u8 to) {
    return ((from >> 3) + (from & 7)) == ((to >> 3) + (to & 7));
}

// true if both squares lie on the “h1–a8” diagonal (↖ or ↘)
inline bool checkDiagLeft(u8 from, u8 to) {
    return ((from >> 3) - (from & 7)) == ((to >> 3) - (to & 7));
}

inline bool checkIfFree(u64 friendB, u8 move) {
	return !(friendB & (1<<move));
} 

// true if ‘to’ is a valid knight‐move away from ‘from’
inline bool checkKnight(u8 from, u8 to) {
    int fileDiff =  static_cast<int>((from & 7)) - static_cast<int>((to & 7));
    int rankDiff =  static_cast<int>((from >> 3)) - static_cast<int>((to >> 3));
    fileDiff = fileDiff < 0 ? -fileDiff : fileDiff;
    rankDiff = rankDiff < 0 ? -rankDiff : rankDiff;
    return (fileDiff == 1 && rankDiff == 2)
        || (fileDiff == 2 && rankDiff == 1);
}



bool Board::is_king_attacked(u64 b, Color color, Color turn) {
	u8 king = king_position[U64(color)];

	const i8* shifts = queen_shifts;

	// 0 - column, 1 - row, 2 - right diagonal, 3 - left diagonal
	for(i8 i = 0; i<4; i++) {
		for(i8 j = 0; j<14; j++) {
			u8 pos = U8(shifts[i*14+j]) + king;

			if(pos > 63 || pos < 0) continue;
			if((i==0 || i == 1) && !checkColumn(king, pos)) continue;
			if((i==2 || i == 2) && !checkRow(king, pos)) continue;

			if(board[U8(opposite(color))] & (1<<pos)) {
				Piece type = get_type(pos, opposite(color));
				if(j == 0 && type == Piece::KING) return true;
				if(j == 0 && (i == 2 || i == 3) && type == Piece::PAWN) return true;
				if(type == Piece::QUEEN || (type == Piece::BISHOP && (i==2 || i==3)) || (type == Piece::ROOK && (i==0 || i == 1))) {
					return true;
				}
				break; // stop checking the line if piece was found
			}
			if(b & (1<<pos)) {
				break; // stop checking the line if piece was found
			}
		}
	}
	return false;
}

std::vector<Moves> get_rook_moves(u8 from, Color color, Board& board, Piece piece, Color turn) {
	vector<Moves> moves;
	u64 b = board.board[2];
	u64 friendB = board.board[U8(color)];
	u64 enemyB  = board.board[U8(opposite(color))];
	const i8* shifts = rook_shifts;
	for(i8 i = 0; i<4; i++) {
		for(i8 j = 0; j<7; j++) {
			i8 to = from + shifts[7*i + j];
			if(to < 0 || to > 63) break;
			if((i==0 || i == 1) && !checkColumn(from, to)) break;
			if((i==2 || i == 3) && !checkRow(from, to)) break;
			if(!checkIfFree(friendB, to)) break;
			if(!board.is_king_attacked((b^(1<<from))|(1<<to), color, turn)) break;
			moves.push_back(Moves(from, U8(to), U8(0), U8(0), piece));
			if(b & U64(1<<to)) break;
		}
	}
	return moves;
}

std::vector<Moves> get_bishop_moves(u8 from, Color color, Board& board, Piece piece, Color turn) {
	vector<Moves> moves;
	const i8* shifts = bishop_shifts;
	u64 b = board.board[2];
	u64 friendB = board.board[U8(color)];
	for(i8 i = 0; i<4; i++) {
		for(i8 j = 0; j<7; j++) {
			i8 to = from + shifts[7*i + j];
			if(to < 0 || to > 63) break;
			if((i==0 || i == 1) && !checkDiagRight(from, to)) break;
			if((i==2 || i == 3) && !checkDiagLeft(from, to)) break;
			if(!checkIfFree(friendB, to)) break;
			if(board.is_king_attacked((b^(1<<from))|(1<<to), color, turn)) break;
			moves.push_back(Moves(from, U8(to), U8(0), U8(0), piece));
			if(b & U64(1<<to)) break;
		}
	}
	return moves;
}

std::vector<Moves> get_queen_moves(u8 from, Color color, Board& board, Piece piece, Color turn) {
	vector<Moves> moves = get_rook_moves(from, color, board, Piece::QUEEN, turn);
	vector<Moves> moves2 = get_bishop_moves(from, color, board, Piece::QUEEN, turn);
	for(auto& i: moves2) moves.push_back(i);
	return moves;
}

std::vector<Moves> get_knight_moves(u8 from, Color color, Board& board, Piece piece, Color turn) {
	vector<Moves> moves;
	u64 b = board.board[2];
	u64 friendB = board.board[U8(color)];
	const i8* shifts = knight_shifts;
	for(i8 i = 0; i<8; i++) {
		i8 to = from + shifts[i];
		if(to < 0 || to > 63) continue;
		if(!checkKnight(from, to)) continue;
		if(!checkIfFree(friendB, to)) continue;
		if(board.is_king_attacked((b^(1<<from))|(1<<to), color, turn)) break;
		moves.push_back(Moves(from, U8(to), U8(0), U8(0), Piece::KNIGHT));
	}
	return moves;
}

std::vector<Moves> get_king_moves(u8 from, Color color, Board& board, Piece piece, bool canCastle, Color turn) {
    std::vector<Moves> moves;
    const u64 occ     = board.board[2];
    const u64 friendB = board.board[U8(color)];
    const u64 enemyB  = board.board[U8(color)^1];

    // 1) normal one‐square moves
    for (int i = 0; i < 8; ++i) {
        int to = from + king_shifts[i];
        if (to < 0 || to >= 64)               continue;
        int df = std::abs((from & 7) - (to & 7));
        int dr = std::abs((from >> 3) - (to >> 3));
        if (df > 1 || dr > 1)                continue;
        if (friendB & (1ULL << to))          continue;

        // build new occupancy for test
        u64 newOcc = (occ ^ (1ULL << from)) | (1ULL << to);
        // if king would be attacked on 'to', skip
        if (board.is_king_attacked(newOcc, color, turn)) 
            continue;

        bool isCap = !!(enemyB & (1ULL << to));
        moves.emplace_back(from, U8(to), U8(isCap), 0, piece);
    }

    // 2) castling
    if (canCastle) {
        for (int k = 0; k < 2; ++k) {
            int shift        = castling_shifts[k];  // +2 kingside, -2 queenside
            int to           = from + shift;
            int rook_from, rook_to;
            u64 between_mask = 0;

            if (color == Color::WHITE) {
                if (shift == +2) {
                    rook_from    = 7;  rook_to = 5;
                    between_mask = (1ULL<<5)|(1ULL<<6);
                } else {
                    rook_from    = 0;  rook_to = 3;
                    between_mask = (1ULL<<1)|(1ULL<<2)|(1ULL<<3);
                }
            } else {
                if (shift == +2) {
                    rook_from    = 63; rook_to = 61;
                    between_mask = (1ULL<<61)|(1ULL<<62);
                } else {
                    rook_from    = 56; rook_to = 59;
                    between_mask = (1ULL<<57)|(1ULL<<58)|(1ULL<<59);
                }
            }

            // must have empty between and rook present
            if ((occ & between_mask) != 0)      continue;
            if (!(friendB & (1ULL<<rook_from))) continue;

            // ensure king does not pass through or end in check
            // test square it crosses
            u64 occ1 = (occ ^ (1ULL<<from)) | (1ULL<<(from + shift/2));
            if (board.is_king_attacked(occ1, color, turn)) continue;
            // test landing square
            u64 occ2 = (occ1 ^ (1ULL<<(from + shift/2))) 
                     | (1ULL<<to) 
                     | (1ULL<<rook_to);
            if (board.is_king_attacked(occ2, color, turn)) continue;

            moves.emplace_back(from, U8(to), U8(rook_from), U8(rook_to), piece);
        }
    }

    return moves;
}


std::vector<Moves> get_pawn_moves(u8 from, Color color, Board& board, Piece piece, Color turn) {
    std::vector<Moves> moves;
    u8 idx    = U8(color);            
    u64 b     = board.board[2];
    u64 friendB = board.board[U8(color)];
    u64 enemyB  = board.board[U8(color) ^ 1];
    int to = from + pawn_shifts[U8(color)][0];
	std::cout<<checkColumn(from, to)<<' '<<to<<' '<<from<<' '<<!(b & (1ULL << to))<<' '<<board.is_king_attacked((b^(1<<from))|(1<<to), color, turn)<<'\n';
    if (to >= 0 && to < 64 && checkColumn(from, to) && !(b & (1ULL << to)) && !board.is_king_attacked((b^(1<<from))|(1<<to), color, turn)) {
        moves.push_back({from, U8(to), 0, 0, piece});

        bool onStart = (color==Color::WHITE && (from>>3)==1)
                         || (color==Color::BLACK && (from>>3)==6);
        if (onStart) {
            int to2 = from + pawn_first_shifts[idx][0];
            if (to2 >= 0 && to2 < 64 && !checkColumn(from, to2) && !(b & (1ULL << to2)) && !board.is_king_attacked((b^(1<<from))|(1<<to), color, turn)) {
                moves.push_back({from, U8(to2), 0, 0, piece});
            }
        }
    }

    for (int c = 1; c <= 2; ++c) {
        to = from + pawn_shifts[idx][c];
        if (to < 0 || to >= 64)        continue;
        if (!checkDiagRight(from, to) && !checkDiagLeft(from, to)) continue;
        if (enemyB & (1ULL << to) && !board.is_king_attacked(b|(1<<to), color, turn)) {
            moves.push_back({from, U8(to), 0, 0, piece});
        }
    }

    return moves;
}

std::vector<Moves> Board::get_piece_legal_move(u8 from, Piece piece, Color color, bool canCastle) {
    std::vector<Moves> moves;
    u64 own      = board[U8(color)];
    u64 enemy    = board[(U8(color)^1) ];
    u64 occupied = board[2];

    switch (piece) {
    case Piece::KING:
        moves = get_king_moves(from, color, *this, piece, canCastle, color);
        break;

    case Piece::QUEEN:
        moves = get_queen_moves(from, color, *this, piece, color);
        break;

    case Piece::ROOK:
        moves = get_rook_moves(from, color, *this, piece, color);
        break;

    case Piece::BISHOP:
        moves = get_bishop_moves(from, color, *this, piece, color);
        break;

    case Piece::KNIGHT:
        moves = get_knight_moves(from, color, *this, piece, color);
        break;

    case Piece::PAWN:
        moves = get_pawn_moves(from, color, *this, piece, color);
        break;

    default:
        break;
    }

    return moves;
}

std::vector<Moves> Board::get_all_legal_moves(Color color) {
    std::vector<Moves> moves;
    bool canCastle = castle_rights[U8(color)];  

    for (int pi = 0; pi < 6; ++pi) {
        Piece piece = static_cast<Piece>(pi);
        u64 bb = pieces[pi][U8(color)];
        while (bb) {
            int from = __builtin_ctzll(bb);
            bb &= bb - 1;

            auto pm = get_piece_legal_move(
                static_cast<u8>(from),
                piece,
                color,
                canCastle
            );

            moves.insert(moves.end(), pm.begin(), pm.end());
        }
    }

    return moves;
}

