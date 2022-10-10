/*
 * mm.c - Malloc package using implicit free list. (without footer in allocated block)
 *        Free block's header stores its allocation status in the lowest bit)
 *        and its previous block's allocation status in the second lowest bit).      
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

/* 8 bytes alignment in 32bit mode */
#define ALIGNMENT 8

/* Rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

//* Basic constants and macros: */
#define WSIZE      4          /* Word and header/footer size (bytes) */
#define DSIZE      8          /* Doubleword size (bytes) */
#define CHUNKSIZE  (1<<12)    /* Extend heap by this amount (bytes) */
#define MINBLOCKSIZE 8        /* Minimum block size: WORD(HDR) + WORD(FTR) */

/* Max value of 2 values */
#define MAX(x, y) ((x) > (y) ? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc)) /* `alloc` could be 0, 1, 2 or 3 */

/* Read and write a word at address p. */
#define GET(p)       (*(unsigned int *)(p))
#define PUT(p, val)  (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)   (GET(p) & ~0x7) /* Take attention!!! size = header + payload + footer */
#define GET_ALLOC(p)  (GET(p) & 0x1)
#define GET_PREV_ALLOC(p)  (GET(p) & 0x2)
#define GET_PREALLOC(p) (GET(p) & 0x2)

/* Set and clear prev_alloc status at adress p*/
#define SET_PREV_ALLOC(p) (PUT(p, GET(p) | 0x2))
#define CLR_PREV_ALLOC(p) (PUT(p, GET(p) & ~0x2))

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)  ((char *)(bp) - WSIZE)
#define FTRP(bp)  ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(HDRP(bp)))
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE((char *)(bp) - DSIZE))

/* Global declarations */
static char *heap_listp, *prev_listp;

/* Function prototypes for internal helper routines */
static void *coalesce(void *bp);
static void *extend_heap(size_t words);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);


/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *) -1)
        return -1;
    PUT(heap_listp, 0);                             /* alignment padding */
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 3));    /* Prologue header */
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 3));    /* Prologue footer */
    PUT(heap_listp + (3*WSIZE), PACK(0, 3));    /* Epilogue header */
    heap_listp += (2*WSIZE);    /* make heap_listp points to epilogue header */
    prev_listp = heap_listp;

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *             Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize; /* Adjusted block size */
    size_t extendsize; /* Amount to extend heap if not fit */
    char *bp;

    /* Ignore spurious requests */
    if (size == 0) {
        return NULL;
    }

    /* Adjust block size to include overhead and alignment reqs */
    if (size < WSIZE)
        asize = MINBLOCKSIZE;
    else
        asize = ALIGN(size + 4);

    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    /* No fit. Get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}

/*
 * mm_free - Freeing a block and coalesce if necessary.
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, GET_PREV_ALLOC(HDRP(bp))));
    PUT(FTRP(bp), PACK(size, GET_PREV_ALLOC(HDRP(bp))));
    
    coalesce(bp);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    if (ptr == NULL)
       return mm_malloc(size);
    if (size == 0) 
       mm_free(ptr);

    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    newptr = mm_malloc(size);
    if (newptr == NULL)
        return NULL;
    size = GET_SIZE(HDRP(oldptr));
    copySize = GET_SIZE(HDRP(newptr));
    if (size < copySize)
        copySize = size;
    memcpy(newptr, oldptr, copySize-WSIZE);
    mm_free(oldptr);
    return newptr;
}

/*
 * coalesce - Merge freed block.
 */
static void* coalesce(void* bp)
{
    void* prev_bp = PREV_BLKP(bp);
    void* next_bp = NEXT_BLKP(bp);

    size_t prev_alloc = GET_PREV_ALLOC(HDRP(bp)); /* Now prev allocation status is in our current header */
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {

    }
    else if (prev_alloc && !next_alloc) {
        size += GET_SIZE(HDRP(next_bp));
        PUT(FTRP(next_bp), PACK(size, 2));
        PUT(HDRP(bp), PACK(size, 2));
    }
    else if (!prev_alloc && next_alloc) {
        size += GET_SIZE(HDRP(prev_bp));
        PUT(FTRP(bp), PACK(size, 2));
        PUT(HDRP(prev_bp), PACK(size, 2)); /* prev is free, PREV_BLKP(prev) must be allocated */
        bp = prev_bp;
    }
    else {
        size += GET_SIZE(HDRP(prev_bp)) +
                GET_SIZE(FTRP(next_bp));
        PUT(FTRP(next_bp), PACK(size, 2));
        PUT(HDRP(prev_bp), PACK(size, 2));
        bp = prev_bp;
    }
    CLR_PREV_ALLOC(HDRP(NEXT_BLKP(bp)));
    prev_listp = bp;
    return bp;

}

/*
 * extend_heap - Extend the heap with a free block and return that block's payload address.
 */
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;
    
    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, GET_PREV_ALLOC(HDRP(bp))));
    PUT(FTRP(bp), PACK(size, GET_PREV_ALLOC(HDRP(bp))));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    /* Coalesce if the previous block was free */
    return coalesce(bp);
}

/* Find freeblock that fits the request size and return it's bp */
static void *find_fit(size_t asize)
{
    for (char* bp = prev_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
    {
        if (!GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) >= asize)
        {
            prev_listp = bp;
            return bp;
        }
    }

    for (char* bp = heap_listp; bp != prev_listp; bp = NEXT_BLKP(bp))
    {
        if (!GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) >= asize)
        {
            prev_listp = bp;
            return bp;
        }
    }
    return NULL;
}

/* Place requested block in a free block, split if necessary */
static void place(void *bp, size_t asize)
{
    size_t blk_size = GET_SIZE(HDRP(bp));
    size_t remain_size = blk_size - asize;
    
    /* split if we have space space */
    if ( remain_size >= MINBLOCKSIZE) {
        /* |4 Byte HDR| Payload | 4 Bytes FTR| */
        /* set up malloced block */
        PUT(HDRP(bp), PACK(asize, 3));
        /* set up spilited block */
        PUT(HDRP(NEXT_BLKP(bp)), PACK(remain_size, 2));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(remain_size, 2));
        /* Clear alloc status of next */
        CLR_PREV_ALLOC(HDRP(NEXT_BLKP(NEXT_BLKP(bp))));
    }
    else {
        /* set up malloced block */
        PUT(HDRP(bp), PACK(blk_size, 3));
        /* Set alloc status of next block */
        SET_PREV_ALLOC(HDRP(NEXT_BLKP(bp)));
    }
    prev_listp = bp;
}