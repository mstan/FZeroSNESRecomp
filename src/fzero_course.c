#include "fzero_course.h"
#include "sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned u16(const uint8_t *p) {
  return p[0] | (unsigned)p[1] << 8;
}
static uint32_t u24(const uint8_t *p) {
  return u16(p) | (uint32_t)p[2] << 16;
}
static void w16(uint8_t *p, unsigned n) {
  p[0] = (uint8_t)n;
  p[1] = (uint8_t)(n >> 8);
}
static bool fail(char *e, size_t n, const char *s) {
  if (n)
    snprintf(e, n, "%s", s);
  return false;
}
/* LoROM aliases are decoded here. Reads may cross a $ffff/$8000 bank edge,
 * but never wrap the donor image or reach RAM/MMIO. */
static const uint8_t *span(const uint8_t *r, size_t size, uint32_t a, size_t n) {
  if (a > 0xffffff || (a & 0xffff) < 0x8000 || (a & 0x7f0000) >= 0x7e0000)
    return NULL;
  size_t off = ((a & 0x7f0000) >> 1) | (a & 0x7fff);
  return off <= size && n <= size - off ? r + off : NULL;
}
static const uint8_t *table(const uint8_t *r, size_t n, uint32_t a, unsigned i, unsigned stride) {
  return span(r, n, a + i * stride, stride);
}
static bool resource(const uint8_t *r, size_t n, uint32_t a, unsigned i, void *out, size_t len) {
  const uint8_t *p = table(r, n, a, i, 3), *data = p ? span(r, n, u24(p), len) : NULL;
  if (!data)
    return false;
  memcpy(out, data, len);
  return true;
}
bool FzeroCourseLayoutRead(const char *path, FzeroCourseLayout *out, char *error, size_t cap) {
  FILE *f = fopen(path, "rb");
  if (!f)
    return fail(error, cap, "Missing .layout extraction manifest");
  const char *keys[] = {"count",     "pools",    "settings",      "palettes",     "maps",
                        "graphics",  "paths",    "names",         "sky_graphics", "sky_back",
                        "sky_front", "minimaps", "map_positions", "terrain",      "gradients",
                        "opponents", "shortcuts", "palette_cycles"};
  uint32_t vals[18] = {0}, seen = 0;
  uint8_t required = 0, course_required[128] = {0};
  unsigned last_required = 0;
  bool format = false, ok = true;
  char line[160];
  while (ok && fgets(line, sizeof(line), f)) {
    size_t n = strlen(line);
    if (n == sizeof(line) - 1 && line[n - 1] != '\n') {
      ok = false;
      break;
    }
    while (n && (line[n - 1] == '\n' || line[n - 1] == '\r'))
      line[--n] = 0;
    if (!n || line[0] == '#')
      continue;
    char *v = strchr(line, '=');
    if (!v) {
      ok = false;
      break;
    }
    *v++ = 0;
    if (!strcmp(line, "require")) {
      char *feature = strchr(v, '|');
      if (!feature) { ok = false; break; }
      *feature++ = 0;
      unsigned flag = !strcmp(feature, "grip-magnets") ? FZERO_COURSE_GRIP_MAGNETS
                      : !strcmp(feature, "up-magnets") ? FZERO_COURSE_UP_MAGNETS
                      : !strcmp(feature, "rainbow-road") ? FZERO_COURSE_RAINBOW : 0;
      if (!flag) {
        fclose(f);
        return fail(error, cap, "Unsupported required course feature");
      }
      uint8_t *destination = &required;
      if (strcmp(v, "all")) {
        char *end;
        unsigned long slot = strtoul(v, &end, 10);
        if (*v < '0' || *v > '9' || *end || slot >= 128) { ok = false; break; }
        destination = &course_required[slot];
        if (slot + 1 > last_required) last_required = (unsigned)slot + 1;
      }
      if (*destination & flag) { ok = false; break; }
      *destination |= (uint8_t)flag;
      continue;
    }
    if (!strcmp(line, "format")) {
      if (format || strcmp(v, "fzero-course-1"))
        ok = false;
      format = true;
      continue;
    }
    unsigned i;
    for (i = 0; i < 18 && strcmp(line, keys[i]); ++i) {
    }
    if (i == 18 || (seen & (1u << i)) || !*v) {
      ok = false;
      break;
    }
    char *end = NULL;
    unsigned long value = strtoul(v, &end, i ? 16 : 10);
    if (*end || value > 0xffffff || (!i && (!value || value > 128))) {
      ok = false;
      break;
    }
    vals[i] = (uint32_t)value;
    seen |= 1u << i;
  }
  if (ferror(f))
    ok = false;
  fclose(f);
  if (!ok || !format || (seen & 0x1ffff) != 0x1ffff ||
      ((seen & (1u << 17)) && !vals[17]) || last_required > vals[0])
    return fail(error, cap, "Invalid course extraction manifest");
  FzeroCourseLayout l = {vals[0],  vals[1],  vals[2],  vals[3],  vals[4],  vals[5],
                         vals[6],  vals[7],  vals[8],  vals[9],  vals[10], vals[11],
                         vals[12], vals[13], vals[14], vals[15], vals[16], vals[17], 0, {0}};
  l.required = required;
  memcpy(l.course_required, course_required, sizeof(course_required));
  *out = l;
  return true;
}
/* Each entry rotates one eight-color road palette. Never touch the shared
 * vehicle/HUD palette. The donor's palette program is not executed. */
static bool palette_cycles(const uint8_t *r, size_t n, const FzeroCourseLayout *l,
                           unsigned i, FzeroCourse *c) {
  if (!l->palette_cycles)
    return true;
  const uint8_t *p = table(r, n, l->palette_cycles, i, 2);
  if (!p)
    return false;
  uint32_t address = (l->palette_cycles & 0xff0000) | u16(p);
  unsigned seen = 0;
  c->has_palette_cycles = 1;
  for (unsigned j = 0; j <= sizeof(c->palette_cycles); ++j) {
    p = span(r, n, address + j * 2, 2);
    if (!p)
      return false;
    unsigned offset = u16(p);
    if (offset & 0x8000)
      return true;
    if (j == sizeof(c->palette_cycles) || offset < 0x20 || offset > 0xf0 || (offset & 15) ||
        (seen & (1u << (offset >> 4))))
      return false;
    seen |= 1u << (offset >> 4);
    c->palette_cycles[c->palette_cycle_count++] = (uint8_t)(offset - 0x20);
  }
  return false;
}
void FzeroCourseCyclePalette(const FzeroCourse *c, uint8_t palette[0xe0]) {
  for (unsigned i = 0; i < c->palette_cycle_count; ++i) {
    uint8_t *p = palette + c->palette_cycles[i];
    unsigned last = u16(p + 14);
    memmove(p + 2, p, 14);
    w16(p, last);
  }
}
static bool layout_data(const uint8_t *r, size_t n, const uint8_t *entry, uint8_t *out, size_t cap,
                        bool grid, uint16_t *length) {
  unsigned rows = u16(entry + 3);
  size_t stride = grid ? 18 : 16;
  if (!rows || rows > cap / stride)
    return false;
  const uint8_t *p = span(r, n, u24(entry), rows * 16);
  if (!p)
    return false;
  for (unsigned row = 0; row < rows; ++row) {
    if (!grid)
      memcpy(out + row * 16, p + row * 16, 16);
    else {
      unsigned flags = 0;
      for (unsigned j = 0; j < 8; ++j) {
        unsigned v = u16(p + row * 16 + j * 2);
        w16(out + row * 18 + j * 2, v >> 2);
        flags |= (v & 3) << (j * 2);
      }
      w16(out + row * 18 + 16, flags);
    }
  }
  *length = (uint16_t)(rows * stride);
  return true;
}
static bool checkpoints(const uint8_t *r, size_t size, uint32_t address, FzeroCourse *c) {
  const uint8_t *records = span(r, size, address, 1);
  if (!records)
    return false;
  unsigned count = 0;
  bool pit = false, ended = false;
  for (unsigned rec = 0; rec < 256; ++rec) {
    const uint8_t *p = span(r, size, address + rec * 9, 9);
    if (!p)
      return false;
    if (!p[0]) {
      ended = true;
      break;
    }
    unsigned bank = address & 0xff0000;
    const uint8_t *ptr = span(r, size, bank | u16(p + 1), 12);
    if (!ptr)
      return false;
    unsigned start = p[3], end = p[4];
    if (start > end)
      return false;
    const uint8_t *arrays[6];
    for (unsigned a = 0; a < 6; ++a) {
      arrays[a] = span(r, size, bank | u16(ptr + a * 2), end + 1);
      if (!arrays[a])
        return false;
    }
    bool closing = p[0] == 255;
    unsigned at = count + (closing ? 1 : 0);
    if (at + (end - start + 1) > 254)
      return false;
    if (closing) {
      c->finish_checkpoint = (uint8_t)at;
    }
    unsigned x = u16(p + 5), y = u16(p + 7);
    w16(c->path + at * 2, x);
    w16(c->path + 0x200 + at * 2, y);
    for (unsigned j = start; j <= end; ++j, ++at) {
      x = (x + (int8_t)arrays[0][j] * 8) & 65535;
      y = (y + (int8_t)arrays[1][j] * 8) & 65535;
      w16(c->path + 2 + at * 2, x);
      w16(c->path + 0x202 + at * 2, y);
      c->path[0x502 + at] = arrays[2][j];
      c->path[0x602 + at] = arrays[3][j];
      c->path[0x702 + at] = arrays[4][j];
      c->path[0x802 + at] = arrays[5][j];
      if (!pit && (arrays[2][j] & 32)) {
        pit = true;
        c->has_pit = 1;
        c->pit_checkpoint = (uint8_t)at;
      }
    }
    if (!closing)
      count = at;
  }
  if (!ended || !count || count > 254)
    return false;
  c->last_checkpoint = (uint8_t)(count - 1);
  return true;
}
bool FzeroCourseExtract(const uint8_t *r, size_t n, const FzeroCourseLayout *l, unsigned i,
                        FzeroCourse *out, char *e, size_t cap) {
  if (!r || !l || i >= l->count)
    return fail(e, cap, "Course index outside extraction manifest");
  FzeroCourse *c = calloc(1, sizeof(*c));
  if (!c)
    return fail(e, cap, "Out of memory");
  bool ok = resource(r, n, l->pools, i, c->pool, sizeof(c->pool)) &&
            resource(r, n, l->palettes, i, c->palette, sizeof(c->palette)) &&
            resource(r, n, l->sky_graphics, i, c->sky_graphics, sizeof(c->sky_graphics)) &&
            resource(r, n, l->sky_back, i, c->sky_back, sizeof(c->sky_back)) &&
            resource(r, n, l->sky_front, i, c->sky_front, sizeof(c->sky_front)) &&

            resource(r, n, l->terrain, i, c->terrain, sizeof(c->terrain));
  const uint8_t *mini = table(r, n, l->minimaps, i, 3);
  const uint8_t *mini_data = mini ? span(r, n, u24(mini) + 0x8000, sizeof(c->minimap)) : NULL;
  if (!mini_data)
    ok = false;
  else
    memcpy(c->minimap, mini_data, sizeof(c->minimap));
  const uint8_t *maps = table(r, n, l->maps, i, 10), *s = table(r, n, l->settings, i, 1),
                *gradient = table(r, n, l->gradients, i, 1),
                *xy = table(r, n, l->map_positions, i, 4),
                *opponents = table(r, n, l->opponents, i, 3), *gp = table(r, n, l->graphics, i, 3),
                *path = table(r, n, l->paths, i, 3), *name = table(r, n, l->names, i, 3);
  if (!maps || !s || !gradient || !xy || !opponents || !gp || !path || !name)
    ok = false;
  if (ok) {
    c->setting = *s;
    c->gradient = *gradient;
    c->map_x = (uint16_t)u16(xy);
    c->map_y = (uint16_t)u16(xy + 2);
    memcpy(c->opponents, opponents, 3);
    ok = layout_data(r, n, maps, c->blocks, sizeof(c->blocks), false, &c->block_size) &&
         layout_data(r, n, maps + 5, c->grid, sizeof(c->grid), true, &c->grid_size) &&
         checkpoints(r, n, u24(path), c);
    const uint8_t *gfx = span(r, n, u24(gp), 256 * 33);
    if (!gfx)
      ok = false;
    else
      for (unsigned t = 0; t < 256; ++t)
        for (unsigned b = 0; b < 32; ++b) {
          if (gfx[t * 33] & 15)
            ok = false;
          c->graphics[t * 64 + b * 2] = gfx[t * 33] | (gfx[t * 33 + 1 + b] >> 4);
          c->graphics[t * 64 + b * 2 + 1] = gfx[t * 33] | (gfx[t * 33 + 1 + b] & 15);
        }
    const uint8_t *str = span(r, n, u24(name), sizeof(c->name));
    if (!str)
      ok = false;
    else {
      unsigned end = 0;
      while (end < sizeof(c->name) && str[end])
        ++end;
      if (end == sizeof(c->name))
        ok = false;
      else
        memcpy(c->name, str, end);
    }
    const uint8_t *start = table(r, n, l->shortcuts, i, 3);
    const uint8_t *data = start ? span(r, n, u24(start), 2) : NULL;
    if (!data)
      ok = false;
    else {
      bool end = false;
      for (unsigned j = 0; j < 17; ++j) {
        const uint8_t *row = span(r, n, u24(start) + j * 17, 2);
        if (!row) {
          ok = false;
          break;
        }
        if (u16(row) & 0x8000) {
          memcpy(c->shortcuts + j * 17, row, 2);
          end = true;
          break;
        }
        if (j == 16 || !(row = span(r, n, u24(start) + j * 17, 17))) {
          ok = false;
          break;
        }
        memcpy(c->shortcuts + j * 17, row, 17);
      }
      if (!end)
        ok = false;
    }
  }
  if (ok)
    ok = palette_cycles(r, n, l, i, c);
  if (ok) {
    c->required = l->required | l->course_required[i];
    if (c->required & ~FZERO_COURSE_FEATURES) {
      free(c);
      return fail(e, cap, "Unsupported required course feature");
    }
    sha256_compute((const uint8_t *)c, offsetof(FzeroCourse, hash), c->hash);
    if (c->has_palette_cycles) {
      uint8_t extended[32 + 16];
      memcpy(extended, c->hash, 32);
      extended[32] = c->has_palette_cycles;
      extended[33] = c->palette_cycle_count;
      memcpy(extended + 34, c->palette_cycles, 14);
      sha256_compute(extended, sizeof(extended), c->hash);
    }
    if (c->required) {
      uint8_t extended[34];
      memcpy(extended, c->hash, 32);
      extended[32] = 0xc1;
      extended[33] = c->required;
      sha256_compute(extended, sizeof(extended), c->hash);
    }
    *out = *c;
  }
  free(c);
  return ok ? true
            : fail(e, cap, "Course resources contain invalid pointers, sizes or checkpoints");
}
