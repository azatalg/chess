#pragma once

#include "../config.h"

namespace chess {

enum class MoveFlag : u8 {
    quiet = 0,
    capture = 1,
    double_push = 2,
    en_passant = 3,
    castle = 4,
    promotion = 8,
    promotion_capture = 9
};

enum class Generation : u8 { All, Captures, Quiets };

class Move {
public:
    constexpr Move() = default;
    constexpr Move(u8 from, u8 to, MoveFlag flag = MoveFlag::quiet,
                   Piece piece = Piece::None, Piece promotion = Piece::Queen,
                   Piece captured = Piece::None)
        : value_(u32(from) | (u32(to) << 6) | (u32(flag) << 12) |
                 (u32(promotion) << 16) | (u32(piece) << 19) |
                 (u32(captured) << 22)) {}

    constexpr u8 from() const { return u8(value_ & 0x3fU); }
    constexpr u8 to() const { return u8((value_ >> 6) & 0x3fU); }
    constexpr MoveFlag flag() const { return MoveFlag((value_ >> 12) & 0xfU); }
    constexpr Piece promotion() const { return Piece((value_ >> 16) & 0x7U); }
    constexpr Piece piece() const { return Piece((value_ >> 19) & 0x7U); }
    constexpr Piece captured() const { return Piece((value_ >> 22) & 0x7U); }
    constexpr bool is_capture() const {
        return flag() == MoveFlag::capture || flag() == MoveFlag::en_passant ||
               flag() == MoveFlag::promotion_capture;
    }
    constexpr bool is_promotion() const {
        return flag() == MoveFlag::promotion || flag() == MoveFlag::promotion_capture;
    }
    constexpr bool operator==(Move other) const { return value_ == other.value_; }

private:
    u32 value_ = 0;
};
static_assert(sizeof(Move) == sizeof(u32), "Move must stay packed into 32 bits");

// The maximum number of legal moves in a chess position is 218. This hot-path
// container deliberately exposes only the operations required by the engine.
class MoveList {
public:
    using iterator = Move*;
    using const_iterator = const Move*;

    constexpr bool empty() const { return count_ == 0; }
    constexpr u16 size() const { return count_; }
    constexpr void clear() { count_ = 0; }
    constexpr void resize(u16 size) { count_ = size; }
    constexpr Move& operator[](u16 position) { return moves_[position]; }
    constexpr const Move& operator[](u16 position) const { return moves_[position]; }
    constexpr iterator begin() { return moves_; }
    constexpr iterator end() { return moves_ + count_; }
    constexpr const_iterator begin() const { return moves_; }
    constexpr const_iterator end() const { return moves_ + count_; }

    constexpr void push_back(Move move) { moves_[count_++] = move; }
    constexpr void emplace_back(u8 from, u8 to, MoveFlag flag = MoveFlag::quiet,
                                Piece piece = Piece::None,
                                Piece promotion = Piece::Queen,
                                Piece captured = Piece::None) {
        moves_[count_++] = Move(from, to, flag, piece, promotion, captured);
    }

private:
    Move moves_[256]{};
    u16 count_ = 0;
};

} // namespace chess
