#ifndef STREAM_TMA
#define STREAM_TMA

#include "cache.h"
#include "champsim_constants.h"

#include <cstdint>
#include <iostream>

namespace l2c_stream
{
    constexpr std::size_t NUM_STREAM_BUFFER = 64;
    constexpr std::size_t NUM_STREAM_BUFFER_FAST = 32;
    constexpr std::size_t NUM_STREAM_BUFFER_SLOW = NUM_STREAM_BUFFER - NUM_STREAM_BUFFER_FAST;
    constexpr std::size_t STREAM_BUFFER_SIZE = 8;
    constexpr std::size_t STREAM_WINDOW = 16;
    constexpr std::size_t PREF_CONFIDENCE = 2;
    constexpr std::size_t PREF_DEGREE = 16;
    constexpr double PREF_THRESHOLD = 0.5; // 50% of mshr

    typedef struct l2c_stream_stats{
        uint64_t num_pref;
        uint64_t num_useful;
        uint64_t num_to_l2c;
        uint64_t num_to_llc;
        double avg_mshr_occupancy_ratio;
    } l2c_stream_stats_t;

    typedef struct stream_buffer{
        uint64_t page;
        int diretion;
        int confidence;
        int pf_idx;
    } stream_buffer_t;
}

#endif
