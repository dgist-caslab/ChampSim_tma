#include "l2c_stream_tma.h"

l2c_stream::stream_buffer_t stream_buffer_fast[l2c_stream::NUM_STREAM_BUFFER_FAST];
l2c_stream::stream_buffer_t stream_buffer_slow[l2c_stream::NUM_STREAM_BUFFER_SLOW];
l2c_stream::l2c_stream_stats_t l2c_stream_fast_stats, l2c_stream_slow_stats;
int replacement_idx_fast, replacement_idx_slow;
double total_occu_fast, total_occu_slow;


void CACHE::prefetcher_initialize() {
    // init stream prefetcher for fast memory
    for(std::size_t i = 0;i < l2c_stream::NUM_STREAM_BUFFER_FAST;i++){
        stream_buffer_fast[i].page = 0;
        stream_buffer_fast[i].diretion = 0;
        stream_buffer_fast[i].confidence = 0;
        stream_buffer_fast[i].pf_idx = -1;
    }
    // init stream pefetcher for slow memory
    for(std::size_t i = 0;i < l2c_stream::NUM_STREAM_BUFFER_SLOW;i++){
        stream_buffer_slow[i].page = 0;
        stream_buffer_slow[i].diretion = 0;
        stream_buffer_slow[i].confidence = 0;
        stream_buffer_slow[i].pf_idx = -1;
    }
    // init l2c_stream_stats by using for statement
    l2c_stream_fast_stats.num_to_l2c = 0;
    l2c_stream_fast_stats.num_to_llc = 0;
    l2c_stream_fast_stats.num_pref = 0;
    l2c_stream_fast_stats.num_useful = 0;
    l2c_stream_fast_stats.avg_mshr_occupancy_ratio = 0;

    l2c_stream_slow_stats.num_to_l2c = 0;
    l2c_stream_slow_stats.num_to_llc = 0;
    l2c_stream_slow_stats.num_pref = 0;
    l2c_stream_slow_stats.num_useful = 0;
    l2c_stream_slow_stats.avg_mshr_occupancy_ratio = 0;


    total_occu_fast = 0;
    total_occu_slow = 0;

    replacement_idx_fast = 0;
    replacement_idx_slow = 0;
}

uint32_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, bool useful_prefetch, uint8_t type, uint32_t metadata_in)
{
  uint64_t page = addr >> LOG2_PAGE_SIZE;
  uint32_t page_offset = (addr >> LOG2_BLOCK_SIZE) & (PAGE_SIZE / BLOCK_SIZE - 1);
  int buf_idx = -1;

  if(page <= (DRAM_SIZE >> LOG2_PAGE_SIZE) && l2c_stream::NUM_STREAM_BUFFER_FAST > 0){ // handle fast
    for (std::size_t i = 0; i < l2c_stream::NUM_STREAM_BUFFER_FAST; i++) {
      if (stream_buffer_fast[i].page == page) {
        buf_idx = i;
        break;
      }
    }
    
    // if not found
    if(buf_idx == -1){
      buf_idx = replacement_idx_fast;
      replacement_idx_fast++; // round robin
      if(replacement_idx_fast >= (int)l2c_stream::NUM_STREAM_BUFFER_FAST){
        replacement_idx_fast = 0;
      }
      stream_buffer_fast[buf_idx].page = page;
      stream_buffer_fast[buf_idx].diretion = 0;
      stream_buffer_fast[buf_idx].confidence = 0;
      stream_buffer_fast[buf_idx].pf_idx = page_offset;
    }

    // train new access
    if((int)page_offset > stream_buffer_fast[buf_idx].pf_idx){
      if((page_offset - stream_buffer_fast[buf_idx].pf_idx) < l2c_stream::STREAM_WINDOW){
          if(stream_buffer_fast[buf_idx].diretion == -1){
              stream_buffer_fast[buf_idx].confidence = 0;
          }else{
              stream_buffer_fast[buf_idx].confidence++;
          }
          stream_buffer_fast[buf_idx].diretion = 1;
      }
    }else if((int)page_offset < stream_buffer_fast[buf_idx].pf_idx){
      if((stream_buffer_fast[buf_idx].pf_idx - page_offset) < l2c_stream::STREAM_WINDOW){
          if(stream_buffer_fast[buf_idx].diretion == 1){
              stream_buffer_fast[buf_idx].confidence = 0;
          }else{
              stream_buffer_fast[buf_idx].confidence++;
          }
          stream_buffer_fast[buf_idx].diretion = -1;
      }
    }

    // do prefetch by using confidence
    if(stream_buffer_fast[buf_idx].confidence >= (int)l2c_stream::PREF_CONFIDENCE){
      for(int i = 0;i<(int)l2c_stream::PREF_DEGREE;i++){
          stream_buffer_fast[buf_idx].pf_idx += stream_buffer_fast[buf_idx].diretion;
          if((stream_buffer_fast[buf_idx].pf_idx < 0) || (stream_buffer_fast[buf_idx].pf_idx > 64)){
              // out of 4KB page bound
              break;
          }
          uint64_t pf_addr = (stream_buffer_fast[buf_idx].page << LOG2_PAGE_SIZE) + (stream_buffer_fast[buf_idx].pf_idx << LOG2_BLOCK_SIZE);
          double mshr_occupancy_ratio = this->get_mshr_occupancy_ratio();
          total_occu_fast += mshr_occupancy_ratio;
          if(mshr_occupancy_ratio < l2c_stream::PREF_THRESHOLD){
              // fill l2c
              prefetch_line(pf_addr, true, metadata_in);
              l2c_stream_fast_stats.num_to_l2c++;
          }else{
              // fill llc
              prefetch_line(pf_addr, false, metadata_in);
              l2c_stream_fast_stats.num_to_llc++;
          }
          l2c_stream_fast_stats.num_pref++;
      }
    }

    if(useful_prefetch){
      l2c_stream_fast_stats.num_useful++;
    }

    return metadata_in;
  }else if(l2c_stream::NUM_STREAM_BUFFER_SLOW > 0){ // handle slow
    for (std::size_t i = 0; i < l2c_stream::NUM_STREAM_BUFFER_SLOW; i++) {
      if (stream_buffer_slow[i].page == page) {
        buf_idx = i;
        break;
      }
    }
    
    // if not found
    if(buf_idx == -1){
      buf_idx = replacement_idx_slow;
      replacement_idx_slow++; // round robin
      if(replacement_idx_slow >= (int)l2c_stream::NUM_STREAM_BUFFER_SLOW){
        replacement_idx_slow = 0;
      }
      stream_buffer_slow[buf_idx].page = page;
      stream_buffer_slow[buf_idx].diretion = 0;
      stream_buffer_slow[buf_idx].confidence = 0;
      stream_buffer_slow[buf_idx].pf_idx = page_offset;
    }

    // train new access
    if((int)page_offset > stream_buffer_slow[buf_idx].pf_idx){
      if((page_offset - stream_buffer_slow[buf_idx].pf_idx) < l2c_stream::STREAM_WINDOW){
          if(stream_buffer_slow[buf_idx].diretion == -1){
              stream_buffer_slow[buf_idx].confidence = 0;
          }else{
              stream_buffer_slow[buf_idx].confidence++;
          }
          stream_buffer_slow[buf_idx].diretion = 1;
      }
    }else if((int)page_offset < stream_buffer_slow[buf_idx].pf_idx){
      if((stream_buffer_slow[buf_idx].pf_idx - page_offset) < l2c_stream::STREAM_WINDOW){
          if(stream_buffer_slow[buf_idx].diretion == 1){
              stream_buffer_slow[buf_idx].confidence = 0;
          }else{
              stream_buffer_slow[buf_idx].confidence++;
          }
          stream_buffer_slow[buf_idx].diretion = -1;
      }
    }

    // do prefetch by using confidence
    if(stream_buffer_slow[buf_idx].confidence >= (int)l2c_stream::PREF_CONFIDENCE){
      for(int i = 0;i<(int)l2c_stream::PREF_DEGREE;i++){
          stream_buffer_slow[buf_idx].pf_idx += stream_buffer_slow[buf_idx].diretion;
          if((stream_buffer_slow[buf_idx].pf_idx < 0) || (stream_buffer_slow[buf_idx].pf_idx > 64)){
              // out of 4KB page bound
              break;
          }
          uint64_t pf_addr = (stream_buffer_slow[buf_idx].page << LOG2_PAGE_SIZE) + (stream_buffer_slow[buf_idx].pf_idx << LOG2_BLOCK_SIZE);
          double mshr_occupancy_ratio = this->get_mshr_occupancy_ratio();
          total_occu_slow += mshr_occupancy_ratio;
          if(mshr_occupancy_ratio < l2c_stream::PREF_THRESHOLD){
              // fill l2c
              prefetch_line(pf_addr, true, metadata_in);
              l2c_stream_slow_stats.num_to_l2c++;
          }else{
              // fill llc
              prefetch_line(pf_addr, false, metadata_in);
              l2c_stream_slow_stats.num_to_llc++;
          }
          l2c_stream_slow_stats.num_pref++;
      }
    }

    if(useful_prefetch){
      l2c_stream_slow_stats.num_useful++;
    }

    return metadata_in;
  }
}

uint32_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)
{
  return metadata_in;
}


void CACHE::prefetcher_cycle_operate() {}

void CACHE::prefetcher_final_stats() {
    l2c_stream_fast_stats.avg_mshr_occupancy_ratio = total_occu_fast / l2c_stream_fast_stats.num_pref;
    l2c_stream_slow_stats.avg_mshr_occupancy_ratio = total_occu_slow / l2c_stream_slow_stats.num_pref;
    std::cout << "L2C Stream Prefetcher TMA Stats(FAST)" << std::endl;
    std::cout << "num_to_l2c:\t" << l2c_stream_fast_stats.num_to_l2c << std::endl;
    std::cout << "num_to_llc:\t" << l2c_stream_fast_stats.num_to_llc << std::endl;
    std::cout << "num_pref:\t" << l2c_stream_fast_stats.num_pref << std::endl;
    std::cout << "num_useful:\t" << l2c_stream_fast_stats.num_useful << std::endl;
    std::cout << "avg_mshr_occupancy_ratio:\t" << l2c_stream_fast_stats.avg_mshr_occupancy_ratio << std::endl << std::endl;
    std::cout << "L2C Stream Prefetcher TMA Stats(SLOW)" << std::endl;
    std::cout << "num_to_l2c:\t" << l2c_stream_slow_stats.num_to_l2c << std::endl;
    std::cout << "num_to_llc:\t" << l2c_stream_slow_stats.num_to_llc << std::endl;
    std::cout << "num_pref:\t" << l2c_stream_slow_stats.num_pref << std::endl;
    std::cout << "num_useful:\t" << l2c_stream_slow_stats.num_useful << std::endl;
    std::cout << "avg_mshr_occupancy_ratio:\t" << l2c_stream_slow_stats.avg_mshr_occupancy_ratio << std::endl;

}