#include "cap.h"

namespace {
    // CAP::MappingTable mapping_table;
    cap::HistoryTable hist_table;
    // define MisraGriesTalbe as many as NUM_CPUS
    cap::MisraGriesTable mg_table[NUM_CPUS];
    // CAP::CxlAwarePrefetcher cap_prefetcher(0.5);
    cap::CxlAwarePrefetcher cap_prefetcher;
    uint64_t num_prefetch_cxl = 0;
    uint64_t num_prefetch_ddr = 0;
    uint64_t num_prefetch_hit_cxl = 0;
    uint64_t num_prefetch_hit_ddr = 0;
}

void CACHE::prefetcher_initialize() {
    //init all of the tables
    hist_table.init(SIZE_OF_HISTORY_TABLE);
    // mapping_table(SIZE_OF_MAP_TABLE, 0, 0);
    for(size_t i = 0; i < NUM_CPUS; i++){
        mg_table[i].init(SIZE_OF_MG_TABLE, DEGREE_DEFAULT);
    }
    cap_prefetcher.init(0.5);

}
void CACHE::prefetcher_cycle_operate() {}
uint32_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, bool useful_prefetch, uint8_t type, uint32_t metadata_in){
  // for stats
  if (cache_hit && useful_prefetch) {
    if (cap::is_cxl_memory(addr)) {
      num_prefetch_hit_cxl++;
    } else {
      num_prefetch_hit_ddr++;
    }
  }

  // 1. define addr is DDR memory or CXL memory by looking addr
  if (cap::is_cxl_memory(addr)) {
    mg_table[cpu].set_access_bit(addr);
    if (cache_hit && useful_prefetch) {
      // update misra-gries table
      cap::MisraGriesEntry ret_entry;
      if (mg_table[cpu].update_hit(addr, &ret_entry)) {
        hist_table.update(ret_entry);
      }
    }

    cap::HistoryEntry* hist_info = hist_table.get_history(addr);
    if (hist_info != nullptr) {
      size_t degree = cap_prefetcher.get_degree(hist_info);
      uint64_t pfn = addr >> LOG2_PAGE_SIZE;
      // std::cout << "pfn: 0x" << std::hex << pfn << std::dec << " degree: " << degree << " num_hit: " << hist_info->hit_cnt << " num_pref: " << hist_info->num_pref << " bitmap:" << hist_info->access_bitmap << std::endl;

      for (size_t i = 0; i < degree; i++) {
        uint64_t pf_addr = addr + (i + 1) * (1 << LOG2_BLOCK_SIZE);
        uint64_t pf_pfn = pf_addr >> LOG2_PAGE_SIZE;
        if (pfn == pf_pfn) { // prefetch only if the next line is in the same page
          if (prefetch_line(pf_addr, true, metadata_in)) {
            // std::cout << "[" << std::dec << i << "]" "prefetch: 0x" << std::hex << pf_addr << std::endl;
            mg_table[cpu].update_num_pref(pf_addr);
            if (cap::is_cxl_memory(pf_addr)) {
              num_prefetch_cxl++;
            } else {
              num_prefetch_ddr++;
            }
          }
        } else {
          // std::cout << "violate page boundary" << std::endl;
          break;
        }
      }
    } else { // no hist, just next-line prefetch
      uint64_t pfn = addr >> LOG2_PAGE_SIZE;
      uint64_t pf_addr = addr + (1 << LOG2_BLOCK_SIZE);
      uint64_t pf_pfn = pf_addr >> LOG2_PAGE_SIZE;
      if (pfn == pf_pfn) { // prefetch only if the next line is in the same page
        if (prefetch_line(pf_addr, true, metadata_in)) {
          mg_table[cpu].update_num_pref(pf_addr);
          if (cap::is_cxl_memory(pf_addr)) {
            num_prefetch_cxl++;
          } else {
            num_prefetch_ddr++;
          }
        }
      }
    }
  } else {
    // [PHW] at this moment, just act like next-line prefetcher;
    uint64_t pfn = addr >> LOG2_PAGE_SIZE;
    uint64_t pf_addr = addr + (1 << LOG2_BLOCK_SIZE);
    uint64_t pf_pfn = pf_addr >> LOG2_PAGE_SIZE;
    if (pfn == pf_pfn) { // prefetch only if the next line is in the same page
      if (prefetch_line(pf_addr, true, metadata_in)) {
        mg_table[cpu].update_num_pref(pf_addr);
        if (cap::is_cxl_memory(pf_addr)) {
          num_prefetch_cxl++;
        } else {
          num_prefetch_ddr++;
        }
      }
    }
  }
  return metadata_in;
}
uint32_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t match, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)
{
  return metadata_in;
}
void CACHE::prefetcher_final_stats()
{
  // print hist table
  // hist_table.print();
  std::cout << "[" << std::dec << cpu << "]num_prefetch_ddr: " << std::dec << num_prefetch_ddr << std::endl;
  std::cout << "[" << std::dec << cpu << "]num_prefetch_hit_ddr: " << std::dec << num_prefetch_hit_ddr << std::endl;
  std::cout << "[" << std::dec << cpu << "]num_prefetch_cxl: " << std::dec << num_prefetch_cxl << std::endl;
  std::cout << "[" << std::dec << cpu << "]num_prefetch_hit_cxl: " << std::dec << num_prefetch_hit_cxl << std::endl;
}