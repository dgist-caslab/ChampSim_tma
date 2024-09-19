#include "l2c_stream.h"

l2c_stream::stream_buffer_t stream_buffer[l2c_stream::NUM_STREAM_BUFFER];
l2c_stream::l2c_stream_stats_t l2c_stream_stats;
int replacement_idx;
double total_occu;


void CACHE::prefetcher_initialize() {
    // init stream prefetcher
    for(std::size_t i = 0;i < l2c_stream::NUM_STREAM_BUFFER;i++){
        stream_buffer[i].page = 0;
        stream_buffer[i].diretion = 0;
        stream_buffer[i].confidence = 0;
        stream_buffer[i].pf_idx = -1;
    }
    // init l2c_stream_stats by using for statement
    l2c_stream_stats.num_to_l2c = 0;
    l2c_stream_stats.num_to_llc = 0;
    l2c_stream_stats.num_pref = 0;
    l2c_stream_stats.num_useful = 0;
    l2c_stream_stats.avg_mshr_occupancy_ratio = 0;
    total_occu = 0;

    replacement_idx = 0;
}

uint32_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, bool useful_prefetch, uint8_t type, uint32_t metadata_in)
{
  uint64_t page = addr >> LOG2_PAGE_SIZE;
  uint32_t page_offset = (addr >> LOG2_BLOCK_SIZE) & (PAGE_SIZE / BLOCK_SIZE - 1);
  int buf_idx = -1;

  for (std::size_t i = 0; i < l2c_stream::NUM_STREAM_BUFFER; i++) {
    if (stream_buffer[i].page == page) {
      buf_idx = i;
      break;
    }
  }
  
  // if not found
  if(buf_idx == -1){
    buf_idx = replacement_idx;
    replacement_idx++; // round robin
    if(replacement_idx >= (int)l2c_stream::NUM_STREAM_BUFFER){
      replacement_idx = 0;
    }
    stream_buffer[buf_idx].page = page;
    stream_buffer[buf_idx].diretion = 0;
    stream_buffer[buf_idx].confidence = 0;
    stream_buffer[buf_idx].pf_idx = page_offset;
  }

  // train new access
  if((int)page_offset > stream_buffer[buf_idx].pf_idx){
    if((page_offset - stream_buffer[buf_idx].pf_idx) < l2c_stream::STREAM_WINDOW){
        if(stream_buffer[buf_idx].diretion == -1){
            stream_buffer[buf_idx].confidence = 0;
        }else{
            stream_buffer[buf_idx].confidence++;
        }
        stream_buffer[buf_idx].diretion = 1;
    }
  }else if((int)page_offset < stream_buffer[buf_idx].pf_idx){
    if((stream_buffer[buf_idx].pf_idx - page_offset) < l2c_stream::STREAM_WINDOW){
        if(stream_buffer[buf_idx].diretion == 1){
            stream_buffer[buf_idx].confidence = 0;
        }else{
            stream_buffer[buf_idx].confidence++;
        }
        stream_buffer[buf_idx].diretion = -1;
    }
  }

  // do prefetch by using confidence
  if(stream_buffer[buf_idx].confidence >= (int)l2c_stream::PREF_CONFIDENCE){
    for(int i = 0;i<(int)l2c_stream::PREF_DEGREE;i++){
        stream_buffer[buf_idx].pf_idx += stream_buffer[buf_idx].diretion;
        if((stream_buffer[buf_idx].pf_idx < 0) || (stream_buffer[buf_idx].pf_idx > 64)){
            // out of 4KB page bound
            break;
        }
        uint64_t pf_addr = (stream_buffer[buf_idx].page << LOG2_PAGE_SIZE) + (stream_buffer[buf_idx].pf_idx << LOG2_BLOCK_SIZE);
        double mshr_occupancy_ratio = this->get_mshr_occupancy_ratio();
        total_occu += mshr_occupancy_ratio;
        if(mshr_occupancy_ratio < l2c_stream::PREF_THRESHOLD){
            // fill l2c
            prefetch_line(pf_addr, true, metadata_in);
            l2c_stream_stats.num_to_l2c++;
        }else{
            // fill llc
            prefetch_line(pf_addr, false, metadata_in);
            l2c_stream_stats.num_to_llc++;
        }
        l2c_stream_stats.num_pref++;

    }
  }

  if(useful_prefetch){
    l2c_stream_stats.num_useful++;
  }

  return metadata_in;
}

uint32_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)
{
  return metadata_in;
}


void CACHE::prefetcher_cycle_operate() {}

void CACHE::prefetcher_final_stats() {
    l2c_stream_stats.avg_mshr_occupancy_ratio = total_occu / l2c_stream_stats.num_pref;
    std::cout << "L2C Stream Prefetcher Stats" << std::endl;
    std::cout << "num_to_l2c:\t" << std::dec << l2c_stream_stats.num_to_l2c << std::endl;
    std::cout << "num_to_llc:\t" << std::dec << l2c_stream_stats.num_to_llc << std::endl;
    std::cout << "num_pref:\t" << std::dec << l2c_stream_stats.num_pref << std::endl;
    std::cout << "num_useful:\t" << std::dec << l2c_stream_stats.num_useful << std::endl;
    std::cout << "hit_ratio:\t" << (double)l2c_stream_stats.num_useful / (double)l2c_stream_stats.num_pref << std::endl;
    std::cout << "avg_mshr_occupancy_ratio:\t" << l2c_stream_stats.avg_mshr_occupancy_ratio << std::endl;
}