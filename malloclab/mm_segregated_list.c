/*
   mm.c - Malloc package using segregated free list.

   Chunk details:
    Chunks of memory are maintained using a `boundary tag' method as
    described in e.g., Knuth or Standish.  (See the paper by Paul
    Wilson ftp://ftp.cs.utexas.edu/pub/garbage/allocsrv.ps for a
    survey of such techniques.)  Sizes of free chunks are stored both
    in the front of each chunk and at the end.  This makes
    consolidating fragmented chunks into bigger chunks very fast.  The
    size fields also hold bits representing whether chunks are free or
    in use.

    An allocated chunk looks like this:
    
        header-> +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
	    |             Size of chunk, in bytes                         |A|
        mem-> +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	    |             User data starts here...                          .
	    .                                                               .
	    .                                                               .
	    .                                                               |
        footer-> +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
	    |             Same as header(`boundary tag`)                  |A|
	    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

    "header" is the front of the chunk for the purpose of most of the
    malloc code, but "mem" is the pointer that is returned to the user. 

    Chunks always begin on even word boundaries, so the mem portion
    (which is returned to the user) is also on an even word boundary, and
    thus at least double-word aligned.

    Free chunks are stored in doubly-linked lists, and look like this:

        header-> +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
	    |             Size of chunk, in bytes                         |A|
        mem-> +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	    |             Forward pointer to next chunk in free list        |
	    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	    |             Back pointer to previous chunk in free list       |
	    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	    |             Unused space (may be 0 bytes long)                .
	    .                                                               .
	    .                                                               |
        footer-> +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
	    |             Same as header(`boundary tag`)                  |A|
	    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    
    The A (ALLOCATED) bit is set for prologue and epilogue block to help
    determine block's boundary.
    Prologue and epilogue block's size are set to 0.
   
   Heap structure:
        heap start-> +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
        |                 4 btes padding                  |
        free list heads-> +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        | Free list's head address(request size <= 8)     |
        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        | Free list's head address(request size <= 16)    |
        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        |                                                 .
        .    Same as above, request size <= power of 2    .
        .                                                 |
        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	    | Free list's head address(request size > 4096)   |
        prologue header-> +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        |             Size of chunk, in bytes           |A|
        prologue footer->-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        |             Size of chunk, in bytes           |A|
        free and allocated chunks-> +-+-+-+-+-+-+-+-+-+-+-+
	    |                                                 .
	    .                                                 .
        .                                                 |
        epilogue footer->-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	    |             Size of chunk, in bytes           |A|
        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    
    The free list head stores the starting address of each
    free list for differnt size classes, from lower than 8
    byts to bigger than 4096 bytes.
    We have 10 free list entry, so no padding is needed.
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
/*
  Debugging macros:
*/
#define DEBUG 0
#define HEAP_CHECK 0

#if DEBUG == 1
# define DBG_PRINTF(...) printf(__VA_ARGS__)
#else
# define DBG_PRINTF(...)
#endif

#if HEAP_CHECK == 1
#define CHECKHEAP(verbose) mm_checkheap(verbose)
#else
#define CHECKHEAP(verbose) 
#endif


/* 8 bytes alignment in 32bit mode */
#define ALIGNMENT 8

/* Rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

//* Basic constants and macros: */
#define WSIZE      4          /* Word and header/footer size (bytes) */
#define DSIZE      8          /* Doubleword size (bytes) */
#define CHUNKSIZE  (1<<12)    /* Extend heap by this amount (bytes) */
#define MINBLOCKSIZE 16       /* Minimum block size: WORD(HDR) + WORD(FDP) + WORD(BKP) + WORD(FTR) */
#define ALLOCATED 1
#define UNALLOCATED 0

/* Max value of 2 values */
#define MAX(x, y) ((x) > (y) ? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc))

/* Read and write a word at address p. */
#define GET(p)       (*(unsigned int *)(p))
#define PUT(p, val)  (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)   (GET(p) & ~0x7)
#define GET_ALLOC(p)  (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)  ((char *)(bp) - WSIZE)
#define FTRP(bp)  ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, get value of foward and back pointer of that chunk (note those are different from HDRP() adn FTRP()) */
#define FDP(bp)  (*(char **)(bp))
#define BKP(bp)  (*(char **)(bp + WSIZE))

/* Given block ptr bp, set forward and back pointer's value of that block by given value*/
#define SET_FDP(bp, fdp) (FDP(bp) = fdp)
#define SET_BKP(bp, bkp) (BKP(bp) = bkp)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(HDRP(bp)))
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE((char *)(bp) - DSIZE))

/* Given block size, compute the free list offset */
#define LIST_OFFSET(size) \
    (size <= 16   ? 0 :\
     size <= 32   ? 1 :\
     size <= 64   ? 2 :\
     size <= 128  ? 3 :\
     size <= 256  ? 4 :\
     size <= 512  ? 5 :\
     size <= 1024 ? 6 :\
     size <= 2048 ? 7 :\
     size <= 4096 ? 8 :\
     9)

/* Global declarations */
static char *heap_listp;
static char *seglist_start;
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
    if ((heap_listp = mem_sbrk(14*WSIZE)) == (void *) -1)
        return -1;
    
    PUT(heap_listp, 0);                 /* Padding for alignment, user memory sits at 14*4 = 48 Bytes after heap start*/
    PUT(heap_listp + (1*WSIZE), 0);     /* Block size <= 16   */
    PUT(heap_listp + (2*WSIZE), 0);     /* Block size <= 32   */
    PUT(heap_listp + (3*WSIZE), 0);     /* Block size <= 64   */
    PUT(heap_listp + (4*WSIZE), 0);     /* Block size <= 128  */
    PUT(heap_listp + (5*WSIZE), 0);     /* Block size <= 256  */
    PUT(heap_listp + (6*WSIZE), 0);     /* Block size <= 512  */
    PUT(heap_listp + (7*WSIZE), 0);     /* Block size <= 1024 */
    PUT(heap_listp + (8*WSIZE), 0);     /* Block size <= 2048 */
    PUT(heap_listp + (9*WSIZE), 0);     /* Block size <= 4096 */
    PUT(heap_listp + (10*WSIZE), 0);    /* Block size >  4096 */
    PUT(heap_listp + (11*WSIZE), PACK(DSIZE, 1));    /* Prologue header */
    PUT(heap_listp + (12*WSIZE), PACK(DSIZE, 1));    /* Prologue footer */
    PUT(heap_listp + (13*WSIZE), PACK(0, 1));        /* Epilogue header */

    seglist_start = heap_listp + WSIZE;
    DBG_PRINTF("seglist_start: %p\n", seglist_start);

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
    CHECKHEAP(0);
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
    CHECKHEAP(0);
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
    DBG_PRINTF("Entering mm_realloc(%p, %zu)\n", ptr, size);
    CHECKHEAP(0);
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
        delete(prev_bp);
        PUT(HDRP(prev_bp), PACK(current_size, 0));
        PUT(FTRP(bp), PACK(current_size, 0));
        insert(prev_bp);
        return prev_bp;
    }
    /* 
     * Case 2, next block is free, we need to delete next block from free list.
     */
    else if (prev_alloc && !next_alloc) {
        DBG_PRINTF("merge next(%p)\n", next_bp);
        current_size += GET_SIZE(HDRP(next_bp));
        /* Delete next block from free list */
        delete(next_bp);
        /* setup merged block */
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
        current_size += GET_SIZE(HDRP(prev_bp));
        current_size += GET_SIZE(FTRP(next_bp));
        /* Delete prev and next block from free list */
        delete(prev_bp);
        delete(next_bp);
        /* setup merged block */
        PUT(HDRP(prev_bp), PACK(current_size, 0));
        PUT(FTRP(next_bp), PACK(current_size, 0));
        insert(prev_bp);
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

    /* Coalesce if the previous block was free */
    return coalesce(bp);
}

/* find_fit - Find freeblock that fits the request size and return it's bp */
static void *find_fit(size_t asize)
{
    DBG_PRINTF("Entering find_fit(%zu), ", asize);
    /* Traverse all the free lists */
    for ( int i = 0; i < 10; i++) {
        DBG_PRINTF("LIST_OFFSET: %d\n", i);
        freelist_headp = *(char **)(seglist_start + i * WSIZE);
        /* Traverse free list */
        for(void* current = freelist_headp; current != NULL; current = FDP(current)) {
            if (GET_SIZE(HDRP(current)) >= asize) {
                DBG_PRINTF("found %p, size: %u\n", current, GET_SIZE(HDRP(current)));
                return current;
            }
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

    /* Locate freelist */
    char **ptr_to_freelist_headp = (char **)(seglist_start + LIST_OFFSET(GET_SIZE(HDRP(bp))) * WSIZE);
    DBG_PRINTF("LIST_OFFSET: %d\n", LIST_OFFSET(GET_SIZE(HDRP(bp))));
    freelist_headp = *ptr_to_freelist_headp;

    /* List is NULL, make bp as list head */
    if (freelist_headp == NULL) {
        DBG_PRINTF("Free list is NULL, make %p the head of free list\n", bp);
        SET_FDP(bp, NULL);
        SET_BKP(bp, NULL);
    }
    /* insert */
    else {
        DBG_PRINTF("Free list not null,  %p will be new head\n", bp);
        /* Set up current block */
        SET_FDP(bp, freelist_headp);
        SET_BKP(bp, 0);
        /* Set up next block */
        SET_BKP(freelist_headp, bp);
    }

    /* Free list head is np now */
    *ptr_to_freelist_headp = bp;
    return;
}
/*
 * delete - Remove given block pointer from free list.
 * delete() is called by place() or coalesce();
 * we dont' call delete() when there's no free block, so what we deleted is what we inserted.
 */
static void delete(void* bp)
{
    DBG_PRINTF("Entering delete(%p)\n", bp);

    /* Locate freelist */
    char **ptr_to_freelist_headp = (char **)(seglist_start + LIST_OFFSET(GET_SIZE(HDRP(bp))) * WSIZE);
    DBG_PRINTF("LIST_OFFSET: %d\n", LIST_OFFSET(GET_SIZE(HDRP(bp))));
    /* Only one free block */
    if(BKP(bp) == NULL && FDP(bp) == NULL) {
        DBG_PRINTF("Only one free block\n");
        *ptr_to_freelist_headp = NULL;
    }
    /* More than one free block, delete the first block */
    else if (BKP(bp) == NULL) {
        DBG_PRINTF("More than one free block, delete the first block\n");
        /* Fix free list head */
        SET_BKP(FDP(bp), NULL);
        DBG_PRINTF("New list head: %p\n", FDP(bp));
        *ptr_to_freelist_headp = FDP(bp);
    }
    /* More than one free block, delete the last block */
    else if (FDP(bp) == NULL) {
        DBG_PRINTF("More than one free block, delete the last block\n");
        SET_FDP(BKP(bp), NULL);
    }
    /* More than two free block, delete the middle block */
    else {
        DBG_PRINTF("More than two free block, delete the middle block\n");
        SET_FDP(BKP(bp), FDP(bp));
        SET_BKP(FDP(bp), BKP(bp));
    }
}

static void check_freelist()
{
    printf("---------------CHECK FREE LIST START----------------------\n");
    for ( int i = 0; i < 10; i++) {
        DBG_PRINTF("LIST_OFFSET: %d\n", i);
        freelist_headp = *(char **)(seglist_start + i * WSIZE);
        printf("freelist_headp: %p\n", freelist_headp);
        char *cur, *next;
        if ((cur = freelist_headp) != NULL)
        {
            printf("Free List: %p<-", BKP(cur));
            while(cur != NULL)
            {
                next = FDP(cur);
                printf("%p(size: %u)->", cur, GET_SIZE(HDRP(cur)));
                /* Check if next free block points to us */
                if(next != NULL && BKP(next) != cur) {
                    printf("\nNext free block does not point to current block!\n");
                    exit(1);
                }
                cur = next;
            }
            printf("%p\n", NULL);
        }
        else
        {
            printf("Free List is NULL.\n");
        }
    }

    printf("--------------- CHECK FREE LIST END ----------------------\n");
}

static void mm_checkheap(int verbose)
{
    check_freelist();
    /* 
     * What we need to check:
     * Check epilogue and prologue blocks
     * Check each block’s address alignment
     * Check heap boundaries
     * Check each block’s header and footer: size(minimum size, slignment), previous/net allocate/free bit consistency, header and footer matching each other
     * Check coalescing: no two consecutive free blocks in the heap
     */
    char *header_start = heap_listp + 11 * WSIZE; /* Prologue header */
    /* Check prologue header */
    if (GET_SIZE(header_start) != DSIZE || GET_ALLOC(header_start) != 1) {
        printf("Prologue Header Malformed. Size: %d, Alloc: %d\n", GET_SIZE(header_start), GET_ALLOC(header_start));
        exit(1);
    }
    if (GET_SIZE(header_start + WSIZE) != DSIZE || GET_ALLOC(header_start + WSIZE) != 1) {
        printf("Prologue Footer Malformed.\n");
        exit(1);
    }

    /* Traverse blocks, check status */
    char *header = header_start + DSIZE;
    while ( GET_SIZE(header) != 0) {
        /* Alignment */
        if((unsigned long)(header + WSIZE) % ALIGNMENT) {
            printf("Address not aligned!\n");
            exit(1);
        }
        /* No adjacent free block */
        char *next = header + GET_SIZE(header);
        if (!GET_ALLOC(header) && !GET_ALLOC(next)) {
                printf("Adjacent free block!\n");
                printf("Current header: %p, size: %d alloc: %dx\n", header, GET_SIZE(header), GET_ALLOC(header));
                printf("Next header: %p, size: %d alloc: %dx\n", header, GET_SIZE(next), GET_ALLOC(next));
                check_freelist();
                exit(1);
        }
        header = next;
    }
}
