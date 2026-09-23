#include "fzero_vehicles.h"
#include "common_rtl.h"
#include "content_pack.h"
#include "cpu_state.h"
#include "fzero_gameplay.h"
#include "fzero_menu_font.inc"
#include "sha256.h"
#include "snes/cart.h"
#include "snes/interp_bridge.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern Snes *g_snes;

/* Stable identities are independent of the four guest racing slots. A race
 * uses the selected ship's donor cohort, retaining three native main rivals.
 * All enabled identities remain in one selector. Only curated vehicle data
 * and independently assembled mechanics enter the canonical cartridge. */
typedef struct Vehicle {
  const char *id, *name;
  unsigned group, slot;
} Vehicle;
static const Vehicle vehicles[] = {{"blue-falcon", "BLUE FALCON", 0, 0},
                                   {"wild-goose", "WILD GOOSE", 0, 1},
                                   {"golden-fox", "GOLDEN FOX", 0, 2},
                                   {"fire-stingray", "FIRE STINGRAY", 0, 3},
                                   {"moon-shadow", "MOON SHADOW", 1, 0},
                                   {"dragon-bird", "DRAGON BIRD", 1, 1},
                                   {"great-star", "GREAT STAR", 1, 2},
                                   {"death-anchor", "DEATH ANCHOR", 1, 3},
                                   {"p-emerald", "P. EMERALD", 2, 1},
                                   {"black-bull", "BLACK BULL", 2, 3},
                                   {"white-cat", "WHITE CAT", 3, 0},
                                   {"red-gazelle", "RED GAZELLE", 3, 2}};
static const unsigned cohort[4][4] = {
    {0, 1, 2, 3}, {4, 5, 6, 7}, {0, 8, 2, 9}, {10, 1, 11, 3}};
static const unsigned menu_order[] = {0, 2, 1, 3, 4, 6, 5, 7, 8, 9, 10, 11};
static uint8_t *stock_image, *art[3], *images[4];
static FzeroGameplaySettings choices, image_settings[4];
static unsigned roster[12], count, selected_image = 99;
static uint16_t last_input;
static unsigned hold;
static uint8_t race_acceleration[4][4][29], race_turn[4][4][30];
enum { IMAGE_SIZE = 0x400000, ID_ADDRESS = 0x14dff };

bool FzeroVehiclesActive(void) {
  const FzeroGameplaySettings *s = FzeroGameplaySettingsCurrent();
  return s->vehicle_packs || s->stock_rebalance;
}
unsigned FzeroVehicleCount(void) { return count; }
static bool enabled(unsigned id) {
  return id < 4 || (id < 12 &&
                    (choices.vehicle_packs & (1u << (vehicles[id].group - 1))));
}
unsigned FzeroVehicleSelected(void) {
  unsigned id = g_ram[ID_ADDRESS];
  return id < 12 && enabled(id) ? id : 0;
}
const char *FzeroVehicleIdentity(void) {
  return count ? vehicles[FzeroVehicleSelected()].id : NULL;
}
static unsigned group_for(unsigned id) {
  if (id >= 4)
    return vehicles[id].group;
  return (choices.stock_rebalance & (1u << id)) ? (id == 0 || id == 2 ? 2 : 3)
                                                : 0;
}
static bool needed(unsigned group) {
  return (choices.vehicle_packs & (1u << (group - 1))) ||
         (group == 2 && (choices.stock_rebalance & 5)) ||
         (group == 3 && (choices.stock_rebalance & 10));
}
static bool manifest(unsigned group, CpPack *pack, char *error, size_t cap) {
  char path[96], line[256];
  snprintf(path, sizeof(path), "assets/vehicle-packs/cgp-p%u.ini", group);
  FILE *f = fopen(path, "rb");
  if (!f) {
    snprintf(error, cap, "Missing CGP P%u vehicle manifest", group);
    return false;
  }
  memset(pack, 0, sizeof(*pack));
  unsigned seen = 0, slots = 0;
  bool ok = true;
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
    unsigned bit = !strcmp(line, "format")          ? 1
                   : !strcmp(line, "id")            ? 2
                   : !strcmp(line, "profile")       ? 4
                   : !strcmp(line, "source_sha256") ? 8
                   : !strcmp(line, "target_sha256") ? 16
                                                    : 0;
    if (bit) {
      if (seen & bit) {
        ok = false;
        break;
      }
      seen |= bit;
      char expected[24];
      snprintf(expected, sizeof(expected), "cgp-p%u", group);
      if (bit == 1)
        ok = !strcmp(v, "fzero-vehicles-1");
      if (bit == 2)
        ok = !strcmp(v, expected);
      if (bit == 4)
        ok = strlen(v) == 1 && v[0] == '0' + (int)group;
      if (bit == 8)
        ok = cp_hash_parse(v, pack->source_hash) != 0;
      if (bit == 16)
        ok = cp_hash_parse(v, pack->target_hash) != 0;
    } else if (!strcmp(line, "vehicle")) {
      char *name = strchr(v, '|'), *slot = name ? strchr(name + 1, '|') : NULL;
      char *role = slot ? strchr(slot + 1, '|') : NULL;
      if (!name || !slot || !role) {
        ok = false;
        break;
      }
      *name++ = *slot++ = *role++ = 0;
      if (strlen(slot) != 1 || *slot < '0' || *slot > '3') {
        ok = false;
        break;
      }
      unsigned index = (unsigned)(*slot - '0'), id = cohort[group][index];
      if (slots & (1u << index)) {
        ok = false;
        break;
      }
      slots |= 1u << index;
      for (char *p = name; *p; ++p)
        *p = (char)toupper((unsigned char)*p);
      ok = !strcmp(v, vehicles[id].id) && !strcmp(name, vehicles[id].name) &&
           !strcmp(role, id < 4 ? "rebalance" : "new");
    } else
      ok = false;
  }
  if (ferror(f))
    ok = false;
  fclose(f);
  if (!ok || seen != 31 || slots != 15) {
    snprintf(error, cap, "Invalid CGP P%u vehicle manifest", group);
    return false;
  }
  return true;
}
bool FzeroVehiclesLoad(const uint8_t *stock, size_t size, char *error,
                       size_t cap) {
  free(stock_image);
  stock_image = NULL;
  for (unsigned i = 0; i < 3; ++i) {
    free(art[i]);
    art[i] = NULL;
  }
  for (unsigned i = 0; i < 4; ++i) {
    free(images[i]);
    images[i] = NULL;
  }
  count = 0;
  selected_image = 99;
  last_input = 0;
  hold = 0;
  choices = *FzeroGameplaySettingsCurrent();
  if (!FzeroVehiclesActive())
    return true;
  if (size != 0x80000 || choices.vehicle_packs > 7 ||
      choices.stock_rebalance > 15) {
    snprintf(error, cap, "Invalid vehicle catalog input");
    return false;
  }
  stock_image = malloc(size);
  if (!stock_image) {
    snprintf(error, cap, "Cannot allocate vehicle source");
    return false;
  }
  memcpy(stock_image, stock, size);
  for (unsigned group = 1; group <= 3; ++group) {
    if (!needed(group))
      continue;
    CpPack pack;
    char path[96];
    size_t length = 0;
    snprintf(path, sizeof(path), "assets/vehicle-packs/cgp-p%u.ips", group);
    if (!manifest(group, &pack, error, cap) ||
        !cp_pack_apply(&pack, stock, size, path, &art[group - 1], &length,
                       error, cap))
      return false;
    if (length != size) {
      snprintf(error, cap, "Invalid vehicle artwork size");
      return false;
    }
    if (memcmp(art[group - 1] + 0x5ec00, stock + 0x5ec00, 0x400) ||
        memcmp(art[group - 1] + 0x46f80, stock + 0x46f80, 0x80)) {
      snprintf(error, cap, "Vehicle artwork changes shared HUD/fog resources");
      return false;
    }
  }
  for (unsigned i = 0; i < 12; ++i)
    if (enabled(menu_order[i]))
      roster[count++] = menu_order[i];
  return true;
}
static void copy_changes(uint8_t *out, const uint8_t *source, unsigned start,
                         unsigned size) {
  for (unsigned i = start; i < start + size; ++i)
    if (source[i] != stock_image[i])
      out[i] = source[i];
}
static void copy_stats(uint8_t *out, const uint8_t *source, unsigned slot) {
  static const unsigned fields[][2] = {
      {0xfa81, 1}, {0xfa85, 1}, {0xfa89, 1}, {0xfa8d, 1}, {0xfa91, 2},
      {0xfa99, 2}, {0xfaa1, 2}, {0xfaa9, 1}, {0xfaad, 1}, {0xfab1, 1},
      {0xfab5, 1}, {0xfab9, 1}, {0xfabd, 1}, {0xfac1, 1}, {0xfac5, 2},
      {0xfacd, 2}, {0xfad5, 2}, {0xfadd, 2}, {0xfae5, 2}, {0xfaed, 1}};
  unsigned field = 0x47;
  for (unsigned i = 0; i < sizeof(fields) / sizeof(*fields); ++i) {
    unsigned off = fields[i][0] - 0x8000 + slot * fields[i][1];
    memcpy(out + off, source + off, fields[i][1]);
    memcpy(out + 0xf0000 + slot * 256 + field, source + off, fields[i][1]);
    field += fields[i][1];
  }
}
bool FzeroVehiclesPrepare(uint8_t **rom, size_t *size, char *error,
                          size_t cap) {
  if (!count) {
    bool ok = FzeroGameplayPrepare(rom, size);
    if (!ok)
      snprintf(error, cap, "%s", FzeroGameplayError());
    return ok;
  }
  size_t base_size = *size;
  uint8_t *base = malloc(base_size);
  if (!base) {
    snprintf(error, cap, "Cannot allocate vehicle adapter");
    return false;
  }
  memcpy(base, *rom, base_size);
  /* The new catalog's original identities come from retail, independently
   * of incidental differences in the Deluxe metadata conversion. The legacy
   * BS mode takes the separate no-catalog path and retains its baseline. */
  for (unsigned slot = 0; slot < 4; ++slot) {
    copy_stats(base, stock_image, slot);
    memcpy(base + 0xf0063 + slot * 256, stock_image + 0x149ab + slot * 19, 19);
    memcpy(base + 0xf0076 + slot * 256, stock_image + 0x14a37 + slot * 19, 19);
  }
  uint8_t identity[4 * 32 + 8] = {1, 0, 0, 0};
  identity[4] = (uint8_t)choices.vehicle_packs;
  identity[5] = (uint8_t)choices.stock_rebalance;
  bool tracks = FzeroBsTracks(), ok = true;
  for (unsigned group = 0; group < 4 && ok; ++group) {
    if (group && !needed(group))
      continue;
    image_settings[group] = choices;
    image_settings[group].enabled &= ~7u;
    if (group) {
      image_settings[group].enabled |= 7;
      image_settings[group].tuning = image_settings[group].boost =
          image_settings[group].exhaust = group - 1;
    }
    size_t length = IMAGE_SIZE;
    images[group] = calloc(1, length);
    if (!images[group]) {
      ok = false;
      break;
    }
    memcpy(images[group], base, base_size);
    FzeroGameplayConfigure(&image_settings[group], false, tracks);
    if (group) {
      for (unsigned slot = 0; slot < 4; ++slot) {
        const uint8_t *donor = art[group - 1];
        copy_changes(images[group], donor, 0x40000 + slot * 0x8000, 0x8000);
        copy_changes(images[group], donor, 0x76200 + slot * 0x180, 0x180);
        copy_changes(images[group], donor, 0x7cd80 + slot * 32, 32);
      }
    }
    ok = FzeroGameplayPrepare(&images[group], &length);
    if (!ok)
      break;
  }
  /* Compose each racing slot by identity, including independently selected
   * retail rebalances. Disabled packs cannot supply hidden CPU opponents. */
  uint8_t *raw[4];
  memcpy(raw, images, sizeof(raw));
  memset(images, 0, sizeof(images));
  for (unsigned group = 0; group < 4 && ok; ++group) {
    if (!raw[group])
      continue;
    images[group] = malloc(IMAGE_SIZE);
    if (!images[group]) {
      ok = false;
      break;
    }
    memcpy(images[group], raw[group], IMAGE_SIZE);
    unsigned common = 0;
    for (unsigned slot = 0; slot < 4; ++slot) {
      unsigned id = cohort[group][slot];
      if (!enabled(id))
        id = slot;
      unsigned source = group_for(id);
      if (source)
        common = source;
      uint8_t *out = images[group];
      /* Undo only the original cohort's owned artwork delta, preserving the
       * independently assembled HUD and common mechanics in this image. */
      unsigned starts[] = {0x40000 + slot * 0x8000, 0x76200 + slot * 0x180,
                           0x7cd80 + slot * 32};
      unsigned lengths[] = {0x8000, 0x180, 32};
      for (unsigned region = 0; region < 3; ++region) {
        unsigned start = starts[region], length = lengths[region];
        if (group)
          for (unsigned i = start; i < start + length; ++i)
            if (art[group - 1][i] != stock_image[i])
              out[i] = base[i];
        if (source)
          copy_changes(out, art[source - 1], start, length);
      }
      copy_stats(out, raw[source], slot);
      memcpy(out + 0xf0000 + slot * 256, raw[source] + 0xf0000 + slot * 256,
             256);
      const uint8_t *record = raw[source] + 0xf0000 + slot * 256;
      if (source) {
        memcpy(race_turn[group][slot], raw[source] + 0x149ab + slot * 30, 30);
        memcpy(race_acceleration[group][slot],
               raw[source] + 0x14a42 + slot * 29, 29);
      } else {
        memcpy(race_turn[group][slot], record + 0x63, 19);
        memcpy(race_turn[group][slot] + 19, base + 0x149f7 + 19, 11);
        memcpy(race_acceleration[group][slot], record + 0x76, 19);
        memset(race_acceleration[group][slot] + 19, 0, 10);
      }
      if (group) {
        memcpy(out + 0x149ab + slot * 30, race_turn[group][slot], 30);
        memcpy(out + 0x14a42 + slot * 29, race_acceleration[group][slot], 29);
      }
      out[0xf00ff + slot * 256] = (uint8_t)id;
      /* Native name labels are fixed 16-byte, zero-terminated fields. */
      unsigned row = slot == 1 ? 2 : slot == 2 ? 1 : slot;
      char *label = (char *)images[group] + 0xf5b20 + row * 16;
      memset(label, ' ', 15);
      label[15] = 0;
      size_t n = strlen(vehicles[id].name);
      memcpy(label + (15 - n) / 2, vehicles[id].name, n);
    }
    if (common) {
      memcpy(images[group] + 0x14ad6, raw[common] + 0x14ad6, 29);
      for (unsigned slot = 0; slot < 4; ++slot)
        images[group][0xf009b + slot * 256] = 0x94;
    }
    sha256_compute(images[group], IMAGE_SIZE, identity + 8 + group * 32);
  }
  for (unsigned i = 0; i < 4; ++i)
    free(raw[i]);
  free(base);
  FzeroGameplayConfigure(&image_settings[0], false, tracks);
  if (!ok) {
    snprintf(error, cap, "Cannot prepare CGP vehicle runtime: %.120s",
             FzeroGameplayError());
    return false;
  }
  uint8_t *grown = realloc(*rom, IMAGE_SIZE);
  if (!grown) {
    snprintf(error, cap, "Cannot allocate vehicle cartridge");
    return false;
  }
  *rom = grown;
  *size = IMAGE_SIZE;
  memcpy(grown, images[0], IMAGE_SIZE);
  uint8_t hash[32];
  sha256_compute(identity, sizeof(identity), hash);
  FzeroGameplaySetSignature(hash);
  static const DispatchEntry empty[1] = {{0}};
  cpu_select_program(empty, 0, NULL, 0);
  interp_bridge_set_scheduler_aot_policy(0);
  fprintf(stderr, "[vehicles] %u identities; CGP packs=%u rebalances=%u\n",
          count, choices.vehicle_packs, choices.stock_rebalance);
  return true;
}
unsigned FzeroVehicleAcceleration(unsigned slot, unsigned speed) {
  unsigned group = selected_image < 4 ? selected_image : 0;
  return race_acceleration[group][slot & 3][speed < 29 ? speed : 28];
}
unsigned FzeroVehicleTurn(unsigned speed) {
  unsigned group = selected_image < 4 ? selected_image : 0;
  return race_turn[group][vehicles[FzeroVehicleSelected()].slot]
                  [speed < 30 ? speed : 29];
}
void FzeroVehiclesSync(void) {
  if (!count || !g_snes || !g_snes->cart || g_snes->cart->romSize != IMAGE_SIZE)
    return;
  unsigned id = FzeroVehicleSelected(), group = group_for(id);
  if (group == selected_image)
    return;
  memcpy(g_snes->cart->rom, images[group], IMAGE_SIZE);
  FzeroGameplayActivateVehicles(&image_settings[group], g_snes->cart->rom);
  selected_image = group;
  fprintf(stderr, "[vehicles] active %s (cohort %u)\n", vehicles[id].id, group);
}
void FzeroVehiclesLoaded(void) {
  selected_image = 99;
  last_input = 0;
  hold = 0;
  FzeroVehiclesSync();
}
static bool menu(void) {
  return count && g_ram[0x54] == 1 && g_ram[0x55] == 1 && g_ram[0x56] == 0;
}
uint16_t FzeroVehiclesInput(uint16_t input) {
  if (!menu()) {
    last_input = input;
    hold = 0;
    return input;
  }
  unsigned id = FzeroVehicleSelected(), index = 0;
  while (index < count && roster[index] != id)
    ++index;
  if (index == count)
    index = 0;
  uint16_t direction = input & 0xf4;
  bool step = direction && (direction != (last_input & 0xf4) ||
                            (++hold >= 24 && hold % 6 == 0));
  if (direction != (last_input & 0xf4))
    hold = 0;
  if (step) {
    int distance = (direction & (64 | 128)) ? 4 : 1;
    if (direction & (16 | 64))
      distance = -distance;
    index = (index + count + distance) % count;
  }
  const char *test = getenv("FZERO_TEST_VEHICLE");
  if (test)
    for (unsigned i = 0; i < count; ++i)
      if (!strcmp(test, vehicles[roster[i]].id))
        index = i;
  id = roster[index];
  g_ram[ID_ADDRESS] = (uint8_t)id;
  FzeroVehiclesSync();
  unsigned slot = vehicles[id].slot, row = slot == 1 ? 2 : slot == 2 ? 1 : slot;
  memcpy(g_ram + 0x14d00, g_snes->cart->rom + 0xf0000 + slot * 256, 256);
  g_ram[0x14c84] = (uint8_t)row;
  g_ram[0x14c85] = 0;
  g_ram[0x14c88] = g_ram[0x14c89] = 0;
  last_input = input;
  return input & ~0xf4;
}
typedef struct VehicleCanvas {
  uint32_t *pixels;
  size_t pitch;
  unsigned scale, extra;
} VehicleCanvas;
static void box(VehicleCanvas c, int x, int y, int w, int h, uint32_t color) {
  x += (int)c.extra;
  for (int yy = y * (int)c.scale; yy < (y + h) * (int)c.scale; ++yy) {
    uint32_t *row = (uint32_t *)((uint8_t *)c.pixels + yy * c.pitch);
    for (int xx = x * (int)c.scale; xx < (x + w) * (int)c.scale; ++xx)
      row[xx] = color;
  }
}
static void label(VehicleCanvas c, int x, int y, const char *s,
                  uint32_t color) {
  for (unsigned i = 0; s[i]; ++i) {
    unsigned ch = (unsigned char)s[i];
    if (ch < 32 || ch > 126)
      ch = '?';
    for (unsigned yy = 0; yy < 8; ++yy)
      for (unsigned xx = 0; xx < 8; ++xx)
        if (FONT8X8[ch - 32][yy] & (1u << xx))
          box(c, x + (int)i * 8 + (int)xx, y + (int)yy, 1, 1, color);
  }
}
static void preview(VehicleCanvas c, unsigned id, int x, int y, unsigned zoom) {
  unsigned group = group_for(id), slot = vehicles[id].slot;
  const uint8_t *source = group ? art[group - 1] : stock_image;
  const uint8_t *tiles = source + 0x41680 + slot * 0x8000,
                *palette = source + 0x7cd80 + slot * 32;
  /* Six native 16x16 objects, DMA'd as their two 8-pixel tile rows. */
  static const unsigned rows[] = {0, 2, 1, 3};
  for (unsigned tile = 0; tile < 24; ++tile)
    for (unsigned yy = 0; yy < 8; ++yy)
      for (unsigned xx = 0; xx < 8; ++xx) {
        unsigned color = 0;
        for (unsigned bit = 0; bit < 4; ++bit)
          color |= ((tiles[tile * 32 + yy * 2 + (bit & 1) + (bit / 2) * 16] >>
                     (7 - xx)) &
                    1)
                   << bit;
        if (!color)
          continue;
        unsigned rgb = palette[color * 2] | palette[color * 2 + 1] << 8;
        uint32_t rgba = 0xff000000u | (((rgb & 31) * 255 / 31) << 16) |
                        ((((rgb >> 5) & 31) * 255 / 31) << 8) |
                        (((rgb >> 10) & 31) * 255 / 31);
        box(c, x + (int)((tile % 6 * 8 + xx) * zoom),
            y + (int)((rows[tile / 6] * 8 + yy) * zoom), (int)zoom, (int)zoom,
            rgba);
      }
}
void FzeroVehiclesOverlay(uint32_t *pixels, unsigned width, unsigned height,
                          size_t pitch) {
  if (!count || !pixels || g_ram[0x54] != 1 || g_ram[0x55] != 1 ||
      g_ram[0x56] > 1 || height < 224 || height % 224)
    return;
  unsigned scale = height / 224;
  if (width / scale < 256)
    return;
  VehicleCanvas c = {pixels, pitch, scale, (width / scale - 256) / 2};
  for (unsigned y = 0; y < height; ++y) {
    uint32_t *row = (uint32_t *)((uint8_t *)pixels + y * pitch);
    for (unsigned x = 0; x < width; ++x)
      row[x] = 0xff000000;
  }
  unsigned id = FzeroVehicleSelected(), index = 0;
  while (index < count && roster[index] != id)
    ++index;
  if (index == count)
    index = 0;
  label(c, 16, 16, "SELECT YOUR CAR", 0xffc0ffff);
  if (g_ram[0x56] == 1) {
    label(c, 16, 46, vehicles[id].name, 0xffffff00);
    preview(c, id, 72, 63, 2);
    bool cgp = group_for(id) != 0;
    label(c, 24, 139, cgp ? "CGP HANDLING" : "ORIGINAL HANDLING", 0xffc0ffff);
    label(c, 24, 157, cgp ? "ENERGY BOOST" : "ORIGINAL S-JETS", 0xffc0ffff);
    label(c, 24, 191, "START TO CONTINUE", 0xffffff00);
    return;
  }
  unsigned first = index / 4 * 4;
  char page[16];
  snprintf(page, sizeof(page), "%u/%u", index / 4 + 1, (count + 3) / 4);
  label(c, 208, 16, page, 0xff80c8e8);
  for (unsigned row = 0; row < 4 && first + row < count; ++row) {
    unsigned item = roster[first + row];
    int y = 40 + (int)row * 38;
    bool selected = item == id;
    if (selected)
      box(c, 8, y, 240, 36, 0xff102838);
    preview(c, item, 16, y + 2, 1);
    label(c, 72, y + 5, vehicles[item].name,
          selected ? 0xffffff00 : 0xffc0ffff);
    unsigned group = group_for(item);
    char origin[24];
    if (item < 4)
      snprintf(origin, sizeof(origin), "%s",
               group ? "CGP REBALANCE" : "ORIGINAL");
    else
      snprintf(origin, sizeof(origin), "CGP P%u", group);
    label(c, 72, y + 20, origin, 0xff80a8c8);
  }
  label(c, 16, 198, "< > PAGE   UP/DOWN SHIP", 0xff80c8e8);
  label(c, 16, 212, "START TO SELECT", 0xffffff00);
}
