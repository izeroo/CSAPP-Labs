/*
 * mm.c - Malloc package using explicit free list.
 *        Newly freed block will be inserted to the head of free list.
 */
#include <stddef.h>
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

// #define DEBUG
#ifdef DEBUG
# define DBG_PRINTF(...) printf(__VA_ARGS__)
# define CHECKHEAP(verbose) mm_checkheap(verbose)
#else
# define DBG_PRINTF(...)
# define CHECKHEAP(verbose)
#endif


/* 8 bytes alignment in 32bit mode */
#define ALIGNMENT 8

/* Rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

//* Basic constants and macros: */
#define WSIZE      4          /* Word and header/footer size (bytes) */
#define DSIZE      8          /* Doubleword size (bytes) */
#define CHUNKSIZE  (1<<12)    /* Extend heap by this amount (bytes) */
#define MINBLOCKSIZE 16

/* Max value of 2 values */
#define MAX(x, y) ((x) > (y) ? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc))

/* Read and write a word at address p. */
#define GET(p)       (*(unsigned int *)(p))
#define PUT(p, val)  (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)        (GET(p) & ~0x7)
#define GET_ALLOC(p)       (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)  ((char *)(bp) - WSIZE)
#define FTRP(bp)  ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of foward and back pointer field (foward and back blocks are logically linked) */
#define FDP(bp)  (*(char **)(bp + WSIZE))
#define BKP(bp)  (*(char **)(bp))

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(HDRP(bp)))
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE((char *)(bp) - DSIZE))

/* Set BK and FD field of a block by given pointer */
#define SET_BKP(bp, bkp) (BKP(bp) = bkp)
#define SET_FDP(bp, fdp) (FDP(bp) = fdp)

/* Global declarations */
static char *heap_listp;
static char *freelist_headp;

/* Function prototypes for internal helper routines */
static void *coalesce(void *bp);
static void *extend_heap(size_t words);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);
static void insert(void *bp); /* insert a free block to free list */
static void delete(void *bp); /* delete a free block from free list */
static void mm_checkheap(int verbose);

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *) -1)
        return -1;
    PUT(heap_listp, 0);                             /* alignment padding */
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));    /* Prologue header */
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));    /* Prologue footer */
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));    /* Epilogue header */

    freelist_headp = NULL;

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL) /* set free list head to the first free block */
        return -1;
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *             Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    DBG_PRINTF("Entering mm_malloc(%zu)\n", size);
    size_t asize; /* Adjusted block size */
    size_t extendsize; /* Amount to extend heap if not fit */
    char *bp;

    /* Ignore spurious requests */
    if (size == 0) {
        return NULL;
    }

    /* Adjust block size to include overhead and alignment reqs */
    if (size < DSIZE)
        asize = 2 * DSIZE; /* Minimum block size: WORD(HDR) + DWORD(Payload) + WORD(FTR) */
    else
        asize = ALIGN(size + 8);

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
    DBG_PRINTF("Entering mm_free(%p)\n", bp);
    /* Modify header and footer then coalesce the block and insert it into free list */
    PUT(HDRP(bp), PACK(GET_SIZE(HDRP(bp)),0));
    PUT(FTRP(bp), PACK(GET_SIZE(HDRP(bp)),0));
    coalesce(bp);
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
    size = GET_SIZE(HDRP(oldptr));
    copySize = GET_SIZE(HDRP(newptr));
    if (size < copySize)
        copySize = size;
    memcpy(newptr, oldptr, copySize-WSIZE);
    mm_free(oldptr);
    return newptr;
}

/*
 * coalesce - Merge freed block if necessary.
 * Return: Pointer of the merged block.
 */
static void* coalesce(void* bp)
{
    DBG_PRINTF("Entering coalesce(%p), ", bp);

    /* 
     * We have to save our adjacent block's address.
     * After `PUT(HDRP(bp), PACK(newsize ,0))` will render NEXT_BLKP(bp) nonsense.
     * It's really nasty and takes me a lot of time to debug.
     */
    void* prev_bp = PREV_BLKP(bp);
    void* next_bp = NEXT_BLKP(bp);

    size_t prev_alloc = GET_ALLOC(FTRP(prev_bp));
    size_t next_alloc = GET_ALLOC(HDRP(next_bp));

    size_t current_size = GET_SIZE(HDRP(bp));

    /* Case 0: no need to coalesce */
    if (prev_alloc && next_alloc) {
        DBG_PRINTF("no merge\n");
        insert(bp); 
        return bp; 
    }
    /* 
     * Case 1, previous block is free
     */
    else if (!prev_alloc && next_alloc) {
        DBG_PRINTF("merge prev(%p)\n", prev_bp);
        /* setup merged block */
        current_size += GET_SIZE(HDRP(prev_bp));
        PUT(HDRP(prev_bp), PACK(current_size, 0));
        PUT(FTRP(bp), PACK(current_size, 0));
        return prev_bp;
    }
    /* 
     * Case 2, next block is free, we need to delete next block from free list.
     */
    else if (prev_alloc && !next_alloc) {
        DBG_PRINTF("merge next(%p)\n", next_bp);
        /* Delete next block from free list */
        delete(next_bp);
        /* setup merged block */
        current_size += GET_SIZE(HDRP(next_bp));
        PUT(HDRP(bp), PACK(current_size, 0));
        PUT(FTRP(bp), PACK(current_size, 0));
        insert(bp);
        return bp;
    }
    /* 
     * Case 3, previous and next block both are free, we need to delete next block from free list.
     */
    else {
        DBG_PRINTF("merge prev(%p) and next(%p)\n", prev_bp, next_bp);
        /* Delete next block from free list */
        delete(next_bp);
        /* setup merged block */
        current_size += GET_SIZE(HDRP(prev_bp));
        current_size += GET_SIZE(FTRP(next_bp));
        PUT(HDRP(prev_bp), PACK(current_size, 0));
        PUT(FTRP(next_bp), PACK(current_size, 0));
        return prev_bp;
    }   
}

/*
 * extend_heap - Extend the heap with a free block and coalesce the new free block if necessary.
 * Return : new free block's payload address.
 */
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* Allocate even number of words to maintain alignment */
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;
    
    /* Initialize free block header/footer and the epilogue header
     * After mem_srbk(size), we are always at the end of epilogue footer, 
     * so we need to change the epilogue ftr to normal header and setup epilogue footer at the end of heap.
     */
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));
    // SET_BKP(bp, NULL);
    // SET_FDP(bp, NULL);
    /* Coalesce if the previous block was free */
    return coalesce(bp);
}

/* find_fit - Find freeblock that fits the request size and return it's bp */
static void *find_fit(size_t asize)
{
    DBG_PRINTF("Entering find_fit(%zu), ", asize);
    /* Free list initailzed in mm_init(), freelist_headp points to the payload of free block */
    /* Traverse free list */
    for(void* current = freelist_headp; current != NULL; current = FDP(current)) {
        if (GET_SIZE(HDRP(current)) >= asize) {
            DBG_PRINTF("found %p, size: %u\n", current, GET_SIZE(HDRP(current)));
            return current;
        }
    }
    /* Not found */
    DBG_PRINTF("not found\n");
    return NULL;
}

/* place - Place requested block in current free block, split if necessary */
static void place(void *bp, size_t asize)
{
    DBG_PRINTF("Entering place(%p)",bp);
    delete(bp);

    size_t size = GET_SIZE(HDRP(bp));
    
    if ((size - asize) >= MINBLOCKSIZE)
    {
        /* set up current block */
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        DBG_PRINTF("split: %p and %p\n", bp, NEXT_BLKP(bp));

         /* set up remain block */
        PUT(HDRP(NEXT_BLKP(bp)), PACK(size-asize, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size-asize, 0));
        coalesce(NEXT_BLKP(bp));
    }
    else {
        DBG_PRINTF("no split\n");
        /* Waste some space, we have no other way */
        PUT(HDRP(bp), PACK(size, 1));
        PUT(FTRP(bp), PACK(size, 1));
    }

}

/*
 * insert - Insert given block pointer to the head of free list.
 * insert() is called by free() or place()
 */
static void insert(void* bp)
{
    DBG_PRINTF("Entering insert(%p)\n", bp);
    if (freelist_headp == NULL) {
        DBG_PRINTF("Free list is NULL, make %p the head of free list\n", bp);
        freelist_headp = bp;
        SET_FDP(bp, NULL);
        SET_BKP(bp, NULL);
        CHECKHEAP(0);
        return;
    }
    /* Set up current block */
    SET_FDP(bp, freelist_headp);
    SET_BKP(bp, 0);
    /* Set next block */
    SET_BKP(freelist_headp, bp);
    /* Free list head is np now */
    freelist_headp = bp;
    CHECKHEAP(0);
}
/*
 * delete - Remove given block pointer from free list.
 * delete() is called by place() or coalesce();
 * we dont' call delete() when there's no free block, so what we deleted is what we inserted.
 */
static void delete(void* bp)
{
    DBG_PRINTF("Entering delete(%p)\n", bp);
    /* Only one free block */
    if(BKP(bp) == NULL && FDP(bp) == NULL) {
        freelist_headp = NULL;
    }
    /* More than one free block, delete the first block */
    else if (BKP(bp) == NULL) {
        /* Fix free list head */
        SET_BKP(FDP(bp), NULL);
        freelist_headp = FDP(bp);
    }
    /* More than one free block, delete the last block */
    else if (FDP(bp) == NULL) {
        SET_FDP(BKP(bp), NULL);
    }
    /* More than two free block, delete the middle block */
    else {
        SET_FDP(BKP(bp), FDP(bp));
        SET_BKP(FDP(bp), BKP(bp));
    }
    CHECKHEAP(0);
}

static void mm_checkheap(int verbose)
{
    DBG_PRINTF("---------------CHECK HEAP START---------------------\n");
    DBG_PRINTF("freelist_headp: %p\n", freelist_headp);
    if (freelist_headp) {
        DBG_PRINTF("Free List: %p<-", BKP(freelist_headp));
        for (char * tmp = freelist_headp; tmp != NULL; tmp = FDP(tmp)) {
            DBG_PRINTF("%p(size: %u)->", tmp, GET_SIZE(HDRP(tmp)));
        }
        DBG_PRINTF("%p\n", NULL);
    }
    DBG_PRINTF("---------------CHECK HEAP END----------------------\n");
}