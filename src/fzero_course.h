#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Decoded resources, never executable donor bytes. One adapter feeds the
 * same retail/Deluxe race engine regardless of the number of installed packs. */
typedef struct FzeroCourseLayout {
  unsigned count;
  uint32_t pools, settings, palettes, maps, graphics, paths, names;
  uint32_t sky_graphics, sky_back, sky_front, minimaps, map_positions;
  uint32_t terrain, gradients, opponents, shortcuts;
} FzeroCourseLayout;
typedef struct FzeroCourse {
  uint8_t pool[0x2400], blocks[0x2200], grid[0x9000];
  uint8_t graphics[0x4000], palette[0xe0];
  uint8_t sky_graphics[0x2000], sky_back[0x700], sky_front[0x540];
  uint8_t minimap[0x200], terrain[0x400], name[128];
  uint8_t path[0xa00]; /* $11fe..$1bfd, normalized checkpoint arrays. */
  uint8_t setting, gradient;
  uint16_t map_x, map_y;
  uint8_t last_checkpoint, finish_checkpoint, pit_checkpoint, has_pit;
  uint8_t opponents[3], shortcuts[17 * 16 + 2];
  uint16_t block_size, grid_size;
  uint8_t hash[32];
} FzeroCourse;
bool FzeroCourseLayoutRead(const char *path, FzeroCourseLayout *out, char *error, size_t cap);
bool FzeroCourseExtract(const uint8_t *rom, size_t size, const FzeroCourseLayout *layout,
                        unsigned index, FzeroCourse *out, char *error, size_t cap);
