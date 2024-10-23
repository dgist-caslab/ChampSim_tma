// Code submitted for the Third Data Prefetching Championship
//
// Author: Alberto Ros, University of Murcia
// Migrated to recent Champsim by: hw park, DGIST
//
// cited from #13: Berti: A Per-Page Best-Request-Time Delta Prefetcher

#ifndef BERTI_H
#define BERTI_H

#include "cache.h"
#include <cassert>
#include <cstdint>
#include <iostream>

namespace BERTI
{
uint32_t l2c_cpu_id;
#define L2C_PAGE_BLOCKS_BITS (LOG2_PAGE_SIZE - LOG2_BLOCK_SIZE)
#define L2C_PAGE_BLOCKS (1 << L2C_PAGE_BLOCKS_BITS)
#define L2C_PAGE_OFFSET_MASK (L2C_PAGE_BLOCKS - 1)

#define L2C_MAX_NUM_BURST_PREFETCHES 3

#define L2C_BERTI_CTR_MED_HIGH_CONFIDENCE 2

#define L2C_TIME_BITS 16
#define L2C_TIME_OVERFLOW ((uint64_t)1 << L2C_TIME_BITS)
#define L2C_TIME_MASK (L2C_TIME_OVERFLOW - 1)

// CURRENT PAGES TABLE
#define L2C_CURRENT_PAGES_TABLE_INDEX_BITS 6
#define L2C_CURRENT_PAGES_TABLE_ENTRIES ((1 << L2C_CURRENT_PAGES_TABLE_INDEX_BITS) - 1) // Null pointer for prev_request
#define L2C_CURRENT_PAGES_TABLE_NUM_BERTI 10
#define L2C_CURRENT_PAGES_TABLE_NUM_BERTI_PER_ACCESS 7

typedef struct __l2c_current_page_entry {
  uint64_t page_addr; // 52 bits
  uint64_t ip; // 10 bits
  uint64_t u_vector; // 64 bits
  uint64_t first_offset; // 6 bits
  int berti[L2C_CURRENT_PAGES_TABLE_NUM_BERTI]; // 70 bits
  unsigned berti_ctr[L2C_CURRENT_PAGES_TABLE_NUM_BERTI]; // 60 bits
  uint64_t last_burst; // 6 bits
  uint64_t lru; // 6 bits
} l2c_current_page_entry;

l2c_current_page_entry l2c_current_pages_table[NUM_CPUS][L2C_CURRENT_PAGES_TABLE_ENTRIES];

// PREVIOUS REQUESTS TABLE
#define L2C_PREV_REQUESTS_TABLE_INDEX_BITS 10
#define L2C_PREV_REQUESTS_TABLE_ENTRIES (1 << L2C_PREV_REQUESTS_TABLE_INDEX_BITS)
#define L2C_PREV_REQUESTS_TABLE_MASK (L2C_PREV_REQUESTS_TABLE_ENTRIES - 1)
#define L2C_PREV_REQUESTS_TABLE_NULL_POINTER L2C_CURRENT_PAGES_TABLE_ENTRIES

typedef struct __l2c_prev_request_entry {
  uint64_t page_addr_pointer; // 6 bits
  uint64_t offset; // 6 bits
  uint64_t time; // 16 bits
} l2c_prev_request_entry;

l2c_prev_request_entry l2c_prev_requests_table[NUM_CPUS][L2C_PREV_REQUESTS_TABLE_ENTRIES];
uint64_t l2c_prev_requests_table_head[NUM_CPUS];

// PREVIOUS PREFETCHES TABLE

#define L2C_PREV_PREFETCHES_TABLE_INDEX_BITS 9
#define L2C_PREV_PREFETCHES_TABLE_ENTRIES (1 << L2C_PREV_PREFETCHES_TABLE_INDEX_BITS)
#define L2C_PREV_PREFETCHES_TABLE_MASK (L2C_PREV_PREFETCHES_TABLE_ENTRIES - 1)
#define L2C_PREV_PREFETCHES_TABLE_NULL_POINTER L2C_CURRENT_PAGES_TABLE_ENTRIES

// We do not have access to the MSHR, so we aproximate it using this structure.
typedef struct __l2c_prev_prefetch_entry {
  uint64_t page_addr_pointer; // 6 bits
  uint64_t offset; // 6 bits
  uint64_t time_lat; // 16 bits // time if not completed, latency if completed
  bool completed; // 1 bit
} l2c_prev_prefetch_entry;

l2c_prev_prefetch_entry l2c_prev_prefetches_table[NUM_CPUS][L2C_PREV_PREFETCHES_TABLE_ENTRIES];
uint64_t l2c_prev_prefetches_table_head[NUM_CPUS];

// RECORD PAGES TABLE
#define L2C_RECORD_PAGES_TABLE_ENTRIES (((1 << 9) + (1 << 8)) - 1) // Null pointer for ip table
#define L2C_TRUNCATED_PAGE_ADDR_BITS 32 // 4 bytes
#define L2C_TRUNCATED_PAGE_ADDR_MASK (((uint64_t)1 << L2C_TRUNCATED_PAGE_ADDR_BITS) -1)

typedef struct __l2c_record_page_entry {
  uint64_t page_addr; // 4 bytes
  uint64_t u_vector; // 8 bytes
  uint64_t first_offset; // 6 bits
  int berti; // 7 bits
  uint64_t lru; // 10 bits
} l2c_record_page_entry;

l2c_record_page_entry l2c_record_pages_table[NUM_CPUS][L2C_RECORD_PAGES_TABLE_ENTRIES];

// IP TABLE

// [PHW]start Berti functions
#define L2C_IP_TABLE_INDEX_BITS 10
#define L2C_IP_TABLE_ENTRIES (1 << L2C_IP_TABLE_INDEX_BITS)
#define L2C_IP_TABLE_INDEX_MASK (L2C_IP_TABLE_ENTRIES - 1)
#define L2C_IP_TABLE_NULL_POINTER L2C_RECORD_PAGES_TABLE_ENTRIES

uint64_t l2c_ip_table[NUM_CPUS][L2C_IP_TABLE_ENTRIES]; // 10 bits


// TIME AND OVERFLOWS
uint64_t l2c_get_latency(uint64_t cycle, uint64_t cycle_prev) {
    return cycle - cycle_prev;
    uint64_t cycle_masked = cycle & L2C_TIME_MASK;
    uint64_t cycle_prev_masked = cycle_prev & L2C_TIME_MASK;
    if (cycle_prev_masked > cycle_masked) {
      return (cycle_masked + L2C_TIME_OVERFLOW) - cycle_prev_masked;
    }
    return cycle_masked - cycle_prev_masked;
}

// STRIDE
int l2c_calculate_stride(uint64_t prev_offset, uint64_t current_offset) {
  int stride;
  if (current_offset > prev_offset) {
    stride = current_offset - prev_offset;
  } else {
    stride = prev_offset - current_offset;
    stride *= -1;
  }
  return stride;
}

void l2c_init_current_pages_table() {
  for (int i = 0; i < L2C_CURRENT_PAGES_TABLE_ENTRIES; i++) {
    l2c_current_pages_table[l2c_cpu_id][i].page_addr = 0;
    l2c_current_pages_table[l2c_cpu_id][i].ip = 0;
    l2c_current_pages_table[l2c_cpu_id][i].u_vector = 0; // not valid
    l2c_current_pages_table[l2c_cpu_id][i].last_burst = 0;
    l2c_current_pages_table[l2c_cpu_id][i].lru = i;
  }
}

uint64_t l2c_get_current_pages_entry(uint64_t page_addr) {
  for (int i = 0; i < L2C_CURRENT_PAGES_TABLE_ENTRIES; i++) {
    if (l2c_current_pages_table[l2c_cpu_id][i].page_addr == page_addr) return i;
  }
  return L2C_CURRENT_PAGES_TABLE_ENTRIES;
}

void l2c_update_lru_current_pages_table(uint64_t index) {
  assert(index < L2C_CURRENT_PAGES_TABLE_ENTRIES);
  for (int i = 0; i < L2C_CURRENT_PAGES_TABLE_ENTRIES; i++) {
    if (l2c_current_pages_table[l2c_cpu_id][i].lru < l2c_current_pages_table[l2c_cpu_id][index].lru) { // Found
      l2c_current_pages_table[l2c_cpu_id][i].lru++;
    }
  }
  l2c_current_pages_table[l2c_cpu_id][index].lru = 0;
}

uint64_t l2c_get_lru_current_pages_entry() {
  uint64_t lru = L2C_CURRENT_PAGES_TABLE_ENTRIES;
  for (int i = 0; i < L2C_CURRENT_PAGES_TABLE_ENTRIES; i++) {
    l2c_current_pages_table[l2c_cpu_id][i].lru++;
    if (l2c_current_pages_table[l2c_cpu_id][i].lru == L2C_CURRENT_PAGES_TABLE_ENTRIES) {
      l2c_current_pages_table[l2c_cpu_id][i].lru = 0;
      lru = i;
    } 
  }
  assert(lru != L2C_CURRENT_PAGES_TABLE_ENTRIES);
  return lru;
}

int l2c_get_berti_current_pages_table(uint64_t index, uint64_t &ctr) {
  assert(index < L2C_CURRENT_PAGES_TABLE_ENTRIES);
  uint64_t max_score = 0;
  uint64_t berti = 0;
  for (int i = 0; i < L2C_CURRENT_PAGES_TABLE_NUM_BERTI; i++) {
    uint64_t score; 
    score = l2c_current_pages_table[l2c_cpu_id][index].berti_ctr[i];
    if (score > max_score) {
      berti = l2c_current_pages_table[l2c_cpu_id][index].berti[i];
      max_score = score;
      ctr = l2c_current_pages_table[l2c_cpu_id][index].berti_ctr[i];
    }
  }
  return berti;
}

void l2c_add_current_pages_table(uint64_t index, uint64_t page_addr, uint64_t ip, uint64_t offset) {
  assert(index < L2C_CURRENT_PAGES_TABLE_ENTRIES);
  l2c_current_pages_table[l2c_cpu_id][index].page_addr = page_addr;
  l2c_current_pages_table[l2c_cpu_id][index].ip = ip;
  l2c_current_pages_table[l2c_cpu_id][index].u_vector = (uint64_t)1 << offset;
  l2c_current_pages_table[l2c_cpu_id][index].first_offset = offset;
  for (int i = 0; i < L2C_CURRENT_PAGES_TABLE_NUM_BERTI; i++) {
    l2c_current_pages_table[l2c_cpu_id][index].berti_ctr[i] = 0;
  }
  l2c_current_pages_table[l2c_cpu_id][index].last_burst = 0;
}

uint64_t l2c_update_demand_current_pages_table(uint64_t index, uint64_t offset) {
  assert(index < L2C_CURRENT_PAGES_TABLE_ENTRIES);
  l2c_current_pages_table[l2c_cpu_id][index].u_vector |= (uint64_t)1 << offset;
  l2c_update_lru_current_pages_table(index);
  return l2c_current_pages_table[l2c_cpu_id][index].ip;
}

void l2c_add_berti_current_pages_table(uint64_t index, int berti) {
  assert(berti != 0);
  assert(index < L2C_CURRENT_PAGES_TABLE_ENTRIES);
  for (int i = 0; i < L2C_CURRENT_PAGES_TABLE_NUM_BERTI; i++) {
    if (l2c_current_pages_table[l2c_cpu_id][index].berti_ctr[i] == 0) {
      l2c_current_pages_table[l2c_cpu_id][index].berti[i] = berti;
      l2c_current_pages_table[l2c_cpu_id][index].berti_ctr[i] = 1;
      break;
    } else if (l2c_current_pages_table[l2c_cpu_id][index].berti[i] == berti) {
      l2c_current_pages_table[l2c_cpu_id][index].berti_ctr[i]++;
      break;
    }
  }
  l2c_update_lru_current_pages_table(index);
}

bool l2c_requested_offset_current_pages_table(uint64_t index, uint64_t offset) {
  assert(index < L2C_CURRENT_PAGES_TABLE_ENTRIES);
  return l2c_current_pages_table[l2c_cpu_id][index].u_vector & ((uint64_t)1 << offset);
}

void l2c_remove_current_table_entry(uint64_t index) {
  l2c_current_pages_table[l2c_cpu_id][index].page_addr = 0;
  l2c_current_pages_table[l2c_cpu_id][index].u_vector = 0;
  l2c_current_pages_table[l2c_cpu_id][index].berti[0] = 0;
}

// PREVIOUS REQUESTS TABLE
void l2c_init_prev_requests_table() {
  l2c_prev_requests_table_head[l2c_cpu_id] = 0;
  for (int i = 0; i < L2C_PREV_REQUESTS_TABLE_ENTRIES; i++) {
    l2c_prev_requests_table[l2c_cpu_id][i].page_addr_pointer = L2C_PREV_REQUESTS_TABLE_NULL_POINTER;
  }
}

uint64_t l2c_find_prev_request_entry(uint64_t pointer, uint64_t offset) {
  for (int i = 0; i < L2C_PREV_REQUESTS_TABLE_ENTRIES; i++) {
    if (l2c_prev_requests_table[l2c_cpu_id][i].page_addr_pointer == pointer
	&& l2c_prev_requests_table[l2c_cpu_id][i].offset == offset) return i;
  }
  return L2C_PREV_REQUESTS_TABLE_ENTRIES;
}

void l2c_add_prev_requests_table(uint64_t pointer, uint64_t offset, uint64_t cycle) {
  // First find for coalescing
  if (l2c_find_prev_request_entry(pointer, offset) != L2C_PREV_REQUESTS_TABLE_ENTRIES) return;

  // Allocate a new entry (evict old one if necessary)
  l2c_prev_requests_table[l2c_cpu_id][l2c_prev_requests_table_head[l2c_cpu_id]].page_addr_pointer = pointer;
  l2c_prev_requests_table[l2c_cpu_id][l2c_prev_requests_table_head[l2c_cpu_id]].offset = offset;
  l2c_prev_requests_table[l2c_cpu_id][l2c_prev_requests_table_head[l2c_cpu_id]].time = cycle & L2C_TIME_MASK;
  l2c_prev_requests_table_head[l2c_cpu_id] = (l2c_prev_requests_table_head[l2c_cpu_id] + 1) & L2C_PREV_REQUESTS_TABLE_MASK;
}

void l2c_reset_pointer_prev_requests(uint64_t pointer) {
  for (int i = 0; i < L2C_PREV_REQUESTS_TABLE_ENTRIES; i++) {
    if (l2c_prev_requests_table[l2c_cpu_id][i].page_addr_pointer == pointer) {
      l2c_prev_requests_table[l2c_cpu_id][i].page_addr_pointer = L2C_PREV_REQUESTS_TABLE_NULL_POINTER;
    }
  }
}

uint64_t l2c_get_latency_prev_requests_table(uint64_t pointer, uint64_t offset, uint64_t cycle) {
  uint64_t index = l2c_find_prev_request_entry(pointer, offset); 
  if (index == L2C_PREV_REQUESTS_TABLE_ENTRIES) return 0;
  return l2c_get_latency(cycle, l2c_prev_requests_table[l2c_cpu_id][index].time);
}

void l2c_get_berti_prev_requests_table(uint64_t pointer, uint64_t offset, uint64_t cycle, int *berti) {
  int my_pos = 0;
  uint64_t extra_time = 0;
  uint64_t last_time = l2c_prev_requests_table[l2c_cpu_id][(l2c_prev_requests_table_head[l2c_cpu_id] + L2C_PREV_REQUESTS_TABLE_MASK) & L2C_PREV_REQUESTS_TABLE_MASK].time;
  for (uint64_t i = (l2c_prev_requests_table_head[l2c_cpu_id] + L2C_PREV_REQUESTS_TABLE_MASK) & L2C_PREV_REQUESTS_TABLE_MASK; i != l2c_prev_requests_table_head[l2c_cpu_id]; i = (i + L2C_PREV_REQUESTS_TABLE_MASK) & L2C_PREV_REQUESTS_TABLE_MASK) {
    // Against the time overflow
    if (last_time < l2c_prev_requests_table[l2c_cpu_id][i].time) {
      extra_time = L2C_TIME_OVERFLOW;
    }
    last_time = l2c_prev_requests_table[l2c_cpu_id][i].time;  
    if (l2c_prev_requests_table[l2c_cpu_id][i].page_addr_pointer == pointer) {
      if (l2c_prev_requests_table[l2c_cpu_id][i].time <= (cycle & L2C_TIME_MASK) + extra_time) {
	berti[my_pos] = l2c_calculate_stride(l2c_prev_requests_table[l2c_cpu_id][i].offset, offset);
	my_pos++;
	if (my_pos == L2C_CURRENT_PAGES_TABLE_NUM_BERTI_PER_ACCESS) return;
      }
    }
  }
  berti[my_pos] = 0;
}

// PREVIOUS PREFETCHES TABLE
void l2c_init_prev_prefetches_table() {
  l2c_prev_prefetches_table_head[l2c_cpu_id] = 0;
  for (int i = 0; i < L2C_PREV_PREFETCHES_TABLE_ENTRIES; i++) {
    l2c_prev_prefetches_table[l2c_cpu_id][i].page_addr_pointer = L2C_PREV_PREFETCHES_TABLE_NULL_POINTER;
  }
}

uint64_t l2c_find_prev_prefetch_entry(uint64_t pointer, uint64_t offset) {
  for (int i = 0; i < L2C_PREV_PREFETCHES_TABLE_ENTRIES; i++) {
    if (l2c_prev_prefetches_table[l2c_cpu_id][i].page_addr_pointer == pointer
	&& l2c_prev_prefetches_table[l2c_cpu_id][i].offset == offset) return i;
  }
  return L2C_PREV_PREFETCHES_TABLE_ENTRIES;
}

void l2c_add_prev_prefetches_table(uint64_t pointer, uint64_t offset, uint64_t cycle) {
  // First find for coalescing
  if (l2c_find_prev_prefetch_entry(pointer, offset) != L2C_PREV_PREFETCHES_TABLE_ENTRIES) return;

  // Allocate a new entry (evict old one if necessary)
  l2c_prev_prefetches_table[l2c_cpu_id][l2c_prev_prefetches_table_head[l2c_cpu_id]].page_addr_pointer = pointer;
  l2c_prev_prefetches_table[l2c_cpu_id][l2c_prev_prefetches_table_head[l2c_cpu_id]].offset = offset;
  l2c_prev_prefetches_table[l2c_cpu_id][l2c_prev_prefetches_table_head[l2c_cpu_id]].time_lat = cycle & L2C_TIME_MASK;
  l2c_prev_prefetches_table[l2c_cpu_id][l2c_prev_prefetches_table_head[l2c_cpu_id]].completed = false;
  l2c_prev_prefetches_table_head[l2c_cpu_id] = (l2c_prev_prefetches_table_head[l2c_cpu_id] + 1) & L2C_PREV_PREFETCHES_TABLE_MASK;
}

void l2c_reset_pointer_prev_prefetches(uint64_t pointer) {
  for (int i = 0; i < L2C_PREV_PREFETCHES_TABLE_ENTRIES; i++) {
    if (l2c_prev_prefetches_table[l2c_cpu_id][i].page_addr_pointer == pointer) {
      l2c_prev_prefetches_table[l2c_cpu_id][i].page_addr_pointer = L2C_PREV_PREFETCHES_TABLE_NULL_POINTER;
    }
  }
}

void l2c_reset_entry_prev_prefetches_table(uint64_t pointer, uint64_t offset) {
  uint64_t index = l2c_find_prev_prefetch_entry(pointer, offset);
  if (index != L2C_PREV_PREFETCHES_TABLE_ENTRIES) {
    l2c_prev_prefetches_table[l2c_cpu_id][index].page_addr_pointer = L2C_PREV_PREFETCHES_TABLE_NULL_POINTER;
  }
}

uint64_t l2c_get_and_set_latency_prev_prefetches_table(uint64_t pointer, uint64_t offset, uint64_t cycle) {
  uint64_t index = l2c_find_prev_prefetch_entry(pointer, offset); 
  if (index == L2C_PREV_PREFETCHES_TABLE_ENTRIES) return 0;
  if (!l2c_prev_prefetches_table[l2c_cpu_id][index].completed) {
    l2c_prev_prefetches_table[l2c_cpu_id][index].time_lat = l2c_get_latency(cycle, l2c_prev_prefetches_table[l2c_cpu_id][index].time_lat);
    l2c_prev_prefetches_table[l2c_cpu_id][index].completed = true;
  }    
  return l2c_prev_prefetches_table[l2c_cpu_id][index].time_lat;
}

uint64_t l2c_get_latency_prev_prefetches_table(uint64_t pointer, uint64_t offset) {
  uint64_t index = l2c_find_prev_prefetch_entry(pointer, offset);
  if (index == L2C_PREV_PREFETCHES_TABLE_ENTRIES) return 0;
  if (!l2c_prev_prefetches_table[l2c_cpu_id][index].completed) return 0;
  return l2c_prev_prefetches_table[l2c_cpu_id][index].time_lat;
}

// RECORD PAGES TABLE
void l2c_init_record_pages_table() {
  for (int i = 0; i < L2C_RECORD_PAGES_TABLE_ENTRIES; i++) {
    l2c_record_pages_table[l2c_cpu_id][i].page_addr = 0;
    l2c_record_pages_table[l2c_cpu_id][i].u_vector = 0;
    l2c_record_pages_table[l2c_cpu_id][i].lru = i;
  }
}

uint64_t l2c_get_lru_record_pages_entry() {
  uint64_t lru = L2C_RECORD_PAGES_TABLE_ENTRIES;
  for (int i = 0; i < L2C_RECORD_PAGES_TABLE_ENTRIES; i++) {
    l2c_record_pages_table[l2c_cpu_id][i].lru++;
    if (l2c_record_pages_table[l2c_cpu_id][i].lru == L2C_RECORD_PAGES_TABLE_ENTRIES) {
      l2c_record_pages_table[l2c_cpu_id][i].lru = 0;
      lru = i;
    } 
  }
  assert(lru != L2C_RECORD_PAGES_TABLE_ENTRIES);
  return lru;
}

void l2c_update_lru_record_pages_table(uint64_t index) {
  assert(index < L2C_RECORD_PAGES_TABLE_ENTRIES);
  for (int i = 0; i < L2C_RECORD_PAGES_TABLE_ENTRIES; i++) {
    if (l2c_record_pages_table[l2c_cpu_id][i].lru < l2c_record_pages_table[l2c_cpu_id][index].lru) { // Found
      l2c_record_pages_table[l2c_cpu_id][i].lru++;
    }
  }
  l2c_record_pages_table[l2c_cpu_id][index].lru = 0;
}

void l2c_add_record_pages_table(uint64_t index, uint64_t page_addr, uint64_t vector, uint64_t first_offset, int berti) {
  assert(index < L2C_RECORD_PAGES_TABLE_ENTRIES);
  l2c_record_pages_table[l2c_cpu_id][index].page_addr = page_addr & L2C_TRUNCATED_PAGE_ADDR_MASK;
  l2c_record_pages_table[l2c_cpu_id][index].u_vector = vector;
  l2c_record_pages_table[l2c_cpu_id][index].first_offset = first_offset;
  l2c_record_pages_table[l2c_cpu_id][index].berti = berti;    
  l2c_update_lru_record_pages_table(index);
}

uint64_t l2c_get_entry_record_pages_table(uint64_t page_addr, uint64_t first_offset) {
  uint64_t trunc_page_addr = page_addr & L2C_TRUNCATED_PAGE_ADDR_MASK;
  for (int i = 0; i < L2C_RECORD_PAGES_TABLE_ENTRIES; i++) {
    if (l2c_record_pages_table[l2c_cpu_id][i].page_addr == trunc_page_addr
	&& l2c_record_pages_table[l2c_cpu_id][i].first_offset == first_offset) { // Found
      return i;
    }
  }
  return L2C_RECORD_PAGES_TABLE_ENTRIES;
}

uint64_t l2c_get_entry_record_pages_table(uint64_t page_addr) {
  uint64_t trunc_page_addr = page_addr & L2C_TRUNCATED_PAGE_ADDR_MASK;  
  for (int i = 0; i < L2C_RECORD_PAGES_TABLE_ENTRIES; i++) {
    if (l2c_record_pages_table[l2c_cpu_id][i].page_addr == trunc_page_addr) { // Found
      return i;
    }
  }
  return L2C_RECORD_PAGES_TABLE_ENTRIES;
}

void l2c_copy_entries_record_pages_table(uint64_t index_from, uint64_t index_to) {
  assert(index_from < L2C_RECORD_PAGES_TABLE_ENTRIES);
  assert(index_to < L2C_RECORD_PAGES_TABLE_ENTRIES);
  l2c_record_pages_table[l2c_cpu_id][index_to].page_addr = l2c_record_pages_table[l2c_cpu_id][index_from].page_addr;
  l2c_record_pages_table[l2c_cpu_id][index_to].u_vector = l2c_record_pages_table[l2c_cpu_id][index_from].u_vector;
  l2c_record_pages_table[l2c_cpu_id][index_to].first_offset = l2c_record_pages_table[l2c_cpu_id][index_from].first_offset;
  l2c_record_pages_table[l2c_cpu_id][index_to].berti = l2c_record_pages_table[l2c_cpu_id][index_from].berti;    
  l2c_update_lru_record_pages_table(index_to);
}

// IP TABLE
void l2c_init_ip_table() {
  for (int i = 0; i < L2C_IP_TABLE_ENTRIES; i++) {
    l2c_ip_table[l2c_cpu_id][i] = L2C_IP_TABLE_NULL_POINTER;
  }
}


// TABLE MOVEMENTS

// Sumarizes the content to the current page to be evicted
// From all timely requests found, we record the best 
void l2c_record_current_page(uint64_t index_current) {
  if (l2c_current_pages_table[l2c_cpu_id][index_current].u_vector) { // Valid entry
    uint64_t record_index = l2c_ip_table[l2c_cpu_id][l2c_current_pages_table[l2c_cpu_id][index_current].ip & L2C_IP_TABLE_INDEX_MASK];
    assert(record_index < L2C_RECORD_PAGES_TABLE_ENTRIES);
    uint64_t confidence;
    l2c_add_record_pages_table(record_index,
			       l2c_current_pages_table[l2c_cpu_id][index_current].page_addr,
			       l2c_current_pages_table[l2c_cpu_id][index_current].u_vector,
			       l2c_current_pages_table[l2c_cpu_id][index_current].first_offset,
			       l2c_get_berti_current_pages_table(index_current, confidence));
  }
}

}
#endif