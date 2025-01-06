// m-c/d 2025
#include <psppower.h>
#include <pspdisplay.h>
#include <pspsdk.h>
#include <pspkernel.h>
#include <pspctrl.h>
#include <cstring>
#include <malloc.h>
#include <cmath>

#define u8  unsigned char
#define u16 unsigned short int
#define u32 unsigned int

#define vrp          volatile u32*
#define vrg(addr)    (*((vrp)(addr)))

#define ME_EDRAM_BASE         0
#define GE_EDRAM_BASE         0x04000000
#define UNCACHED_USER_MASK    0x40000000
#define CACHED_KERNEL_MASK    0x80000000
#define UNCACHED_KERNEL_MASK  0xA0000000
