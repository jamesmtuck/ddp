#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/stat.h>
#include <set>
#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

  uint64_t *globalHT_=0;

  void HT_Alloc_Table(uint64_t size) {
	   printf("globalHT_ is allocated!\n");
     globalHT_ = (uint64_t*) malloc(size);
  }

  uint64_t* HT_Get_Table() {
    if(!globalHT_)
	     printf("globalHT is null!\n");
    return globalHT_;
  }

  uint64_t KnuthHash(void *addr) {
    // For 8 byte aligned boundaries, this right shift should be 3
    // Here, we are assuming a 4 byte alignment.
	  printf("hashing\n");
    return ((uint64_t)addr >> 2) * 2654435761u;
  }

  void HT_Insert_Value(void *addr, uint64_t *table, uint64_t tableSize) {
    uint64_t hash = KnuthHash(addr);
    uint64_t index = hash % tableSize;
    uint64_t multiple = index/sizeof(uint64_t);
    // increment table by multiple
    table += multiple;
    // set bit at the offset
    uint64_t offset = index % sizeof(uint64_t);
    uint64_t val = 1 << offset;
    *table |= val;
  }

  uint64_t HT_Membership_Check(void *addr, uint64_t *table, uint64_t tableSize) {
    uint64_t hash = KnuthHash(addr);
    uint64_t index = hash % tableSize;
    uint64_t multiple = index/sizeof(uint64_t);
    // increment table by multiple
    table += multiple;
    // set bit at the offset
    uint64_t offset = index % sizeof(uint64_t);
    uint64_t val = 1 << offset;
    // Check if this bit is set...
    bool bit_set = *table & val;
    return bit_set;
  }

#ifdef __cplusplus
}
#endif
