#include <stdlib.h>

int main(void)
{
  void *a, *b, *c, *d;
  free(a);  // ILLF
  free(b);  // ILLF
  free(c);  // ILLF
  b = realloc(a, 11);   // ILLF
  c = realloc(b, 23);
  free(b);  // DBLF
  realloc(b, 1);    //DBLF
  a = calloc(37, 121);
  free(a+1);    // ILLF
  free(a+2);    // ILLF
  d = realloc(malloc(124), 4313);
  d = realloc(calloc(124, 123), 4313);
  d = realloc(realloc(d, 124), 4313);
  d = realloc(realloc(realloc(realloc(a, 12), 15), 189), 48);
  d = realloc(realloc(realloc(realloc(a+2, 12), 15), 189), 48); // ILLFREE
  free(a);  //DBLF
  free(d+1);    //ILLF
  return 0;
}
