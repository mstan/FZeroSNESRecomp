#include "fzero_course_runtime.h"
#include "fzero_tracks.h"
#include "fzero_gameplay.h"
#include "fzero_vehicles.h"
#include "fzero_deluxe.h"
#include "fzero_records.h"
#include "snes/cart.h"
extern Snes *g_snes;
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

/* Native records lettering. Keep the three-cup arrangement and ruled rows. */
static void record_text(Canvas c, int x, int y, const char *s, unsigned limit, uint32_t color) {
  for (unsigned i = 0; s[i] && i < limit; ++i) {
    unsigned ch = (unsigned)toupper((unsigned char)s[i]);
    if (ch >= 'A' && ch <= 'Z') {
      const uint8_t *glyph = g_snes->cart->rom + 0x75ea0 + (ch - 'A') * 16;
      for (int yy = 0; yy < 8; ++yy)
        for (int xx = 0; xx < 8; ++xx)
          if (glyph[yy * 2 + 1] & (128u >> xx))
            box(c, x + (int)i * 8 + xx, y + yy, 1, 1, color);
    } else {
      char glyph[2] = {(char)ch, 0};
      text(c, x + (int)i * 8, y, glyph, 1, color);
    }
  }
}
static const char *scroll_label(const char *label, unsigned width, bool selected) {
  size_t n = strlen(label);
  if (selected && n > width) {
    unsigned offset = FzeroRecordsViewState()->label_tick / 20 % (unsigned)(n - width + 12);
    offset = offset < 6 ? 0 : offset - 6;
    label += offset > n - width ? n - width : offset;
  }
  return label;
}
void FzeroRecordsOverlay(uint32_t *pixels, unsigned width, unsigned height, size_t pitch) {
  FzeroRecordsView *v = FzeroRecordsViewState();
  if (!v || !pixels || height < 224 || height % 224 || width / (height / 224) < 256) return;
  bool detail = FzeroRecordsDetail();
  if (g_ram[0x54] || g_ram[0x55] != (FzeroDeluxeActive() ? 3 : detail ? 5 : 3)) return;
  if (FzeroDeluxeActive() && g_ram[0x56] != (detail ? 4 : 0)) return;
  Canvas c = {pixels, pitch, height / 224, (width / (height / 224) - 256) / 2};
  char label[80];
  snprintf(label, sizeof(label), "%s / %s", FzeroVehicleRecordName(v->vehicle), v->practice ? "PRACTICE" : "GP");
  box(c, 0, 0, 256, 16, 0xff000000);
  record_text(c, (256 - (int)strlen(label) * 8) / 2, 4, label, 32, 0xffb8e8ff);
  if (detail) {
    const CpCup *cup = FzeroTracksRuntimeCup(FzeroRecordsCup(), NULL);
    const CpTrack *track = FzeroTracksRuntimeTrack(FzeroRecordsCup(), FzeroRecordsOrder());
    box(c, 8, 76, 120, 38, 0xff000000);
    record_text(c, 16, 82, scroll_label(track->name, 14, true), 14, 0xffffffff);
    record_text(c, 16, 104, scroll_label(cup->name, 14, true), 14, 0xffb8e8ff);
    return;
  }
  static const uint32_t colors[] = {0xffb8fff0, 0xffffffa0, 0xffffc8e0};
  for (unsigned col = 0; col < 3; ++col) {
    int left = col == 2 ? 136 : 16, top = col == 1 ? 128 : 24;
    const CpCup *cup = FzeroTracksRuntimeCup(v->page * 3 + col, NULL);
    box(c, left - 8, top - 4, 128, 94, 0xff000000);
    if (!cup) continue;
    unsigned title_width = (unsigned)strlen(cup->name);
    if (title_width > 14) title_width = 14;
    record_text(c, left + (112 - (int)title_width * 8) / 2, top,
                scroll_label(cup->name, 14, v->selected / 5 == col), 14, colors[col]);
    for (unsigned row = 0; row < 5; ++row) {
      const CpTrack *track = FzeroTracksRuntimeTrack(v->page * 3 + col, row);
      if (!track) continue;
      unsigned slot = col * 5 + row;
      bool selected = v->selected == slot;
      static const unsigned start[] = {5, 0xac, 0x153};
      bool played = (g_sram[start[col] + row * 33] & 15) != 9;
      int y = top + 16 + row * 16;
      snprintf(label, sizeof(label), "%u", row + 1);
      record_text(c, left, y, label, 1, colors[col]);
      record_text(c, left + 16, y, scroll_label(track->name, 12, selected), 12,
          selected ? 0xffffffff : played ? colors[col] : 0xff808080);
      box(c, left, y + 9, 112, 1, colors[col]);
      if (selected) text(c, left - 8, y, ">", 1, 0xffffff00);
    }
  }
  box(c, 128, 128, 128, 32, 0xff000000);
  record_text(c, 144, 144, "EXIT", 4, 0xffffffff);
  if (v->selected == 15) text(c, 136, 144, ">", 1, 0xffffff00);
  box(c, 128, 192, 128, 32, 0xff000000);
  snprintf(label, sizeof(label), "L/R CUPS %u/%u", v->page + 1, (FzeroTracksRuntimeCount() + 2) / 3);
  record_text(c, 144, 192, label, 14, 0xffb8e8ff);
  record_text(c, 144, 204, FzeroVehicleCount() ? "X/Y CAR" : "SHARED TIMES", 14, 0xffb8e8ff);
  if (FzeroVehicleCount() || FzeroBsTracks()) record_text(c, 144, 216, "SELECT GP/PR", 14, 0xffb8e8ff);
}
void FzeroTracksOverlay(uint32_t *pixels, unsigned width, unsigned height, size_t pitch) {
  FzeroRecordsOverlay(pixels, width, height, pitch);
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
  bool practice = g_ram[0x58] != 0;
  bool choosing_class = !practice && FzeroTracksClassSelected();
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
  /* Practice's native LEAGUE label extends below our heading. Clear the
   * entire header down to the first list row before drawing its replacement. */
  box(c, 118, 55, 116, 17, 0xff000000);
  text(c, 120, 55, "LEAGUE", 6, 0xffc0ffff);
  text(c, 234 - (int)strlen(page) * 8, 55, page, 5, 0xff80c8e8);
  box(c, 112, 151, 122, 29, 0xff000000);
  if (practice) {
    box(c, 106, 197, 140, 8, 0xff000000);
    text(c, 108, 197, "UP/DOWN TO SELECT", 17, 0xff80c8e8);
    return;
  }
  text(c, 120, 151, "CLASS", 5, 0xffffff00);
  static const char *classes[] = {"BEGINNER", "STANDARD", "EXPERT", "MASTER", "LEGEND"};
  unsigned level=g_ram[choosing_class && !FzeroDeluxeActive() ? 0x5a : 0x57];
  text(c, 126, 168, classes[level < 5 ? level : 0], 12, 0xffc0ffff);
  if (choosing_class)
    text(c, 115, 168, ">", 1, 0xffffff00);
  box(c, 106, 197, 140, 8, 0xff000000);
  text(c, 108, 197, choosing_class ? "CHOOSE CLASS" : "UP/DOWN TO SELECT", 17, 0xff80c8e8);
}
