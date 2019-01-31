/*
 * Contributed by:
 *   Rajesh Vanka (rvanka@ncsu.edu)
 *   James Tuck (jtuck@ncsu.edu)
 *
 * This is the main instrumentation point for the DDP instrumentation library. The main
 * entry point is Prof_Init.  It triggers DDPExit to run atexit.  profile_module_finish
 * takes care of dumping the final results to either a file or an SQLite database.  
 * 
 * The other files in the same directory provide interfaces for particular kinds of 
 * dependence profiling.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include "fcntl.h"
#include <sys/stat.h>
#include <set>
#include <iostream>
#include <fstream>
#include <vector>

#ifdef __cplusplus
extern "C" {
#endif

// External API
// Todo: clean up naming conventions
void Prof_Init(unsigned int tableSize, unsigned int is_edge_prof);
void Update_Counters(unsigned int val, unsigned int offset);
void Clear_Counters();
static void DDPExit();
void update_sqlite_database(const char *path, const char *dbName, 
			    const char *tableName,
			    const char *fileName,
			    int fileid,
			    struct profiler_common *array,
			    int size);


// Depreceated: don't rely on these, soon to be deleted
//xxxx
#define COUNTER_SIZE 1000000
#define EDGE_COUNTER_SIZE 100000
#define MAX_SNAPSHOTS 10
unsigned long long Counters[COUNTER_SIZE]; 
unsigned EdgeCounters[EDGE_COUNTER_SIZE];
  //extern int HashTableSize;
unsigned int edge_prof = 0;
//xxxxx

void HT_Alloc_Table(int size);

extern char *__LLVM_ProfilingToolname;


  /**
     This is the main instrumentation point for the DDP instrumentation 
     library. The main entry point is Prof_Init.  It triggers DDPExit 
     to run atexit.
   */
void Prof_Init(unsigned int tableSize, unsigned int is_edge_prof) {
  edge_prof = is_edge_prof;  
  Clear_Counters();
  HT_Alloc_Table(tableSize);
  //HashTableSize = tableSize;	  
  //atexit(DDPExit);
}

#if 0
static void DDPExit() {
  if(edge_prof) {    
    printf("Ld,Count\n");
    for(unsigned int i = 0; i < EDGE_COUNTER_SIZE; i++) {
      if(EdgeCounters[i]) {
	printf("%u,%u\n", i, EdgeCounters[i]);
      }
    }    
    printf("Edge Profiling Output\n");
    FILE* pFile = fopen ( "llvmprof.out" , "wb" );    
    unsigned int EdgeCtrSize = EDGE_COUNTER_SIZE;
    //enum ProfilingType pt = BlockInfo;
    unsigned int pt = 0;// EdgeInfo;
    printf("pt:%u, EdgeCtrSize:%u\n", pt, EdgeCtrSize);
    fwrite(&pt, sizeof(unsigned), 1, pFile);
    fwrite(&EdgeCtrSize, sizeof(unsigned), 1, pFile);
    fwrite(EdgeCounters,sizeof(unsigned), EdgeCtrSize, pFile);    
    fclose(pFile);    
    return;
  }
  //else {
  //	Dump_Prof_Data("Prof.out", 0, 0);
  //}
  
  //Dump_SnapShots();
  //FILE *PF = fopen("Prof.out", "wb");
  //if(PF != NULL) {
  //	fwrite(Counters, sizeof(unsigned int), COUNTER_SIZE, PF);
  //	fclose(PF);
  //}
  //else {
  //	printf("Can't open Prof.out\n");
  //}
}
#endif
 
struct profiler_common {
  int refid;
  int *gv;
  int total;
  int *tot;
  int *extra;
};

void profiler_update_file(const char* path,
			  const char *filename, 
			  const char *tableName,
			  const char *fileName,
			  struct profiler_common *array,
			  int size)
{
  char name[1024];
  if (strlen(path)>0)
    sprintf(name,"%s/%s",path,filename);
  else
    sprintf(name,"%s",filename);
  //fprintf(stderr,"Writing profile to %s\n",name);
  FILE *out = fopen(name,"a");
  if (out==NULL)
    {
      fprintf(stderr,"Couldn't open file %s. Exiting without saving data.\n",name);
      return;
    }
  for(int i=0; i<size; i++)
    {
      fprintf(out,"%d,%u,%d\n",array[i].refid, *array[i].gv, array[i].total);
    }
  fclose(out);
}
  
void profiler_update_db(const char* path, 
			const char* dbName, 
			const char* tableName,
			const char* fileName, int fileid,
			struct profiler_common *array, int size) 
{
  update_sqlite_database(path, dbName, tableName, fileName, fileid, array, size);    
  /*
    // May be helpful for debugging
    printf("Database: %s\n",db);
    for (int i=0; i<size; i++)
    printf("%d:%p = [%d]\n",array[i].refid, array[i].gv, *(array[i].gv));
  */
}


void Update_Counters(unsigned int val, unsigned int offset) {
  //printf("Update_Counters: %u, %u\n", val, offset);
  if(edge_prof) {
    assert(offset < EDGE_COUNTER_SIZE);
    EdgeCounters[offset] += val;
  }
  else {
    assert(offset < COUNTER_SIZE);
    Counters[offset] += val;
  }
}

void Clear_Counters() {
  memset(Counters, 0, COUNTER_SIZE*sizeof(unsigned long long));
}



unsigned int Compare_Values(unsigned int addr1, unsigned int addr2) {
  if(addr1 != addr2) {
    return 1;
  }
  else {
    return 0;
  }
}

unsigned int Count_Bits(unsigned int *addr, unsigned int numElements){
   unsigned int count=0;
   for(int i=0; i<numElements; i++){
#ifdef __GNUC__
      count += __builtin_popcount(addr[i]);
#else
      assert(0 && "__builtin_popcount is only implemented by GNU toolset");
#endif
   }
   return count;
}

#ifdef __cplusplus
}
#endif
