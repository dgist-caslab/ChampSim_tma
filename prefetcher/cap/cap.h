#ifndef CAP_H
#define CAP_H

#include <vector>
#include <utility>
#include <cstdint>
#include "cache.h"
#include "msl/bits.h"
#include "champsim_constants.h"
#include <iostream>

#include <cmath>
#include <iomanip>

namespace cap{

#define SIZE_OF_MAP_TABLE 256
#define SIZE_OF_HISTORY_TABLE 8192
#define SIZE_OF_MG_TABLE 1024
#define DEGREE_DEFAULT 4 
#define DEGREE_MAX 16

#define PAGE_FRAME_MASK 0xFFFFFFFFFFFFF000
#define PAGE_OFFSET_MASK 0x0000000000000FFF

typedef struct{
  uint64_t pfn;
  uint64_t hit_cnt;
  int degree;
  uint64_t num_pref;
  std::bitset<64> access_bitmap;
} HistoryEntry;

typedef struct{
  uint64_t pfn;
  uint64_t hit_cnt;
  uint64_t lives;
  uint64_t num_pref;
  std::bitset<64> access_bitmap;
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
    table.resize(size, {0,0,0,0,0});
  }

  void print(){
    for(size_t i=0; i < table.size(); i++){
      std::cout << "idx: " << i << " pfn: 0x" << std::hex << table[i].pfn << std::dec << " hit_cnt: " << table[i].hit_cnt << " num_pref: " << table[i].num_pref << " degree: " << table[i].degree << std::endl;
    }
  }

  HistoryEntry* get_history(uint64_t addr){
    uint64_t pfn = addr >> LOG2_PAGE_SIZE;
    for(size_t i=0; i < table.size(); i++){
      if(table[i].pfn == pfn){
        update_plru(i);
        return &table[i];
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
        table[i].access_bitmap |= mg_entry.access_bitmap;

        // if(table[i].degree <= 0){
        //   reset_entry(i);
        // }
        update_plru(i);
        return;
      }
    }

    // if table is full or not found empty entry, evict an entry
    size_t evicted_idx = evict_lru_leaf();
    table[evicted_idx].pfn = mg_entry.pfn;
    table[evicted_idx].hit_cnt = mg_entry.hit_cnt;
    table[evicted_idx].degree = DEGREE_DEFAULT;
    table[evicted_idx].num_pref = mg_entry.num_pref;
    update_plru(evicted_idx);
    // std::cout << evicted_idx << " pfn: 0x" << std::hex << table[evicted_idx].pfn << std::dec << " hit_cnt: " << table[evicted_idx].hit_cnt << std::endl;
  }

  private:
  size_t size;
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
    table[leaf_idx] = {0,0,0,0,0};
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
    table.resize(size, {0,0,0,0,0});
    available_entries = size;
  }

  // set access bit 
  void set_access_bit(uint64_t addr){
    uint64_t pfn = addr >> LOG2_PAGE_SIZE;
    uint64_t offset = addr & PAGE_OFFSET_MASK;
    for (auto& entry : table) {
      if (entry.pfn == pfn) {
        entry.access_bitmap.set(offset >> LOG2_BLOCK_SIZE);
        return;
      }
    }
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
        entry.lives++;
        // std::cout << "idx: " << &entry - &table[0] << " pfn: 0x" << std::hex << entry.pfn << std::dec << " hit_cnt: " << entry.hit_cnt << std::endl;
        if(entry.hit_cnt >= hit_thd){
          // should update history table
          ret->hit_cnt = entry.hit_cnt;
          ret->lives = 0;
          ret->num_pref = entry.num_pref;
          ret->pfn = entry.pfn;
          ret->access_bitmap = entry.access_bitmap;
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
          entry.lives = 1;
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

  void clear_entry(MisraGriesEntry& entry){
    entry.pfn = 0;
    entry.hit_cnt = 0;
    entry.lives = 0;
    entry.num_pref = 0;
    entry.access_bitmap.reset();
  }
  void cooldown(){
    for(size_t i = 0;i<table.size();i++){
      if(table[i].pfn != 0 && table[i].hit_cnt > 0){
        table[i].lives--;
        // if(table[i].num_pref > 1){
        //   table[i].num_pref--;
        // }
        if(table[i].lives == 0){ // erase from misra-gries table
          clear_entry(table[i]);
          available_entries++;
        }
      }
    }
    // std::cout << "[cooldown] available: " << std::dec << available_entries << std::endl;
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
  size_t get_degree(HistoryEntry* hist_info){  // hist_info can't be nullptr
    // TODO: calculate sparseness of the page by using access_bitmp
    // int sparse_lv = hist_info->access_bitmap.count();
    double fractal_entropy = calculateFractalEntropy(hist_info->access_bitmap);
    // std::cout << "fractal entropy: " << std::fixed << std::setprecision(3) << fractal_entropy << std::endl;
    float hit_rate = (float)hist_info->hit_cnt / (float)hist_info->num_pref;
    int degree_change = 0;

    if (hit_rate <= 0.2) {
      degree_change = -1;
    } else if (hit_rate <= 0.4) {
      degree_change = 0;
    } else if (hit_rate <= 0.6) {
      degree_change = 1;
    } else if (hit_rate <= 0.8) {
      degree_change = 2;
    } else {
      degree_change = 4;
    }

    hist_info->degree = std::max(0, std::min(DEGREE_MAX, (int)hist_info->degree + degree_change));

    // std::cout << "bitmap: " << hist_info->access_bitmap << std::endl;
    // std::cout << "pfn: 0x" << std::hex << hist_info->pfn << std::dec << "\tnum_hit: " << hist_info->hit_cnt << "\thit_rate: " << std::fixed << std::setprecision(3) << hit_rate << "\tfractal_entropy: " << std::fixed << std::setprecision(3) << fractal_entropy << std::endl;

    // std::cout << "pfn: 0x" << std::hex << hist_info->pfn << "\tchange: " << std::dec << degree_change << "\tdegree: " << hist_info->degree << std::endl;
    // if((int)hist_info->degree > sparse_lv && hist_info->hit_cnt > DEGREE_MAX){
    //   hist_info->degree = sparse_lv;
    // }
    // std::cout << "degree: " << hist_info->degree << std::endl;
    // 1. calculate prefetch benefit
    // hist_info.
    // 2. if benefit is enough, prefetch
    // 3. calculate prefetch degree
    return hist_info->degree;
  }

  private:
  float weight;
  // fractal entropy by using box counting method
  int countBoxes(const std::bitset<64>& bitmap, int boxSize){
    int cnt=0;
    int numBoxes = bitmap.size() / boxSize;
    for(int i = 0;i<numBoxes;i++){
      bool foundOne = false;
      for(int j=0;j<boxSize;++j){
        if(bitmap[i*boxSize + j] == 1){
          foundOne = true;
          break;
        }
      }
      if(foundOne){
        cnt++;
      }
    }
    return cnt;
  }

  double calculateFractalEntropy(const std::bitset<64>& bitmap){
    double slope = 0;
    std::vector<int> boxSizes = {1,2,4,8,16,32,64};

    std::vector<double> logBoxSizes;
    std::vector<double> logCounts;

    for(int boxSize : boxSizes){
      int count = countBoxes(bitmap, boxSize);
      // print count
      // std::cout << "boxSize: " << boxSize << " count: " << count << std::endl;
      if( count > 0){
        logBoxSizes.push_back(log(1.0/boxSize));
        logCounts.push_back(log(count));
      }
    }
    // print all logBoxSizes and logCounts
    // for(int i=0;i<logBoxSizes.size();i++){
    //   std::cout << "logBoxSizes: " << logBoxSizes[i] << " logCounts: " << logCounts[i] << std::endl;
    // }

    // get slope
    double sumX = 0, sumY = 0, sumXY = 0, sumXX = 0;
    int n = logBoxSizes.size();
    for (int i = 0; i < n; ++i) {
        sumX += logBoxSizes[i];
        sumY += logCounts[i];
        sumXY += logBoxSizes[i] * logCounts[i];
        sumXX += logBoxSizes[i] * logBoxSizes[i];
    }

    slope = (n * sumXY - sumX * sumY) / (n * sumXX - sumX * sumX);

    return slope;
  }
};
}
#endif