#include "fzero_course.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x)                                                                                   \
  do {                                                                                             \
    if (!(x)) {                                                                                    \
      fprintf(stderr, "%d: %s\n", __LINE__, #x);                                                   \
      exit(1);                                                                                     \
    }                                                                                              \
  } while (0)
static uint8_t rom[0x40000];
static unsigned next = 0x1000;
static unsigned addr(unsigned off) {
  return (off / 0x8000) * 0x10000 + 0x8000 + off % 0x8000;
}
static unsigned off(unsigned a) {
  return ((a & 0x7f0000) >> 1) | (a & 0x7fff);
}
static void word(unsigned o, unsigned n) {
  rom[o] = (uint8_t)n;
  rom[o + 1] = (uint8_t)(n >> 8);
}
static void pointer(unsigned o, unsigned n) {
  word(o, n);
  rom[o + 2] = (uint8_t)(n >> 16);
}
static unsigned allocate(unsigned n) {
  unsigned result = next;
  next += n;
  CHECK(next < sizeof(rom));
  return result;
}
static unsigned resource(unsigned size) {
  unsigned t = allocate(3), data = allocate(size);
  pointer(t, addr(data));
  return addr(t);
}
int main(void) {
  FzeroCourseLayout l = {0};
  l.count = 1;
  l.pools = resource(0x2400);
  l.palettes = resource(0xe0);
  l.sky_graphics = resource(0x2000);
  l.sky_back = resource(0x700);
  l.sky_front = resource(0x540);
  l.terrain = resource(0x400);
  l.minimaps = resource(0x200); /* This format uses a bank-relative offset. */
  rom[off(l.minimaps) + 1] &= 0x7f;
  l.settings = addr(allocate(1));
  rom[off(l.settings)] = 0xc7;
  l.gradients = addr(allocate(1));
  l.map_positions = addr(allocate(4));
  l.opponents = addr(allocate(3));
  unsigned maps = allocate(10), blocks = allocate(16), grid = allocate(16);
  l.maps = addr(maps);
  pointer(maps, addr(blocks));
  word(maps + 3, 1);
  pointer(maps + 5, addr(grid));
  word(maps + 8, 1);
  word(grid, 0x123);
  l.graphics = resource(256 * 33);
  l.names = resource(128);
  l.shortcuts = resource(2);
  unsigned shortcut = off(l.shortcuts);
  unsigned sd = rom[shortcut] | rom[shortcut + 1] << 8 | rom[shortcut + 2] << 16;
  word(off(sd), 0xffff);
  /* Keep same-bank path records, pointers and arrays together. */
  next = 0x10000;
  l.paths = resource(18);
  unsigned t = off(l.paths), segments = off(rom[t] | rom[t + 1] << 8 | rom[t + 2] << 16);
  unsigned ptrs = allocate(12), arrays = allocate(6);
  rom[segments] = 1;
  word(segments + 1, addr(ptrs) & 65535);
  word(segments + 5, 100);
  word(segments + 7, 200);
  for (unsigned i = 0; i < 6; ++i)
    word(ptrs + i * 2, addr(arrays + i) & 65535);
  rom[arrays] = 1;
  rom[arrays + 1] = 255;
  rom[arrays + 2] = 32;
  FzeroCourse *c = malloc(sizeof(*c)), *previous = malloc(sizeof(*c));
  CHECK(c && previous);
  char error[256];
  CHECK(FzeroCourseExtract(rom, sizeof(rom), &l, 0, c, error, sizeof(error)));
  CHECK(c->grid[0] == 0x48 && c->grid[1] == 0 && c->grid[16] == 3);
  CHECK(c->path[0] == 100 && c->path[2] == 108 && c->path[0x202] == 192);
  CHECK(c->has_pit && c->pit_checkpoint == 0); /* Checkpoint zero is a valid pit. */
  *previous = *c;
  unsigned cycles = allocate(2), entries = allocate(8);
  l.palette_cycles = addr(cycles);
  word(cycles, addr(entries) & 65535);
  word(entries, 0x20);
  word(entries + 2, 0xf0);
  word(entries + 4, 0xffff);
  CHECK(FzeroCourseExtract(rom, sizeof(rom), &l, 0, c, error, sizeof(error)));
  CHECK(c->has_palette_cycles && c->palette_cycle_count == 2);
  CHECK(memcmp(c->hash, previous->hash, 32));
  uint8_t colors[0xe0], before[0xe0];
  for (unsigned i = 0; i < sizeof(colors); ++i)
    before[i] = colors[i] = (uint8_t)i;
  FzeroCourseCyclePalette(c, colors);
  CHECK(colors[0] == 14 && colors[1] == 15 && colors[2] == 0);
  CHECK(colors[0xd0] == 0xde && colors[0xd2] == 0xd0);
  CHECK(!memcmp(colors + 0x10, before + 0x10, 0xc0));
  *previous = *c;
  word(entries, 0x10); /* Shared HUD/car palette is forbidden. */
  CHECK(!FzeroCourseExtract(rom, sizeof(rom), &l, 0, c, error, sizeof(error)));
  word(entries, 0x21);
  CHECK(!FzeroCourseExtract(rom, sizeof(rom), &l, 0, c, error, sizeof(error)));
  word(entries, 0xf0); /* Duplicates would rotate twice. */
  CHECK(!FzeroCourseExtract(rom, sizeof(rom), &l, 0, c, error, sizeof(error)));
  word(entries, 0x20);
  word(entries + 4, 0x100);
  CHECK(!FzeroCourseExtract(rom, sizeof(rom), &l, 0, c, error, sizeof(error)));
  word(entries + 4, 0xffff);
  word(cycles, 0x1234); /* RAM/MMIO pointer. */
  CHECK(!FzeroCourseExtract(rom, sizeof(rom), &l, 0, c, error, sizeof(error)));
  word(cycles, addr(entries) & 65535);
  CHECK(!FzeroCourseExtract(rom, sizeof(rom), &l, 1, c, error, sizeof(error)));
  word(maps + 3, 0xffff);
  CHECK(!FzeroCourseExtract(rom, sizeof(rom), &l, 0, c, error, sizeof(error)));
  word(maps + 3, 1);
  pointer(off(l.pools), 0x7e8000);
  CHECK(!FzeroCourseExtract(rom, sizeof(rom), &l, 0, c, error, sizeof(error)));
  CHECK(
      !memcmp(c, previous, sizeof(*c))); /* Failed extraction never publishes partial resources. */
  free(c);
  free(previous);
  puts("Typed extraction, LoROM bounds, row expansion and checkpoint-zero pit passed");
  return 0;
}
