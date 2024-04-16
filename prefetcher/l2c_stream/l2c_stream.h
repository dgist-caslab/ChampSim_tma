#ifndef STREAM
#define STREAM

#include "cache.h"

#include <cstdint>

namespace l2c_stream
{
    constexpr std::size_t NUM_STREAM_BUFFER = 64;
    constexpr std::size_t STREAM_BUFFER_SIZE = 8;
    constexpr std::size_t STREAM_WINDOW = 16;
    constexpr std::size_t PREF_CONFIDENCE = 2;
    constexpr std::size_t PREF_DEGREE = 2;
    constexpr double PREF_THRESHOLD = 0.8; // 80% of mshr

    typedef struct stream_buffer{
        uint64_t page;
        int diretion;
        int confidence;
        int pf_idx;
    } stream_buffer_t;
}

#endif