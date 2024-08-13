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

// 워드, 더블, 페이지 사이즈 정의
#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<12)

//최대값 반환
#define MAX(x, y) ((x) > (y) ? (x) : (y))

//헤더와 풋터 영역에 넣을 데이터 생성
#define PACK(size, alloc) ((size) | (alloc))

// 인자 주소부터 4바이트(unsinged int)씩 데이터를 읽어오거나 쓴다.
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (unsigned int )(val))

//블록의 크기와 할당 여부를 반환한다.
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

//블록의 payload 주소를 기반으로 헤더의 주소와 풋터의 주소를 반환한다.
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

//블록의 payload 주소를 기반으로 이전 블록과 다음 블록의 payload 주소를 반환한다.
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

//가용 리스트의 첫번째 블록을 가리키는 변수이다.
static void* heap_listp;
static void* free_listp;
//전임자와 후임자의 주소를 반환하는 매크로이다.
#define PREV(bp) (*(void **)(bp))  // 이전 블록 주소
#define NEXT(bp) (*(void **)(bp+WSIZE))  // 다음 블록 주소

/* 
 * mm_init - initialize the malloc package.
 */

void append_free_list(void *bp){
    
    //이전의 최초 가용 블록
    PREV(free_listp) = bp;
    
    //최근 가용 블록 업데이트
    PREV(bp) = NULL;
    NEXT(bp) = free_listp;
    free_listp = bp;
}

void delete_free_list(void *bp){

    if(bp == free_listp){
        PREV(NEXT(bp)) = NULL;
        free_listp = NEXT(bp);
    }
    else{
        NEXT(PREV(bp)) = NEXT(bp);
        PREV(NEXT(bp)) = PREV(bp);
    }
}

static void *coalesce(void *bp){
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));
    
    if(prev_alloc && next_alloc){
        append_free_list(bp);
        return bp;
    }
    else if(!prev_alloc && next_alloc) {
        //앞의 가용 블록과 합쳐준다.
        delete_free_list(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        //bp를 합쳐지기 전 앞의 payload 주소로 보낸다. 이전 블록의 리스트 관계를 업데이트한다.
        bp = PREV_BLKP(bp);
    }
    else if(prev_alloc && !next_alloc) {
        delete_free_list(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    else {
        delete_free_list(NEXT_BLKP(bp));
        delete_free_list(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        size += GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        
        bp = PREV_BLKP(bp);
    }
    append_free_list(bp);
    return bp;

}

static void *extend_heap(size_t words){
    char *bp;
    size_t size = (words%2) ? (words+1) * WSIZE : words * WSIZE;
    //사이즈만큼 할당 받는다.
    if((long)(bp = mem_sbrk(size)) == -1) return NULL;
    //새로 받은 하나의 블록의 헤더와 풋터를 업데이트해주고, 마지막 공간에 에필로그 헤더를 넣어준다.
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));
    
    //하나의 큰 가용 블록을 업데이트해준다.

    return coalesce(bp);
}

int mm_init(void)
{
    //초기화를 위해 6워드 사이즈 만큼 할당 받는다.
    if((heap_listp = mem_sbrk(6*WSIZE)) == (void *)-1) return -1;
    //unused, 프롤로그,전임자,후임자,에필로그 총 24바이트이다.
    PUT(heap_listp, 0);
    PUT(heap_listp + (WSIZE*1), PACK(DSIZE*2, 1));
    PUT(heap_listp + (WSIZE*2), NULL);
    PUT(heap_listp + (WSIZE*3), NULL);
    PUT(heap_listp + (WSIZE*4), PACK(DSIZE*2, 1));
    PUT(heap_listp + (WSIZE*5), PACK(0, 1));
    // heal_listp는 프롤로그 블록의 payload 위치를 가리킨다.
    heap_listp += (WSIZE*2);
    //초기화이므로 마지막 가용 가능한 블록을 프롤로그 블록이라고 가정한다.
    free_listp = heap_listp;
    
    //말록을 위한 페이지를 확장한다.
    if(extend_heap(CHUNKSIZE/WSIZE) == NULL) return -1;
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *find_fit(size_t asize){
    void *bp;
    for(bp = free_listp;  !GET_ALLOC(HDRP(bp)); bp = NEXT(bp)){
        if(asize <= GET_SIZE(HDRP(bp))){
            return bp;
        }
    }
    return NULL;
}

void place(void *bp, size_t asize){
    size_t csize = GET_SIZE(HDRP(bp));
    if((csize-asize) >= DSIZE*3){
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        delete_free_list(bp);

        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize-asize, 0));
        PUT(FTRP(bp), PACK(csize-asize, 0));
        append_free_list(bp);
    }
    else{
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
        delete_free_list(bp);
    }
}

void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    char* bp;

    if(size == 0) return NULL;

    if(size <= DSIZE) asize = DSIZE*3;
    else asize = DSIZE * ((size +  (2 * DSIZE) + (DSIZE-1)) / DSIZE);

    if((bp = find_fit(asize)) != NULL){
        place(bp, asize);
        return bp;
    }

    extendsize = MAX(asize,CHUNKSIZE);
    if((bp = extend_heap(extendsize/WSIZE)) == NULL) return NULL;
    place(bp,asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);
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
    copySize = GET_SIZE(HDRP(ptr));
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}