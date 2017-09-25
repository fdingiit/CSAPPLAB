#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "implicit.h"
#include "memlib.h"
#include "utils.h"

#include "config.h"

/* special block that has no free memory in fact,
 * but it is also valuable, cuz this block may come from
 * a split of a large block, which only left the room
 * for block header and footer. if we need to alloc a free
 * block just after this meaningless block, we could
 * reuse the header and footer of it, which will
 * make us save some bytes.
 *
 * NOTE: if a block has 0 byte, IT MUST BE MARKED AS FREE
 * */
#define MIN_BLK_SIZE    (RB_HDR_SIZE + RB_FTR_SIZE)

#define BLK_FREE    0
#define BLK_ALLOC   1

#define PACK(size, alloc) ((size) | (alloc))    /* pack size with alloc */

#define GET(p)          (*(unsigned int*)(p))
#define SET(p, val)     (*(unsigned int*)(p) = (val))

#define GET_SIZE(p)     (GET(p) & ~0x7)
#define GET_ALLOC(p)    (GET(p) & 0x1)

/* padding block */
#define PADDING_BLK_SIZE 4

/* prologue block */
#define PB_HDR_SIZE 4
#define PB_FTR_SIZE 4

/* epilogue block */
#define EB_HDR_SIZE 4

#define EB_HDRP(p)      ((void*)(p) - EB_HDR_SIZE)          /* epilogue block header pointer */
#define SET_EB(p)       SET(EB_HDRP(p), PACK(0, 1))         /* set epilogue block */
#define EB(p)           (GET_SIZE(EB_HDRP(p)) == 0)         /* does p point to a epilogue block? */

/* regular block */
#define RB_HDR_SIZE 4
#define RB_FTR_SIZE 4

#define RB_ALLOC(p)     (GET_ALLOC(RB_HDRP(p)))                 /* is the block alloced? */
#define RB_SIZE(p)      (GET_SIZE(RB_HDRP(p)))                  /* total size of this block, including header and footer */
#define RB_AVL_SIZE(p)  (RB_SIZE(p) - RB_HDR_SIZE - RB_FTR_SIZE)    /* available memory size */

#define RB_HDRP(p) ((void*)(p) - RB_HDR_SIZE)       /* regular block header pointer */
#define RB_FTRP(p) ((void*)(p) + RB_AVL_SIZE(p))    /* regular block footer pointer */

#define RB_NEXT_HDRP(p) RB_HDRP((void*)(p) + RB_SIZE(p))            /* next block header pointer */
#define RB_PREV_FTRP(p) ((void*)(p) - RB_HDR_SIZE - RB_FTR_SIZE)    /* prev block footer pointer */

#define SET_RB(p, size, alloc) do {                     \
    SET(RB_HDRP(p), PACK(size, alloc));                 \
    SET(RB_FTRP(p), PACK(size, alloc)); } while(0)

/* pointer calculation */
#define PREV_BLKP(p) ((void*)(p) - PREV_BLK_SIZE(p))    /* prev block pointer */
#define NEXT_BLKP(p) ((void*)(p) + RB_SIZE(p))          /* next block pointer */

/* block info */
#define PREV_BLK_ALLOC(p)   GET_ALLOC(RB_PREV_FTRP(p))
#define PREV_BLK_SIZE(p)    GET_SIZE(RB_PREV_FTRP(p))
#define NEXT_BLK_ALLOC(p)   GET_ALLOC(RB_NEXT_HDRP(p))
#define NEXT_BLK_SIZE(p)    GET_SIZE(RB_NEXT_HDRP(p))

#define TAIL_BLK(p) EB(NEXT_BLKP(ptr))      /* is ptr a tailing block? */

void dump(char *, size_t, void *);

static void *heap_listp;    /* start point of the implicit heap list */
static void *heap_curp;     /* current block pointer */

/**
 * Try to place a block in bp
 * @param bp
 * @param size
 * @return
 */
void *place(void *bp, size_t size) {
    size_t asize, rsize, osize, nsize;
    void *splitp;

    if (RB_ALLOC(bp) == BLK_ALLOC) {
        return NULL;
    }

    asize = ALIGN(size);                                /* aligned size */
    nsize = asize + RB_HDR_SIZE + RB_FTR_SIZE;          /* new block size */
    osize = RB_SIZE(bp);                                /* old block size */

    /* no enough room */
    if (nsize > osize) {
        return NULL;
    }

    rsize = osize - nsize;                              /* remind size if the block will be split into 2 parts */

    /* cannot split */
    if (rsize < MIN_BLK_SIZE) {
        SET_RB(bp, osize, BLK_ALLOC);
        return bp;
    }

    /* need to split */
    SET_RB(bp, nsize, BLK_ALLOC);
    splitp = NEXT_BLKP(bp);
    SET_RB(splitp, rsize, BLK_FREE);

    return bp;
}

/******************************************
 * fit strategies
 ******************************************/

/**
 *
 * @param size
 * @return
 */
void *first_fit(size_t size) {
#ifdef DEBUG
    printf("[DEBUG] in first_fit(), size = %ld\n", size);
#endif

    void *p;

    p = heap_listp;
    while (!EB(p)) {
        /* try to find a block that big enough
         * according to gprof, this is the most time-consuming
         * code, cuz time:O(n) of this function for each hit */
        if (RB_ALLOC(p) == BLK_FREE && RB_AVL_SIZE(p) >= size) {
            return place(p, size);
        }
        p = NEXT_BLKP(p);
    }

    return NULL;
}

/**
 *
 * @param size
 * @return
 */
void *next_fit(size_t size) {
#ifdef DEBUG
    printf("[DEBUG] in next_fit(), size = %ld, heap_curp =  %p\n", size, heap_curp);
#endif

    void *oldp, *p;

    oldp = p = NEXT_BLKP(heap_curp);

    do {
        if (EB(p)) {
            /* start from head of the heap list */
            p = heap_listp;
        } else {
            if (RB_ALLOC(p) == BLK_FREE && RB_AVL_SIZE(p) >= size) {
#ifdef DEBUG
                printf("[DEBUG] in next_fit(), FOUND! heap_curp =  %p\n", bp);
#endif
                return place(p, size);
            }
            p = NEXT_BLKP(p);
        }
    } while (p != oldp);    /* stop if we return to the starting point */

    return NULL;
}

/**
 *
 * @param size
 * @return
 */
void *best_fit(size_t size) {
#ifdef DEBUG
    printf("[DEBUG] in best_fit(), size = %ld\n", size);
#endif

    void *p, *bestp;
    size_t best, nsize;

    best = MAX_HEAP;        /* cannot bigger than the maxheap */
    bestp = NULL;
    p = heap_listp;

    while (!EB(p)) {
        /* try to find a block that fits best */
        nsize = RB_AVL_SIZE(p);
        if (RB_ALLOC(p) == BLK_FREE && nsize >= size && nsize < best) {
            best = nsize;
            bestp = p;
        }
        p = NEXT_BLKP(p);
    }

    /* find nothing fit */
    if (best == MAX_HEAP) {
        return NULL;
    }

    return place(bestp, size);
}

/**
 *
 * @param size
 * @return
 */
void *find_fit(size_t size) {
    // three strategies:
    // 1. first fit
    // 2. next fit
    // 3. best fit
#ifdef USE_FIRST_FIT
    return first_fit(size);
#elif USE_NEXT_FIT
    return next_fit(size);
#elif USE_BEST_FIT
    return best_fit(size);
#endif
    return NULL;
}

/******************************************
 * helper functions
 ******************************************/

/**
 *
 * @param bp
 * @return
 */
void *coalesce(void *bp) {
    // four cases: (1 for alloc, 0 for free)
    // prev, now, next
    // 1, 0, 1
    // 0, 0, 1
    // 1, 0, 0
    // 0, 0, 0
    int size;
    void *p;

    size = RB_SIZE(bp);
    p = bp;

    if (NEXT_BLK_ALLOC(bp) == BLK_FREE) {
#ifdef DEBUG
        printf("[DEBUG] coalescing next block: %p, size: %d\n", NEXT_BLKP(bp), NEXT_BLK_SIZE(bp));
#endif
        size += NEXT_BLK_SIZE(bp);
    }

    if (PREV_BLK_ALLOC(bp) == BLK_FREE) {
#ifdef DEBUG
        printf("[DEBUG] coalescing prev block: %p, size: %d\n", PREV_BLKP(bp), PREV_BLK_SIZE(bp));
#endif
        size += PREV_BLK_SIZE(bp);
        p = PREV_BLKP(bp);
    }

    SET_RB(p, size, BLK_FREE);

#ifdef DUMP_HEAP
    dump("coalesce", size, bp);
#endif
#ifdef USE_NEXT_FIT
    heap_curp = p;
#endif
    return p;
}

/**
 * Extend the heap
 * It is the ONLY function that other functions can call
 * When they want to alloc memory
 * @param size: size of memory to extend the heap
 *              NOTE: it is caller's duty to rounds up to the nearest multiple of ALIGNMENT, and
 *                    it is caller's duty to calculate the header and footer within size
 * @return the address of free memory, or NULL if failed
 */
void *extend_heap(int size) {
#ifdef DEBUG
    printf("[DEBUG] in extend_heap(), size = %d\n", size);
#endif

    void *old_brkp;

    if (size == 0) {
        return NULL;
    }

    /* do nothing if out of memory */
    if ((old_brkp = mem_sbrk(size)) == (void *) -1) {
        return NULL;
    }

    SET_EB(NEXT_BLKP(old_brkp));

    return old_brkp;
}

/**
 *
 * @param size not contain header and footer
 * @return
 */
void *do_malloc(size_t size) {
    void *tailp, *curp;
    size_t pasize, tsize;

    size = ALIGN(size);

    /* if no fitting block, try to extend the heap
     * wish we have free block at the tail of the heap that can be use
     * nor we have to alloc a totally new block for it */
    tailp = PREV_BLKP(mem_sbrk(0));
    if (RB_ALLOC(tailp) == BLK_FREE) {
#ifdef DEBUG
        printf("[DEBUG] in do_malloc(): reuse tail block\n");
#endif
        pasize = RB_AVL_SIZE(tailp);        /* prev available memory size */
        tsize = size - pasize;              /* we need to alloc */

        if (extend_heap(tsize) == NULL) {
            return NULL;
        }

        curp = tailp;
    } else {
        tsize = size + RB_HDR_SIZE + RB_FTR_SIZE;

        if ((curp = extend_heap(tsize)) == NULL) {
            return NULL;
        }
    }

    SET_RB(curp, size + RB_HDR_SIZE + RB_FTR_SIZE, BLK_ALLOC);

#ifdef DUMP_HEAP
    dump("alloc", size, curp);
#endif
#ifdef USE_NEXT_FIT
    heap_curp = curp;
#endif

    return curp;
}

/******************************************
 * allocator open APIs
 ******************************************/

/**
 *
 * @return
 */
int implicit_mm_init(void) {
    if ((heap_listp = extend_heap(PADDING_BLK_SIZE + PB_HDR_SIZE + PB_FTR_SIZE + EB_HDR_SIZE)) == (void *) -1) {
        return 1;
    }

    /* see CS:APP3e page 829 */
    SET(heap_listp, 0xDEADBEEF);
    SET(heap_listp + PADDING_BLK_SIZE, PACK(PB_HDR_SIZE + PB_FTR_SIZE, BLK_ALLOC));
    SET(heap_listp + PADDING_BLK_SIZE + PB_HDR_SIZE, PACK(PB_HDR_SIZE + PB_FTR_SIZE, BLK_ALLOC));
    SET(heap_listp + PADDING_BLK_SIZE + PB_HDR_SIZE + PB_FTR_SIZE, PACK(0, BLK_ALLOC));

    heap_listp += PADDING_BLK_SIZE + PB_HDR_SIZE;

#ifdef USE_NEXT_FIT
    heap_curp = heap_listp;
#endif
    return 0;
}

/**
 *
 * @param size
 * @return
 */
void *implicit_mm_malloc(size_t size) {
    size_t asize;
    void *curp;

    if (size == 0) {
        return NULL;
    }

    asize = ALIGN(size);

    /* try to find a free block */
    if ((curp = find_fit(asize)) != NULL) {
#ifdef DUMP_HEAP
        dump("find fit", asize, cur);
#endif
    } else {
        curp = do_malloc(asize);
    }

#ifdef USE_NEXT_FIT
    heap_curp = curp;
#endif
    return curp;
}

/**
 *
 * @param ptr
 */
void implicit_mm_free(void *ptr) {
    SET_RB(ptr, RB_SIZE(ptr), BLK_FREE);
#ifdef DUMP_HEAP
    dump("free", RB_SIZE(ptr), ptr);
#endif
    coalesce(ptr);
}

/**
 *
 * @param ptr
 * @param size
 * @return
 */
void *implicit_mm_realloc(void *ptr, size_t size) {
    void *p;                            /* new block pointer that we should return */
    void *hdrp, *ftrp, *splitp;
    void *prep;
    size_t bsize, rsize, nsize, fsize;
    int alloc;

    /* equivalent to malloc */
    if (ptr == NULL) {
        return implicit_mm_malloc(size);
    }

    /* equivalent to free */
    if (size == 0) {
        implicit_mm_free(ptr);
        return ptr;
    }

    /* 1. resize a free block is meaningless;
     * 2. if we want to resize a block that is smaller than MIN_BLK_SIZE, that means we points to a wrong block;
     * 3. if the block size if no aligned, something wrong;
     * 4. if the header is not same with the footer, something wrong.
     * */
    if ((hdrp = RB_HDRP(ptr)) == NULL || (ftrp = RB_FTRP(ptr)) == NULL) {
#ifdef DEBUG
        printf("[DEBUG] in implicit_mm_realloc(): illegal header or footer\n");
#endif
        return NULL;
    }

    bsize = RB_SIZE(ptr), alloc = RB_ALLOC(ptr);        /* current block size and its alloc state */
    if (alloc == BLK_FREE || bsize <= MIN_BLK_SIZE || bsize % ALIGNMENT != 0 || GET(hdrp) != GET(ftrp)) {
#ifdef DEBUG
        printf("[DEBUG] in implicit_mm_realloc(): illegal realloc: %p, %ld, %d, %ld, %ld, 0x%x, 0x%x\n",
            ptr, size, alloc, bsize, bsize % ALIGNMENT, GET(hdrp), GET(ftrp));
#endif
        return NULL;
    }

    /* legal block indeed, try to resize */
    nsize = ALIGN(size) + RB_HDR_SIZE + RB_FTR_SIZE;        /* new size of the realloc block */

    /* same size with the origin block, do nothing */
    if (nsize == bsize) {
#ifdef DEBUG
        printf("[DEBUG] in implicit_mm_realloc(): same size, return\n");
#endif
        p = ptr;
        goto realloc;
    }

    /* smaller than the origin block, see if we must split it */
    if (nsize < bsize) {
        rsize = bsize - nsize;

        /* need to split */
        if (rsize >= MIN_BLK_SIZE) {
            SET_RB(ptr, nsize, BLK_ALLOC);
            splitp = NEXT_BLKP(ptr);
            SET_RB(splitp, rsize, BLK_FREE);
        }
        p = ptr;
        goto realloc;
    }

    /* larger than the origin block,
     * see if we can use any free block */

    /* check the following block:
     * if the next block is alloc, we can do nothing with it
     * if the next block is free, and it has enough room, use it
     * if the next block is free, but it has not enough room, but
     * it is the tailing block, use it and extend the heap */
    if (NEXT_BLK_ALLOC(ptr) == BLK_FREE) {
        fsize = NEXT_BLK_SIZE(ptr);     /* block size that we can use of the next block */

        if (nsize <= bsize + fsize) {
            /* enough size */
            fsize -= (nsize - bsize);
            SET_RB(ptr, nsize, BLK_ALLOC);
            if (fsize >= MIN_BLK_SIZE) {
                SET_RB(NEXT_BLKP(ptr), fsize, BLK_FREE);
            }
            p = ptr;
            goto realloc;
        } else if (EB(NEXT_BLKP(ptr))) {
            /* not enough, but the next block is the tail */
            fsize = nsize - bsize - fsize;
            extend_heap(fsize);
            SET_RB(ptr, nsize, BLK_ALLOC);
            p = ptr;
            goto realloc;
        }
    }

    /* check previous */
    prep = PREV_BLKP(ptr);
    fsize = RB_SIZE(prep);
    if ((RB_ALLOC(prep) == BLK_FREE) && (nsize <= bsize + fsize)) {
        fsize -= (nsize - bsize);
        memcpy(prep, ptr, RB_AVL_SIZE(ptr));
        SET_RB(prep, nsize, BLK_ALLOC);
        if (fsize >= MIN_BLK_SIZE) {
            SET_RB(NEXT_BLKP(prep), fsize, BLK_FREE);
        }
        p = prep;
        goto realloc;
    }

    // TODO:
    /* there is a case that, the prev and the next block
     * all do not have enough room to realloc,
     * but the sum of them could fill the hole */

    /* we have no free block to reuse
     * try to extend the heap
     * 1. alloc new memory
     *    if ptr points to the tail block, then we can just alloc a smaller block
     *    than the whole size
     *    if ptr is not, we have to alloc one with full size
     * 2. copy if needed
     * 3. free old if needed
     * */
    if (TAIL_BLK(ptr)) {
        /* tail block */
        if ((p = extend_heap(nsize - bsize)) == (void *) -1) {
            return NULL;
        }
    } else {
        /* need a totally new block */
        if ((p = do_malloc(ALIGN(size))) == NULL) {
            return NULL;
        }
        memcpy(p, ptr, RB_AVL_SIZE(ptr));
        implicit_mm_free(ptr);
    }

    SET_RB(p, nsize, BLK_ALLOC);
    goto realloc;

    realloc:
#ifdef  DUMP_HEAP
    dump("realloc", size, p);
#endif
#ifdef USE_NEXT_FIT
    heap_curp = p;
#endif
    return p;
}


/******************************************
 * heap dumper
 ******************************************/
void dump(char *msg, size_t size, void *p) {
    void *s;

    s = heap_listp;

    printf("\n");
    printf("after %s %d(0x%x) memory at %p:\n", msg, (int) size, (uint) size, p);
    printf("==========================================================================================\n");
    while (s != NULL && !EB(s)) {
        printf("%s\t", RB_ALLOC(s) ? "alloc" : "free");
        printf("%8d(%8x)\t", RB_SIZE(s), RB_SIZE(s));
        printf("%p --- ", s);
        printf("%p\t", s + RB_SIZE(s) - 1);
        printf("%8x\t%8x\n", GET(RB_HDRP(s)), GET(RB_FTRP(s)));

        s = NEXT_BLKP(s);
    }
    printf("==========================================================================================\n");
}