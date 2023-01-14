#ifndef __H_MEMORY__
#define __H_MEMORY__

#include <stddef.h>

void* malloc_himem(size_t size, int use_high_memory);
void free_himem(void* ptr, int use_high_memory);

#endif