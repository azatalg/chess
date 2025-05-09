/* Bit shifts of moves precomputed */

// KNIGHT
constexpr short knight_shifts[] = { +17, +15, +10, +6, -6, -10, -15, -17 };

// KING
constexpr short king_shifts[] = { +8, +9, +1, -7, -8, -9, -1, +7 };

// CASTLING 
constexpr short castling_shifts[] = { +2, -2 }; // kingside (+2), queenside (-2)


// ROOK (sliding multi-shift)
constexpr short rook_shifts[] = {
    +8, +16, +24, +32, +40, +48, +56,
    -8, -16, -24, -32, -40, -48, -56,
    +1, +2, +3, +4, +5, +6, +7,
    -1, -2, -3, -4, -5, -6, -7
};

// BISHOP (sliding multi-shift)
constexpr short bishop_shifts[] = {
    +9, +18, +27, +36, +45, +54, +63,
    -9, -18, -27, -36, -45, -54, -63,
	+7, +14, +21, +28, +35, +42, +49,
    -7, -14, -21, -28, -35, -42, -49
};

// QUEEN (rook + bishop shifts)
constexpr short queen_shifts[] = {
    +8, +16, +24, +32, +40, +48, +56,
    -8, -16, -24, -32, -40, -48, -56,
    +1, +2, +3, +4, +5, +6, +7,
    -1, -2, -3, -4, -5, -6, -7,
    +9, +18, +27, +36, +45, +54, +63,
    -9, -18, -27, -36, -45, -54, -63,
    +7, +14, +21, +28, +35, +42, +49,
    -7, -14, -21, -28, -35, -42, -49
};

// PAWN
constexpr short pawn_shifts[2][3] = {{8, 9, 7}, {-8, -9, -7}}; // 0 - color white, 1 - color black
// PAWN START
constexpr short pawn_first_shifts[2][3] = {{16}, {-16}};

