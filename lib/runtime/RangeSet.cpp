#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/stat.h>
#include <set>
#include <iostream>
#include <fstream>
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

class RangeInfo {
public:
  RangeInfo():min(-1),max(0){}
  uint64_t min;
  uint64_t max;
};

void *RangeSet_New() {
  RangeInfo *ri = new RangeInfo();
  return ri;
}

void RangeSet_Insert_Value(void *Set, void *addr) {
  RangeInfo *ri = (RangeInfo *)Set;
  if (ri->min > (uint64_t)addr)
    ri->min = (uint64_t)addr;
  if (ri->max < (uint64_t)addr)
    ri->max = (uint64_t)addr;
}

int RangeSet_MembershipCheck(void *addr, void *Set) {
  RangeInfo *ri = (RangeInfo*)Set;
  int res = ((uint64_t)addr < ri->min) | ((uint64_t)addr > ri->max);
  return !(res);
}

void RangeSet_Free(void *set) {
  RangeInfo *ri = (RangeInfo*)set;
  delete ri;
}

#ifdef __cplusplus
}
#endif
