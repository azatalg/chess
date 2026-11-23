#pragma once

#include <cstddef>
#include <cstdint>

namespace chess {

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i32 = std::int32_t;
using i16 = std::int16_t;

enum class Color : u8 { White, Black };
enum class Piece : u8 { Pawn, Knight, Bishop, Rook, Queen, King, None };

constexpr Color opposite(Color color) {
    return color == Color::White ? Color::Black : Color::White;
}
constexpr std::size_t index(Color color) { return static_cast<std::size_t>(color); }
constexpr std::size_t index(Piece piece) { return static_cast<std::size_t>(piece); }
constexpr u64 square_bit(u8 square) { return 1ULL << square; }
constexpr u8 no_square = 64;

} // namespace chess
