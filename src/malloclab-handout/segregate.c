#include <string.h>
#include <stdint.h>

#include "segregate.h"
#include "memlib.h"
#include "utils.h"

#define BLK_MIN_SIZE    (ALIGNMENT + BLK_HDR_SIZE)

#define FLT_SLOT_NUM    11          /* freelist_table slots number*/
#define FLT_SIZE        (FLT_SLOT_NUM * sizeof(void *))

#define SET(p, val)     (*(unsigned int*)(p) = val)
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


static void **freelist_table;

size_t flt_index(int v);

/**
 *
 * @param freelistp
 * @param bp
 */
void freelist_insert(void *freelistp, void *bp) {
    *(uintptr_t *) bp = *(uintptr_t *) (freelistp);
    *(uintptr_t *) freelistp = *(uintptr_t *) bp;
}


/**
 *
 * @param freelistp
 * @param size not aligned, and exclude header and footer
 * @return
 */
void *freelist_alloc(void *freelistp, size_t size) {
    void *p, *nextp;
    size_t asize, bavasize, rsize, bsize;

    if ((p = (void *) (*(uintptr_t *) (freelistp))) == NULL) {
        return NULL;
    }

    /* if the size <= ALIGNMENT, then the min size of the block is ALIGNMENT,
     * we have no need to split the free block cuz it's meaningless,
     * so, the first freelist contains blocks of the same size of ALIGNMENT.
     * we aways alloc block from the head of the list, so there is no need
     * to maintain a prev pointer */
    if (size <= ALIGNMENT) {
        nextp = NEXT_FREE_BLKP(p);
        *(uintptr_t *) (freelistp) = *(uintptr_t *) (nextp);
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
            rsize = bavasize - asize;
            if (rsize >= BLK_MIN_SIZE) {
                /* need to split */
                bsize = asize + BLK_HDR_SIZE;
                SET_BLK(p, bsize);
                SET_BLK(NEXT_BLKP(p), rsize);
                freelist_insert(freelist_table + flt_index(rsize), NEXT_BLKP(p));
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


/******************************************
 * allocator open APIs
 ******************************************/

/**
 *
 * @return
 */
int segregate_mm_init(void) {
    // TODO:
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
    // TODO:
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
    return bp;
}

/**
 *
 * @param ptr
 */
void segregate_mm_free(void *ptr) {
    size_t avasize, index;

    avasize = BLK_AVAL_SIZE(ptr);
    index = flt_index(avasize);

    freelist_insert(freelist_table + index, ptr);
}

/**
 *
 * @param ptr
 * @param size
 * @return
 */
void *segregate_mm_realloc(void *ptr, size_t size) {
    return NULL;
}

/******************************************
 * heap dumper
 ******************************************/
void dump(char *msg, size_t size, void *p) {

}