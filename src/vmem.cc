/*
 *    Copyright 2023 The ChampSim Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "vmem.h"

#include <cassert>

#include "champsim.h"
#include "champsim_constants.h"
#include "dram_controller.h"
#include <fmt/core.h>

#include <iostream>

VirtualMemory::VirtualMemory(uint64_t page_table_page_size, std::size_t page_table_levels, uint64_t minor_penalty, MEMORY_CONTROLLER& dram, MEMORY_CONTROLLER& dram_slow)
    : next_ppage_fast(VMEM_RESERVE_CAPACITY), last_ppage(1ull << (LOG2_PAGE_SIZE + champsim::lg2(page_table_page_size / PTE_BYTES) * page_table_levels)),
      minor_fault_penalty(minor_penalty), pt_levels(page_table_levels), pte_page_size(page_table_page_size)
{
  assert(page_table_page_size > 1024);
  assert(page_table_page_size == (1ull << champsim::lg2(page_table_page_size)));
  assert(last_ppage > VMEM_RESERVE_CAPACITY);

  auto required_bits = champsim::lg2(last_ppage);

  next_ppage_slow = DRAM_SIZE;
  
  if (required_bits > 64)
    fmt::print("WARNING: virtual memory configuration would require {} bits of addressing.\n", required_bits); // LCOV_EXCL_LINE
  if (required_bits > champsim::lg2(dram.size() + dram_slow.size()))
    fmt::print("WARNING: physical memory size is smaller than virtual memory size. req:{}, F+S:{}\n", required_bits, champsim::lg2(dram.size() + dram_slow.size())); // LCOV_EXCL_LINE

}

uint64_t VirtualMemory::shamt(std::size_t level) const { return LOG2_PAGE_SIZE + champsim::lg2(pte_page_size / PTE_BYTES) * (level - 1); }

uint64_t VirtualMemory::get_offset(uint64_t vaddr, std::size_t level) const
{
  return (vaddr >> shamt(level)) & champsim::bitmask(champsim::lg2(pte_page_size / PTE_BYTES));
}

uint64_t VirtualMemory::ppage_front(bool is_slow) const
{
  if(is_slow){
    assert(available_ppages_slow() > 0);
    return next_ppage_slow;
  }else{
    if(available_ppages_fast() > 0)
      return next_ppage_fast;
    else
      return next_ppage_slow;
  }
}

void VirtualMemory::ppage_pop(uint64_t paddr) { 
  // static constexpr uint64_t HALF_GB_IN_PAGES = 536870912;
  if(paddr < DRAM_SIZE){
    next_ppage_fast += PAGE_SIZE;
  }else{
    next_ppage_slow += PAGE_SIZE;
  }
  // fmt::print("[VMEM] {} paddr: {:x} next_ppage_fast: {:x} next_ppage_slow: {:x}\n", __func__, paddr, next_ppage_fast, next_ppage_slow);
  // if constexpr (champsim::debug_print) {
  //   std::cout << " next_ppage: " << std::hex << next_ppage << std::endl;
  //   if(next_ppage > DRAM_SIZE && flag_slow_alloc == false){
  //     flag_slow_alloc = true;
  //     fmt::print("[DEBUG] next_ppage: {:#x}, DRAM_SIZE: {:#x}\n", next_ppage, DRAM_SIZE);
  //   }
  //   if (next_ppage % HALF_GB_IN_PAGES == 0) {
  //     fmt::print("[DEBUG] Allocated memory size: {} GB\n", (next_ppage) / (1ULL << 30));
  //   }
  // }
}

std::size_t VirtualMemory::available_ppages_fast() const {
  return (DRAM_SIZE - next_ppage_fast) / PAGE_SIZE;
}
std::size_t VirtualMemory::available_ppages_slow() const {
  return (last_ppage - next_ppage_slow) / PAGE_SIZE;
}
uint64_t VirtualMemory::get_last_ppage_fast() { return next_ppage_fast; }
uint64_t VirtualMemory::get_last_ppage_slow() { return next_ppage_slow; }

std::pair<uint64_t, uint64_t> VirtualMemory::va_to_pa(uint32_t cpu_num, uint64_t vaddr)
{
  bool is_slow = true;
  if(tma_idx % tma_thd == 0){
    is_slow = false;
  }
  auto [ppage, fault] = vpage_to_ppage_map.insert({{cpu_num, vaddr >> LOG2_PAGE_SIZE}, ppage_front(is_slow)});

  // this vpage doesn't yet have a ppage mapping
  // so alloc new pa to va
  if (fault){
    ppage_pop(ppage->second);
    if(tma_idx % tma_thd == 0){
      tma_idx = 0;
    }
    tma_idx++;
  }
  // fmt::print("[VMEM] {} paddr: {:x} vaddr: {:x} fault: {} tma_idx: {}\n", __func__, ppage->second, vaddr, fault, tma_idx);

  auto paddr = champsim::splice_bits(ppage->second, vaddr, LOG2_PAGE_SIZE);
  if constexpr (champsim::debug_print) {
    fmt::print("[VMEM] {} paddr: {:x} vaddr: {:x} fault: {}\n", __func__, paddr, vaddr, fault);
  }

  return {paddr, fault ? minor_fault_penalty : 0};
}

std::pair<uint64_t, uint64_t> VirtualMemory::get_pte_pa(uint32_t cpu_num, uint64_t vaddr, std::size_t level)
{
  if (next_pte_page == 0) {
    next_pte_page = ppage_front(false);
    ppage_pop(next_pte_page);
  }

  std::tuple key{cpu_num, vaddr >> shamt(level), level};
  auto [ppage, fault] = page_table.insert({key, next_pte_page});

  // this PTE doesn't yet have a mapping
  if (fault) {
    next_pte_page += pte_page_size;
    if (!(next_pte_page % PAGE_SIZE)) {
      next_pte_page = ppage_front(false);
      ppage_pop(next_pte_page);
    }
  }

  auto offset = get_offset(vaddr, level);
  auto paddr = champsim::splice_bits(ppage->second, offset * PTE_BYTES, champsim::lg2(pte_page_size));
  if constexpr (champsim::debug_print) {
    fmt::print("[VMEM] {} paddr: {:x} vaddr: {:x} pt_page_offset: {} translation_level: {} fault: {}\n", __func__, paddr, vaddr, offset, level, fault);
  }
  // fmt::print("[VMEM] {} paddr: {:x} vaddr: {:x} pt_page_offset: {} translation_level: {} fault: {}\n", __func__, paddr, vaddr, offset, level, fault);

  return {paddr, fault ? minor_fault_penalty : 0};
}
