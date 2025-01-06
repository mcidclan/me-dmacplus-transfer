// m-c/d 2025
#pragma once
#include "kcall.h"
#include "cache.h"
#include "dmacplus.h"
#include "clock.h"

#define me_section_size   (&__stop__me_section - &__start__me_section)
#define _meLoop           vrg((0xbfc00040 + me_section_size))

static inline u32* meSetUserMem(const u32 size) {
  static void* _base = nullptr;
  if (!_base) {
    _base = memalign(16, size*4);
    memset(_base, 0, size);
    sceKernelDcacheWritebackInvalidateAll();
    return (u32*)(UNCACHED_USER_MASK | (u32)_base);
  } else if (!size) {
    free(_base);
  }
  return nullptr;
}

static volatile bool _meExit = false;
static inline void meExit() {
  _meExit = true;
  meDCacheWritebackInvalidAll();
}
