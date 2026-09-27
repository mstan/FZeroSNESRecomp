#include "fzero_course_runtime.h"
#include "fzero_tracks.h"
#include "fzero_gameplay.h"
#include "fzero_vehicles.h"
#include "fzero_deluxe.h"
#include "fzero_records.h"
#include "fzero_native_font.h"
#include "snes/cart.h"
#include "snes/ppu.h"
extern Snes *g_snes;
#include "common_rtl.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "fzero_menu_font.inc"

typedef struct Canvas {
  uint32_t *pixels;
  size_t pitch;
  unsigned scale, extra, brightness;
} Canvas;
static void box(Canvas c, int x, int y, int w, int h, uint32_t color) {
  if (c.brightness < 15)
    color = (color & 0xff000000u) | (((color >> 16 & 255) * c.brightness / 15) << 16) |
        (((color >> 8 & 255) * c.brightness / 15) << 8) | ((color & 255) * c.brightness / 15);
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
static void record_course_name(Canvas c, const FzeroCourse *course, const char *s) {
  static const uint8_t alphabet[] = {
    0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x8e,
    0x6c, 0x6e, 0x6f, 0x8a, 0x8b, 0x8c, 0x8d, 0x6d, 0x8f,
    0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa6, 0xa5, 0xa7};
  static const uint32_t colors[] = {0, 0xff000000, 0xffffffff, 0xff4aff4a};
  unsigned length = 0;
  for (; s[length] && length < 14; ++length) {
    unsigned ch = (unsigned)toupper((unsigned char)s[length]);
    if (ch == ' ') continue;
    unsigned code = ch >= 'A' && ch <= 'Z' ? alphabet[ch - 'A'] :
                    ch >= '0' && ch <= '9' ? 0x80 + ch - '0' : 0;
    const uint8_t *halves[2] = {NULL, NULL};
    if (course)
      for (unsigned g = 0; g < course->intro_glyph_count; ++g)
        if (code && course->intro_glyphs[g].code == code) {
          halves[0] = course->intro_glyphs[g].pixels;
          halves[1] = halves[0] + 16;
          break;
        }
    /* Unused native letter slots contain other sprites. Packs may supply
     * their own glyphs; otherwise use readable small-font lettering below. */
    if (!halves[0] && code && ch != 'J' && ch != 'Q' && ch != 'X' && ch != 'Z')
      for (unsigned h = 0; h < 2; ++h)
        halves[h] = FzeroNativeLetterTile(g_snes->cart->rom, g_snes->cart->romSize, code + h * 16);
    int x = 16 + (int)length * 8;
    if (halves[0] && halves[1]) {
      for (unsigned y = 0; y < 16; ++y)
        for (unsigned px = 0; px < 8; ++px) {
          const uint8_t *row = halves[y / 8] + y % 8 * 2;
          unsigned color = (row[0] >> (7 - px) & 1) | (row[1] >> (7 - px) & 1) << 1;
          if (color) box(c, x + (int)px, 79 + (int)y, 1, 1, colors[color]);
        }
    } else {
      char glyph[] = {(char)ch, 0};
      record_text(c, x, 84, glyph, 1, colors[3]);
    }
  }
  box(c, 16, 98, (int)length * 8, 1, 0xffffffff);
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
static unsigned read_word(const uint8_t *p) { return p[0] | (unsigned)p[1] << 8; }
static uint32_t record_color(unsigned rgb) {
  return 0xff000000u | ((rgb & 31) * 255 / 31 << 16) |
      (((rgb >> 5) & 31) * 255 / 31 << 8) | (((rgb >> 10) & 31) * 255 / 31);
}
static unsigned horizon_pixel(const FzeroCourse *course, const uint8_t *map, unsigned x, unsigned y) {
  unsigned tile = read_word(map + ((x / 256) * 224 + (y / 8) * 32 + x / 8 % 32) * 2);
  if ((tile & 1023) >= 256) return 0;
  unsigned px = x % 8, py = y % 8;
  if (tile & 0x4000) px = 7 - px;
  if (tile & 0x8000) py = 7 - py;
  const uint8_t *graphic = course->sky_graphics + (tile & 255) * 32 + py * 2;
  unsigned color = 0;
  for (unsigned bit = 0; bit < 4; ++bit)
    color |= ((graphic[bit / 2 * 16 + bit % 2] >> (7 - px)) & 1) << bit;
  unsigned palette = (tile >> 10) & 7;
  return color && palette >= 1 ? palette * 16 + color : 0;
}
static void records_art(Canvas canvas, const FzeroRecordsView *view, unsigned cup, unsigned order) {
  const FzeroCourse *course = FzeroTracksCourseAt(cup, order);
  if (course) {
    /* Keep the native 256x40 venue strip. Use the actual course's two horizon
     * layers and palette, rather than tinting a retail venue illustration. */
    for (unsigned y = 0; y < 40; ++y)
      for (unsigned x = 0; x < 256; ++x) {
        unsigned color = horizon_pixel(course, course->sky_back, x, y + 16);
        if (!color) color = horizon_pixel(course, course->sky_front, x, y + 16);
        if (!color) color = 96;
        box(canvas, (int)x, 16 + (int)y, 1, 1,
            record_color(read_word(course->palette + (color - 16) * 2)));
      }
  }
  uint32_t icon[16 * 16];
  if (!FzeroDeluxeActive() || !FzeroVehicleRecordIcon(view->vehicle, icon)) return;
  /* The native detail screen owns eleven 16x16 car reservations: ten ranked
   * times and best lap. Empty rows are hidden at Y=224. Use their positions,
   * but draw the identity owning this records namespace, not its donor slot. */
  for (unsigned row = 0; row < 11; ++row) {
    const uint8_t *oam = g_ram + 0x200 + row * 4;
    unsigned x = oam[0], y = oam[1];
    if (y + 16 > 224 || x + 16 > 256) continue;
    for (unsigned yy = 0; yy < 16; ++yy)
      for (unsigned xx = 0; xx < 16; ++xx)
        box(canvas, (int)(x + xx), (int)(y + yy), 1, 1,
            icon[yy * 16 + xx] ? icon[yy * 16 + xx] : 0xff000000);
  }
}
void FzeroRecordsOverlay(uint32_t *pixels, unsigned width, unsigned height, size_t pitch) {
  FzeroRecordsView *v = FzeroRecordsViewState();
  if (!v || !pixels || height < 224 || height % 224 || width / (height / 224) < 256) return;
  if (g_ram[0x54] || g_ram[0x55] < 2 || g_ram[0x55] > (FzeroDeluxeActive() ? 3 : 5)) return;
  bool detail = v->display_detail != 0;
  unsigned brightness = g_snes->ppu->inidisp;
  Canvas c = {pixels, pitch, height / 224, (width / (height / 224) - 256) / 2,
              brightness & 128 ? 0 : brightness & 15};
  char label[80];
  snprintf(label, sizeof(label), "%s / %s", FzeroVehicleRecordName(v->vehicle), v->practice ? "PRACTICE" : "GP");
  box(c, 0, 0, 256, 16, 0xff000000);
  record_text(c, (256 - (int)strlen(label) * 8) / 2, 4, label, 32, 0xffb8e8ff);
  if (detail) {
    unsigned index = v->page * 3 + v->display_selected / 5, order = v->display_selected % 5;
    records_art(c, v, index, order);
    const CpCup *cup = FzeroTracksRuntimeCup(index, NULL);
    const CpTrack *track = FzeroTracksRuntimeTrack(index, order);
    if (!cup || !track) return;
    box(c, 8, 76, 120, 38, 0xff000000);
    record_course_name(c, FzeroTracksCourseAt(index, order), scroll_label(track->name, 14, true));
    record_text(c, 16, 102, scroll_label(cup->name, 14, true), 14, 0xffffffff);
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
  Canvas c = {pixels, pitch, scale, (width / scale - 256) / 2, 15};
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
