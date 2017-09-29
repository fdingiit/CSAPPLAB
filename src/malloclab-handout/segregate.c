#include <string.h>
#include <stdint.h>
#include <stdio.h>

#include "segregate.h"
#include "memlib.h"
#include "utils.h"

/* the smallest block's size we maintain*/
#define BLK_MIN_SIZE    (BLK_HDR_SIZE + ALIGNMENT + BLK_FTR_SIZE)

#define BLK_FREE    0
#define BLK_ALLOC   1

#define PACK(size, alloc) ((size) | (alloc))    /* pack size with alloc */

#define GET_SIZE(p)     (GET(p) & ~0x7)
#define GET_ALLOC(p)    (GET(p) & 0x1)

/* padding block */
#define PADDING_BLK_SIZE    4

/* prologue block */
#define PB_HDR_SIZE     4
#define PB_FTR_SIZE     4

/* epilogue block */
#define EB_HDR_SIZE     4

#define EB_HDRP(p)      ((void*)(p) - EB_HDR_SIZE)          /* epilogue block header pointer */
#define SET_EB(p)       SET(EB_HDRP(p), PACK(0, 1))         /* set epilogue block */
#define EB(p)           (BLK_SIZE(p) == 0 && BLK_STATE(p))         /* does p point to a epilogue block? */

#define SET(p, val)     (*(unsigned int*)(p) = (val))
#define GET(p)          (*(unsigned int*)(p))

#define BLK_HDR_SIZE    4
#define BLK_FTR_SIZE    4

#define BLK_SIZE(p)             GET_SIZE(BLK_HDRP(p))
#define BLK_AVAL_SIZE(p)        (BLK_SIZE(p) - BLK_HDR_SIZE - BLK_FTR_SIZE)

#define BLK_STATE(p)            (GET_ALLOC(BLK_HDRP(p)))                 /* is the block alloced? */

#define BLK_HDRP(p)             ((void*)(p) - BLK_HDR_SIZE)
#define BLK_FTRP(p)             ((void*)(p) + BLK_AVAL_SIZE(p))

#define SET_BLK(p, size, alloc)    do {                     \
    SET(BLK_HDRP(p), PACK(size, alloc));                    \
    SET(BLK_FTRP(p), PACK(size, alloc)); } while(0)

/* pointer calculation */
#define PREV_BLKP(p)    ((void*)(p) - PREV_BLK_SIZE(p))    /* prev block pointer */
#define NEXT_BLKP(p)    ((void*)(p) + BLK_SIZE(p))          /* next block pointer */

#define NEXT_HDRP(p)    BLK_HDRP((void*)(p) + BLK_SIZE(p))            /* next block header pointer */
#define PREV_FTRP(p)    ((void*)(p) - BLK_HDR_SIZE - BLK_FTR_SIZE)    /* prev block footer pointer */

/* block info */
#define PREV_BLK_ALLOC(p)   GET_ALLOC(PREV_FTRP(p))
#define PREV_BLK_SIZE(p)    GET_SIZE(PREV_FTRP(p))
#define NEXT_BLK_ALLOC(p)   GET_ALLOC(NEXT_HDRP(p))
#define NEXT_BLK_SIZE(p)    GET_SIZE(NEXT_HDRP(p))

/* freelist table */
#define FLT_SLOT_NUM    11          /* freelist_table slots number*/
#define FLT_SIZE        (FLT_SLOT_NUM * sizeof(void *))

/* free block pointers */
#define NEXT_FREE_BLKP(p)       ((void*) (*(uintptr_t*)(p)))
#define PREV_FREE_BLKP(p)       ((void*) (*((uintptr_t*)(p) + 1)))

#define SET_NEXT_FREE_BLK(bp, next)    (*(uintptr_t*)(bp) = (uintptr_t)(next))
#define SET_PREV_FREE_BLK(bp, prev)    (*((uintptr_t*)(bp) + 1) = (uintptr_t)(prev))

#define DELETE_FREE_BLK(bp) do {                    \
    void *prevp, *nextp;                            \
                                                    \
    if (BLK_SIZE(bp) > BLK_MIN_SIZE) {              \
        prevp = PREV_FREE_BLKP(bp);                 \
        nextp = NEXT_FREE_BLKP(bp);                 \
                                                    \
        SET_NEXT_FREE_BLK(prevp, nextp);            \
        if (nextp) {                                \
            SET_PREV_FREE_BLK(nextp, prevp);        \
        }                                           \
    }                                               \
} while (0)


static void **freelist_table;
static void *heap_listp;

size_t flt_index(int v);

void dump(char *msg, size_t size, void *p);

void *coalesce(void *bp);

/**
 *
 * @param freelistp
 * @param bp
 */
void freelist_insert2(void *freelistp, void *bp) {
    void *nextp;

    if (BLK_STATE(bp) != BLK_FREE) {
        fprintf(stderr, "freeing a alloc block! %p\n", bp);
        exit(0);
    }

    nextp = NEXT_FREE_BLKP(freelistp);

    /* no need to maintain prev pointer if it is the smallest block
     * the alBLK_MIN_SIZElocator allowed */
    if (BLK_SIZE(bp) == BLK_MIN_SIZE) {
        SET_NEXT_FREE_BLK(bp, nextp);
        SET_NEXT_FREE_BLK(freelistp, bp);
        return;
    }

    nextp = NEXT_FREE_BLKP(freelistp);
    SET_NEXT_FREE_BLK(bp, nextp);
    SET_PREV_FREE_BLK(bp, freelistp);
    SET_NEXT_FREE_BLK(freelistp, bp);
    if (nextp != NULL) {
        SET_PREV_FREE_BLK(nextp, bp);
    }
}

/**
 *
 * @param bp
 */
void freelist_insert(void *bp) {
    void *p;

    p = coalesce(bp);
    freelist_insert2(freelist_table + flt_index(BLK_AVAL_SIZE(p)), p);
}


/**
 *
 * @param freelistp
 * @param size not aligned, and exclude header and footer
 * @return
 */
void *freelist_alloc(void *freelistp, size_t size) {
    void *p;
    size_t asize, bavasize, rsize;

    if ((p = NEXT_FREE_BLKP(freelistp)) == NULL) {
        return NULL;
    }

    /* if the size > ALIGNMENT, then we may need to split a big block into two small one.
     * if the reminder will be smaller than BLK_MIN_SIZE after splitting, do not split
     * if not, insert the reminder to a right freelist
     */
    asize = ALIGN(size);

    while (p) {
        bavasize = BLK_AVAL_SIZE(p);

        if (bavasize >= asize) {
            DELETE_FREE_BLK(p);
            SET_BLK(p, asize + BLK_HDR_SIZE + BLK_FTR_SIZE, BLK_ALLOC);

            rsize = bavasize - asize;
            if (rsize >= BLK_MIN_SIZE) {
                /* need to split */
                SET_BLK(NEXT_BLKP(p), rsize, BLK_FREE);
                freelist_insert(NEXT_BLKP(p));
            }
            return p;
        }
        p = NEXT_FREE_BLKP(p);
    }

    return NULL;
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

    SET_EB(mem_sbrk(0));
    return old_brk;
}



/******************************************
 * helper functions
 ******************************************/

/**
 * Given a number v, get its index in the freelist_table
 * @param v
 * @return
 */
size_t flt_index(int v) {
    /**
     * see http://graphics.stanford.edu/%7Eseander/bithacks.html#IntegerLog
     * and http://graphics.stanford.edu/%7Eseander/bithacks.html#DetermineIfPowerOf2
     */
    int ret;
    const unsigned int b[] = {0x2, 0xC, 0xF0, 0xFF00, 0xFFFF0000};
    const unsigned int S[] = {1, 2, 4, 8, 16};
    int i;
    int vc = v;

    register unsigned int r = 0; // result of log2(v) will go here
    for (i = 4; i >= 0; i--) // unroll for speed...
    {
        if (v & b[i]) {
            v >>= S[i];
            r |= S[i];
        }
    }

    ret = r - 3 + ((vc & (vc - 1)) != 0);
    if (ret < 0) {
        return 0;
    } else if (ret > 10) {
        return 10;
    }
    return ret;
}

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

    size = BLK_SIZE(bp);
    p = bp;

    if (NEXT_BLK_ALLOC(bp) == BLK_FREE) {
#ifdef DEBUG
        printf("[DEBUG] coalescing next block: %p, size: %d\n", NEXT_BLKP(bp), NEXT_BLK_SIZE(bp));
#endif
        DELETE_FREE_BLK(NEXT_BLKP(bp));
        size += NEXT_BLK_SIZE(bp);
    }

    if (PREV_BLK_ALLOC(bp) == BLK_FREE) {
#ifdef DEBUG
        printf("[DEBUG] coalescing prev block: %p, size: %d\n", PREV_BLKP(bp), PREV_BLK_SIZE(bp));
#endif
        DELETE_FREE_BLK(PREV_BLKP(bp));
        size += PREV_BLK_SIZE(bp);
        p = PREV_BLKP(bp);
    }

    SET_BLK(p, size, BLK_FREE);

#ifdef DUMP_HEAP
    dump("coalesce", size, bp);
#endif
    return p;
}


/******************************************
 * allocator open APIs
 ******************************************/

/**
 *
 * @return
 */
int segregate_mm_init(void) {
    // 1. extend the heap for the freelist_table;
    // 2. init the freelist_table:
    //    {1-8, 9-16, 17-32, 33-64, ..., 4097-@#$}, 11 slots, 8 bytes per slot to store a pointer (64bit platform)
    // 3. extend the heap for padding, pb, and eb.
    if ((freelist_table = extend_heap(FLT_SIZE + PADDING_BLK_SIZE + PB_HDR_SIZE + PB_FTR_SIZE + EB_HDR_SIZE)) == NULL) {
        return 1;
    }
    memset(freelist_table, 0, FLT_SIZE);
    heap_listp = freelist_table + FLT_SLOT_NUM;

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
void *segregate_mm_malloc(size_t size) {
    // 1. calculate the index N according to `size';
    // 2. try to find a free block that big enough in the freelist_table[N]:
    // 2.1 if found, place it, and move the reminder to the right freelist if needed;
    // 2.2 if not found, try freelist_table[N+1], continually, until nothing to check;
    // 3. if cannot find any block, try to extend the heap.
    size_t index, i, bsize;
    void *p, *bp;

    index = flt_index(size);

    /* iterate all freelist */
    for (i = index; i < FLT_SLOT_NUM; i++) {
        p = freelist_table + i;
        if ((bp = freelist_alloc(p, size)) != NULL) {
#ifdef DEBUG
            dump("alloc from freelist", size, bp);
#endif
            return bp;
        }
    }

    /* no room in freelist */
    bsize = ALIGN(size) + BLK_HDR_SIZE + BLK_FTR_SIZE;
    if ((bp = extend_heap(bsize)) == NULL) {
        return NULL;
    }

    SET_BLK(bp, bsize, BLK_ALLOC);
#ifdef DEBUG
    dump("alloc from heap", size, bp);
#endif
    return bp;
}

/**
 *
 * @param ptr
 */
void segregate_mm_free(void *ptr) {
    freelist_insert(ptr);
#ifdef DEBUG
    dump("free", BLK_AVAL_SIZE(ptr), ptr);
#endif
}

/**
 *
 * @param ptr
 * @param size
 * @return
 */
void *segregate_mm_realloc(void *ptr, size_t size) {
    size_t asize, avasize;
    void *np;

    if (ptr == NULL) {
        return segregate_mm_malloc(size);
    }

    if (size == 0) {
        segregate_mm_free(ptr);
        return NULL;
    }

    asize = ALIGN(size), avasize = BLK_AVAL_SIZE(ptr);

    /* need more space */
    if (asize > avasize) {
        np = segregate_mm_malloc(asize);
        memcpy(np, ptr, BLK_AVAL_SIZE(ptr));
        segregate_mm_free(ptr);
        return np;
    }

    /* if shrink */
    if (asize < avasize) {
        /* need to insert the reminder to freelist */
        if (avasize - asize >= BLK_MIN_SIZE) {
            SET_BLK(ptr, asize + BLK_HDR_SIZE + BLK_FTR_SIZE, BLK_ALLOC);
            SET_BLK(NEXT_BLKP(ptr), avasize - asize, BLK_FREE);
            segregate_mm_free(NEXT_BLKP(ptr));
        }
    }

    return ptr;
}

/******************************************
 * freelist dumper
 ******************************************/
void freelist_dump(char *msg, size_t size, void *p) {
    void *s, *bp;
    size_t i, index;

    index = flt_index(size);

    printf("\n");
    printf("after %s %d(0x%x) memory at %p:\n", msg, (int) size, (uint) size, p);
    printf("==========================================================================================\n");
    printf("freelist_table:\n");
    for (i = 0; i < FLT_SLOT_NUM; i++) {
        s = freelist_table + i;

        if ((bp = (void *) *(uintptr_t *) (s)) == NULL) {
            continue;
        }

        if (i == index) {
            printf("***");
        }

        printf("slot [%zu]:\t", i);
        while (bp) {
            printf("%p(%u)\t", bp, BLK_AVAL_SIZE(bp));
            bp = NEXT_FREE_BLKP(bp);
        }
        printf("\n");
    }
    printf("==========================================================================================\n");
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
        printf("%s\t", BLK_STATE(s) ? "alloc" : "free");
        printf("%8d(%8x)\t", BLK_SIZE(s), BLK_SIZE(s));
        printf("%p --- ", s);
        printf("%p\t", s + BLK_SIZE(s) - 1);
        printf("%8x\t%8x\n", GET(BLK_HDRP(s)), GET(BLK_FTRP(s)));

        s = NEXT_BLKP(s);
    }
    printf("==========================================================================================\n");
}