#include <stdio.h> 

const char *in_a_dll();
 
main() 
{ 
  printf("%x\n", in_a_dll (0));
  printf("%s\n", in_a_dll (0));
  printf("%s\n", in_a_dll (1));
} 


