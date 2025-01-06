// m-c/d 2025
#include "main.h"

PSP_MODULE_INFO("mdmac", 0, 1, 1);
PSP_HEAP_SIZE_KB(-1024);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_VFPU | PSP_THREAD_ATTR_USER);

static void* lli = nullptr;
static volatile u32* mem = nullptr;
#define meCounter         (mem[0])
#define meStart           (mem[1])
// used to sync drawing
#define syncDrawSc        (mem[2])
#define syncDrawMe        (mem[3])
#define syncTrue          (mem[4])

#define CLOCK_CANVAS_HEIGHT   128
#define SCR_SLICE_BUFF_SIZE   (16 * 4 * 512)

static u8 scDrawBase[SCR_SLICE_BUFF_SIZE * 4] __attribute__((aligned(16)));

__attribute__((noinline, aligned(4)))
static int meLoop() {
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
  } while(!_meExit);
  return _meExit;
}

extern char __start__me_section;
extern char __stop__me_section;
__attribute__((section("_me_section"), noinline, aligned(4)))
void meHandler() {
  vrg(0xbc100004) = 0xffffffff; // enable NMI
  vrg(0xbc100040) = 2;          // mem
  vrg(0xbc100050) = 0x7f;       // enable clocks: ME, AW bus RegA, RegB & Edram, DMACPlus, DMAC
  asm volatile("sync");
  ((FCall)_meLoop)();
}

static int initMe() {
  memcpy((void *)0xbfc00040, (void*)&__start__me_section, me_section_size);
  _meLoop = CACHED_KERNEL_MASK | (u32)&meLoop;
  meDCacheWritebackInvalidAll();
  vrg(0xBC10004C) = 0b100;
  asm volatile("sync");
  vrg(0xBC10004C) = 0x0;
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
  while (1) {
    if (syncDrawSc) {
      drawClockHand(&cfg, a++);
      syncDrawSc = false;
    }
    sceKernelDelayThread(1000);
  }
  return 0;
}

int main() {
  scePowerSetClockFrequency(333, 333, 166);

  if (pspSdkLoadStartModule("ms0:/PSP/GAME/me/kcall.prx", PSP_MEMORY_PARTITION_KERNEL) < 0){
    pspDebugScreenInit();
    exitSample("Can't load the PRX, exiting...");
    return 0;
  }

  kcall(&initMe);
  
  mem = meSetUserMem(10);
  syncDrawSc = false;
  syncDrawMe = false;
  syncTrue = true;

  initLLI();
  
  sceDisplaySetFrameBuf((void*)(0x44000000), 512, 3, PSP_DISPLAY_SETBUF_NEXTFRAME);
  pspDebugScreenInitEx(0, PSP_DISPLAY_PIXEL_FORMAT_8888, 0);
  pspDebugScreenSetOffset(0);

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
  sceKernelDeleteThread(uid);
  
  meSetUserMem(0);
  free(lli);

  pspDebugScreenInit();
  exitSample("Exiting...");
  return 0;
}
