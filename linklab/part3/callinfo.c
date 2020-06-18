#define UNW_LOCAL_ONLY
#include <libunwind.h>
#include <stdlib.h>

int get_callinfo(char *fname, size_t fnlen, unsigned long long *ofs)
{
  unw_cursor_t cursor;
  unw_context_t context;
  unw_getcontext(&context);
  unw_init_local(&cursor, &context);
  
  unw_word_t offset;
  while(1){
    unw_get_proc_name(&cursor, fname, fnlen, &offset);
    if(fname[0] == 'm' && fname[1] == 'a' && fname[2] =='i' && fname[3] == 'n') break;
    unw_step(&cursor);
  }
  (*ofs) = offset-5;
  return 0;
}