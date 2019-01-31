#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cassert>

#include <iostream>
#include <fstream>
#include <map>

#include "sigdef.h"

using namespace std;

void print(int *sign) {
  printf("Signature:\n");
  for (int i=0; i<LIMIT; ) {
    for(int j=0;j<8; j++,i++)
      printf("%08x",sign[i]);
    printf("\n");
  }
  printf("\n");
}


int main(int argc, char *argv[]) {
   assert(argc == 3 && "trace <insertion trace> <check trace>");

   cout << "Testing "<<SigName<<endl;
   Signature s = Allocate();

   ifstream insertFile(argv[1]);
   if(!insertFile) {
      cout<<"Unable to open file "<<argv[1]<<endl;
      return -1;
   }

   long int insertCount=0;
   map<unsigned int,int> inserted;
   while(insertFile) {
      insertCount++;
      unsigned int addr;
      insertFile>>hex>>addr;
      Insert(s,(int8_t *)addr);
      inserted[addr] = 1;
      cout<<"Insert @"<<hex<<addr<<" in signature"<<endl;
   }
   insertFile.close();

   ifstream checkFile(argv[2]);
   if(!checkFile) {
      cout<<"Unable to open file "<<argv[2]<<endl;
      return -1;
   }

   int true_conflict=0;
   int false_conflict=0;
   int error=0;
   long int checkCount=0;
   while(checkFile) {
      checkCount++;
      unsigned int addr;
      checkFile>>hex>>addr;
      int m = Membership(s,(int8_t *)addr);
      bool perfect = ( inserted.find(addr) != inserted.end() );
      if(m && perfect ) {
         true_conflict++;
         cout<<"(True) Check @"<<hex<<addr<<endl;
      } else if(m && !perfect) {
         false_conflict++;
         cout<<"(False) Check @"<<hex<<addr<<endl;
      } else if(!m && perfect) {
         cout<<"(Error) Check @"<<hex<<addr<<endl;
         error++;
      }
   }
   checkFile.close();

   //print (s);

  printf("Total insertions=%ld\n",insertCount);
  printf("Total checks=%ld\n",checkCount);
  printf("Total Conflicts by signature=%d\n",true_conflict+false_conflict);
  printf("True  Conflicts by signature=%d\n",true_conflict);
  printf("False Positives by signature=%d (%2.4lf%%)\n",false_conflict, (false_conflict)/((double)checkCount)*100);
  if(error)
    printf("Warning: %d errors found!",error);

  return 0;
}
