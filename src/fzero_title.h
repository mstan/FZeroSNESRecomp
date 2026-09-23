#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Presentation resources only, extracted using an artwork-only patch against
 * the verified stock input. The original/BS engine still runs the title. */
void FzeroTitleReset(void);
bool FzeroTitlePrepare(const uint8_t *stock, size_t size, const char *patch,
                       char *error, size_t error_size);
const uint8_t *FzeroTitleHash(void);
void FzeroTitleLoad(uint16_t vram[0x8000], uint8_t palette[0x80]);
