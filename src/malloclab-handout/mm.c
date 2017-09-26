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
#include "mm.h"

#ifdef USE_IMPLICIT
    #include "implicit.h"
#endif
#ifdef USE_SEGREGATE_FIT
    #include "segregate.h"
#endif

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "fdingiit",
    /* First member's full name */
    "fd",
    /* First member's email address */
    "fdingiit@gmail.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
#ifdef USE_IMPLICIT
    return implicit_mm_init();
#endif
#ifdef USE_SEGREGATE_FIT
    return segregate_mm_init();
#endif
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
#ifdef USE_IMPLICIT
    return implicit_mm_malloc(size);
#endif
#ifdef USE_SEGREGATE_FIT
    return segregate_mm_malloc(size);
#endif
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
#ifdef USE_IMPLICIT
    implicit_mm_free(ptr);
#endif
#ifdef USE_SEGREGATE_FIT
    return segregate_mm_free(ptr);
#endif
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
#ifdef USE_IMPLICIT
    return implicit_mm_realloc(ptr, size);
#endif
#ifdef USE_SEGREGATE_FIT
    return segregate_mm_realloc(ptr, size);
#endif
}














