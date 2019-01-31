#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

typedef int* Signature;

#define DeclareFns(Type,Name)  \
  Type Name##AllocateFn();	     \
  void Name##InsertFn(Type,int8_t*); \
  int Name##MembershipFn(Type,int8_t*);



DeclareFns(Signature,SimpleSignature32)
DeclareFns(Signature,SimpleSignature64)
DeclareFns(Signature,SimpleSignature128)
DeclareFns(Signature,SimpleSignature256)
DeclareFns(Signature,SimpleSignature1024)
DeclareFns(Signature,ArraySignature_32_32)
DeclareFns(Signature,ArraySignature_32_128)
DeclareFns(Signature,DDPPerfectSet)


#define INSERT 250
#define CHECK 100000
#define RANGE 100000

int main()
{
  int8_t array[RANGE]={0};

  Signature array32 = ArraySignature_32_32AllocateFn();
  Signature perfect = DDPPerfectSetAllocateFn();

  srand(time(NULL));

  for(int i=0; i<INSERT; i++) {
    int j = rand()%RANGE;
    ArraySignature_32_32InsertFn(array32,array+j);
    DDPPerfectSetInsertFn(perfect,array+j);
    array[j] = 1;

    printf("Insert @%d\n",j);
  }

  int true_conflict=0;
  int false_conflict=0;
  int error=0;

  for(int i=0; i<CHECK; i++) {
    int j = rand()%RANGE;

    int m1 = ArraySignature_32_32MembershipFn(array32,array+j);
    int m2 = DDPPerfectSetMembershipFn(perfect,array+j);

    printf("Check @%5d: ",j);    

    if (!array[j]) {
      printf(" [Not In Set] ");
    } else {
      printf("   [In Set]   ");

    }

    if ((m1 && m2) || (!m1 && !m2)) {
      printf("  Match  ");
    } else { 
      printf(" NoMatch ");
    }

    printf("[ P(%c) A(%c) ]", m2?'T':'F', m1?'T':'F');

    
    if((m1 && m2) && array[j]) {
      true_conflict++;
      //printf("(True (2)) ");    
    } else
      if((m1 || m2) && array[j]) {
	true_conflict++;
	//printf("(True (1)) ");    
      }
    if((m1 && m2) && !array[j]) {
      false_conflict++;
      //printf("(False (2)) ");    
    } else
      if((m1 || m2) && !array[j]) {
	false_conflict++;
	//printf("(False (1)) ");    
      }
    

    if((!m1 || !m2) && array[j]) {
      error++;
      printf(" (Error!)\n");    
    } else {
      printf(" \n");    
    }
  }

  printf("Total Conflicts=%d\n",true_conflict+false_conflict);
  printf("True  Conflicts=%d\n",true_conflict);
  printf("False Positives=%d (%2.1lf%%)\n",false_conflict, (false_conflict)/((double)CHECK)*100);
  if(error)
    printf("Warning: %d errors found!",error);

  return 0;
}
