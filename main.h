// m-c/d 2025
#pragma once
#include "kcall.h"
#include "cache.h"
#include "dmacplus.h"
#include "clock.h"

// me
#define ME_EDRAM_BASE         0
#define GE_EDRAM_BASE         0x04000000
#define UNCACHED_USER_MASK    0x40000000
#define CACHED_KERNEL_MASK    0x80000000
#define UNCACHED_KERNEL_MASK  0xA0000000

#define ME_HANDLER_BASE       0xbfc00000

#define u8  unsigned char
#define u16 unsigned short int
#define u32 unsigned int

#define hwp          volatile u32*
#define hw(addr)     (*((hwp)(addr)))
#define uhw(addr)    ((u32*)(0x40000000 | ((u32)addr)))

inline void meGetUncached32(volatile u32** const mem, const u32 size) {
  static void* _base = nullptr;
  if (!_base) {
    _base = memalign(16, size*4);
    memset(_base, 0, size);
    *mem = (u32*)(UNCACHED_USER_MASK | (u32)_base);
    sceKernelDcacheWritebackInvalidateAll();
    return;
  } else if (!size) {
    free(_base);
  }
  *mem = nullptr;
  sceKernelDcacheWritebackInvalidateAll();
  return;
}

inline void meHalt() {
  asm volatile(".word 0x70000000");
}
