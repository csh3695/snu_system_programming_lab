#include <stdlib.h>                                                                                                
                                                                                                                    
int main(void)                                                                                                     
{                                                                                                                  
  void *a;                                                                                                         
  void *b;                                                                                                         
                                                                                                                  
  a = malloc(1024);                                                                                                
  free(a);                                                                                                        
  free(a);                                                                                                        
  free((void*)0x1706e90);                                                                                         
  b = realloc(b, 1024);                                                                                           
  a = realloc(a, 1024);                                                                                           
  free(b);                                                                                                        
  b = realloc(b, 1024);                                                                                           
                                                                                                                  
  return 0;                                                                                                       
}     