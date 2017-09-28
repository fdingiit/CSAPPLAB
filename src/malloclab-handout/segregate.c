#include <string.h>
#include <stdint.h>
#include <stdio.h>

#include "segregate.h"
#include "memlib.h"
#include "utils.h"

#define BLK_MIN_SIZE    (ALIGNMENT + ALIGN(sizeof(void *)))

#define FLT_SLOT_NUM    11          /* freelist_table slots number*/
#define FLT_SIZE        (FLT_SLOT_NUM * sizeof(void *))

#define SET(p, val)     (*(unsigned int*)(p) = (val))
#define GET(p)          (*(unsigned int*)(p))


#define BLK_HDR_SIZE    8           /* we force the header be 8 bytes, cause we use 8-byte-alignment */

#define BLK_SIZE(p)             GET(BLK_HDRP(p))
#define BLK_AVAL_SIZE(p)        (BLK_SIZE(p) - BLK_HDR_SIZE)

#define BLK_HDRP(p)             ((void*)(p) - BLK_HDR_SIZE)

#define SET_BLK(p, size)        SET(BLK_HDRP(p), size)

/* block pointers */
#define NEXT_BLKP(p)            ((void*)(p) + BLK_SIZE(p))

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

size_t flt_index(int v);

void dump(char *msg, size_t size, void *p);

void *coalesce(void *freelistp, void *bp);

/**
 *
 * @param freelistp
 * @param bp
 */
void *freelist_insert2(void *freelistp, void *bp) {
    void *p, *nextp;

    nextp = NEXT_FREE_BLKP(freelistp);

    /* just insert into head if it is the smallest block */
    if (BLK_SIZE(bp) == BLK_MIN_SIZE) {
        SET_NEXT_FREE_BLK(bp, nextp);
        SET_NEXT_FREE_BLK(freelistp, bp);
        return NULL;
    }

    /* try coalesce and insert */
    p = coalesce(freelistp, bp);

    if (freelistp != freelist_table + flt_index(BLK_AVAL_SIZE(p))) {
        return p;
    }

    nextp = NEXT_FREE_BLKP(freelistp);
    SET_NEXT_FREE_BLK(p, nextp);
    SET_PREV_FREE_BLK(p, freelistp);
    SET_NEXT_FREE_BLK(freelistp, p);
    if (nextp != NULL) {
        SET_PREV_FREE_BLK(nextp, p);
    }

    return NULL;
}

/**
 *
 * @param bp
 */
void freelist_insert(void *bp) {
    size_t index;
    void *p = bp;

    while (1) {
        index = flt_index(BLK_AVAL_SIZE(p));
        if ((p = freelist_insert2(freelist_table + index, p)) == NULL) {
            return;
        }
    }
}


/**
 *
 * @param freelistp
 * @param size not aligned, and exclude header and footer
 * @return
 */
void *freelist_alloc(void *freelistp, size_t size) {
    void *p;
    size_t asize, bavasize, rsize, bsize;

    if ((p = NEXT_FREE_BLKP(freelistp)) == NULL) {
        return NULL;
    }

    /* if the size <= ALIGNMENT, then the min size of the block is ALIGNMENT,
     * we have no need to split the free block cuz it's meaningless,
     * so, the first freelist contains blocks of the same size of ALIGNMENT.
     * we aways alloc block from the head of the list, so there is no need
     * to maintain a prev pointer */
    if (size <= ALIGNMENT) {
        SET_NEXT_FREE_BLK(freelistp, NEXT_FREE_BLKP(p));
        return p;
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
            rsize = bavasize - asize;

            if (rsize >= BLK_MIN_SIZE) {
                /* need to split */
                bsize = asize + BLK_HDR_SIZE;
                SET_BLK(p, bsize);
                SET_BLK(NEXT_BLKP(p), rsize);
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
 * @param freelistp
 * @param bp
 * @return
 */
void *coalesce(void *freelistp, void *bp) {
    void *p, *coalp, *nextp;

    p = NEXT_FREE_BLKP(freelistp);      /* first node in list */
    coalp = bp;

    while (p) {
        nextp = NEXT_FREE_BLKP(p);

        if (coalp + BLK_SIZE(coalp) == p) {
            DELETE_FREE_BLK(p);
            SET_BLK(coalp, BLK_SIZE(coalp) + BLK_SIZE(p));
        } else if (p + BLK_SIZE(p) == coalp) {
            DELETE_FREE_BLK(p);
            SET_BLK(p, BLK_SIZE(coalp) + BLK_SIZE(p));
            coalp = p;
        }

        p = nextp;
    }

    return coalp;
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
    if ((freelist_table = extend_heap(FLT_SIZE)) == NULL) {
        return 1;
    }
    memset(freelist_table, 0, FLT_SIZE);
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
    void *p, *bp, *tailp;

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
    bsize = ALIGN(size) + BLK_HDR_SIZE;
    if ((tailp = extend_heap(bsize)) == NULL) {
        return NULL;
    }

    bp = tailp + BLK_HDR_SIZE;
    SET_BLK(bp, bsize);
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
    dump("free", avasize, ptr);
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
            SET_BLK(ptr, asize);
            SET_BLK(NEXT_BLKP(ptr), avasize - asize);
            segregate_mm_free(NEXT_BLKP(ptr));
        }
    }

    return ptr;
}

/******************************************
 * freelist dumper
 ******************************************/
void dump(char *msg, size_t size, void *p) {
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