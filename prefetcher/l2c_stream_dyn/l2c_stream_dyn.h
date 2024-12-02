#ifndef STREAM
#define STREAM

#include "cache.h"

#include <cstdint>
#include <iostream>

namespace{
    uint64_t num_prefetch_cxl = 0;
    uint64_t num_prefetch_ddr = 0;
    uint64_t num_prefetch_hit_cxl = 0;
    uint64_t num_prefetch_hit_ddr = 0;

    bool is_cxl_memory(uint64_t addr){
    if( addr > DRAM_SIZE){
        return true;
    }else{
        return false;
    }
    }
}

namespace l2c_stream_dyn
{
    constexpr std::size_t NUM_STREAM_BUFFER = 64;
    constexpr std::size_t STREAM_BUFFER_SIZE = 8;
    constexpr std::size_t STREAM_WINDOW = 16;
    constexpr std::size_t PREF_CONFIDENCE = 2;
    constexpr int PREF_DEGREE_MAX = 16;
    constexpr int PREF_DEGREE_DEFAULT = 4;
    constexpr double PREF_THRESHOLD = 0.5; // 80% of mshr

    typedef struct l2c_stream_dyn_stats{
        uint64_t num_pref;
        uint64_t num_useful;
        uint64_t num_to_l2c;
        uint64_t num_to_llc;
        double avg_mshr_occupancy_ratio;
    } l2c_stream_dyn_stats_t;

    typedef struct stream_buffer{
        uint64_t page;
        int diretion;
        int confidence;
        int pf_idx;
        int degree;
    } stream_buffer_t;
}

#endif
