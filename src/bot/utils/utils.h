#pragma once

#include "../../config.h"
#include <chrono>

namespace chess::ai {

class Random {
public:
    explicit Random(u64 seed = 1) : state_(seed) {}
    u32 next();

private:
    u64 state_;
};

class Timer {
public:
    using Clock = std::chrono::steady_clock;
    Timer() : started_(Clock::now()) {}
    i32 elapsed_milliseconds() const;

private:
    Clock::time_point started_;
};

} // namespace chess::ai
