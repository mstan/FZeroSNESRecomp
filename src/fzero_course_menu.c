#include "fzero_course_runtime.h"
#include "fzero_tracks.h"
#include "fzero_gameplay.h"
#include "fzero_vehicles.h"
#include "fzero_deluxe.h"
#include "common_rtl.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "fzero_menu_font.inc"

typedef struct Canvas {
  uint32_t *pixels;
  size_t pitch;
  unsigned scale, extra;
} Canvas;
static void box(Canvas c, int x, int y, int w, int h, uint32_t color) {
  x += (int)c.extra;
  for (int yy = y * (int)c.scale; yy < (y + h) * (int)c.scale; ++yy) {
    uint32_t *row = (uint32_t *)((uint8_t *)c.pixels + yy * c.pitch);
    for (int xx = x * (int)c.scale; xx < (x + w) * (int)c.scale; ++xx)
      row[xx] = color;
  }
}
static void text(Canvas c, int x, int y, const char *s, unsigned limit, uint32_t color) {
  for (unsigned i = 0; s[i] && i < limit; ++i) {
    unsigned ch = (unsigned)toupper((unsigned char)s[i]);
    if (ch < 32 || ch > 126)
      ch = '?';
    for (int yy = 0; yy < 8; ++yy)
      for (int xx = 0; xx < 8; ++xx)
        if (FONT8X8[ch - 32][yy] & (1u << xx))
          box(c, x + (int)i * 8 + xx, y + yy, 1, 1, color);
  }
}
void FzeroTracksOverlay(uint32_t *pixels, unsigned width, unsigned height, size_t pitch) {
  FzeroVehiclesOverlay(pixels,width,height,pitch);
  if (!pixels || !FzeroTracksMenuVisible() || height < 224 || height % 224)
    return;
  static bool logged;
  if (!logged) {
    fprintf(stderr, "[track-library] in-game menu overlay %ux%u\n", width, height);
    logged = true;
  }
  unsigned scale = height / 224;
  if (width / scale < 256)
    return;
  Canvas c = {pixels, pitch, scale, (width / scale - 256) / 2};
  unsigned count = FzeroTracksRuntimeCount(), selected = FzeroTracksMenuIndex();
  bool choosing_class = FzeroTracksClassSelected();
  unsigned first = selected < 5 ? 0 : selected - 4;
  box(c, 110, 68, 124, 82, 0xff000000);
  for (unsigned row = 0; row < 5 && first + row < count; ++row) {
    const CpCup *cup = FzeroTracksRuntimeCup(first + row, NULL);
    uint32_t color = first + row == selected ? 0xffc0ffff : 0xff808080;
    const char *label = cup->name;
    size_t length = strlen(label);
    if (length > 13 && first + row == selected) {
      unsigned offset = (unsigned)snes_frame_counter / 20 % (unsigned)(length - 13 + 12);
      offset = offset < 6 ? 0 : offset - 6;
      if (offset > length - 13)
        offset = (unsigned)length - 13;
      label += offset;
    }
    text(c, 126, 72 + (int)row * 16, label, 13, color);
    if (first + row == selected && !choosing_class)
      text(c, 115, 72 + (int)row * 16, ">", 1, 0xffffff00);
  }
  char page[32];
  if (count < 100)
    snprintf(page, sizeof(page), "%u/%u", selected + 1, count);
  else
    snprintf(page, sizeof(page), "%u", selected + 1);
  box(c, 118, 55, 78, 8, 0xff000000);
  text(c, 120, 55, "LEAGUE", 6, 0xffc0ffff);
  box(c, 190, 55, 44, 8, 0xff000000);
  text(c, 234 - (int)strlen(page) * 8, 55, page, 5, 0xff80c8e8);
  box(c, 112, 151, 122, 29, 0xff000000);
  text(c, 120, 151, "CLASS", 5, 0xffffff00);
  static const char *classes[] = {"BEGINNER", "STANDARD", "EXPERT", "MASTER", "LEGEND"};
  unsigned level=g_ram[choosing_class && !FzeroDeluxeActive() ? 0x5a : 0x57];
  text(c, 126, 168, classes[level < 5 ? level : 0], 12, 0xffc0ffff);
  if (choosing_class)
    text(c, 115, 168, ">", 1, 0xffffff00);
  box(c, 106, 197, 140, 8, 0xff000000);
  text(c, 108, 197, choosing_class ? "CHOOSE CLASS" : "< > MORE LEAGUES", 17, 0xff80c8e8);
}
