#include "cap.h"

namespace {
    // CAP::MappingTable mapping_table;
    cap::HistoryTable hist_table;
    // define MisraGriesTalbe as many as NUM_CPUS
    cap::MisraGriesTable mg_table[NUM_CPUS];
    // CAP::CxlAwarePrefetcher cap_prefetcher(0.5);
    cap::CxlAwarePrefetcher cap_prefetcher;
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
uint32_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, bool useful_prefetch, uint8_t type, uint32_t metadata_in) {
    // 1. define addr is DDR memory or CXL memory by looking addr
    if (cap::is_cxl_memory(addr)) {
        if(cache_hit && useful_prefetch){
            // update misra-gries table
            cap::MisraGriesEntry ret_entry;
            bool need_update_hist = mg_table[cpu].update_hit(addr, &ret_entry);
            if (need_update_hist) {
              // std::cout << "cpu: " << std::dec << cpu << " ret_entry : pfn: " << std::hex << ret_entry.pfn << std::dec << " hit_cnt: " << ret_entry.hit_cnt
              // << " num_pref: " << ret_entry.num_pref << std::endl;
              hist_table.update(ret_entry);
            }
        }

        cap::HistoryEntry* hist_info = hist_table.get_history(addr);
        if (hist_info != nullptr) {
          // TDOO do some prefetch shit
          size_t pref_cnt = 0;
          for (size_t i = 0; i < hist_info->degree; i++) {
            uint64_t pfn = addr >> LOG2_PAGE_SIZE;
            uint64_t pf_addr = addr + (1 << LOG2_BLOCK_SIZE);
            uint64_t pf_pfn = pf_addr >> LOG2_PAGE_SIZE;
            if (pfn == pf_pfn) { // prefetch only if the next line is in the same page
              mg_table[cpu].update_num_pref(pf_addr);
              prefetch_line(pf_addr, true, metadata_in);
              pref_cnt++;
            } else {
              break;
            }
          }
        //   std::cout << "[cap_prefetch]pfn: 0x" << std::hex << hist_info->pfn << std::dec << " hit_cnt: " << hist_info->hit_cnt << " num_pref: " << hist_info->num_pref << " degree: " << hist_info->degree << std::endl;
        } else {
          // no hist, just next-line prefetch
          uint64_t pfn = addr >> LOG2_PAGE_SIZE;
          uint64_t pf_addr = addr + (1 << LOG2_BLOCK_SIZE);
          uint64_t pf_pfn = pf_addr >> LOG2_PAGE_SIZE;
          if (pfn == pf_pfn) { // prefetch only if the next line is in the same page
            mg_table[cpu].update_num_pref(pf_addr);
            prefetch_line(pf_addr, true, metadata_in);
          }
        }
    } else {
      // [PHW] at this moment, just act like next-line prefetcher;
      uint64_t pfn = addr >> LOG2_PAGE_SIZE;
      uint64_t pf_addr = addr + (1 << LOG2_BLOCK_SIZE);
      uint64_t pf_pfn = pf_addr >> LOG2_PAGE_SIZE;
      if (pfn == pf_pfn) { // prefetch only if the next line is in the same page
        mg_table[cpu].update_num_pref(pf_addr);
        prefetch_line(pf_addr, true, metadata_in);
      }
    }
    return metadata_in;
}
uint32_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t match, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in) {
    return metadata_in;
}
void CACHE::prefetcher_final_stats() {
    // print hist table
    hist_table.print();
}