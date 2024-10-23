#ifndef CAP_H
#define CAP_H

#include <vector>
#include <utility>
#include <cstdint>
#include "cache.h"
#include "msl/bits.h"
#include "champsim_constants.h"
#include <iostream>
namespace cap{

#define SIZE_OF_MAP_TABLE 256
#define SIZE_OF_HISTORY_TABLE 8192
#define SIZE_OF_MG_TABLE 1024
#define DEGREE_DEFAULT 4 
#define DEGREE_MAX 16

typedef struct{
  uint64_t pfn;
  uint64_t hit_cnt;
  uint64_t degree;
  uint64_t num_pref;
} HistoryEntry;

typedef struct{
  uint64_t pfn;
  uint64_t hit_cnt;
  uint64_t num_pref;
} MisraGriesEntry;

bool is_cxl_memory(uint64_t addr){
  if( addr > DRAM_SIZE){
    return true;
  }else{
    return false;
  }
}

// define history table 
class HistoryTable{
  public:
  HistoryTable(){}
  void init(size_t B_size){
    size = B_size;
    plru_tree.resize(size - 1, false);
    table.resize(size, {0,0,0,0});
    empty_entries = size;
  }
  void print(){
    for(size_t i=0; i < table.size(); i++){
      std::cout << "idx: " << i << " pfn: 0x" << std::hex << table[i].pfn << std::dec << " hit_cnt: " << table[i].hit_cnt << " num_pref: " << table[i].num_pref << " degree: " << table[i].degree << std::endl;
    }
  }
  HistoryEntry* get_history(uint64_t addr){
    uint64_t pfn = addr >> LOG2_PAGE_SIZE;
    for(auto& entry: table){
      if(entry.pfn == pfn){
        return &entry;
      }
    }
    return nullptr;
  }
  void update(MisraGriesEntry mg_entry){
    // find the entry in the table
    // std::cout << "arg pfn: 0x" << std::hex << mg_entry.pfn << std::dec << " hit_cnt: " << mg_entry.hit_cnt << " num_pref: " << mg_entry.num_pref << std::endl;
    for(size_t i=0; i < table.size(); i++){
      if(table[i].pfn == mg_entry.pfn){
        // TODO:update existing entry
        table[i].hit_cnt += mg_entry.hit_cnt;
        table[i].num_pref += mg_entry.num_pref;
        // TODO: need new degree
        float hit_rate = (float)table[i].hit_cnt / (float)table[i].num_pref;
        if(hit_rate < 0.2){
            table[i].degree = 1;
        }else if(hit_rate >= 0.2 && hit_rate < 0.4){
            table[i].degree = 4;
        }else if(hit_rate >= 0.4 && hit_rate < 0.6){
            table[i].degree = 8;
        }else if(hit_rate >= 0.6 && hit_rate < 0.8){
            table[i].degree = 12;
        }else{
            table[i].degree = DEGREE_MAX;
        }
        // std::cout << "update existing entry idx: " << i << " pfn: 0x" << std::hex << table[i].pfn << std::dec << " hit_cnt: " << table[i].hit_cnt << " num_pref: " << table[i].num_pref << std::endl;
        update_plru(i);
        return;
      }
    }

    // find empty entry in the table
    // no need to find empty one, just follow the plru
    // for(size_t i=0; i < table.size(); i++){
    //   if(table[i].pfn == 0){
    //     table[i].pfn = mg_entry.pfn;
    //     table[i].hit_cnt = mg_entry.hit_cnt;
    //     table[i].degree = DEGREE_DEFAULT;
    //     table[i].num_pref = mg_entry.num_pref;
    //     std::cout << "update new entry idx: " << i << " pfn: 0x" << std::hex << table[i].pfn << std::dec << " hit_cnt: " << table[i].hit_cnt << std::endl;
    //     update_plru(i);
    //     return;
    //   }
    // }

    // if table is full or not found empty entry, evict an entry
    size_t evicted_idx = evict_lru_leaf();
    if(table[evicted_idx].pfn == 0){
      // std::cout << "[" << std::dec << empty_entries << "] new_idx: ";
      empty_entries--;
    }else{
      // std::cout << "evicted_idx: ";
    }
    table[evicted_idx].pfn = mg_entry.pfn;
    table[evicted_idx].hit_cnt = mg_entry.hit_cnt;
    table[evicted_idx].degree = DEGREE_DEFAULT;
    table[evicted_idx].num_pref = mg_entry.num_pref;
    update_plru(evicted_idx);
    // std::cout << evicted_idx << " pfn: 0x" << std::hex << table[evicted_idx].pfn << std::dec << " hit_cnt: " << table[evicted_idx].hit_cnt << std::endl;
  }

  private:
  size_t size;
  size_t empty_entries;
  // create vector that has uint64_t, uint32_t, uint8_t for each entry
  std::vector<HistoryEntry> table;
  std::vector<bool> plru_tree;

  size_t evict_lru_leaf(){
    size_t leaf_idx = 0;
    size_t node = 0;
    while(node < plru_tree.size()){
      if (!plru_tree[node]){
        node = node * 2 + 1;
      }else{
        node = node * 2 + 2;
      }
    }
    leaf_idx = node - plru_tree.size();
    reset_entry(leaf_idx);
    return leaf_idx;
  }
  void update_plru(size_t leaf_idx){
    size_t node = leaf_idx + plru_tree.size();
    while(node > 0){
      size_t parent = (node - 1) / 2;
      if(node == parent*2 + 1){
        plru_tree[parent] = true;
      }else{
        plru_tree[parent] = false;
      }
      node = parent;
    }
  }

  void reset_entry(size_t leaf_idx){
    table[leaf_idx] = {0,0,0,0};
  }
};

// define misra-gries table per core
class MisraGriesTable{
  public:
  MisraGriesTable(){}
  void init(size_t b_size, size_t b_hit_thd){
    // initialize table
    size = b_size;
    hit_thd = b_hit_thd;
    table.resize(size, {0,0,0});
    available_entries = size;
  }

  // increase num_pref if entry is found
  void update_num_pref(uint64_t addr){
    uint64_t pfn = addr >> LOG2_PAGE_SIZE;
    for (auto& entry : table) {
      if (entry.pfn == pfn) {
        if(entry.num_pref < DEGREE_MAX){
          entry.num_pref += 1;
        }
        return;
      }
    }
    // don't mind if not found
  }

  // add a entry update function and it return updated entry
  bool update_hit(uint64_t addr, MisraGriesEntry *ret){
    uint64_t pfn = addr >> LOG2_PAGE_SIZE;
    // uint64_t offset = addr & (PAGE_SIZE - 1);
    for (auto& entry : table) {
      if (entry.pfn == pfn) {
        entry.hit_cnt++;
        // std::cout << "idx: " << &entry - &table[0] << " pfn: 0x" << std::hex << entry.pfn << std::dec << " hit_cnt: " << entry.hit_cnt << std::endl;
        if(entry.hit_cnt >= hit_thd){
          // should update history table
          ret->hit_cnt = entry.hit_cnt;
          ret->num_pref = entry.num_pref;
          ret->pfn = entry.pfn;
          // std::cout << std::hex << "pfn: 0x" << ret->pfn << std::dec << " hit_cnt: " << ret->hit_cnt << " num_pref: " << ret->num_pref << std::endl;
          clear_entry(entry);
          available_entries++;
          return true;
        }
      }
    }
    // If not found, insert a new entry
    if(available_entries > 0){
      for(auto& entry: table){
        if(entry.pfn == 0){
          entry.pfn = pfn;
          entry.hit_cnt = 1;
          entry.num_pref = hit_thd;
          available_entries--;
          break;
        }
      }
    }else{
      // If table is full, apply cooldown
      cooldown();
    }
    return false;
  }

  private:
  std::vector<MisraGriesEntry> table;
  size_t size;
  size_t available_entries;
  size_t hit_thd;

  void cooldown(){
    for(size_t i = 0;i<table.size();i++){
      if(table[i].pfn != 0 && table[i].hit_cnt > 0){
        table[i].hit_cnt--;
        if(table[i].num_pref > 1){
          table[i].num_pref--;
        }
        if(table[i].hit_cnt == 0){ // erase from misra-gries table
          table[i].pfn = 0;
          table[i].num_pref = 0;
          available_entries++;
        }
      }
    }
    // std::cout << "[cooldown] available: " << std::dec << available_entries << std::endl;
  }
  void clear_entry(MisraGriesEntry& entry){
    entry = {0,0,0};
  }
};

class CxlAwarePrefetcher{
  public:
  CxlAwarePrefetcher(){}
  void init(float b_weight){
    weight = b_weight;
  }
  // TODO: calculate prefetch degree
  /*   0 : do not prefetch
   *   n : prefetch degree
   **/
  int get_degree(HistoryEntry* hist_info){  // hist_info can't be nullptr
    int degree = 0;
    // 1. calculate prefetch benefit
    // hist_info.
    // 2. if benefit is enough, prefetch
    // 3. calculate prefetch degree
    return degree;
  }

  private:
  float weight;
};
}
#endif