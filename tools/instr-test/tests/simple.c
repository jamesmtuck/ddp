#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <string.h>

#include "sigdef.h"

#define INSERT 10
#define CHECK 1000000
#define RANGE (2*512*1024*1024)

void print(int *sign) {
  printf("\n");
  for (int i=0; i<LIMIT; ) {
    for(int j=0;j<8; j++,i++)
      printf("%8x",sign[i]);
    printf("\n");
  }
}


int main()
{
  int8_t *array = malloc(sizeof(int8_t)*RANGE);
  memset(array,RANGE,0);
  Signature s = Allocate();

  printf("Testing %s:\n",SigName);

  srand(time(NULL));

  for(int i=0; i<INSERT; i++) {
    int j = rand()%RANGE;
    Insert(s,array+j);
    array[j] = 1;

    printf("Insert @%d Signature:%x\n",j,*s);
  }

  print (s);

  int true_conflict=0;
  int false_conflict=0;
  int error=0;

  for(int i=0; i<CHECK; i++) {
    int j = rand()%RANGE;
    int m = Membership(s,array+j);
    
    if(m && array[j]) {
      true_conflict++;
      printf("(True) Check @%d \n",j); 
    }
    if(m && !array[j]) {
      false_conflict++;
      printf("(False) Check @%d \n",j);
    }
    if(!m && array[j])
      error++;

  }

  print (s);

  printf("Total Conflicts by signature=%d\n",true_conflict+false_conflict);
  printf("True  Conflicts by signature=%d\n",true_conflict);
  printf("False Positives by signature=%d (%2.4lf%%)\n",false_conflict, (false_conflict)/((double)CHECK)*100);
  if(error)
    printf("Warning: %d errors found!",error);

  return 0;
}
