// m-c/d 2025
#pragma once
#include "common.h"
#include <pspthreadman.h>

#define DMA_CONTROL_SC2SC(width, size)  DMA_CONTROL_CONFIG(1, 1, 0, 0, (width), (size))
#define DMA_CONTROL_SC2ME(width, size)  DMA_CONTROL_CONFIG(0, 0, 1, 0, (width), (size))
#define DMA_CONTROL_ME2SC(width, size)  DMA_CONTROL_CONFIG(0, 0, 0, 1, (width), (size))

#define DMA_CONTROL_CONFIG(idst, isrc, mdst, msrc, width, size) ( \
  (1U << 31) |                  /* terminal count interrupt enable bit */ \
  ((unsigned)(idst) << 27) |    /* dst addr increment ?                */ \
  ((unsigned)(isrc) << 26) |    /* src addr increment ?                */ \
  ((unsigned)(mdst) << 25) |    /* dst AHB master select               */ \
  ((unsigned)(msrc) << 24) |    /* src AHB master select               */ \
  ((unsigned)(width) << 21) |   /* dst transfer width                  */ \
  ((unsigned)(width) << 18) |   /* src transfer width                  */ \
  (1U << 15) |                  /* dst burst size or dst step ?        */ \
  (1U << 12) |                  /* src burst size or src step ?        */ \
  ((unsigned)(size)))           /* transfer size                       */

// note: addr increment
//  needs to be set while copying data from sc to sc

//  0 sc, 1 me, lli will not work if not set correctly

// note: master select
//  0 sc, 1 me, lli will not work if not set correctly

// note: transfer width
//  0 => 1 byte,
//  1 => 2 bytes,
//  2 => 4 bytes,
//  3 => 8 bytes,
//  4 => 16 bytes

struct DMADescriptor {
  u32 src;
  u32 dst;
  u32 next;
  u32 ctrl;
  u32 status;
} __attribute__((aligned(16)));

// hardware registers behind sceDmacplusSc2Me
inline void dmacplusFromSc(const u32 src, const u32 dst, const u32 size) {
  vrg(0xBC800180) = src;                          // src
  vrg(0xBC800184) = dst;                          // dest
  vrg(0xBC800188) = 0;                            // addr of the next LLI
  vrg(0xBC80018c) = DMA_CONTROL_SC2ME(4, size);   // control attr
  vrg(0xBC800190) =                               // status
    (1 << 0);                                     // channel enable
  asm volatile("sync");
}

inline void dmacplusLLIFromSc(const DMADescriptor* const lli) {
  // int intr = sceKernelCpuSuspendIntr();
  vrg(0xBC800180) = lli->src;                          // src
  vrg(0xBC800184) = lli->dst;                          // dest
  vrg(0xBC800188) = lli->next;                         // addr of the next LLI
  vrg(0xBC80018c) = lli->ctrl;                         // control attr
  vrg(0xBC800190) = lli->status;                       // status
  asm volatile("sync");
  // sceKernelCpuResumeIntrWithSync(intr);
}

// hardware registers behind sceDmacplusMe2Sc
inline void dmacplusFromMe(const u32 src, const u32 dst, const u32 size) {
  vrg(0xBC8001a0) = src;                          // src
  vrg(0xBC8001a4) = dst;                          // dest
  vrg(0xBC8001a8) = 0;                            // addr of the next LLI
  vrg(0xBC8001ac) = DMA_CONTROL_ME2SC(4, size);   // control attr
  vrg(0xBC8001b0) =                               // status
    (1 << 0);                                     // channel enable
  asm volatile("sync");
}

inline void dmacplusLLIFromMe(const DMADescriptor* const lli) {
  // int intr = sceKernelCpuSuspendIntr();
  vrg(0xBC8001a0) = lli->src;                          // src
  vrg(0xBC8001a4) = lli->dst;                          // dest
  vrg(0xBC8001a8) = lli->next;                         // addr of the next LLI
  vrg(0xBC8001ac) = lli->ctrl;                         // control attr
  vrg(0xBC8001b0) = lli->status;                       // status
  asm volatile("sync");
  // sceKernelCpuResumeIntrWithSync(intr);
}

inline void dmacplusLLIOverSc(const DMADescriptor* const lli) {
  vrg(0xBC8001c0) = lli->src;                          // src
  vrg(0xBC8001c4) = lli->dst;                          // dest
  vrg(0xBC8001c8) = lli->next;                         // addr of the next LLI
  vrg(0xBC8001cc) = lli->ctrl;                         // control attr
  vrg(0xBC8001d0) = lli->status;                       // status
  asm volatile("sync");
}

inline void dmacplusInitLcd() {
  *((u32*)0xBC800104) = 0;          // px format
  *((u32*)0xBC800108) = 512;        // buff size
  *((u32*)0xBC80010C) = 512;        // stride
  *((u32*)0xBC800100) = 0x44000000; // buffer addr
}

inline void cleanChannels() {
  vrg(0xBC800190) = 0;
  vrg(0xBC8001b0) = 0;
  vrg(0xBC8001d0) = 0;
  asm volatile("sync");
}

inline void waitChannels() {
  while (vrg(0xBC800190) ||
          vrg(0xBC8001b0) ||
            vrg(0xBC8001d0)) {
    asm volatile("sync");
    sceKernelDelayThread(100);
  };
}
