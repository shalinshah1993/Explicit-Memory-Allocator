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

/*Size of word */
#define WSIZE 4    

/* Size of double word */
#define DSIZE 8      

/* Initial size of heap (for expanding) */
#define CHUNKSIZE (1<<12) 

/* Extra bytes used by header and footer */
#define OVERHEAD 8

/* Pack a size and allocated bit into a word */
#define PACK(size,alloc) ((size) | (alloc))

/* Read and write a word at address p */ 
#define GET(p) (*(size_t *)(p)) 
#define PUT(p,val) (*(size_t *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1) 

/* Given block ptr bp, compute address of its header and footer */

#define HDRP(bp) ((char *)(bp) - WSIZE) 
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) 

/* Given block ptr, compute address of next and previous blocks */

#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE))) 
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE))) 
 
/*heap_listp is the beginning of the heap pointer*/
static char *heap_listp = NULL;
static char *head = NULL;
/* 
 mm_init - initialize the malloc package.
*/
int mm_init(void)
{
	if ((heap_listp = mem_sbrk(4*WSIZE)) == NULL)	
		return -1; 
	
	//This is the free space for alignment issues i.e 4 byte padding
	PUT(heap_listp, 0);  
	//The prologue and epilogue are defined here with prologue having header/footer(8|1) and epilogue having only header(0|1) 
	PUT(heap_listp+WSIZE, PACK(OVERHEAD, 1)); 
	PUT(heap_listp+DSIZE, PACK(OVERHEAD, 1)); 
	PUT(heap_listp+WSIZE+DSIZE, PACK(0, 1)); 
	//heap_listp points to the prologue block
	heap_listp += DSIZE; 

	if (extend_heap(CHUNKSIZE/WSIZE) == NULL) 
		return -1; 

	//Head is pointing to the first byte of payload 
	head = heap_listp + WSIZE ;	
	*head = NULL;

	return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    int newsize = ALIGN(size + SIZE_T_SIZE);
    void *p = mem_sbrk(newsize);
    if (p == (void *)-1)
	return NULL;
    else {
        *(size_t *)p = size;
        return (void *)((char *)p + SIZE_T_SIZE);
    }
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
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
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}



/*
 *extend_heap - This function is used to extend the heap whenever required  
 */

static void *extend_heap(size_t words) { 
	char *bp; 
	size_t size;
 
	size = (words % 2) ? (words+1)*WSIZE : words*WSIZE; 
 	if ((int)(bp = mem_sbrk(size)) < 0) 
	       	return NULL; 
   	
	//Make a new Empty free block's header and footer
	PUT(HDRP(bp), PACK(size, 0)); 
	PUT(FTRP(bp), PACK(size, 0)); 
	//Make new Prologue
	PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); 

	return coalesce(bp);
} 

static void *coalesce(void *bp) { 
	size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); 
	size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); 
	size_t size = GET_SIZE(HDRP(bp));

	//Both are alloacted 
	if (prev_alloc && next_alloc) { 
		return bp;  
	//Previous allocated and next unallocated
	}else if (prev_alloc && !next_alloc) {
		size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
                PUT(HDRP(bp), PACK(size, 0));
                PUT(FTRP(bp), PACK(size, 0));
	//Previos unallocated and next allocated
	}else if (!prev_alloc && next_alloc) {
		size += GET_SIZE(HDRP(PREV_BLKP(bp))); 
		PUT(FTRP(bp), PACK(size, 0)); 
		PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
	 	bp = PREV_BLKP(bp); 
	//Previous and next both are unallocated
   	}else {
	//	printf("There is some Error!Cannot be Coalesced\n");
		size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
		PUT();	
	}
	return bp; 
}



