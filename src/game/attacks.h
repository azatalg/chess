#pragma once

#include "../config.h"
#include <EASTL/array.h>

namespace chess::attacks {

inline constexpr int directions[8][2] = {
    {0,1},{1,0},{0,-1},{-1,0},{1,1},{1,-1},{-1,-1},{-1,1}
};

constexpr bool valid(int file, int rank) {
    return file >= 0 && file < 8 && rank >= 0 && rank < 8;
}

constexpr u64 leaper_mask(int square, const int (&deltas)[8][2]) {
    u64 result = 0;
    const int file = square & 7, rank = square >> 3;
    for (const auto& delta : deltas) {
        const int f = file + delta[0], r = rank + delta[1];
        if (valid(f, r)) result |= 1ULL << (r * 8 + f);
    }
    return result;
}

constexpr auto knight_table() {
    constexpr int deltas[8][2] = {{1,2},{2,1},{2,-1},{1,-2},
                                   {-1,-2},{-2,-1},{-2,1},{-1,2}};
    eastl::array<u64, 64> table{};
    for (int square = 0; square < 64; ++square) table[square] = leaper_mask(square, deltas);
    return table;
}

constexpr auto king_table() {
    constexpr int deltas[8][2] = {{0,1},{1,1},{1,0},{1,-1},
                                   {0,-1},{-1,-1},{-1,0},{-1,1}};
    eastl::array<u64, 64> table{};
    for (int square = 0; square < 64; ++square) table[square] = leaper_mask(square, deltas);
    return table;
}

constexpr auto pawn_table(Color color) {
    eastl::array<u64, 64> table{};
    const int step = color == Color::White ? 1 : -1;
    for (int square = 0; square < 64; ++square) {
        const int file = square & 7, rank = square >> 3;
        for (int df : {-1, 1})
            if (valid(file + df, rank + step))
                table[square] |= 1ULL << ((rank + step) * 8 + file + df);
    }
    return table;
}

constexpr auto ray_table() {
    eastl::array<eastl::array<u64, 64>, 8> table{};
    for (int d = 0; d < 8; ++d) for (int square = 0; square < 64; ++square) {
        int file = (square & 7) + directions[d][0];
        int rank = (square >> 3) + directions[d][1];
        while (valid(file, rank)) {
            table[d][square] |= 1ULL << (rank * 8 + file);
            file += directions[d][0];
            rank += directions[d][1];
        }
    }
    return table;
}

constexpr auto between_table() {
    constexpr int opposite[8] = {2,3,0,1,6,7,4,5};
    eastl::array<eastl::array<u64, 64>, 64> table{};
    const auto rays = ray_table();
    for (u8 from = 0; from < 64; ++from) for (u8 to = 0; to < 64; ++to)
        for (int d = 0; d < 8; ++d)
            if (rays[d][from] & (1ULL << to)) {
                table[from][to] = rays[d][from] & rays[opposite[d]][to];
                break;
            }
    return table;
}

constexpr auto line_table() {
    constexpr int opposite[8] = {2,3,0,1,6,7,4,5};
    eastl::array<eastl::array<u64, 64>, 64> table{};
    const auto rays = ray_table();
    for (u8 from = 0; from < 64; ++from) for (u8 to = 0; to < 64; ++to)
        for (int d = 0; d < 8; ++d)
            if (rays[d][from] & (1ULL << to)) {
                table[from][to] = rays[d][from] | rays[opposite[d]][from] | (1ULL << from);
                break;
            }
    return table;
}

inline constexpr auto knight = knight_table();
inline constexpr auto king = king_table();
inline constexpr eastl::array<eastl::array<u64, 64>, 2> pawn = {
    pawn_table(Color::White), pawn_table(Color::Black)
};
inline constexpr auto ray = ray_table();
inline constexpr auto between = between_table();
inline constexpr auto line = line_table();

} // namespace chess::attacks
