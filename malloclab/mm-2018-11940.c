/*
 * mm.c
 * In this approach, explicit & segregated free list is applied.
 * A block consists of 4B Header, 4B Footer, and minimum 8B Payload.
 * First blk of the payload is called block pointer(blkptr, bp)
 * and contains next free blk's bp (as linked list) if free blk.
 * Second blk of the payload contains previousfree blk's bp(as linked list) if free blk.
 * Free list is segregated as 10 lists by blksize.
 * It covers 2^n ~ 2^(n+1)-1 range of blksize.
 * It uses pseudo-bestfit technic to find blk to assign.
 * It gets at most (and generally) 8 blks that has enough size,
 * and pick one with the smallest size to fully utilise given memory.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/

/* single word (4) or double word (8) alignment */
#define ALIGNMENT   8
#define WORD        4       // 1WORD
#define DWRD        8       // 2WORD
#define MINBLK      16      // minimun blk size : Header - Next - Prev - Footer => 4WORD
#define HBUFF       1<<12   // buffer size for heap extending
/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define MAX(a, b)   ((a)>(b)? (a):(b))
#define MIN(a, b)   ((a)<(b)? (a):(b))

#define GET(p)              (*(size_t *)(p))                                // Get content of (*p)
#define PUT(p, val)         (*(size_t *)(p) = (size_t) val)                 // Write val to (*p)
#define PUT_T(p, val, t)    (*(size_t *)(p) = ((size_t) (val & ~0x7) | t))  // Write with assign bit t
#define GET_SIZE(p)         (GET(p) & ~0x7)                                 // Get size of given blk(given as headptr)
#define GET_ALLOC(blkp)     (GET(HEADER(blkp)) & 0x1)                       // Get allocation info of given blk(given as blkptr)

#define HEADER(blkp)        ((void *)blkp - WORD)                           // Header of given blk(given as blkptr)
#define FOOTER(blkp)        ((void *)blkp - DWRD + GET_SIZE(HEADER(blkp)))  // Footer of given blk(given as blkptr)

#define BP_NEXT(blkp)       ((void *)blkp + GET_SIZE((void *)blkp - WORD))  // Next Block Pointer (in heap)
#define BP_PREV(blkp)       ((void *)blkp - GET_SIZE((void *)blkp - DWRD))  // Previous Block Pointer (in heap)

#define FBP_NEXT(blkp)      ((void *)GET(blkp))             // Next Free Block Pointer (in free-list)
#define FBP_PREV(blkp)      ((void *)GET(blkp + WORD))      // Previous Free Block Pointer (in free-list)

#define LOBOUND             (void *)mem_heap_lo()           // Heap lowerbound (unit: 1WORD)
#define UPBOUND             (void *)mem_heap_hi()-3         // Heap upperbound (unit: 1WORD)

void **segbegin(void *bp);
void **segbegin_size(size_t size);
void *find_block(size_t size);
void fl_insert(void *bp_tgt);
void fl_delete(void *bp_tgt);
void fl_merge(void *bp1, void *bp2);
void allocate(void* p, size_t size);
void *heap_extend(size_t size);
void *coalesce(void *ptr);

/* seglist begin ptr */
void *bg0 = 0; /*   ~2^ 6    */
void *bg1 = 0; /*   ~2^ 7    */
void *bg2 = 0; /*   ~2^ 8    */
void *bg3 = 0; /*   ~2^ 9    */
void *bg4 = 0; /*   ~2^10    */
void *bg5 = 0; /*   ~2^11    */
void *bg6 = 0; /*   ~2^12    */
void *bg7 = 0; /*   ~2^13    */
void *bg8 = 0; /*   ~2^14    */
void *bg9 = 0; /*   2^14~    */

/* 
 * segbegin - given block ptr, return adequete segregated list begin ptr
 */
void **segbegin(void *bp){
    size_t size = GET_SIZE(HEADER(bp));
    if(size < (1<<6)) return &bg0;
    if(size < (1<<7)) return &bg1;
    if(size < (1<<8)) return &bg2;
    if(size < (1<<9)) return &bg3;
    if(size < (1<<10)) return &bg4;
    if(size < (1<<11)) return &bg5;
    if(size < (1<<12)) return &bg6;
    if(size < (1<<13)) return &bg7;
    if(size < (1<<14)) return &bg8;
    return &bg9;
}

/* 
 * segbegin_size - given block size, return adequete segregated list begin ptr
 */
void **segbegin_size(size_t size){
    if(size < (1<<6)) return &bg0;
    if(size < (1<<7)) return &bg1;
    if(size < (1<<8)) return &bg2;
    if(size < (1<<9)) return &bg3;
    if(size < (1<<10)) return &bg4;
    if(size < (1<<11)) return &bg5;
    if(size < (1<<12)) return &bg6;
    if(size < (1<<13)) return &bg7;
    if(size < (1<<14)) return &bg8;
    return &bg9;
}

/* 
 * mm_init - initialize the malloc package.
    - initialize all the free-list begin ptr
    - make the very first block
    - extend heap to HBUFF size
 */
int mm_init(void)
{
    void *begin;
    if ((begin = mem_sbrk(DWRD*3)) == (void *)(-1))     // Prolog-Header-Next-Prev-Footer-Epilog : 6WORD size
        return -1;                                      // allocation failed
    bg0 = bg1 = bg2 = bg3 = bg4 = bg5 = bg6 = bg7 = bg8 = bg9 = 0;  // initialize all the free-list begin ptr
    PUT(begin, 0);                      // PROLOG
    PUT(begin + WORD, 2*DWRD);          // HEADER
    PUT(begin + 2*WORD, NULL);          // NEXT
    PUT(begin + 3*WORD, NULL);          // PREV
    PUT(begin + 4*WORD, 2*DWRD);        // FOOTER
    PUT(begin + 5*WORD, 0);             // EPILOG
    begin += DWRD;                      // set the very first blk's blkptr
    PUT(segbegin(begin), begin);        // insert this blk to free list
    heap_extend(HBUFF);                // extend heap to HBUFF size
    return 0;
}

/* 
 * find_block - find free blk which has adequate space for input size
    - Start finding from a segregated list whose size range covers input size
    - If not found, go to next segregated list (ordered by size range it covers)
    - It uses pseudo-Bestfit(heuristic) - get at most 8 adquete blk and return one with the smallest size
 */
void *find_block(size_t size)
{
    /* 항상 해당하는 segregated list에서부터 찾아야 함 */
    void* p;
    size_t s = size;
    int count = 8;
    size_t fitsize = mem_heapsize();
    void* fitptr = NULL;
    while(1){
        for(p = (void *)GET(segbegin_size(s)); p != NULL; p = FBP_NEXT(p)){ // get begin ptr for segregated free list that covers size s
            if(GET_SIZE(HEADER(p)) >= size){                        // if this blk has enough size
                count--;                                            // decrease count
                if(GET_SIZE(HEADER(p)) < fitsize){                  // update firptr only if new one's size is smaller than existing one's
                    fitsize = GET_SIZE(HEADER(p));
                    fitptr = p;
                }
                if(!count) return fitptr;                           // return after 8 blk's size comparisons
            }
        }
        if(s >= (1<<14)) break;                                     // break after examined the last segregated list
        s = (s << 1);                                               // go to the next segregated list
    }
    return fitptr;
}

/* 
 * fl_insert - insert blk with bp = bp_tgt into adequate free list
    - Should insert into appropriate segregated free list that covers the size which input block has
    - Always insert into the first of the list
 */
void fl_insert(void *bp_tgt)
{
    /* Address Ordered */
    void** segbg = segbegin(bp_tgt);
    void* next = (void *)GET(segbg);            // 원래의 begin

    if(next == NULL){                   // begin is null(빈 list에 삽입)
        PUT(segbg, bp_tgt);             // 지금 넣는 item이 begin이 됨
        PUT(bp_tgt, NULL);              // next free blk is NULL
        PUT(bp_tgt + WORD, NULL);       // prev free blk is NULL
        return;
    }

    PUT(next + WORD, bp_tgt);       // next의 prev가 target
    PUT(bp_tgt + WORD, NULL);       // target prev가 NULL
    PUT(bp_tgt, next);              // target next가 원래 next
    PUT(segbg, bp_tgt);             // begin을 target로 set
    return;
}

/* 
 * fl_delete - delete blk with bp = bp_tgt from free list
    - Should delete from free list - set ptr of adjacent free blocks, set begin ptr
 */
void fl_delete(void *bp_tgt)
{
    void** segbg = segbegin(bp_tgt);

    void* bp_prev = FBP_PREV(bp_tgt);           // free list의 다음 item
    void* bp_next = FBP_NEXT(bp_tgt);           // free list의 이전 item

    if((!bp_prev) && (!bp_next)){               // target밖에 없는 list였을 경우
        PUT(segbg, NULL);                       // begin이 NULL (empty list)
        return;
    }
    if(bp_prev) PUT(bp_prev, bp_next);          // 제일 첫 item이 아닐 경우 이전 item의 next가 target의 next
    else PUT(segbg, bp_next);                   // 제일 첫 item이었을 경우 begin을 바꿈
    if(bp_next) PUT(bp_next + WORD, bp_prev);   // 제일 끝 item이 아닐 경우 이후 item의 prev가 target의 prev
}

/* 
 * fl_merge - merge two adjacent input blks into one blk
    - Merge two blks into one blk and insert it into free list
    - It is assumed that two input blk is not allocated and adjacent, and both in segregated free list
 */
void fl_merge(void *bp1, void *bp2)
{
    size_t size = GET_SIZE(HEADER(bp1)) + GET_SIZE(HEADER(bp2));    // merge한 후의 size
    fl_delete(bp1);                                                 // free list에서 bp1을 제거
    fl_delete(bp2);                                                 // free list에서 bp2를 제거
    PUT_T(HEADER(bp1), size, 0);                                    // bp1의 header에 size up
    PUT_T(FOOTER(bp2), size, 0);                                    // bp2의 footer에 size up
    fl_insert(bp1);                                                 // 블록을 합치고 나서 free list에 다시 삽입
}

/* 
 * allocate - allocate input size to input blk with blkptr=p
    - If the target blk size is bigger enough to form a blk after assigning given size, create new free blk
    - If not, just assign all the blksize(can be bigger than given size)
 */
void allocate(void* p, size_t size)
{
    if(p == NULL) return;

    size_t size_left = GET_SIZE(HEADER(p)) - size;  // size that would remain after allocation
    fl_delete(p);                                   // p를 free list에서 제거

    if(size_left >= MINBLK){                        // split하고 남은 공간이 하나의 blk을 형성할 수 있음
        PUT_T(FOOTER(p), size_left, 0);             // old footer
        PUT_T(HEADER(p), size, 1);                  // new header
        PUT_T(FOOTER(p), size, 1);                  // new footer
        PUT_T(FOOTER(p) + WORD, size_left, 0);      // new header
        fl_insert(FOOTER(p) + DWRD);                // rest blk을 insert
    }
    else{                                           // 남은 공간만으로 하나의 blk을 형성할 수 없음
        PUT_T(HEADER(p), GET(HEADER(p)), 1);        // set alloc bit to 1
        PUT_T(FOOTER(p), GET(FOOTER(p)), 1);        // set alloc bit to 1
    }
    return;
}

/* 
 * heap_extend - extend the heap by a given size
    - Should Keep the last 4B as Epilogue blk
 */
void *heap_extend(size_t size)
{
    void *p = mem_sbrk(size);           // enlarge the heap
    if(p == (void *)(-1)){              // case if enlarging failed
        return NULL;
    }
    PUT_T(HEADER(p), size, 0);          // new blk header
    PUT_T(FOOTER(p), size, 0);          // new blk footer
    PUT(UPBOUND, 0);                    // epilogue
    fl_insert(p);                       // insert new blk into freelist
    return coalesce(p);                 // coalesce inserted blk and return
}

/* 
 * coalesce - coalesce from the given ptr, and return coalesced blk's blkptr
    - Check both previous and next block
 */
void *coalesce(void *ptr){
    /* free list에 ptr이 이미 존재해야 함 */
    if(!ptr) return ptr;                                // return if null
 
    void* prev = BP_PREV(ptr);                          // prev block pointer
    void* next = BP_NEXT(ptr);                          // next block pointer

    if((ptr > (LOBOUND + DWRD)) && (!GET_ALLOC(prev))){ // ptr이 첫 blk이 아니면서, 이전 blk이 free인 경우
        fl_merge(prev, ptr);                            // merge with it
        ptr = prev;                                     // set new block pointer
    }
    if((next < UPBOUND) && (!GET_ALLOC(next))){        // ptr이 마지막 blk이 아니면서, 이후 blk이 free인 경우
        fl_merge(ptr, next);                            // merge with it
    }
    return ptr;
}


/* 
 * mm_malloc - Allocate a block by finding proper free block
    - Look for big enough free blk first
    - Extend heap and allocate if failed
 */
void *mm_malloc(size_t size)
{
    if (!size) return NULL;             // return if size is zero

    size_t nsize = ALIGN(size) + DWRD;  // size to allocate : PAYLOAD(ALIGNED_SIZE) + header + footer
    void *p;                            
    if((p = find_block(nsize))){        // if found fit or bigger block
        allocate(p, nsize);             // allocate there
    }
    else{
        p = heap_extend(nsize);         // else, extend heap(PAYLOAD + header + footer)
        allocate(p, nsize);             // allocate to extended blk
    }
    return p;
}

/*
 * mm_free - Free blk with blkptr=ptr
    - Set assign-bit to zero
    - Insert blk into free list
    - coalesce blk
 */
void mm_free(void *ptr)
{
    if(!ptr) return;                        // return if ptr is NULL
    if(!GET_ALLOC(ptr)) return;             // return if ptr is already free
    size_t size = GET_SIZE(HEADER(ptr));    // get the size to free

    PUT_T(HEADER(ptr), size, 0);            // set allocation bit to zero
    PUT_T(FOOTER(ptr), size, 0);            // set allocation bit to zero
    fl_insert(ptr);                         // insert ptr into free list
    coalesce(ptr);                          // coalesce inserted blk
    return;
}

/*
 * mm_realloc - Realloc size to given blk
    - 기존 blk에 기록된 내용이 보존되어야 함
    - reallocation이 불필요한 경우 : newsize < oldsize -> 기존
    - 내용 copy가 필요 없는 경우 : 뒤로 인접한 blk을 합쳐 주어진 크기를 만들 수 있는 경우
            / heap의 마지막 blk으로 heap을 extend하여 주어진 크기를 만들 수 있는 경우
    - 내용 copy가 필요한 경우 : 위 경우 제외, 새로운 blk을 찾아 malloc후 기존 blk을 free해야 하는 경우
    - 한번 realloc된 blk은 여러 번 realloc될 가능성이 높으므로, 충분한 여유 size를 더해 할당(1<<8)
 */
void *mm_realloc(void *ptr, size_t size)
{
    if(!ptr) return mm_malloc(size);                // if ptr is NULL, it works like malloc(size)
    if(!size){                                      // if size is 0, it works like free(ptr)
        mm_free(ptr);
        return NULL;
    }
    if(!GET_ALLOC(ptr)) return mm_malloc(size);     // realloc to freed blk, it works like malloc(size)

    size_t oldsize = GET_SIZE(HEADER(ptr));         // 기존에 할당되어 있던 크기
    size_t newsize = ALIGN(size) + DWRD + (1<<8);   // 새로 할당하려는 크기 + 여유공간

    if(oldsize >= newsize) return ptr;              // 새로 할당하려는 크기가 기존 할당 크기보다 작으면 재할당이 필요 없음

    void *newptr;
    void *next = BP_NEXT(ptr);

    /* Target blk의 바로 인접한 뒤에 free blk이 있고, 둘을 합치면 newsize를 할당할 수 있는 경우 */    
    if(((next-WORD) < UPBOUND) && (!GET_ALLOC(next)) && ((oldsize + GET_SIZE(HEADER(next)) > newsize))){
        newsize = oldsize + GET_SIZE(HEADER(next));     // 뒷 블록을 자르지 않고 합칠 것
        fl_delete(next);                                // 뒷 블록을 free list에서 제거
        PUT_T(HEADER(ptr), newsize, 1);                 // 새 블록의 header에 assign bit, 크기 기록
        PUT_T(FOOTER(ptr), newsize, 1);                 // 새 블록의 footer에 assign bit, 크기 기록
        return ptr;
    }
    /* Target blk이 Heap의 마지막 blk일 경우 - Heap 확장으로 크기를 늘릴 수 있음 */
    if((next - WORD) == UPBOUND){
        next = mem_sbrk(newsize - oldsize);     // 크기 차이만큼 크기를 늘림
        PUT_T(HEADER(ptr), newsize, 1);         // 새 블록의 header에 assign bit, 크기 기록
        PUT_T(FOOTER(ptr), newsize, 1);         // 새 블록의 footer에 assign bit, 크기 기록
        return ptr;      
    }
    /* 새로운 blk에 재할당이 필요한 경우 */
    newptr = mm_malloc(MAX(newsize, HBUFF));   // 주어진 크기를 malloc
    memcpy(newptr, ptr, size);                  // 기존 blk에서 내용을 복사
    mm_free(ptr);                               // 기존 blk을 할당 해제
    return newptr;
}

/*
 * mm_check - checks below, return 1 iff heap is consistent(else return 0)
    - Is every block in heap?   (No exceed)
    - Is every free block coalesced?    (No adjacent multiple free blocks)
    - Is every block aligned on 8-byte boundaries?
    - Is every block in free list marked as free?
    - Does free list well-linked in both directions?
 */
int mm_check(void)
{
    void *top = mem_heap_lo();
    void *btm = mem_heap_hi();
    void *ptr;
    assert(btm == (top + mem_heapsize()-1));
    int consistency = 1;
    for(ptr = top + DWRD; BP_NEXT(ptr) < btm; ptr = BP_NEXT(ptr)){
        /* Blkptr out of range */
        if(ptr < top){
            consistency = 0;
            fprintf(stderr, "Block Pointer exceeded Top of Heap(%x, %x)\n", (unsigned int)ptr, (unsigned int)top);
        }
        /* Blkptr out of range */
        if(ptr > btm){
            consistency = 0;
            fprintf(stderr, "Block Pointer exceeded Bottom of Heap(%x, %x)\n", (unsigned int)ptr, (unsigned int)btm);
        }
        /* Blk not coalesced */
        if((!GET_ALLOC(ptr)) && (!GET_ALLOC(BP_NEXT(ptr)))){
            consistency = 0;
            fprintf(stderr, "Not coalesced blocks(%x, %x)\n", (unsigned int)ptr, (unsigned int)BP_NEXT(ptr));
        }
        /* Blk not aligned on 8B boundaries */
        if(((size_t)ptr)%8){
            consistency = 0;
            fprintf(stderr, "Misaligned block(%x)\n", (unsigned int)ptr);
        }
    }
    void* bgptr[10] = {bg0, bg1, bg2, bg3, bg4, bg5, bg6, bg7, bg8, bg9};
    for(int i=0; i<10; i++){
        void *p;
        for(p = bgptr[i]; (p!=NULL); p = FBP_NEXT(p)){
            /* Blk in free list not marked as free */
            if(GET_ALLOC(p)){
                consistency = 0;
                fprintf(stderr, "Block(ptr %x) in free list not marked as free\n", (unsigned int)p);
            }
            /* Blk link broken */
            if(FBP_PREV(p) && (FBP_NEXT(FBP_PREV(p)) != p)){
                consistency = 0;
                fprintf(stderr, "Free list link broken(%x, %x)\n", (unsigned int)FBP_PREV(p), (unsigned int)p);
            }
        }
    }
	return consistency;
}