#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <string.h>

typedef int* Signature;

#define DeclareFns(Type,Name)  \
  const char *SigName = #Name; \
  Type Name##AllocateFn(); \
  void Name##InsertFn(Type,int8_t*); \
  int Name##MembershipFn(Type,int8_t*); \
  Type Allocate() { return Name##AllocateFn(); } \
  void Insert(Type t,int8_t* p) { Name##InsertFn(t,p); } \
  int Membership(Type t, int8_t*p) { return Name##MembershipFn(t,p); }


#define BankedSignature_3x1024
//#define ArraySignature_32_32

#if (defined SimpleSignature32)
DeclareFns(Signature,SimpleSignature32)
#define LIMIT (32/32)
#elif (defined SimpleSignature64)
DeclareFns(Signature,SimpleSignature64)
#define LIMIT (64/32)
#elif (defined SimpleSignature128)
DeclareFns(Signature,SimpleSignature128)
#define LIMIT (128/32)
#elif (defined SimpleSignature256)
DeclareFns(Signature,SimpleSignature256)
#define LIMIT (256/32)
#elif (defined SimpleSignature1024)
#define LIMIT (1024/32)
DeclareFns(Signature,SimpleSignature1024)
#elif (defined ArraySignature_32_32)
#define LIMIT (1024/32)
DeclareFns(Signature,ArraySignature_32_32)
#elif (defined ArraySignature_32_128)
#define LIMIT (32*128/32)
DeclareFns(Signature,ArraySignature_32_128)
#elif (defined DDPPerfectSet)
#define LIMIT 1
DeclareFns(Signature,DDPPerfectSet)
#elif (defined DDPHashTableSet)
#define LIMIT 1
DeclareFns(Signature,DDPHashTableSet)
#elif (defined BankedSignature_3x512)
#define LIMIT (3*512/32)
DeclareFns(Signature,BankedSignature_3x512)
#elif (defined BankedSignature_2x512)
#define LIMIT (2*512/32)
DeclareFns(Signature,BankedSignature_2x512)
#elif (defined BankedSignature_3x1024)
#define LIMIT (3*1024/32)
DeclareFns(Signature,BankedSignature_3x1024)
#elif (defined BankedSignature_3x2048)
#define LIMIT (3*2048/32)
DeclareFns(Signature,BankedSignature_3x2048)
#elif (defined BankedSignature_2x1024)
#define LIMIT (2*1024/32)
DeclareFns(Signature,BankedSignature_2x1024)

#elif (defined BankedSignature_2x4096)
#define LIMIT (2*4096/32)
DeclareFns(Signature,BankedSignature_2x4096)

#elif (defined BankedSignature_2x8192)
#define LIMIT (2*8192/32)
DeclareFns(Signature,BankedSignature_2x8192)

#elif (defined BankedSignature_4x256)
#define LIMIT (4*256/32)
DeclareFns(Signature,BankedSignature_4x256)

#elif (defined DumpSetBankedSignature_2x8192)
#define LIMIT (2*8192/32)
DeclareFns(Signature,DumpSet_BankedSignature_2x8192)

#elif (defined RangeAndBankedSignature_2x1024)
#define LIMIT (2*1024/32)
DeclareFns(Signature,RangeAndBankedSignature_2x1024)

#else
#define LIMIT 1
DeclareFns(Signature,SimpleSignature32)
#endif

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


int main(int argc, char **argv)
{
  //int8_t *array = malloc(sizeof(int8_t)*RANGE);
  //memset(array,RANGE,0);


  printf("Testing %s using %s:\n",argv[1],SigName);

  FILE *f = fopen(argv[1],"r");


  char line[1024];
  char *tmp=NULL;
  while (tmp=fgets(line,1000,f)) {
    int r1=0,r2=0;

    Signature s;

    s = Allocate();

    //printf("%s",tmp);
    
    sscanf(tmp,"%d %d", &r1, &r2);
    
    tmp = strchr(tmp,'(') ;
    while( *tmp && *tmp < '0' || *tmp > '9' )
      tmp++;

    //printf("%p %s",tmp,tmp);

    int ld=0,st=0;
    if (*tmp) {
      sscanf(tmp,"%x",&ld);
      
      tmp = strchr(tmp,'(') + 1;
      
      while( *tmp && *tmp < '0' || *tmp > '9' )
	tmp++;
      
      //printf("%s",tmp);

      if(tmp)
	sscanf(tmp,"%x",&st);
    }

    if (1) {

      printf("Compare signatures:\n");
      
      s = Allocate();
      Insert(s,(int*)st);
      printf("%x:------------------------------------------------ ",st); print(s);
      
      int m = Membership(s,(int*)ld);

      if(m==r1) {
	printf("*** verified ****\n");
      } else {
	printf("*** rejected ****\n");
      }

      s = Allocate();
      Insert(s,(int*)ld);
      printf("%x:------------------------------------------------ ",ld); print(s); 
    }

    if(ferror(f))
      break;
  }


    /*
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

  printf("Total Conflicts=%d\n",true_conflict+false_conflict);
  printf("True  Conflicts=%d\n",true_conflict);
  printf("False Positives=%d (%2.1lf%%)\n",false_conflict, (false_conflict)/((double)CHECK)*100);
  if(error)
    printf("Warning: %d errors found!",error);
    */
  return 0;
}
