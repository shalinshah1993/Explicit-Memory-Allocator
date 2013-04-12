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


#define MAX(a,b) (a > b) ? a : b
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

/*Given the pointer of a free block get the address of next/prev block*/

#define GET_PREV_FREE_BLKP(bp) ((int *)(bp))
#define GET_NEXT_FREE_BLKP(bp) ((int *)((char *)bp + WSIZE)) 
#define GET_NEXT_FREE_BLKP_VAL(bp) *((int *)((char *)bp + WSIZE))
#define GET_PREV_FREE_BLKP_VAL(bp) *((int *)bp) 

/*heap_listp is the beginning of the heap pointer and head is the beginning of the List and tail points to last block in the list*/
char *heap_listp;
char *head,*tail;

//This are the prototypes of the functions used in the code as supporting routines
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
void *mm_malloc(size_t size);
static void place(void *bp, size_t asize);
static void *find_fit(size_t asize);

/* 
 mm_init - initialize the malloc package.
*/
int mm_init(void)
{
	heap_listp = mem_heap_lo();
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

	//Heap is created of this much size ie 1K
	if (extend_heap(CHUNKSIZE/WSIZE) == NULL) 
		return -1; 

	//Head is pointing to the first Free Block in the List 
	head = heap_listp + DSIZE;	
	tail = head;

	return 0;
}

/*malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
  /*  int newsize = ALIGN(size + SIZE_T_SIZE);
    void *p = mem_sbrk(newsize);
    if (p == (void *)-1)
	return NULL;
    else {
        *(size_t *)p = size;
        return (void *)((char *)p + SIZE_T_SIZE);
    }*/

	size_t asize,extendsize; 	 /* adjusted block size */
	char *bp;  	                 /* amount to extend heap if no fit */

	if (size <= 0) 
		return NULL;  
	//If the requested size is less than word size then simply include padding and ask for that size or else apply some formula to find size 
	if (size <= DSIZE)           
		 asize = DSIZE+OVERHEAD; 
	else 
		 asize = DSIZE*((size+(OVERHEAD)+(DSIZE-1))/DSIZE); 

	if ((bp = find_fit(asize)) != NULL) { 
		place(bp, asize); 
		return bp; 
	}
	//If not enough space is available in the heap then extend the heap first and then place it
	extendsize = MAX(asize,CHUNKSIZE); 
	if ((bp = extend_heap(extendsize/WSIZE)) == NULL) 	
		return NULL; 
	place(bp, asize); 
	return bp; 
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
	size_t size = GET_SIZE(HDRP(ptr)); 
	int *temp,*temp1,*prev,*next;
	
	temp = (int *)ptr;	
	prev = GET_PREV_FREE_BLKP(ptr);

	//Simply free the allocated block and coalesce it accordindly depending on whether next/prev blocks are free/allocated
	PUT(HDRP(ptr), PACK(size, 0)); 
   	PUT(FTRP(ptr), PACK(size, 0)); 
	
	 //And also set next free block field to NULL since this is the last block
        next = GET_NEXT_FREE_BLKP(ptr);
        
        //Also add this free block to the list of free blocks so find the previous free block in the list and insert this one in between
	while(GET_ALLOC(HDRP(PREV_BLKP(temp)))){
		if(GET_SIZE(HDRP(PREV_BLKP(temp))) == 8)
			break;
		else
			temp = (int *)PREV_BLKP(temp);
	}
		
//After the loop ends temp still points to block after the free block or the first block after prologue so go one step back and also set next pointer of previous block to this free block which is what temp1 does
	if(!GET_ALLOC(HDRP(PREV_BLKP(temp)))){
                temp = (int *)PREV_BLKP(temp);
                temp1 = GET_NEXT_FREE_BLKP(temp);
		*next = *temp1;				
                *temp1 = (int *)ptr;
		*prev = temp;
	//If there is no free block before this one then check for the free block after ptr
        }else{
		*prev = NULL;
		temp = (int *)ptr;
        	while(GET_ALLOC(HDRP(NEXT_BLKP(temp)))){
			temp = (int *)NEXT_BLKP(temp);
		}
		//Set the next to next free blk found and prev of that blkp to ptr
		if(!GET_ALLOC(HDRP(NEXT_BLKP(temp))) && (GET_SIZE(HDRP(NEXT_BLKP(temp)) != 0))){
			temp = (int *)NEXT_BLKP(temp);
			temp1 = GET_PREV_FREE_BLKP(temp);
			*temp1 = (int *)ptr;
			*next = (int *)temp;
		//There is no free blkp after this or before this so this is the only free blk
		}else{
			*next = NULL;
		}
	}

	return coalesce(ptr); 
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
	int *prev,*temp,*next,*temp1; 	

	size = (words % 2) ? (words+1)*WSIZE : words*WSIZE; 
 	if ((int)(bp = mem_sbrk(size)) < 0) 
	       	return NULL; 
   	
	//Make a new Empty free block's header and footer
	PUT(HDRP(bp), PACK(size, 0)); 
	PUT(FTRP(bp), PACK(size, 0));
 
	//Make the prev and next block pointer 
	prev = GET_PREV_FREE_BLKP(bp);	
	temp = (int *)bp;
	//Run a loop to check for previous free block if not found then simply make it NULL or else set it to there address
	while(GET_ALLOC(HDRP(PREV_BLKP(temp)))){
		if(GET_SIZE(HDRP(PREV_BLKP(temp))) == 8){
			break;
		}else
			temp = (int *)PREV_BLKP(temp);		
	}

//After the loop ends temp still points to block after the free block or the first block after prologue so go one step back and also set next pointer of previous block to this free block which is what temp1 does
	if(!GET_ALLOC(HDRP(PREV_BLKP(temp)))){
		temp = (int *)PREV_BLKP(temp);			
		temp1 = GET_NEXT_FREE_BLKP(temp);
		*temp1 = (int *)bp;
	}else{ 
		temp = GET_PREV_FREE_BLKP(bp);
		*temp = NULL;
	}
	*prev = temp;
	
	//And also set next free block field to NULL since this is the last block
	next = GET_NEXT_FREE_BLKP(bp);
	*next = NULL;

	//Make new Epilogue
	PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); 

	return coalesce(bp);
} 

static void *coalesce(void *bp) { 
	size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); 
	size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); 
	size_t size = GET_SIZE(HDRP(bp));
	char *prev,*next,*head,*ftr;
	int *nextFree,*prevFree;

	//Both are alloacted 
	if (prev_alloc && next_alloc) { 
		return bp;  
	//Previous allocated and next unallocated
	}else if (prev_alloc && !next_alloc) {
	        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
		//First set next free blck pointer of bp to next to next free block and this next to next free blck's prev pointer is set to bp 
                nextFree = GET_NEXT_FREE_BLKP(bp);
                prevFree = GET_NEXT_FREE_BLKP_VAL(NEXT_BLKP(bp));
		*nextFree = *prevFree;
		*prevFree = bp;
		PUT(HDRP(bp), PACK(size, 0));
                PUT(FTRP(bp), PACK(size, 0));			
	//Previos unallocated and next allocated
	}else if (!prev_alloc && next_alloc) {
		size += GET_SIZE(HDRP(PREV_BLKP(bp)));
		//Set prev block's next free block to next free block of bp and prev free block of next free block of bp to prev block of bp
		prev = PREV_BLKP(bp);
		nextFree = GET_NEXT_FREE_BLKP(prev);
		*nextFree = GET_NEXT_FREE_BLKP_VAL(bp);
		prevFree = GET_PREV_FREE_BLKP(GET_NEXT_FREE_BLKP_VAL(bp));
		*prevFree = prev; 
		PUT(FTRP(bp), PACK(size, 0)); 
		PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
	 	bp = PREV_BLKP(bp); 
	//Previous and next both are unallocated
   	}else {
		//Increase the the size to size of 3 blocks
		size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
		//Get the header of prev block and ftr of next block
		prev = PREV_BLKP(bp);
		next = NEXT_BLKP(bp);
		head = HDRP(prev);
		ftr = FTRP(next);
		nextFree = GET_NEXT_FREE_BLKP(prev);
		prevFree = GET_PREV_FREE_BLKP(GET_NEXT_FREE_BLKP_VAL(next));
		//Simply set next free block of prev to next free block of next and vice versa
		*nextFree = GET_NEXT_FREE_BLKP_VAL(next);
		*prevFree = prev;
		//Set the size and allocation bit of header/footer and bp to prev block 
		PUT(head,PACK(size,0));
		PUT(ftr,PACK(size,0));
		bp = prev;
	}
	return bp; 
}

static void *find_fit(size_t asize) { 
	void *bp;
	 
	for (bp = head; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
		if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))
			return bp; 
	}
	return NULL;    /* no fit */
}

static void place(void *bp, size_t asize) {
	 size_t csize = GET_SIZE(HDRP(bp));
	 int *prev,*next,*prev1,*next1,*prevFreeBlk,*nextFreeBlk;	

    	 //If the space allocated has padding greater then a word then split the block into 2 parts or else do place it directly 
	 if ((csize - asize) >= (DSIZE + OVERHEAD)){
		prevFreeBlk = GET_PREV_FREE_BLKP_VAL(bp);
		nextFreeBlk = GET_NEXT_FREE_BLKP_VAL(bp);
		PUT(HDRP(bp), PACK(asize, 1));
		PUT(FTRP(bp), PACK(asize, 1)); 
		bp = NEXT_BLKP(bp); 
		PUT(HDRP(bp), PACK(csize-asize, 0)); 	
		PUT(FTRP(bp), PACK(csize-asize, 0));
		prev = GET_PREV_FREE_BLKP(bp);
		next = GET_NEXT_FREE_BLKP(bp);
		*prev = prevFreeBlk;
		*next = nextFreeBlk;
		prev1 = GET_PREV_FREE_BLKP(nextFreeBlk);
		next1 = GET_NEXT_FREE_BLKP(prevFreeBlk);
		*prev1 = bp;
		*next1 = bp;		
	//If not splitted then simply remove this block from the list of free blocks 
	} else { 
		prevFreeBlk = GET_PREV_FREE_BLKP_VAL(bp);
		nextFreeBlk = GET_NEXT_FREE_BLKP_VAL(bp);
		prev = GET_NEXT_FREE_BLKP(prevFreeBlk);
		next = GET_PREV_FREE_BLKP(nextFreeBlk);
		*prev = nextFreeBlk;
		*next = prevFreeBlk;
		PUT(HDRP(bp), PACK(csize, 1)); 
		PUT(FTRP(bp), PACK(csize, 1)); 
	} 
}


