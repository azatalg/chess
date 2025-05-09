#include "../config.h"
class Board {
public:
    enum Piece { PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING };
    enum Color { WHITE, BLACK };

    u64 board;
    u64 pieces[6][2];

    Board() : board(18446744073709551615ULL), pieces{} {
        /*  WHITE  -------------------------------------------------- */
        pieces[PAWN]   [WHITE] = 0x000000000000FF00ULL;   // 8‑15
        pieces[KNIGHT] [WHITE] = 0x0000000000000042ULL;   // b1, g1
        pieces[BISHOP] [WHITE] = 0x0000000000000024ULL;   // c1, f1
        pieces[ROOK]   [WHITE] = 0x0000000000000081ULL;   // a1, h1
        pieces[QUEEN]  [WHITE] = 0x0000000000000008ULL;   // d1
        pieces[KING]   [WHITE] = 0x0000000000000010ULL;   // e1

        /*  BLACK  -------------------------------------------------- */
        pieces[PAWN]   [BLACK] = 0x00FF000000000000ULL;   // 48‑55
        pieces[KNIGHT] [BLACK] = 0x4200000000000000ULL;   // b8, g8
        pieces[BISHOP] [BLACK] = 0x2400000000000000ULL;   // c8, f8
        pieces[ROOK]   [BLACK] = 0x8100000000000000ULL;   // a8, h8
        pieces[QUEEN]  [BLACK] = 0x0800000000000000ULL;   // d8
        pieces[KING]   [BLACK] = 0x1000000000000000ULL;   // e8
    }
};
