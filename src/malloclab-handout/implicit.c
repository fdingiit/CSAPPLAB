#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "implicit.h"
#include "memlib.h"
#include "utils.h"

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


void dump(char *, size_t);

static void *heap_listp;    /* start point of the implicit heap list */

/**
 * Try to place a block in bp
 * @param bp
 * @param size
 * @return
 */
void *place(void *bp, size_t size) {
    size_t asize, avasize, rsize, osize, nsize;
    void *split;

    if (RB_ALLOC(bp) == BLK_ALLOC) {
        return NULL;
    }

    asize = ALIGN(size);                                /* aligned size */
    avasize = RB_AVL_SIZE(bp);                          /* available size */

    /* no enough room */
    if (size > avasize) {
        return NULL;
    }

    osize = RB_SIZE(bp);                                /* old block size */
    nsize = asize + RB_HDR_SIZE + RB_FTR_SIZE;          /* new block size if split */
    rsize = osize - nsize;                              /* remind size if the block will be split into 2 parts */

    /* just fit, or cannot use remain memory after splitting */
    if (rsize < MIN_BLK_SIZE) {
        SET_RB(bp, osize, BLK_ALLOC);
        return bp;
    }

    /* need to split */
    SET_RB(bp, nsize, BLK_ALLOC);
    split = NEXT_BLKP(bp);
    SET_RB(split, rsize, BLK_FREE);

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
    printf("[DEBUG] in first_fit(), size = %d\n", size);
#endif

    void *p, *ret;

    p = heap_listp;
    while (!EB(p)) {
        /* try to find a block that big enough */
        if (RB_ALLOC(p) == BLK_FREE && RB_AVL_SIZE(p) >= size) {
            if ((ret = place(p, size)) != NULL) {
                return ret;
            }
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
    return NULL;
}

/**
 *
 * @param size
 * @return
 */
void *best_fit(size_t size) {
    return NULL;
}

/**
 *
 * @param size
 * @return
 */
void *find_fit(size_t size) {
    // TODO:
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
    dump("coalesce", 0);
#endif
    return p;
}

/**
 * Extend the heap
 * @param size: size of memory to extend the heap
 *              NOTE: it is caller's duty to rounds up to the nearest multiple of ALIGNMENT, and
 *                    it is caller's duty to calculate the header and footer within size
 * @return the address of free memory, or NULL if failed
 */
void *extend_heap(int size) {
#ifdef DEBUG
    printf("[DEBUG] in extend_heap(), size = %d\n", size);
#endif

    void *old_brk;

    if (size == 0) {
        return NULL;
    }

    /* just do nothing if not aligned */
    if (size % ALIGNMENT != 0) {
        return NULL;
    }

    /* do nothing if out of memory */
    if ((old_brk = mem_sbrk(size)) == (void *) -1) {
        return NULL;
    }

    SET_RB(old_brk, size, BLK_FREE);
    SET_EB(NEXT_BLKP(old_brk));

    return old_brk;
}

/**
 *
 * @param size
 * @param alloc
 * @return
 */
void *do_malloc(size_t size, int alloc) {
    void *tail, *prev, *cur;
    size_t pasize, tsize;

    /* if no fitting block, try to extend the heap
     * wish we have free block at the tail of the heap that can be use
     * or we have to alloc a totally new block for it */
    tail = mem_sbrk(0);
    if (PREV_BLK_ALLOC(tail) == BLK_FREE || alloc) {
#ifdef DEBUG
        printf("[DEBUG] in do_malloc(): reuse tail block: %d\n", alloc);
#endif
        prev = PREV_BLKP(tail);
        pasize = RB_AVL_SIZE(prev);     /* prev available memory size */
        tsize = size - pasize;          /* we need to alloc */

        if (extend_heap(tsize) == NULL) {
            return NULL;
        }

        SET_RB(prev, size + RB_HDR_SIZE + RB_FTR_SIZE, BLK_ALLOC);
        cur = prev;
    } else {
        tsize = size + RB_HDR_SIZE + RB_FTR_SIZE;

        if ((cur = extend_heap(tsize)) == NULL) {
            return NULL;
        }

        /* and then try to place it at the new extended memory */
        SET_RB(cur, tsize, BLK_ALLOC);
    }

#ifdef DUMP_HEAP
    dump("alloc", size);
#endif
    return cur;
}

/******************************************
 * allocator open APIs
 ******************************************/

/**
 *
 * @return
 */
int implicit_mm_init(void) {
    if ((heap_listp = mem_sbrk(PADDING_BLK_SIZE + PB_HDR_SIZE + PB_FTR_SIZE + EB_HDR_SIZE)) == (void *) -1) {
        return 1;
    }

    SET(heap_listp, 0xDEADBEEF);        /* padding block */
    SET(heap_listp + PADDING_BLK_SIZE, PACK(8, BLK_ALLOC));     /* prologue block header */
    SET(heap_listp + PADDING_BLK_SIZE + PB_HDR_SIZE, PACK(8, BLK_ALLOC));   /* prologue block footer */
    SET(heap_listp + PADDING_BLK_SIZE + PB_HDR_SIZE + PB_FTR_SIZE, PACK(0, BLK_ALLOC)); /* epilogue block header */

    heap_listp += PADDING_BLK_SIZE + PB_HDR_SIZE;
    return 0;
}

/**
 *
 * @param size
 * @return
 */
void *implicit_mm_malloc(size_t size) {
    size_t asize;
    void *cur;

    if (size == 0) {
        return NULL;
    }

    asize = ALIGN(size);

    /* try to find a free block */
    if ((cur = find_fit(asize)) != NULL) {
#ifdef DUMP_HEAP
        dump("find fit", asize);
#endif
        return cur;
    }

    cur = do_malloc(asize, BLK_FREE);

    return cur;
}

/**
 *
 * @param ptr
 */
void implicit_mm_free(void *ptr) {
    SET_RB(ptr, RB_SIZE(ptr), BLK_FREE);
#ifdef DUMP_HEAP
    dump("free", RB_SIZE(ptr));
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
    void *hdrp, *ftrp, *split, *prep, *p;
    size_t bsize, rsize, nsize;
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
    hdrp = RB_HDRP(ptr);
    ftrp = RB_FTRP(ptr);
    if (hdrp == NULL || ftrp == NULL) {
#ifdef DEBUG
        printf("[DEBUG] in implicit_mm_realloc(): null header or footer\n");
#endif
        return NULL;
    }

    bsize = RB_SIZE(ptr), alloc = RB_ALLOC(ptr);
    if (alloc == BLK_FREE || bsize <= MIN_BLK_SIZE || bsize % ALIGNMENT != 0 || GET(hdrp) != GET(ftrp)) {
#ifdef DEBUG
        printf("[DEBUG] in implicit_mm_realloc(): illegal realloc: %p, %d, %d, %d, %d, 0x%x, 0x%x\n",
            ptr, size, alloc, bsize, bsize % ALIGNMENT, GET(hdrp), GET(ftrp));
#endif
        return NULL;
    }

    /* legal block indeed, try to resize */
    nsize = ALIGN(size) + RB_HDR_SIZE + RB_FTR_SIZE;

    /* same size with the origin block, do nothing */
    if (nsize == bsize) {
#ifdef DEBUG
        printf("[DEBUG] in implicit_mm_realloc(): same size, return\n");
#endif
        return ptr;
    }

    /* smaller than the origin block, see if we must split it */
    if (nsize < bsize) {
        rsize = bsize - nsize;

        /* need to split */
        if (rsize >= MIN_BLK_SIZE) {
            SET_RB(ptr, nsize, BLK_ALLOC);
            split = NEXT_BLKP(ptr);
            SET_RB(split, rsize, BLK_FREE);
#ifdef DUMP_HEAP
            dump("split", nsize);
#endif
        }

        return ptr;
    }

    /* larger than the origin block, see if we have enough
     * free space just after/before it, or we must alloc+copy+free */

    /* check after */
    if ((NEXT_BLK_ALLOC(ptr) == BLK_FREE) && (nsize <= bsize + NEXT_BLK_SIZE(ptr))) {
        SET_RB(ptr, nsize, BLK_ALLOC);
        return ptr;
    }

    /* check previous */
    prep = PREV_BLKP(ptr);
    if ((PREV_BLK_ALLOC(ptr) == BLK_FREE) && (nsize <= bsize + PREV_BLK_SIZE(ptr))) {
        SET_RB(prep, nsize, BLK_ALLOC);
        memcpy(prep, ptr, RB_AVL_SIZE(ptr));
        return ptr;
    }

    /* 1. alloc new memory
     * 2. copy
     * 3. free old if needed
     * */
    p = do_malloc(ALIGN(size), BLK_ALLOC);
    SET_RB(p, nsize, BLK_ALLOC);
    memcpy(p, ptr, RB_AVL_SIZE(ptr));
    if (p != ptr) {
        implicit_mm_free(ptr);
    }

#ifdef  DUMP_HEAP
    dump("realloc", size);
#endif
    return p;
}


/******************************************
 * heap dumper
 ******************************************/
void dump(char *msg, size_t size) {
    void *s;

    s = heap_listp;

    printf("\n");
    printf("after %s %d(0x%x) memory:\n", msg, (int) size, (uint) size);
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