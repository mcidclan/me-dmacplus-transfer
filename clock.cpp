// m-c/d 2025
#include "clock.h"

void drawLine(const BufferConfig* const cfg, u32 x1, u32 y1, u32 x2, u32 y2) {
  int dx = abs((int)x2 - (int)x1);
  int dy = abs((int)y2 - (int)y1);
  int sx = (x1 < x2) ? 1 : -1;
  int sy = (y1 < y2) ? 1 : -1;
  int err = dx - dy;
  while (true) {
    const u32 bufferIndex = y1 * cfg->bufferWidth + x1;
    if (bufferIndex >= cfg->startOffset && bufferIndex < cfg->endOffset) {
      cfg->buffer[bufferIndex - cfg->startOffset] = 0xFFFFFFFF;
    }
    if (x1 == x2 && y1 == y2) {
      break;
    }
    int e2 = 2 * err;
    if (e2 > -dy) {
      err -= dy;
      x1 += sx;
    }
    if (e2 < dx) {
      err += dx;
      y1 += sy;
    }
  }
}

void cleanBuffer(const BufferConfig* const cfg) {
  const size_t bufferSize = cfg->endOffset - cfg->startOffset;
  memset(&cfg->buffer[0], 0, bufferSize * sizeof(u32));
}

void drawClockHand(const BufferConfig* const cfg, const float angle) {
  constexpr float pi = 3.14159f;
  
  cleanBuffer(cfg);
  const u32 centerX = cfg->canvasWidth / 2;
  const u32 centerY = cfg->bufferHeight / 2;
  const float deg = 5.0f;

  // Main line
  float rad = angle * pi / 180.0f;
  const u32 mainEndX = centerX + (u32)(cosf(rad) * cfg->lineLength);
  const u32 mainEndY = centerY + (u32)(sinf(rad) * cfg->lineLength);
  // drawLine(cfg, centerX, centerY, mainEndX, mainEndY);

  const float len = cfg->lineLength * 0.80f;

  rad = (angle + deg) * pi / 180.0f;
  const u32 rightEndX = centerX + (u32)(cosf(rad) * len);
  const u32 rightEndY = centerY + (u32)(sinf(rad) * len);
  drawLine(cfg, centerX, centerY, rightEndX, rightEndY);

  rad = (angle - deg) * pi / 180.0f;
  const u32 leftEndX = centerX + (u32)(cosf(rad) * len);
  const u32 leftEndY = centerY + (u32)(sinf(rad) * len);
  drawLine(cfg, centerX, centerY, leftEndX, leftEndY);

  drawLine(cfg, mainEndX, mainEndY, rightEndX, rightEndY);
  drawLine(cfg, mainEndX, mainEndY, leftEndX, leftEndY);
}