//------------------------------------------------------------------------------
//
// memtrace
//
// trace calls to the dynamic memory manager
//
#define _GNU_SOURCE

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <memlog.h>
#include <memlist.h>
#include "callinfo.h"

//
// function pointers to stdlib's memory management functions
//
static void *(*mallocp)(size_t size) = NULL;
static void (*freep)(void *ptr) = NULL;
static void *(*callocp)(size_t nmemb, size_t size);
static void *(*reallocp)(void *ptr, size_t size);

//
// statistics & other global variables
//
static unsigned long n_malloc  = 0;
static unsigned long n_calloc  = 0;
static unsigned long n_realloc = 0;
static unsigned long n_allocb  = 0;
static unsigned long n_freeb   = 0;
static item *list = NULL;

//
// init - this function is called once when the shared library is loaded
//
__attribute__((constructor))
void init(void)
{
  char *error;
  if(!mallocp){
    mallocp = dlsym(RTLD_NEXT, "malloc");
    if((error = dlerror()) != NULL){
      fputs(error, stderr);
      exit(1);
    }
  }
  if(!callocp){
    callocp = dlsym(RTLD_NEXT, "calloc");
    if((error = dlerror()) != NULL){
      fputs(error, stderr);
      exit(1);
    }
  }
  if(!reallocp){
    reallocp = dlsym(RTLD_NEXT, "realloc");
    if((error = dlerror()) != NULL){
      fputs(error, stderr);
      exit(1);
    }
  }
  if(!freep){
    freep = dlsym(RTLD_NEXT, "free");
    if((error = dlerror()) != NULL){
      fputs(error, stderr);
      exit(1);
    }
  }

  LOG_START();

  // initialize a new list to keep track of all memory (de-)allocations
  // (not needed for part 1)
  list = new_list();

  // ...
}

void *malloc(size_t size){
  n_malloc ++;
  n_allocb += size;
  void *ptr = mallocp(size);
  alloc(list, ptr, size);
  LOG_MALLOC(size, ptr);
  return ptr;
}

void *calloc(size_t nmemb, size_t size){
  n_calloc ++;
  n_allocb += size*nmemb;
  void *ptr = callocp(nmemb, size);
  alloc(list, ptr, nmemb*size);
  LOG_CALLOC(nmemb, size, ptr);
  return ptr;
}

void *realloc(void* ptr, size_t size){
  item* meminlist = find(list, ptr);
  void* new_ptr;
  if((meminlist == NULL) || !(meminlist -> cnt)){
    new_ptr = reallocp(NULL, size);
    LOG_REALLOC(ptr, size, new_ptr);
    if(meminlist == NULL) LOG_ILL_FREE();
    else LOG_DOUBLE_FREE();
  }
  else{
    n_freeb += (meminlist -> size);
    dealloc(list, ptr);
    new_ptr = reallocp(ptr, size);
    LOG_REALLOC(ptr, size, new_ptr);
  }
  n_realloc ++;
  n_allocb += size;
  alloc(list, new_ptr, size);
  return new_ptr;
}

void free(void* ptr){
  LOG_FREE(ptr);
  item* meminlist = find(list, ptr);
  if(meminlist == NULL){
    LOG_ILL_FREE();
    return;
  }
  else if(!(meminlist->cnt)){
    LOG_DOUBLE_FREE();
    return;
  }
  n_freeb += (find(list, ptr)->size);
  dealloc(list, ptr);
  freep(ptr);
  return;
}

//
// fini - this function is called once when the shared library is unloaded
//
__attribute__((destructor))
void fini(void)
{
  // ...
  int avg = 0;
  if((n_malloc + n_calloc + n_realloc)>0) avg = n_allocb/(n_malloc + n_calloc + n_realloc);
  LOG_STATISTICS(n_allocb, avg, n_freeb);
  int num = 0;
  if(list != NULL){
    item *i = list;
    while(i != NULL){
      if(i->cnt){
        if(!num) {
          LOG_NONFREED_START();
          num = 1;
        }
        LOG_BLOCK(i->ptr, i->size, i->cnt, i->fname, i->ofs);
      }
      i = i->next;
    }
  }
  LOG_STOP();

  // free list (not needed for part 1)
  free_list(list);
}

// ...
