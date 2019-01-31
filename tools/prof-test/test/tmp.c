#include <stdio.h>

int foo();

int main() {
  printf("Hello!");

  int i;
  for(i=0; i<100; i++) {
    printf("i=%d\n",i);
  }

  printf("All done!");
  
  foo();
  
  return 0;
}


int foo() {
  printf("Hello!");

  int i;
  for(i=0; i<100; i++) {
    printf("i=%d\n",i);
  }

  printf("All done!");
  return 0;
}
