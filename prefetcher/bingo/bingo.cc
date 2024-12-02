#include "bingo.h"

void CACHE::prefetcher_initialize(){
    /*=== Bingo Settings ===*/
    const int REGION_SIZE = 2 * 1024;  /* size of spatial region = 2KB */
    const int PC_WIDTH = 16;           /* number of PC bits used in PHT */
    const int MIN_ADDR_WIDTH = 5;      /* number of Address bits used for PC+Offset matching */
    const int MAX_ADDR_WIDTH = 16;     /* number of Address bits used for PC+Address matching */
    const int FT_SIZE = 64;            /* size of filter table */
    const int AT_SIZE = 128;           /* size of accumulation table */
    const int PHT_SIZE = SIZE_OF_PHT; //1024 * 8;     /* size of pattern history table (PHT) */	//EDIT BY Neelu, 6 instead of 8.
    const int PHT_WAYS = WAYS_IN_PHT; //16;           /* associativity of PHT */ 	Neelu: Assoc. 12 for 6*1024 entries.
    const int PF_STREAMER_SIZE = 128;  /* size of prefetch streamer */
    /*======================*/

    /* number of PHT sets must be a power of 2 */
    // assert(__builtin_popcount(PHT_SIZE / PHT_WAYS) == 1);

    /* construct prefetcher for all cores */
    // assert(PAGE_SIZE % REGION_SIZE == 0);
    BINGO::prefetchers = vector<BINGO::Bingo>(
        NUM_CPUS, BINGO::Bingo(REGION_SIZE >> LOG2_BLOCK_SIZE, MIN_ADDR_WIDTH, MAX_ADDR_WIDTH, PC_WIDTH, FT_SIZE,
                      AT_SIZE, PHT_SIZE, PHT_WAYS, PF_STREAMER_SIZE, BINGO::DEBUG_LEVEL));

	cout <<"Bingo PHT WAYS: " << PHT_WAYS << " and PHT_SIZE: " << PHT_SIZE << endl; 

}
void CACHE::prefetcher_cycle_operate() {}
uint32_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, bool useful_prefetch, uint8_t type, uint32_t metadata_in){
    if(!this->warmup && warmup_flag_l2 == 0)
    {
        BINGO::prefetchers[cpu].reset_stats();
        warmup_flag_l2 = 1;

    }
    if(useful_prefetch && cache_hit){
        if(is_cxl_memory(addr)){
            num_prefetch_hit_cxl++;
        }else{
            num_prefetch_hit_ddr++;
        }
    }

    if (BINGO::DEBUG_LEVEL >= 2) {
        cerr << "CACHE::BINGOetcher_operate(addr=0x" << hex << addr << ", ip=0x" << ip << ", cache_hit=" << dec
             << (int)cache_hit << ", type=" << (int)type << ")" << dec << endl;
        cerr << "[CACHE::BINGOetcher_operate] CACHE{core=" << this->cpu << ", NAME=" << this->NAME << "}" << dec
             << endl;
    }

    if (type != 0) // [PHW] LOAD = 0 in channel.h
        return metadata_in;

    uint64_t block_number = addr >> LOG2_BLOCK_SIZE;

    /* update BINGO with most recent LOAD access */
    BINGO::prefetchers[cpu].access(block_number, ip);

    /* issue prefetches */
    BINGO::prefetchers[cpu].prefetch(this, block_number);

    if (BINGO::DEBUG_LEVEL >= 3) {
        BINGO::prefetchers[cpu].log();
        cerr << "=======================================" << dec << endl;
    }

    return metadata_in;

}
uint32_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t match, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in){
    uint64_t evicted_block_number = evicted_addr >> LOG2_BLOCK_SIZE;

    // [PHW]
    // auto begin = std::next(std::begin(this->block), set, )
    // auto target = get_span(std::begin(this->block), set, match);
    auto set_span = this->get_set_span(set);
    auto target = std::next(set_span.first, match);


    // if (this->block[set][match].valid == 0)
    if(target->valid == 0)
        return metadata_in; /* no eviction */

    /* inform all sms modules of the eviction */
    for (int i = 0; i < int(NUM_CPUS); i += 1)
        BINGO::prefetchers[i].eviction(evicted_block_number);

    return metadata_in;
}
void CACHE::prefetcher_final_stats() {
	 BINGO::prefetchers[cpu].print_stats();
     std::cout << "num_prefetch_ddr: " << std::dec << num_prefetch_ddr << std::endl;
     std::cout << "num_prefetch_hit_ddr: " << std::dec << num_prefetch_hit_ddr << std::endl;
     std::cout << "num_prefetch_cxl: " << std::dec << num_prefetch_cxl << std::endl;
     std::cout << "num_prefetch_hit_cxl: " << std::dec << num_prefetch_hit_cxl << std::endl;
}