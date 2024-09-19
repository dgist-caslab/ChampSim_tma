#include "l2c_stream_dyn.h"

l2c_stream_dyn::stream_buffer_t stream_buffer_dyn[l2c_stream_dyn::NUM_STREAM_BUFFER];
l2c_stream_dyn::l2c_stream_dyn_stats_t l2c_stream_dyn_stats;
int dyn_replacement_idx;
double total_dyn_occu;


void CACHE::prefetcher_initialize() {
    // init stream prefetcher
    for(std::size_t i = 0;i < l2c_stream_dyn::NUM_STREAM_BUFFER;i++){
        stream_buffer_dyn[i].page = 0;
        stream_buffer_dyn[i].diretion = 0;
        stream_buffer_dyn[i].confidence = 0;
        stream_buffer_dyn[i].pf_idx = -1;
        stream_buffer_dyn[i].degree = l2c_stream_dyn::PREF_DEGREE_DEFAULT;
    }
    // init l2c_stream_dyn_stats by using for statement
    l2c_stream_dyn_stats.num_to_l2c = 0;
    l2c_stream_dyn_stats.num_to_llc = 0;
    l2c_stream_dyn_stats.num_pref = 0;
    l2c_stream_dyn_stats.num_useful = 0;
    l2c_stream_dyn_stats.avg_mshr_occupancy_ratio = 0;
    total_dyn_occu = 0;

    dyn_replacement_idx = 0;
}

uint32_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, bool useful_prefetch, uint8_t type, uint32_t metadata_in)
{
  uint64_t page = addr >> LOG2_PAGE_SIZE;
  uint32_t page_offset = (addr >> LOG2_BLOCK_SIZE) & (PAGE_SIZE / BLOCK_SIZE - 1);
  int buf_idx = -1;

  for (std::size_t i = 0; i < l2c_stream_dyn::NUM_STREAM_BUFFER; i++) {
    if (stream_buffer_dyn[i].page == page) {
      buf_idx = i;
      break;
    }
  }
  
  // if not found
  if(buf_idx == -1){
    buf_idx = dyn_replacement_idx;
    dyn_replacement_idx++; // round robin
    if(dyn_replacement_idx >= (int)l2c_stream_dyn::NUM_STREAM_BUFFER){
      dyn_replacement_idx = 0;
    }
    stream_buffer_dyn[buf_idx].page = page;
    stream_buffer_dyn[buf_idx].diretion = 0;
    stream_buffer_dyn[buf_idx].confidence = 0;
    stream_buffer_dyn[buf_idx].pf_idx = page_offset;
    stream_buffer_dyn[buf_idx].degree = l2c_stream_dyn::PREF_DEGREE_DEFAULT;
  }

  // train new access
  if((int)page_offset > stream_buffer_dyn[buf_idx].pf_idx){
    if((page_offset - stream_buffer_dyn[buf_idx].pf_idx) < l2c_stream_dyn::STREAM_WINDOW){
        if(stream_buffer_dyn[buf_idx].diretion == -1){
            stream_buffer_dyn[buf_idx].confidence = 0;
        }else{
            stream_buffer_dyn[buf_idx].confidence++;
        }
        stream_buffer_dyn[buf_idx].diretion = 1;
    }
  }else if((int)page_offset < stream_buffer_dyn[buf_idx].pf_idx){
    if((stream_buffer_dyn[buf_idx].pf_idx - page_offset) < l2c_stream_dyn::STREAM_WINDOW){
        if(stream_buffer_dyn[buf_idx].diretion == 1){
            stream_buffer_dyn[buf_idx].confidence = 0;
        }else{
            stream_buffer_dyn[buf_idx].confidence++;
        }
        stream_buffer_dyn[buf_idx].diretion = -1;
    }
  }

  // do prefetch by using confidence
  if(stream_buffer_dyn[buf_idx].confidence >= (int)l2c_stream_dyn::PREF_CONFIDENCE){
    for(int i = 0;i<(int)l2c_stream_dyn::PREF_DEGREE_MAX;i++){
        stream_buffer_dyn[buf_idx].pf_idx += stream_buffer_dyn[buf_idx].diretion;
        if((stream_buffer_dyn[buf_idx].pf_idx < 0) || (stream_buffer_dyn[buf_idx].pf_idx > 64)){
            // out of 4KB page bound
            break;
        }
        uint64_t pf_addr = (stream_buffer_dyn[buf_idx].page << LOG2_PAGE_SIZE) + (stream_buffer_dyn[buf_idx].pf_idx << LOG2_BLOCK_SIZE);
        double mshr_occupancy_ratio = this->get_mshr_occupancy_ratio();
        total_dyn_occu += mshr_occupancy_ratio;
        if(mshr_occupancy_ratio < l2c_stream_dyn::PREF_THRESHOLD){
            // fill l2c
            prefetch_line(pf_addr, true, metadata_in);
            l2c_stream_dyn_stats.num_to_l2c++;
        }else{
            // fill llc
            prefetch_line(pf_addr, false, metadata_in);
            l2c_stream_dyn_stats.num_to_llc++;
        }
        l2c_stream_dyn_stats.num_pref++;

    }
  }

  if(useful_prefetch){
    l2c_stream_dyn_stats.num_useful++;
    if(stream_buffer_dyn[buf_idx].degree < l2c_stream_dyn::PREF_DEGREE_MAX){
      stream_buffer_dyn[buf_idx].degree = stream_buffer_dyn[buf_idx].degree*2;
    }
  }else{
    if(stream_buffer_dyn[buf_idx].degree > l2c_stream_dyn::PREF_DEGREE_DEFAULT){
      stream_buffer_dyn[buf_idx].degree = stream_buffer_dyn[buf_idx].degree/2;
    }
  }

  return metadata_in;
}

uint32_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)
{
  return metadata_in;
}


void CACHE::prefetcher_cycle_operate() {}

void CACHE::prefetcher_final_stats() {
    l2c_stream_dyn_stats.avg_mshr_occupancy_ratio = total_dyn_occu / l2c_stream_dyn_stats.num_pref;
    std::cout << "L2C Stream Prefetcher Stats" << std::endl;
    std::cout << "num_to_l2c:\t" << std::dec << l2c_stream_dyn_stats.num_to_l2c << std::endl;
    std::cout << "num_to_llc:\t" << std::dec <<l2c_stream_dyn_stats.num_to_llc << std::endl;
    std::cout << "num_pref:\t" << std::dec <<l2c_stream_dyn_stats.num_pref << std::endl;
    std::cout << "num_useful:\t" << std::dec << l2c_stream_dyn_stats.num_useful << std::endl;
    std::cout << "hit_ratio:\t" << (double)l2c_stream_dyn_stats.num_useful / (double)l2c_stream_dyn_stats.num_pref << std::endl;
    std::cout << "avg_mshr_occupancy_ratio:\t" << l2c_stream_dyn_stats.avg_mshr_occupancy_ratio << std::endl;
}