// m-c/d 2025
#include "main.h"

PSP_MODULE_INFO("mdmac", 0, 1, 1);
PSP_HEAP_SIZE_KB(-1024);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_VFPU | PSP_THREAD_ATTR_USER);

static void* lli = nullptr;
static volatile u32* mem = nullptr;
#define meStart           (mem[0])
#define meStop            (mem[1])
#define meCounter         (mem[2])

// used to sync drawing
#define syncDrawSc        (mem[3])
#define syncDrawMe        (mem[4])
#define syncTrue          (mem[5])

#define CLOCK_CANVAS_HEIGHT   128
#define SCR_SLICE_BUFF_SIZE   (16 * 4 * 512)

static u8 scDrawBase[SCR_SLICE_BUFF_SIZE * 4] __attribute__((aligned(16)));

static void meExit() {
  meStop = 1;
  do {
    asm volatile("sync");
  } while(meStop < 2);
}

u32 scThreadStop = 0;
static void scThreadExit() {
  scThreadStop = 1;
  do {
    sceKernelDelayThread(10);
  } while (scThreadStop < 2);
}

__attribute__((noinline, aligned(4)))
static void meLoop() {
  do {
    meDCacheWritebackInvalidAll();
  } while(!mem || !meStart);
  
  // drawClockMe
  const BufferConfig cfg = {
    .buffer = (u32*)(ME_EDRAM_BASE | UNCACHED_KERNEL_MASK),
    .bufferWidth = 512,
    .bufferHeight = CLOCK_CANVAS_HEIGHT,
    .startOffset = 0,
    .endOffset = 512 * CLOCK_CANVAS_HEIGHT / 2,
    .canvasWidth = 480,
    .lineLength = 50,
  };
  
  float angle = 0.0f;
  do {
    if (syncDrawMe) {
      drawClockHand(&cfg, angle++);
      syncDrawMe = false;
    }
    meCounter++;
  } while(meStop == 0);
  meStop = 2;
  meHalt();
}

extern char __start__me_section;
extern char __stop__me_section;
__attribute__((section("_me_section")))
void meHandler() {
  hw(0xbc100050) = 0x0f;        // Enable ME, AW bus (RegA, RegB & Edram) clocks
  hw(0xbc100004) = 0xffffffff;  // Enable NMIs
  hw(0xbc100040) = 0x02;        // Allows 64MB ram
  asm("sync");
  
  asm volatile(
    "li          $k0, 0x30000000\n"
    "mtc0        $k0, $12\n"
    "sync\n"
    "la          $k0, %0\n"
    "li          $k1, 0x80000000\n"
    "or          $k0, $k0, $k1\n"
    "jr          $k0\n"
    "nop\n"
    :
    : "i" (meLoop)
    : "k0"
  );
}

static int meInit() {
  #define me_section_size (&__stop__me_section - &__start__me_section)
  memcpy((void *)ME_HANDLER_BASE, (void*)&__start__me_section, me_section_size);
  sceKernelDcacheWritebackInvalidateAll();
  // Hardware reset the media engine
  hw(0xbc10004c) = 0x04;
  hw(0xbc10004c) = 0x0;
  asm volatile("sync");
  return 0;
}

void initLLI() {
  lli = memalign(16, sizeof(DMADescriptor) * 6);
  sceKernelDcacheWritebackInvalidateAll();

  DMADescriptor* const _lli = (DMADescriptor*)(UNCACHED_USER_MASK | (u32)lli);
  
  // me lli for sync
  _lli[2] = {
    .src = ((u32)&syncTrue),
    .dst = ((u32)&syncDrawMe),
    .next = 0,
    .ctrl = DMA_CONTROL_ME2SC(0, 4), // see note
    .status = 1,
  };
  
  // me lli for drawing
  {
    const u32 control = DMA_CONTROL_ME2SC(4, 4095);
    _lli[1] = {
      .src = ME_EDRAM_BASE + SCR_SLICE_BUFF_SIZE * 2,
      .dst = GE_EDRAM_BASE + SCR_SLICE_BUFF_SIZE * 3,
      .next = ((u32)&(_lli[2])),
      .ctrl = control,
      .status = 1,
    };
    
    _lli[0] = {
      .src = ME_EDRAM_BASE,
      .dst = GE_EDRAM_BASE + SCR_SLICE_BUFF_SIZE,
      .next = ((u32)&(_lli[1])),
      .ctrl = control,
      .status = 1,
    };
  }
  
  // sc lli for sync
  _lli[5] = {
    .src = ((u32)&syncTrue),
    .dst = ((u32)&syncDrawSc),
    .next = 0,
    .ctrl = DMA_CONTROL_SC2SC(0, 4), // see note
    .status = 1,
  };
  
  // sc lli for drawing
  {
    const u32 control = DMA_CONTROL_SC2SC(4, 4095);
    _lli[4] = {
      .src = ((u32)&scDrawBase) + SCR_SLICE_BUFF_SIZE * 2,
      .dst = GE_EDRAM_BASE + SCR_SLICE_BUFF_SIZE * 7,
      .next = (u32)&(_lli[5]),
      .ctrl = control,
      .status = 1,
    };
    _lli[3] = {
      .src = ((u32)&scDrawBase),
      .dst = GE_EDRAM_BASE + SCR_SLICE_BUFF_SIZE * 5,
      .next = (u32)&(_lli[4]),
      .ctrl = control,
      .status = 1,
    };
  }
}

int gather() {
  if (!syncDrawSc && !syncDrawMe) {
    cleanChannels();
    DMADescriptor* const _lli = (DMADescriptor*)(UNCACHED_USER_MASK | (u32)lli);
    dmacplusLLIFromMe(&(_lli[0]));
    dmacplusLLIOverSc(&(_lli[3]));
    waitChannels();
  }
  return 1;
}


void exitSample(const char* const str) {
  pspDebugScreenClear();
  pspDebugScreenSetXY(0, 1);
  pspDebugScreenPrintf(str);
  sceKernelDelayThread(1000000);
  sceKernelExitGame();
}

int drawClockSc(unsigned int args, void *argp) {
  (void)args;
  (void)argp;
  const BufferConfig cfg = {
    .buffer = (u32*)(UNCACHED_USER_MASK | (u32)scDrawBase),
    .bufferWidth = 512,
    .bufferHeight = CLOCK_CANVAS_HEIGHT,
    .startOffset = (512 * CLOCK_CANVAS_HEIGHT / 2),
    .endOffset = 512 * CLOCK_CANVAS_HEIGHT,
    .canvasWidth = 480,
    .lineLength = 50,
  };
  float a = 0.0f;
  do {
    if (syncDrawSc) {
      drawClockHand(&cfg, a++);
      syncDrawSc = false;
    }
    sceKernelDelayThread(1000);
  } while (scThreadStop == 0);
  
  scThreadStop = 2;
  sceKernelExitDeleteThread(0);
  return 0;
}

int main() {
  scePowerSetClockFrequency(333, 333, 166);

  sceDisplaySetFrameBuf((void*)(UNCACHED_USER_MASK | GE_EDRAM_BASE), 512, 3, PSP_DISPLAY_SETBUF_NEXTFRAME);
  pspDebugScreenInitEx(0, PSP_DISPLAY_PIXEL_FORMAT_8888, 0);
  pspDebugScreenSetOffset(0);

  if (pspSdkLoadStartModule("ms0:/PSP/GAME/me/kcall.prx", PSP_MEMORY_PARTITION_KERNEL) < 0){
    exitSample("Can't load the PRX, exiting...");
    return 0;
  }

  kcall(&meInit);
  
  meGetUncached32(&mem, 10);
  syncDrawSc = false;
  syncDrawMe = false;
  syncTrue = true;

  initLLI();
  
  const SceUID uid = sceKernelCreateThread("scClockHand", drawClockSc,
    0x20, 0x10000, PSP_THREAD_ATTR_USER | PSP_THREAD_ATTR_VFPU, 0);
  if (uid >= 0) {
   sceKernelStartThread(uid, 0, 0);
  }
  
  meStart = 1;
  SceCtrlData ctl;
  
  do {
    sceCtrlPeekBufferPositive(&ctl, 1);
    
    sceDisplayWaitVblankStart();
    pspDebugScreenSetXY(0, 1);
    pspDebugScreenPrintf("Counter: %u    ", meCounter);
    
    kcall((FCall)(CACHED_KERNEL_MASK | (u32)&gather)); // gather and sync
  } while(!(ctl.Buttons & PSP_CTRL_HOME));
  
  meExit();
  scThreadExit();
  meGetUncached32(&mem, 0);
  free(lli);

  exitSample("Exiting...");
  return 0;
}
