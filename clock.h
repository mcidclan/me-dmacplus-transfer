// m-c/d 2025
#pragma once
#include "common.h"

typedef struct {
  u32* buffer;
  u32 bufferWidth;
  u32 bufferHeight;
  u32 startOffset;
  u32 endOffset;
  u32 canvasWidth;
  u32 lineLength;
} BufferConfig;

void drawLine(const BufferConfig* const cfg, u32 x1, u32 y1, u32 x2, u32 y2);
void drawClockHand(const BufferConfig* const cfg, const float angle);
