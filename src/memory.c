#include <doslib.h>
#include <iocslib.h>

#include "memory.h"

//
//  high memory operations
//
inline static void* __malloc_himem(size_t size) {

    struct REGS in_regs = { 0 };
    struct REGS out_regs = { 0 };

    in_regs.d0 = 0xF8;      // IOCS _HIMEM
    in_regs.d1 = 1;         // HIMEM_MALLOC
    in_regs.d2 = size;

    TRAP15(&in_regs, &out_regs);

    int rc = out_regs.d0;

    return (rc == 0) ? (void*)out_regs.a1 : NULL;
}

inline static void __free_himem(void* ptr) {
    
    struct REGS in_regs = { 0 };
    struct REGS out_regs = { 0 };

    in_regs.d0 = 0xF8;      // IOCS _HIMEM
    in_regs.d1 = 2;         // HIMEM_FREE
    in_regs.d2 = (size_t)ptr;

    TRAP15(&in_regs, &out_regs);
}

//
//  main memory operations using DOSCALL (with malloc, we cannot allocate more than 64k, why?)
//
inline static void* __malloc_mainmem(size_t size) {
  int addr = MALLOC(size);
  return (addr >= 0x81000000) ? NULL : (char*)addr;
}

inline static void __free_mainmem(void* ptr) {
  if (ptr == NULL) return;
  MFREE((int)ptr);
}

//
//  generic memory operations
//
void* malloc_himem(size_t size, int use_high_memory) {
    return use_high_memory ? __malloc_himem(size) : __malloc_mainmem(size);
}

void free_himem(void* ptr, int use_high_memory) {
    if (use_high_memory) {
        __free_himem(ptr);
    } else {
        __free_mainmem(ptr);
    }
}