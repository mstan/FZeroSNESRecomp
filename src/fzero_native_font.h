#pragma once
#include <stddef.h>
#include <stdint.h>

/* The native $0F:8000 OBJ stream contains the two-plane course lettering.
 * Read ROM rather than live VRAM, whose tiles change during menu transitions. */
static inline const uint8_t *FzeroNativeLetterTile(const uint8_t *rom, size_t length, unsigned tile) {
  const unsigned sizes[] = {32, 8, 16, 24};
  for (size_t pos = 0x78000; pos + 2 <= length;) {
    unsigned header = rom[pos++], first = rom[pos++];
    unsigned count = header & 63, mode = header >> 6, size = sizes[mode];
    if (!count || first + count > 256 || pos + count * size > length) break;
    if (mode == 2 && tile >= first && tile < first + count)
      return rom + pos + (tile - first) * size;
    pos += count * size;
  }
  return NULL;
}
