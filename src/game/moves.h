#pragma once
#include "../config.h"

class Moves {
public:
    u32 move;

    Moves(u8 start, u8 end, u8 startCast, u8 endCast, Piece type) {
        move = compress(start, end, startCast, endCast, static_cast<u8>(type));
    }

    u32 compress(u8 start, u8 end, u8 startCast, u8 endCast, u8 type) {
        u32 result = 0;
        result |= static_cast<u32>(start);
        result |= static_cast<u32>(end) << 6;
        result |= static_cast<u32>(type) << 12;
        result |= static_cast<u32>(startCast) << 15;
        result |= static_cast<u32>(endCast) << 21;
        return result;
    }

    constexpr static u32 START_MASK     = 0b111111;          // bits 0-5
    constexpr static u32 END_MASK       = 0b111111 << 6;     // bits 6-11
    constexpr static u32 TYPE_MASK      = 0b111 << 12;       // bits 12-14
    constexpr static u32 STARTCAST_MASK = 0b111111 << 15;    // bits 15-20
    constexpr static u32 ENDCAST_MASK   = 0b111111 << 21;    // bits 21-26

    u8 getStart() const {
        return static_cast<u8>(move & START_MASK);
    }

    u8 getEnd() const {
        return static_cast<u8>((move & END_MASK) >> 6);
    }

    Piece getPiece() const {
        return static_cast<Piece>((move & TYPE_MASK) >> 12);
    }

    u8 getStartCast() const {
        return static_cast<u8>((move & STARTCAST_MASK) >> 15);
    }

    u8 getEndCast() const {
        return static_cast<u8>((move & ENDCAST_MASK) >> 21);
    }
};

