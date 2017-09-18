#include <stdio.h>

extern int mm_init (void);
extern void *mm_malloc (size_t size);
extern void mm_free (void *ptr);
extern void *mm_realloc(void *ptr, size_t size);

int implicit_mm_init(void);
void *implicit_mm_malloc(size_t size);
void implicit_mm_free(void *ptr);
void *implicit_mm_realloc(void *ptr, size_t size);

