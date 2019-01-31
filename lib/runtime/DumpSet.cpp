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
#include <map>

#ifdef __cplusplus
extern "C" {
#endif

typedef std::set<unsigned int> UIntSet;

typedef struct info_def {
  UIntSet set;
  unsigned int min;
  unsigned int max;
} Info;

typedef std::map<int, Info> Map;

static Map m;

void DumpSet_Init(int refid) {
  //UIntSet* NewSet = new UIntSet;
//	printf("Get_New_Set: %p\n", NewSet);
  m[refid].set = UIntSet();
  m[refid].min = -1;
  m[refid].max = 0;
}

void DumpSet_Insert_Value(int refid, unsigned int addr) {
	UIntSet &local = m[refid].set;
//	printf("Perfect_Insert_Value: %p, %p\n", local, Set);
	local.insert(addr);
	
	if (m[refid].min > addr)
	  m[refid].min = addr;
	if (m[refid].max < addr)
	  m[refid].max = addr;
}

void DumpSet_MembershipCheck(unsigned int addr, int refid, int res) {
  UIntSet &local = m[refid].set;
  char fname[1024];
  sprintf(fname,"dumpset.%d.log",refid);
  int r =  (local.find(addr) != local.end());
  int range=0;
  if ( addr >= m[refid].min && addr <= m[refid].max )
    range = 1;
  if(r==res)
    return;
    
  FILE *f = fopen(fname,"a");
  if (f==NULL)
    return;
  
  fprintf(f,"%d %d (range:%d) [ ( %x ) ^ ( ",res,r,range,addr);

  UIntSet::iterator it,end=local.end();
  for(it=local.begin(); it!=end; it++)
      fprintf(f,"%x ",*it);
      
  fprintf(f,") ]\n");
  fclose(f);  
  //	printf("Perfect_MembershipCheck: %p, %p\n", local, Set); 
}

void DumpSet_Free(int refid) {
  m.erase(refid);
}

#ifdef __cplusplus
}
#endif
