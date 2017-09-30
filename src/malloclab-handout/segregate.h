#ifndef _SEGREGATE_H
#define _SEGREGATE_H

#include <stdlib.h>

extern int mm_init (void);
extern void *mm_malloc (size_t size);
extern void mm_free (void *ptr);
extern void *mm_realloc(void *ptr, size_t size);

int segregate_mm_init(void);
void *segregate_mm_malloc(size_t size);
void segregate_mm_free(void *ptr);
void *segregate_mm_realloc(void *ptr, size_t size);

#endif //_SEGREGATE_H
