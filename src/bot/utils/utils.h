#pragma once 
#include "../config.h"

namespace Random {
    u64 seed = 1; 
    u32 random();
}

class Timer {
private:
    u64 T0;

public:
    const u64 M_TIME = 100'000; // 100 ms

    Timer();
    u64 get_time();
    inline i32 elapsed_time();
};