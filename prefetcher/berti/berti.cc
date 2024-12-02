#include "berti.h"

// namespace berti{ // is it necessary to have this namespace?
//     // To access cpu in my functions
// }
void CACHE::prefetcher_initialize()
{
  BERTI::l2c_cpu_id = cpu; //[PHW] get cpu id from where?, inthe cache context?
                           //    l2c_cpu_id = CACHE::cpu;  //[PHW] choose one
  std::cout << "CPU " << cpu << " L2C Berti prefetcher" << std::endl;
  BERTI::l2c_init_current_pages_table();
  BERTI::l2c_init_prev_requests_table();
  BERTI::l2c_init_prev_prefetches_table();
  BERTI::l2c_init_record_pages_table();
  BERTI::l2c_init_ip_table();
}
void CACHE::prefetcher_cycle_operate() {}
uint32_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, bool useful_prefetch, uint8_t type, uint32_t metadata_in)
{
  BERTI::l2c_cpu_id = cpu;
  // [PHW] what is line_addr mean?
  // [PHW] i think it is the cache-line index. hmmm.
  // [PHW] check current champsion code to cache addressingkj
  // uint64_t line_addr = addr >> LOG2_BLOCK_SIZE;
  uint64_t line_addr = addr;
  uint64_t page_addr = line_addr >> L2C_PAGE_BLOCKS_BITS; // page addr == pfn
  uint64_t offset = line_addr & L2C_PAGE_OFFSET_MASK;

  if (useful_prefetch) {
    // std::cout << "useful prefetch" << std::endl;
    if (is_cxl_memory(addr)) {
      num_prefetch_hit_cxl++;
    } else {
      num_prefetch_hit_ddr++;
    }
  }

  // print line_addr by hex
  // std::cout << "[PHW] debug0 addr: " << std::hex << addr <<" line_addr: " << line_addr << " page_addr: " << page_addr << " offset: " << offset << std::endl;

  // Update current pages table
  // Find the entry
  uint64_t index = BERTI::l2c_get_current_pages_entry(page_addr);

  // If not accessed recently
  if (index == L2C_CURRENT_PAGES_TABLE_ENTRIES || !BERTI::l2c_requested_offset_current_pages_table(index, offset)) {

    if (index < L2C_CURRENT_PAGES_TABLE_ENTRIES) { // Found
      // std::cout << "[debug 01-00]Assign same pointer to group IP: " << BERTI::l2c_ip_table[BERTI::l2c_cpu_id][ip & L2C_IP_TABLE_INDEX_MASK] << std::endl;
      // If offset found, already requested, so return;
      if (BERTI::l2c_requested_offset_current_pages_table(index, offset)){
        return metadata_in;
      }

      uint64_t first_ip = BERTI::l2c_update_demand_current_pages_table(index, offset);
      assert(BERTI::l2c_ip_table[BERTI::l2c_cpu_id][first_ip & L2C_IP_TABLE_INDEX_MASK] != L2C_IP_TABLE_NULL_POINTER);

      // Update berti
      if (cache_hit) {
        uint64_t pref_latency = BERTI::l2c_get_latency_prev_prefetches_table(index, offset);
        // std::cout << "[debug 01-05]pref_latency: " << std::dec << pref_latency << std::endl;
        if (pref_latency != 0) {
          // std::cout << "[debug 01-06]pref_latency: " << std::dec << pref_latency << std::endl;
          // Find berti distance from pref_latency cycles before
          int berti[L2C_CURRENT_PAGES_TABLE_NUM_BERTI_PER_ACCESS];
          BERTI::l2c_get_berti_prev_requests_table(index, offset, current_cycle - pref_latency, berti);
          for (int i = 0; i < L2C_CURRENT_PAGES_TABLE_NUM_BERTI_PER_ACCESS; i++) {
            if (berti[i] == 0)
              break;
            assert(abs(berti[i]) < L2C_PAGE_BLOCKS);
            BERTI::l2c_add_berti_current_pages_table(index, berti[i]);
          }

          // Eliminate a prev prefetch since it has been used
          BERTI::l2c_reset_entry_prev_prefetches_table(index, offset);
        }
      }

      if (first_ip != ip) {
        // Assign same pointer to group IPs
        BERTI::l2c_ip_table[BERTI::l2c_cpu_id][ip & L2C_IP_TABLE_INDEX_MASK] = BERTI::l2c_ip_table[BERTI::l2c_cpu_id][first_ip & L2C_IP_TABLE_INDEX_MASK];
        // std::cout << "[debug 01-0N]Assign same pointer to group IP: " << BERTI::l2c_ip_table[BERTI::l2c_cpu_id][ip & L2C_IP_TABLE_INDEX_MASK] << std::endl;
      }
    } else { // Not found: Add entry

      // Find victim and clear pointers to it
      uint64_t victim_index = BERTI::l2c_get_lru_current_pages_entry(); // already updates lru
      // std::cout << "[debug 02-02]victim_index: " << std::dec << victim_index << std::endl;
      assert(victim_index < L2C_CURRENT_PAGES_TABLE_ENTRIES);
      BERTI::l2c_reset_pointer_prev_requests(victim_index);   // Not valid anymore
      BERTI::l2c_reset_pointer_prev_prefetches(victim_index); // Not valid anymore

      // Copy victim to record table
      BERTI::l2c_record_current_page(victim_index);

      // Add new current page
      index = victim_index;
      BERTI::l2c_add_current_pages_table(index, page_addr, ip & L2C_IP_TABLE_INDEX_MASK, offset);

      // Set pointer in IP table
      uint64_t index_record = BERTI::l2c_get_entry_record_pages_table(page_addr, offset);
      // std::cout << "[debug 02-04]index_record: " << std::dec << index_record << std::endl;
      // The ip pointer is null
      if (BERTI::l2c_ip_table[BERTI::l2c_cpu_id][ip & L2C_IP_TABLE_INDEX_MASK] == L2C_IP_TABLE_NULL_POINTER) {
        if (index_record == L2C_RECORD_PAGES_TABLE_ENTRIES) { // Page not recorded
          // std::cout << "[debug 02-05] page not recorded" << std::endl;
          // Get free record page pointer.
          uint64_t new_pointer = BERTI::l2c_get_lru_record_pages_entry();
          BERTI::l2c_ip_table[BERTI::l2c_cpu_id][ip & L2C_IP_TABLE_INDEX_MASK] = new_pointer;
        } else {
          // std::cout << "[debug 02-06] page recorded" << std::endl;
          BERTI::l2c_ip_table[BERTI::l2c_cpu_id][ip & L2C_IP_TABLE_INDEX_MASK] = index_record;
        }
      } else if (BERTI::l2c_ip_table[BERTI::l2c_cpu_id][ip & L2C_IP_TABLE_INDEX_MASK] != index_record) {
        // If the current IP is valid, but points to another address
        // we replicate it in another record entry (lru)
        // such that the recorded page is not deleted when the current entry summarizes
        uint64_t new_pointer = BERTI::l2c_get_lru_record_pages_entry();
        BERTI::l2c_copy_entries_record_pages_table(BERTI::l2c_ip_table[BERTI::l2c_cpu_id][ip & L2C_IP_TABLE_INDEX_MASK], new_pointer);
        BERTI::l2c_ip_table[BERTI::l2c_cpu_id][ip & L2C_IP_TABLE_INDEX_MASK] = new_pointer;
        // std::cout << "[debug 02-07] l2c_ip_table[ip w/ mask]: " << std::dec << (ip & L2C_IP_TABLE_INDEX_MASK) << "\tnew_pointer: " << new_pointer << std::endl;
      }
    }

    // std::cout << "index: " << index << std::endl;
    BERTI::l2c_add_prev_requests_table(index, offset, current_cycle);

    // PREDICT
    uint64_t u_vector = 0;
    uint64_t first_offset = BERTI::l2c_current_pages_table[BERTI::l2c_cpu_id][index].first_offset;
    int berti = 0;
    bool recorded = false;

    uint64_t ip_pointer = BERTI::l2c_ip_table[BERTI::l2c_cpu_id][ip & L2C_IP_TABLE_INDEX_MASK];
    uint64_t pgo_pointer = BERTI::l2c_get_entry_record_pages_table(page_addr, first_offset); // this variable is sustained, 767 is not found
    uint64_t pg_pointer = BERTI::l2c_get_entry_record_pages_table(page_addr);
    uint64_t berti_confidence = 0;
    int current_berti = BERTI::l2c_get_berti_current_pages_table(index, berti_confidence);
    uint64_t match_confidence = 0;
    // std::cout << "[debug 03-01]ip_pointer: " << std::dec << ip_pointer << "\tpgo_pointer: " << pgo_pointer << "\tpg_pointer: " << pg_pointer << "\tcurrent_berti: " << current_berti << std::endl;

    // If match with current page+first_offset, use record
    if (pgo_pointer != L2C_RECORD_PAGES_TABLE_ENTRIES
        && (BERTI::l2c_record_pages_table[BERTI::l2c_cpu_id][pgo_pointer].u_vector | BERTI::l2c_current_pages_table[BERTI::l2c_cpu_id][index].u_vector)
               == BERTI::l2c_record_pages_table[BERTI::l2c_cpu_id][pgo_pointer].u_vector) {
      u_vector = BERTI::l2c_record_pages_table[BERTI::l2c_cpu_id][pgo_pointer].u_vector;
      berti = BERTI::l2c_record_pages_table[BERTI::l2c_cpu_id][pgo_pointer].berti;
      match_confidence = 1; // High confidence
      recorded = true;
    } else
      // If match with current ip+first_offset, use record
      if (BERTI::l2c_record_pages_table[BERTI::l2c_cpu_id][ip_pointer].first_offset == first_offset
          && (BERTI::l2c_record_pages_table[BERTI::l2c_cpu_id][ip_pointer].u_vector | BERTI::l2c_current_pages_table[BERTI::l2c_cpu_id][index].u_vector)
                 == BERTI::l2c_record_pages_table[BERTI::l2c_cpu_id][ip_pointer].u_vector) {
        u_vector = BERTI::l2c_record_pages_table[BERTI::l2c_cpu_id][ip_pointer].u_vector;
        berti = BERTI::l2c_record_pages_table[BERTI::l2c_cpu_id][ip_pointer].berti;
        match_confidence = 1; // High confidence
        recorded = true;
      } else
        // If no exact match, trust current if it has already a berti
        if (current_berti != 0 && berti_confidence >= L2C_BERTI_CTR_MED_HIGH_CONFIDENCE) { // Medium-High confidence
          u_vector = BERTI::l2c_current_pages_table[BERTI::l2c_cpu_id][index].u_vector;
          berti = current_berti;
        } else
          // If match with current page, use record
          if (pg_pointer != L2C_RECORD_PAGES_TABLE_ENTRIES) { // Medium confidence
            u_vector = BERTI::l2c_record_pages_table[BERTI::l2c_cpu_id][pg_pointer].u_vector;
            berti = BERTI::l2c_record_pages_table[BERTI::l2c_cpu_id][pg_pointer].berti;
            recorded = true;
          } else
            // If match with current ip, use record
            if (BERTI::l2c_record_pages_table[BERTI::l2c_cpu_id][ip_pointer].u_vector) { // Medium confidence
              u_vector = BERTI::l2c_record_pages_table[BERTI::l2c_cpu_id][ip_pointer].u_vector;
              berti = BERTI::l2c_record_pages_table[BERTI::l2c_cpu_id][ip_pointer].berti;
              recorded = true;
            }

    // Burst for the first access of a page or if pending bursts
    if (first_offset == offset || BERTI::l2c_current_pages_table[BERTI::l2c_cpu_id][index].last_burst != 0) {
      uint64_t first_burst;
      if (BERTI::l2c_current_pages_table[BERTI::l2c_cpu_id][index].last_burst != 0) {
        first_burst = BERTI::l2c_current_pages_table[BERTI::l2c_cpu_id][index].last_burst;
        BERTI::l2c_current_pages_table[BERTI::l2c_cpu_id][index].last_burst = 0;
      } else if (berti >= 0) {
        first_burst = offset + 1;
      } else {
        first_burst = offset - 1;
      }
      if (recorded && match_confidence) {
        int bursts = 0;
        if (berti > 0) {
          for (uint64_t i = first_burst; i < offset + berti; i++) {
            if (i >= L2C_PAGE_BLOCKS)
              break; // Stay in the page
            // Only if previously requested and not demanded
            uint64_t pf_line_addr = (page_addr << L2C_PAGE_BLOCKS_BITS) | i;
            uint64_t pf_addr = pf_line_addr << LOG2_BLOCK_SIZE;
            uint64_t pf_page_addr = pf_line_addr >> L2C_PAGE_BLOCKS_BITS;
            uint64_t pf_offset = pf_line_addr & L2C_PAGE_OFFSET_MASK;
            if ((((uint64_t)1 << i) & u_vector) && !BERTI::l2c_requested_offset_current_pages_table(index, pf_offset)) {
              //[PHW] is it right to use internal_PQ here?
              if (std::size(this->internal_PQ) < this->PQ_SIZE && bursts < L2C_MAX_NUM_BURST_PREFETCHES) {
                // bool prefetched = prefetch_line(ip, addr, pf_addr, true, 0); // [PHW] fill l2 is true
                bool prefetched = false;
                if (pf_page_addr == page_addr) {
                  prefetched = prefetch_line(pf_addr, true, 0); // [PHW] fill l2 is true
                }
                if (prefetched) {
                  assert(pf_page_addr == page_addr);
                  // std::cout << "[debug 05-01]prefetched: 0x" << std::hex << pf_addr << std::endl;
                  if (is_cxl_memory(pf_addr)) {
                    num_prefetch_cxl++;
                  } else {
                    num_prefetch_ddr++;
                  }
                  BERTI::l2c_add_prev_prefetches_table(index, pf_offset, current_cycle);
                  bursts++;
                }
              } else { // record last burst
                BERTI::l2c_current_pages_table[BERTI::l2c_cpu_id][index].last_burst = i;
                break;
              }
            }
          }
        } else if (berti < 0) {
          for (int i = first_burst; i > ((int)offset) + berti; i--) {
            if (i < 0)
              break; // Stay in the page
            // Only if previously requested and not demanded
            uint64_t pf_line_addr = (page_addr << L2C_PAGE_BLOCKS_BITS) | i;
            uint64_t pf_addr = pf_line_addr << LOG2_BLOCK_SIZE;
            uint64_t pf_page_addr = pf_line_addr >> L2C_PAGE_BLOCKS_BITS;
            uint64_t pf_offset = pf_line_addr & L2C_PAGE_OFFSET_MASK;
            if ((((uint64_t)1 << i) & u_vector) && !BERTI::l2c_requested_offset_current_pages_table(index, pf_offset)) {
              if (std::size(this->internal_PQ) < this->PQ_SIZE && bursts < L2C_MAX_NUM_BURST_PREFETCHES) {
                // bool prefetched = prefetch_line(ip, addr, pf_addr, true, 0);
                bool prefetched = false;
                if(pf_page_addr == page_addr){
                  prefetched = prefetch_line(pf_addr, true, 0); // [PHW] fill l2 is true
                }
                if (prefetched) {
                  assert(pf_page_addr == page_addr);
                  // std::cout << "[debug 05-02]prefetched: 0x" << std::hex << pf_addr << std::endl;
                  if (is_cxl_memory(pf_addr)) {
                    num_prefetch_cxl++;
                  } else {
                    num_prefetch_ddr++;
                  }
                  BERTI::l2c_add_prev_prefetches_table(index, pf_offset, current_cycle);
                  bursts++;
                }
              } else { // record last burst
                BERTI::l2c_current_pages_table[BERTI::l2c_cpu_id][index].last_burst = i;
                break;
              }
            }
          }
        } else { // berti == 0 (zig zag of all)
          for (int i = first_burst, j = (first_offset << 1) - i; i < L2C_PAGE_BLOCKS || j >= 0; i++, j = (first_offset << 1) - i) {
            // Only if previously requested and not demanded
            // Dir ++
            uint64_t pf_line_addr = (page_addr << L2C_PAGE_BLOCKS_BITS) | i;
            uint64_t pf_addr = pf_line_addr << LOG2_BLOCK_SIZE;
            uint64_t pf_page_addr = pf_line_addr >> L2C_PAGE_BLOCKS_BITS;
            uint64_t pf_offset = pf_line_addr & L2C_PAGE_OFFSET_MASK;
            if ((((uint64_t)1 << i) & u_vector) && !BERTI::l2c_requested_offset_current_pages_table(index, pf_offset)) {
              if (std::size(this->internal_PQ) < this->PQ_SIZE && bursts < L2C_MAX_NUM_BURST_PREFETCHES) {
                // bool prefetched = prefetch_line(ip, addr, pf_addr, true, 0);
                bool prefetched = false;
                if (pf_page_addr == page_addr) {
                  prefetched = prefetch_line(pf_addr, true, 0); // [PHW] fill l2 is true
                }
                if (prefetched) {
                  assert(pf_page_addr == page_addr);
                  // std::cout << "[debug 05-03]prefetched: 0x" << std::hex << pf_addr << std::endl;
                  if (is_cxl_memory(pf_addr)) {
                    num_prefetch_cxl++;
                  } else {
                    num_prefetch_ddr++;
                  }
                  BERTI::l2c_add_prev_prefetches_table(index, pf_offset, current_cycle);
                  bursts++;
                }
              } else { // record last burst
                BERTI::l2c_current_pages_table[BERTI::l2c_cpu_id][index].last_burst = i;
                break;
              }
            }
            // Dir --
            pf_line_addr = (page_addr << L2C_PAGE_BLOCKS_BITS) | j;
            pf_addr = pf_line_addr << LOG2_BLOCK_SIZE;
            pf_page_addr = pf_line_addr >> L2C_PAGE_BLOCKS_BITS;
            pf_offset = pf_line_addr & L2C_PAGE_OFFSET_MASK;
            if ((((uint64_t)1 << j) & u_vector) && !BERTI::l2c_requested_offset_current_pages_table(index, pf_offset)) {
              if (std::size(this->internal_PQ) < this->PQ_SIZE && bursts < L2C_MAX_NUM_BURST_PREFETCHES) {
                // bool prefetched = prefetch_line(ip, addr, pf_addr, true, 0);
                bool prefetched = false;
                if (pf_page_addr == page_addr) {
                  prefetched = prefetch_line(pf_addr, true, 0); // [PHW] fill l2 is true
                }
                if (prefetched) {
                  assert(pf_page_addr == page_addr);
                  if (is_cxl_memory(pf_addr)) {
                    num_prefetch_cxl++;
                  } else {
                    num_prefetch_ddr++;
                  }
                  // std::cout << "[debug 05-04]prefetched: 0x" << std::hex << pf_addr << std::endl;
                  BERTI::l2c_add_prev_prefetches_table(index, pf_offset, current_cycle);
                  bursts++;
                }
              } else {
                // record only positive burst
              }
            }
          }
        }
      } else { // not recorded
      }
    }

    if (berti != 0) {
      // TODO: Remove prefetch if already used!
      uint64_t pf_line_addr = line_addr + berti;
      uint64_t pf_addr = pf_line_addr << LOG2_BLOCK_SIZE;
      uint64_t pf_page_addr = pf_line_addr >> L2C_PAGE_BLOCKS_BITS;
      uint64_t pf_offset = pf_line_addr & L2C_PAGE_OFFSET_MASK;
      // std::cout << "[PHW] debug1 line_addr: " << line_addr << " berti: " << berti <<  " pf_line_addr: " << pf_line_addr << " pf_addr: " << pf_addr << "
      // pf_page_addr: " << pf_page_addr << " pf_offset: " << pf_offset << std::endl;
      if (!BERTI::l2c_requested_offset_current_pages_table(index, pf_offset)   // Only prefetch if not demanded
          && (!match_confidence || (((uint64_t)1 << pf_offset) & u_vector))) { // And prev. accessed
        // bool prefetched = prefetch_line(ip, addr, pf_addr, true, 0);
        bool prefetched = false;
        if (pf_page_addr == page_addr) {
          prefetched = prefetch_line(pf_addr, true, 0); // [PHW] fill l2 is true
          if (is_cxl_memory(pf_addr)) {
            num_prefetch_cxl++;
          } else {
            num_prefetch_ddr++;
          }
        }
        if (prefetched) {
          // std::cout << "[PHW] debug2 pf_page_addr: " << pf_page_addr << " page_addr: " << page_addr << " berti: " << berti << std::endl;
          assert(pf_page_addr == page_addr);
          BERTI::l2c_add_prev_prefetches_table(index, pf_offset, current_cycle);
        }
      }
    }
  }
  return metadata_in;
}
uint32_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t match, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)
{
  BERTI::l2c_cpu_id = cpu;
  // uint64_t line_addr = (addr >> LOG2_BLOCK_SIZE);
  uint64_t line_addr = addr;
  uint64_t page_addr = line_addr >> L2C_PAGE_BLOCKS_BITS;
  uint64_t offset = line_addr & L2C_PAGE_OFFSET_MASK;

  uint64_t pointer_prev = BERTI::l2c_get_current_pages_entry(page_addr);

  if (pointer_prev < L2C_CURRENT_PAGES_TABLE_ENTRIES) { // Not found, not entry in prev requests
    uint64_t pref_latency = BERTI::l2c_get_and_set_latency_prev_prefetches_table(pointer_prev, offset, current_cycle);
    uint64_t demand_latency = BERTI::l2c_get_latency_prev_requests_table(pointer_prev, offset, current_cycle);

    // First look in prefetcher, since if there is a hit, it is the time the miss started
    // If no prefetch, then its latency is the demand one
    if (pref_latency == 0) {
      pref_latency = demand_latency;
    }

    if (demand_latency != 0) { // Not found, berti will not be found neither

      // Find berti (distance from pref_latency + demand_latency cycles before
      int berti[L2C_CURRENT_PAGES_TABLE_NUM_BERTI_PER_ACCESS];
      BERTI::l2c_get_berti_prev_requests_table(pointer_prev, offset, current_cycle - (pref_latency + demand_latency), berti);
      // std::cout<< "[PHW]debug 3 demand_lat: " << std::dec << demand_latency << " pref_lat: " << pref_latency << " current_cycle: " << current_cycle <<
      // std::endl;
      for (int i = 0; i < L2C_CURRENT_PAGES_TABLE_NUM_BERTI_PER_ACCESS; i++) {
        if (berti[i] == 0)
          break;
        assert(abs(berti[i]) < L2C_PAGE_BLOCKS);
        BERTI::l2c_add_berti_current_pages_table(pointer_prev, berti[i]);
      }
    }
  }

  uint64_t victim_index = BERTI::l2c_get_current_pages_entry(evicted_addr >> LOG2_PAGE_SIZE);
  if (victim_index < L2C_CURRENT_PAGES_TABLE_ENTRIES) {
    // Copy victim to record table
    BERTI::l2c_record_current_page(victim_index);
    BERTI::l2c_remove_current_table_entry(victim_index);
  }

  return metadata_in;
}
void CACHE::prefetcher_final_stats()
{
  std::cout << "CPU " << cpu << " L2C Berti prefetcher final stats" << std::endl;
  std::cout << "num_prefetch_ddr: " << std::dec << num_prefetch_ddr << std::endl;
  std::cout << "num_prefetch_hit_ddr: " << std::dec << num_prefetch_hit_ddr << std::endl;
  std::cout << "num_prefetch_cxl: " << std::dec << num_prefetch_cxl << std::endl;
  std::cout << "num_prefetch_hit_cxl: " << std::dec << num_prefetch_hit_cxl << std::endl;
}