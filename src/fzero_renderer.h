#pragma once

#include "fzero_video.h"
#include "snes/ppu.h"

void FzeroRendererReset(void);
/* Join rendering workers on host shutdown. Reset/state loads reuse the pool. */
void FzeroRendererShutdown(void);
unsigned FzeroRendererWorkerCount(void);
void FzeroRendererBeginFrame(const uint8_t ram[0x20000], unsigned frame);
void FzeroRendererCaptureLine(const Ppu *ppu, unsigned line);
void FzeroRendererEndFrame(const Ppu *ppu, const uint32_t stock[256 * 224]);
bool FzeroRendererDraw(uint32_t *output, FzeroViewport viewport, double alpha);
/* capacity is in pixels. Native UI/OBJ stay crisp; only Mode 7 is resampled. */
bool FzeroRendererDrawHd(uint32_t *output, size_t capacity,
                         FzeroViewport viewport, double alpha, unsigned scale);
#define FZERO_RENDERER_COMBINED_PRESENTATION 1
/* Produce the native thumbnail/rewind frame and HD presentation together.
 * native may be NULL; otherwise it holds width*224 pixels. Buffers must not
 * overlap. hd_capacity is in pixels. Native composition stays exact. */
bool FzeroRendererDrawPresentation(uint32_t *native, uint32_t *hd, size_t hd_capacity,
                                   FzeroViewport viewport, double alpha, unsigned scale);
bool FzeroRendererHasFrame(void);
bool FzeroRendererLoadCapture(const char *path);
const uint32_t *FzeroRendererStockFrame(void);
