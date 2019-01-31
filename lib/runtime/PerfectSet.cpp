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

	
// I do not think that we should be using standard C++ set library because
// these functions call set library functions which might be efficient but
// may be for our purposes since set library organizes the red-black trees
// based on pointer values here, which might not be the best thing to do. 
// Moreover, calling set library functions introduces additional overhead from
// making function calls even in the hot functions below. Probably we should 
// implement out own set class (copy std::set code) so compiler can easily
// inline functions.
	
typedef std::set<uint64_t> UIntSet;

int* Get_New_Set() {
	UIntSet* NewSet = new UIntSet;
//	printf("Get_New_Set: %p\n", NewSet);
	return (int*)NewSet;
}

void PerfectSet_Insert_Value(void *Set, void *addr) {
	UIntSet* local = (UIntSet*)Set;
//	printf("Perfect_Insert_Value: %p, %p\n", local, Set);
	local->insert((uint64_t)addr);
}

unsigned int PerfectSet_MembershipCheck(void *addr, void *Set) {
	UIntSet* local = (UIntSet*)Set;
//	printf("Perfect_MembershipCheck: %p, %p\n", local, Set);
	return (local->find((uint64_t)addr) != local->end());
}

unsigned int PerfectSet_Population(void * Set) {
	UIntSet* local = (UIntSet*)Set;
	return (local->size());
}

void Free_Set(void *Set) {
	UIntSet* local = (UIntSet*)Set;
	local->clear();
//	printf("FreeSet: %p, %p\n", local, Set);
	delete local;
}

#ifdef __cplusplus
}
#endif
