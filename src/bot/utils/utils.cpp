#include "utils.h"

namespace chess::ai {

u32 Random::next() {
    state_ = (0x5deece66dULL * state_ + 0xbULL) & ((1ULL << 48) - 1);
    return static_cast<u32>(state_ >> 16);
}

i32 Timer::elapsed_milliseconds() const {
    return static_cast<i32>(std::chrono::duration_cast<std::chrono::milliseconds>(
        Clock::now() - started_).count());
}

} // namespace chess::ai
