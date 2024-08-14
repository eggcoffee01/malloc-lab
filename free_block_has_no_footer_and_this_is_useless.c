/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
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
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define MAX(x, y) ((x) > (y) ? (x) : (y))

//word, double word size define
#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1 << 12)
//packing size with allocated
#define PACK(size, alloc) ((size) | (alloc))

//read and write(4byte)
#define GET(bp) (*(unsigned int *)(bp))
#define PUT(bp, val) (*(unsigned int *)(bp) = (val))

//get size and allocated
#define GET_SIZE(bp) (GET(bp) & ~0x7)
#define GET_ALLOC(bp) (GET(bp) & 0x1)
#define GET_PREV_ALLOC(bp) (GET(bp) & 0x2)

//get header and footer
#define HEADER(bp) ((char*)(bp) - WSIZE)
#define FOOTER(bp) ((char*)(bp) + GET_SIZE(HEADER(bp)) - DSIZE)

//GET next block, prev block
#define NEXT(bp) ((char*) bp + GET_SIZE(HEADER(bp)))
#define PREV(bp) ((char*) bp - GET_SIZE((char*)(bp) - DSIZE))

//define default ptr
static void *heap_listp;


/* 
 * mm_init - initialize the malloc package.
 */
static void *merge_free(void *bp){

    size_t size = GET_SIZE(HEADER(bp));
    size_t next_alloc = GET_ALLOC(HEADER(NEXT(bp)));
    size_t prev_alloc = GET_PREV_ALLOC(HEADER(bp));

    if(prev_alloc && next_alloc){
        PUT(HEADER(bp), PACK(size, 2));
        return bp;
    }
    else if (!prev_alloc && next_alloc)
    {
        size += GET_SIZE(HEADER(PREV(bp)));
        bp = PREV(bp);
        PUT(HEADER(bp), PACK(size, 2));
        PUT(FOOTER(bp), PACK(size, 0));
    } 
    else if (prev_alloc && !next_alloc)
    {
        size += GET_SIZE(HEADER(NEXT(bp)));
        PUT(HEADER(bp), PACK(size, 2));
        PUT(FOOTER(bp), PACK(size, 0));
    }
    else{
        size += GET_SIZE(HEADER(PREV(bp)));
        size += GET_SIZE(HEADER(NEXT(bp)));
        bp = PREV(bp);
        PUT(HEADER(bp), PACK(size, 2));
        PUT(FOOTER(bp), PACK(size, 0));
    }
    
    return bp;
}

static void *extend_heap(size_t words){
    
    size_t size;
    void *bp;

    size = (words%2) ? (words+1) * WSIZE : words * WSIZE;

    if((bp = mem_sbrk(size)) == (void *)-1) return (void *)-1;   
    size_t prev_alloc = GET_PREV_ALLOC(HEADER(bp));
    PUT(HEADER(bp), PACK(size, prev_alloc));
    PUT(FOOTER(bp), PACK(size, 0));
    PUT(HEADER(NEXT(bp)), PACK(0, 1));

    return merge_free(bp);
}

int mm_init(void)
{
    if((heap_listp = mem_sbrk(WSIZE * 4)) == (void *)-1) return -1;    

    PUT(heap_listp, 0);
    PUT(heap_listp + (WSIZE*1), PACK(DSIZE, 1));
    PUT(heap_listp + (WSIZE*2), PACK(DSIZE, 1));
    PUT(heap_listp + (WSIZE*3), PACK(0, 3));

    heap_listp += (WSIZE*2);

    if (extend_heap(CHUNKSIZE/WSIZE) == (void *)-1) return -1;
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
static void *find_fit(size_t asize){
    void *bp;
    
    for(bp = heap_listp; GET_SIZE(HEADER(bp)) > 0; bp = NEXT(bp)){
        if(!GET_ALLOC(HEADER(bp)) && asize <= GET_SIZE(HEADER(bp))) return bp;
    }

    return (void *)-1;
}

void place(void *bp, size_t asize){
    size_t full_size = GET_SIZE(HEADER(bp));
    size_t remain_size = full_size - asize;
    size_t prev_alloc = GET_PREV_ALLOC(HEADER(bp));
    
    if(remain_size >= DSIZE){
        PUT(HEADER(bp), PACK(asize, prev_alloc+1));
        PUT(HEADER(NEXT(bp)), PACK(remain_size, 2));   
        PUT(FOOTER(NEXT(bp)), PACK(remain_size, 0));     
    }
    else{
        PUT(HEADER(bp), PACK(full_size, prev_alloc+1));
        PUT(HEADER(NEXT(bp)), PACK(GET(HEADER(NEXT(bp))), 2));
    }
}

void *mm_malloc(size_t size)
{
    if(size == 0) return (void *)-1;

    size_t asize;
    size_t extendsize;
    void *bp;
    
    if(size <= WSIZE) asize = DSIZE;
    else asize = DSIZE * ((size + WSIZE + (DSIZE-1))/ DSIZE);

    if((bp = find_fit(asize)) != (void *)-1){
        place(bp, asize);
        return bp;
    }

    extendsize = MAX(asize, CHUNKSIZE);
    if((bp = extend_heap(extendsize/WSIZE)) == (void *)-1) return NULL;
    place(bp, asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HEADER(ptr));
    size_t prev_alloc = GET_PREV_ALLOC(HEADER(ptr));
    PUT(HEADER(ptr), PACK(size, prev_alloc));
    PUT(FOOTER(ptr), PACK(size, 0));
    PUT(HEADER(NEXT(ptr)), GET(HEADER(NEXT(ptr))) - 2);
    merge_free(ptr);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{

    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = GET_SIZE(HEADER(ptr));
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}