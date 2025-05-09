#pragma once 
#include "utils.h"
#include <bits/stdc++.h>

// Timmer
extern "C" {
    int gettimeofday(struct timeval* tv, void* tz);
}

Timer::Timer(){
    T0 = get_time();
}

u64 Timer::get_time(){
    struct timeval t;
    gettimeofday(&t, nullptr);
    return static_cast<uint64_t>(t.tv_sec) * 1000000UL + static_cast<uint64_t>(t.tv_usec);
}

inline i32 Timer::elapsed_time(){
    return get_time() - T0;
}

// Random
u32 Random::random(){
    seed= (0x5DEECE66DUL * seed + 0xBUL) & ((1UL << 48) - 1);
    return(seed >> 16);
}


