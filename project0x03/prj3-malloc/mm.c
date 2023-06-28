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
 * provide your information in the following struct.
 ********************************************************/
team_t team = {
    /* Your student ID */
    "20182186",
    /* Your full name*/
    "Seungwon Kim",
    /* Your email address */
    "sprauncy76@gmail.com",
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8 
#define WSIZE       4       /* Word and header/footer size (bytes) */
#define DSIZE       8       /* Doubleword size (bytes) */
#define CHUNKSIZE  (1<<12)  /* Extend heap by this amount (bytes) */ 


#define INITCHUNKSIZE 64
#define MIN_BLOCK_SIZE 16

/* Return Max & min value between 'x' and 'y' */
#define MAX(x, y) ((x) > (y)? (x) : (y)) 
#define MIN(x, y) ((x) < (y)? (x) : (y)) 

/* Rounds up 'size' to the nearest multiple of ALIGNMENT.
   The ALIGNMENT value is assumed to be a power of 2.
   This is achieved by adding ALIGNMENT-1 to the 'size' and then
   masking off the lower bits to make it a multiple of ALIGNMENT. */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7) 
/* Calculates the size of size_t, rounded up to the nearest multiple of ALIGNMENT.
   It uses the ALIGN macro to perform the rounding. */
#define SIZE_T_SIZE (ALIGN(sizeof(size_t))) 


/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc)) 

/* Read and write a word at address p */
#define GET(p)       (*(unsigned int *)(p)) 
#define PUT(p, val)  (*(unsigned int *)(p) = (val)) 

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~0x7) 
#define GET_ALLOC(p) (GET(p) & 0x1) 


/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp) ((char *)(bp) - WSIZE) 
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) 

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* Set the value of the target pointer 'p' to 'ptr' */
#define TARGET_PTR(p, ptr) (*(unsigned int *)(p) = (unsigned int)(ptr))

/* Get the next, previous pointer from a free block pointer 'ptr' */
#define GET_NEXT_PTR(ptr) ((char *)(ptr))
#define GET_PREV_PTR(ptr) ((char *)(ptr) + WSIZE)

/* Get the next, previous free block pointer from a free block pointer 'ptr' in the segregated free list */
#define SEGLIST_NEXT(ptr) (*(char **)(ptr))
#define SEGLIST_PREV(ptr) (*(char **)(GET_PREV_PTR(ptr)))

/* Size of buffer to be added for realloc requests */
#define BUFFER_SIZE_4_REALLOC 128

/* Function prototypes for internal helper routines */
static void *extend_heap(size_t words); 
static void *coalesce(void *bp);
static void *place(void *bp, size_t asize);
static void insert_node(void* ptr, size_t size);
static void delete_node(void *ptr);

/* global variable */
void *exp_listp = 0; 

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    static char *heap_listp;

    exp_listp = NULL;
    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1) return -1;

    PUT(heap_listp,0);                            /* Alignment padding */
    PUT(heap_listp + (WSIZE * 1),PACK(DSIZE,1));  /* Prologue header */
    PUT(heap_listp + (WSIZE * 2),PACK(DSIZE,1));  /* Prologue footer */
    PUT(heap_listp + (WSIZE * 3),PACK(0,1));      /* Epilogue header */
    heap_listp += (2*WSIZE);
    /* Extend the empty heap with a free block of INITCHUNKSIZE bytes */
    if (extend_heap(INITCHUNKSIZE) == NULL) 
	    return -1;
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    if (size == 0)
        return NULL;

    size_t adj_block_size = ALIGN(size + SIZE_T_SIZE); /* Adjusted block size */
    size_t extendsize;
    void *best_fit = NULL;  /* Best fit block */
    size_t best_fit_size = (size_t)-1;  /* Size of the best fit block */
    void *ptr = exp_listp;

    /* Search the best fit block in the free list */
    while (ptr != NULL) {
        size_t block_size = GET_SIZE(HDRP(ptr));
        if (!GET_ALLOC(HDRP(ptr)) && block_size >= adj_block_size) {
            /* If the current block size is smaller than the current best fit block size, update the best fit block */
            if (block_size < best_fit_size) {
                best_fit = ptr;
                best_fit_size = block_size;
            }
            /* If the best fit block size is exactly the same as the adjusted block size, break the loop */
            if (best_fit_size == adj_block_size)
                break;
        }
        ptr = SEGLIST_NEXT(ptr);
    }

    /* If no suitable block was found, extend the heap */
    if (best_fit == NULL) {
        extendsize = MAX(adj_block_size, CHUNKSIZE);
        if ((best_fit = extend_heap(extendsize)) == NULL)
            return NULL;
        best_fit_size = GET_SIZE(HDRP(best_fit));
    }
    best_fit = place(best_fit, adj_block_size);
    return best_fit;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr), PACK(size,0));
    PUT(FTRP(ptr), PACK(size,0));
    insert_node(ptr, size);
    coalesce(ptr);
    return;
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    if (ptr == NULL) {
        // If ptr is NULL, treat it as equivalent to mm_malloc(size)
        return mm_malloc(size);
    }

    if (size == 0) {
        // If size is 0, treat it as equivalent to mm_free(ptr)
        mm_free(ptr);
        return NULL;
    }

    size_t adj_size = size;
    if (adj_size <= DSIZE)
        adj_size = 2 * DSIZE;
    else
        adj_size = ALIGN(size + DSIZE);

    size_t cur_size = GET_SIZE(HDRP(ptr));

    if (cur_size >= adj_size) {
        size_t remainder = cur_size - adj_size;
        if (remainder >= MIN_BLOCK_SIZE) {
            // Split the block if there is enough space
            PUT(HDRP(ptr), PACK(adj_size, 1));
            PUT(FTRP(ptr), PACK(adj_size, 1));

            void *next_block = NEXT_BLKP(ptr);
            PUT(HDRP(next_block), PACK(remainder, 0));
            PUT(FTRP(next_block), PACK(remainder, 0));

            insert_node(next_block, remainder);
        }
        return ptr;
    } 
    else {
        void *next_block = NEXT_BLKP(ptr);
        size_t next_size = GET_SIZE(HDRP(next_block));

        if (!GET_ALLOC(HDRP(next_block)) && (cur_size + next_size) >= adj_size) {
            // Combine the current block and the next block
            delete_node(next_block);

            size_t combined_size = cur_size + next_size;
            size_t remainder = combined_size - adj_size;

            if (remainder >= MIN_BLOCK_SIZE) {
                // Split the combined block if there is enough space
                PUT(HDRP(ptr), PACK(adj_size, 1));
                PUT(FTRP(ptr), PACK(adj_size, 1));

                void *split_block = NEXT_BLKP(ptr);
                PUT(HDRP(split_block), PACK(remainder, 0));
                PUT(FTRP(split_block), PACK(remainder, 0));

                insert_node(split_block, remainder);
            } 
            else {
                // Use the entire combined block
                PUT(HDRP(ptr), PACK(combined_size, 1));
                PUT(FTRP(ptr), PACK(combined_size, 1));
            }
            return ptr;
        } 
        else {
            // Allocate a new block and copy the data
            void *new_ptr = mm_malloc(size);
            if (new_ptr == NULL)
                return NULL;
            
            memcpy(new_ptr, ptr, MIN(size, cur_size - DSIZE));
            
            // Free the old block
            mm_free(ptr);

            return new_ptr;
        }
    }
}

static void *extend_heap(size_t words){
    void *bp;
    size_t new_block_size;
    new_block_size = ALIGN(words);
    
    if((long)(bp = mem_sbrk(new_block_size)) == -1)
        return NULL;

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(new_block_size, 0));         /* Free block header */
    PUT(FTRP(bp), PACK(new_block_size, 0));         /* Free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));       /* New epilogue header */
    
    insert_node(bp, new_block_size);
    /* Coalesce if the previous block was free */
    return coalesce(bp);
}

static void *coalesce(void *bp){
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));
    
    if (prev_alloc && next_alloc) {                         /* Case 1 */
        return bp;
    }

    else if (prev_alloc && !next_alloc) {                   /* Case 2 */
        delete_node(bp);
        delete_node(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    
    else if (!prev_alloc && next_alloc) {                   /* Case 3 */
        delete_node(bp);
        delete_node(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    else {                                                  /* Case 4 */
        delete_node(bp);
        delete_node(PREV_BLKP(bp));
        delete_node(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    
    insert_node(bp,size);
    return bp;
}

static void* place(void* bp, size_t block_size) {
    size_t cur_size = GET_SIZE(HDRP(bp));
    size_t remain_size = cur_size - block_size;

    delete_node(bp);

    if (remain_size <= 2 * DSIZE) {
        PUT(HDRP(bp), PACK(cur_size, 1));
        PUT(FTRP(bp), PACK(cur_size, 1));
    }
    else if (block_size >= 100) {
        PUT(HDRP(bp), PACK(remain_size, 0));
        PUT(FTRP(bp), PACK(remain_size, 0));
        
        PUT(HDRP(NEXT_BLKP(bp)), PACK(block_size, 1));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(block_size, 1));
        insert_node(bp, remain_size); 
        return NEXT_BLKP(bp); 
    }
    else {
        PUT(HDRP(bp), PACK(block_size, 1));
        PUT(FTRP(bp), PACK(block_size, 1));
        PUT(HDRP(NEXT_BLKP(bp)), PACK(remain_size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(remain_size, 0));
        insert_node(NEXT_BLKP(bp), remain_size);
    }
    return bp;
}

static void insert_node(void* ptr, size_t size){
    void *cur_node = exp_listp;
    void *prev_node = NULL;

    // Search for the correct position in the free list to insert the node (sorted by size)
    while ((cur_node != NULL) && (size > GET_SIZE(HDRP(cur_node)))) {
        prev_node = cur_node;
        cur_node = SEGLIST_NEXT(cur_node);
    }

    // If we need to insert at the beginning of the list
    if (prev_node == NULL) {
        if (cur_node != NULL) { // If list is not empty
            // Link current block with the first block in the list
            TARGET_PTR(GET_NEXT_PTR(ptr), cur_node);
            TARGET_PTR(GET_PREV_PTR(cur_node), ptr);
            // The previous block of the new block should be NULL
            TARGET_PTR(GET_PREV_PTR(ptr), NULL);
        } else { // If list is empty
            // The new block is the only block in the list. So, its next and previous should point to NULL
            TARGET_PTR(GET_NEXT_PTR(ptr), NULL);
            TARGET_PTR(GET_PREV_PTR(ptr), NULL);
        }
        // Set the new block as the first block
        exp_listp = ptr;
    } 
    else {
        // If we need to insert at the end of the list
        if (cur_node == NULL) {
            // Link the new block with the last block in the list
            TARGET_PTR(GET_NEXT_PTR(ptr), NULL);
            TARGET_PTR(GET_PREV_PTR(ptr), prev_node);
            TARGET_PTR(GET_NEXT_PTR(prev_node), ptr);
        } 
        else { 
            // If we need to insert in the middle of the list
            // Link the new block with the next and previous blocks
            TARGET_PTR(GET_NEXT_PTR(ptr), cur_node);
            TARGET_PTR(GET_PREV_PTR(ptr), prev_node);
            TARGET_PTR(GET_NEXT_PTR(prev_node), ptr);
            TARGET_PTR(GET_PREV_PTR(cur_node), ptr);
        }
    }
    return;
}

static void delete_node(void* ptr) {
    void *next_node = SEGLIST_NEXT(ptr);
    void *prev_node = SEGLIST_PREV(ptr);

    // If the node to be deleted is the only node in the list
    if (next_node == NULL && prev_node == NULL) {
        exp_listp = NULL;
    }
    else {
        // If the node to be deleted is at the beginning of the list
        if (prev_node == NULL) {
            TARGET_PTR(GET_PREV_PTR(next_node), NULL);
            exp_listp = next_node;
        } 
        // If the node to be deleted is at the end of the list
        else if (next_node == NULL) {
            TARGET_PTR(GET_NEXT_PTR(prev_node), NULL);
        } 
        // If the node to be deleted is in the middle of the list
        else {
            TARGET_PTR(GET_NEXT_PTR(prev_node), next_node);
            TARGET_PTR(GET_PREV_PTR(next_node), prev_node);
        }
    }
    return;
}