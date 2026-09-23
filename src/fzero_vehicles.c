#include "fzero_vehicles.h"
#include "common_rtl.h"
#include "content_pack.h"
#include "cpu_state.h"
#include "fzero_gameplay.h"
#include "sha256.h"
#include "snes/cart.h"
#include "snes/interp_bridge.h"
#include "snes/ppu.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern Snes *g_snes;

/* Stable identities are independent of the four guest racing slots. Grand Prix
 * uses the selected ship's donor cohort, retaining three native main rivals.
 * Practice composes its independently selected rival into a spare slot.
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
static void menu_resources(bool upload);
static void rival_resources(bool upload);
static void rival_compose(unsigned id, unsigned slot);
static unsigned selected_rival = 99;
static unsigned selected_rival_slot = 99;
static uint8_t race_acceleration[4][4][29], race_turn[4][4][30];
enum { IMAGE_SIZE = 0x400000, ID_ADDRESS = 0x14dff,
       RIVAL_ID = 0x14ce3, RIVAL_FIRST = 0x14ce4,
       RIVAL_VERSION = 0x14ce5, RIVAL_SLOT = 0x14ce6,
       RIVAL_ART = 0x350000, RIVAL_PALETTES = 0x358000 };

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
  uint8_t identity[4 * 32 + 8] = {2, 0, 0, 0};
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
      for (unsigned j = 0; j < 15; ++j)
        if (label[j] == '.')
          label[j] = '_';
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
  if (g_ram[0x58] && selected_rival < 12 &&
      slot == selected_rival_slot)
    return race_acceleration[group_for(selected_rival)][vehicles[selected_rival].slot]
                            [speed < 29 ? speed : 28];
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
  /* No Rival clears the native rival workspace on race entry. Preserve its
   * sentinel instead of interpreting that cleared byte as Blue Falcon. */
  if (g_ram[0x58] && g_ram[0x54] >= 2 && g_ram[0xcf2] >= 0xfe) {
    g_ram[RIVAL_ID] = g_ram[0xcf2];
    g_ram[RIVAL_VERSION] = 1;
  }
  unsigned rival = g_ram[0x58] ? g_ram[RIVAL_ID] : 99;
  if (rival >= 12 || !enabled(rival)) rival = 99;
  if (g_ram[0x54] < 2)
    g_ram[RIVAL_SLOT] = (uint8_t)((vehicles[id].slot + 1) % 4);
  unsigned rival_slot = g_ram[RIVAL_SLOT] & 3;
  if (group == selected_image && rival == selected_rival &&
      (rival >= 12 || rival_slot == selected_rival_slot))
    return;
  memcpy(g_snes->cart->rom, images[group], IMAGE_SIZE);
  FzeroGameplayActivateVehicles(&image_settings[group], g_snes->cart->rom);
  selected_image = group;
  selected_rival = rival;
  selected_rival_slot = rival_slot;
  if (rival < 12)
    rival_compose(rival, rival_slot);
  menu_resources(false);
  fprintf(stderr, "[vehicles] active %s (cohort %u)\n", vehicles[id].id, group);
}
void FzeroVehiclesLoaded(void) {
  if (count && g_ram[0x58] && g_ram[RIVAL_VERSION] != 1) {
    unsigned slot = g_ram[0xcf2], group = group_for(FzeroVehicleSelected());
    unsigned id = slot < 4 ? cohort[group][slot] : slot;
    if (id < 12 && !enabled(id)) id = slot;
    g_ram[RIVAL_ID] = (uint8_t)id;
    g_ram[RIVAL_FIRST] = 0;
    g_ram[RIVAL_VERSION] = 1;
    g_ram[RIVAL_SLOT] = (uint8_t)(slot & 3);
  }
  selected_image = 99;
  FzeroVehiclesSync();
}

/* Two native screen columns are a viewport onto the additive roster. The
 * guest still owns its cursor, slide, dimming, confirmation and info panel.
 * Page bindings live in WRAM so rewind/snapshots restore the same viewport. */
enum {
  MENU_STATE = 0x14ce0,
  MENU_ART = 0x300000,
  MENU_PALETTES = 0x340000,
  MENU_CARDS = 0x341000
};
static unsigned page_count(void) { return (count + 3) / 4; }
static unsigned page(unsigned column) {
  unsigned value = g_ram[MENU_STATE + 1 + column];
  return value < page_count() ? value : 0;
}
static unsigned menu_id(unsigned position) {
  if (count <= 4 && position >= 4)
    return 99;
  unsigned index = page(position / 4) * 4 + position % 4;
  return index < count ? roster[index] : 99;
}
static unsigned preview_id(const uint8_t *ram, unsigned row) {
  unsigned column = (ram[0x14c84] & 4) / 4;
  unsigned current = ram[MENU_STATE + 1 + column];
  unsigned neighbor = (current + page_count() + (column ? 1 : -1)) % page_count();
  unsigned index = neighbor * 4 + row;
  return index < count ? roster[index] : 99;
}
static unsigned read16(const uint8_t *p) { return p[0] | (unsigned)p[1] << 8; }
static unsigned address(unsigned offset) {
  return ((offset / 0x8000) << 16) | 0x8000 | (offset & 0x7fff);
}
static void pointer(uint8_t *p, unsigned offset) {
  unsigned bus = address(offset);
  p[0] = bus;
  p[1] = bus >> 8;
  p[2] = bus >> 16;
}
static void rival_compose(unsigned id, unsigned slot) {
  unsigned group = group_for(id), original = vehicles[id].slot;
  uint8_t *rom = g_snes->cart->rom, *record = rom + 0xf0000 + slot * 256;
  memcpy(rom + RIVAL_ART, images[group] + 0x40000 + original * 0x8000, 0x8000);
  memcpy(record, images[group] + 0xf0000 + original * 256, 256);
  record[0] = (uint8_t)(address(RIVAL_ART) >> 16);
  const uint8_t *source = group ? art[group - 1] : stock_image;
  memcpy(rom + RIVAL_PALETTES, source + 0x7cd80 + original * 32, 32);
  pointer(record + 0x97, RIVAL_PALETTES);
  /* Allow the rival's authored top speed without changing the player's
   * identity-owned acceleration, steering, boost or exhaust tables. */
  if (group) rom[0x16f6] = 0x80;
}
static unsigned rival_id(unsigned position) {
  unsigned index = g_ram[RIVAL_FIRST] + position;
  return index < count ? roster[index] : 99;
}
static void rival_resources(bool upload) {
  uint8_t *rom = g_snes->cart->rom;
  static const unsigned tiles[] = {0x164, 0x167, 0x16a, 0x16d,
                                    0x184, 0x187, 0x18a, 0x18d};
  for (unsigned position = 0; position < 8; ++position) {
    unsigned id = rival_id(position), slot = id < 12 ? vehicles[id].slot : 0;
    unsigned group = id < 12 ? group_for(id) : 0;
    const uint8_t *source = group ? art[group - 1] : stock_image;
    unsigned palettes = RIVAL_PALETTES + 32 + position * 64;
    memcpy(rom + palettes, source + 0x76180 + slot * 32, 32);
    memcpy(rom + palettes + 32, source + 0x7cd80 + slot * 32, 32);
    if (id >= 12) memset(rom + palettes, 0, 64);
    for (unsigned entry = 0; entry < 12; ++entry) {
      uint8_t *p = rom + 0xf4386 + entry * 16;
      if (p[10] != position) continue;
      /* These are MVN source/destination pairs, not ordinary 24-bit pointers. */
      for (unsigned kind = 0; kind < 2; ++kind) {
        unsigned bus = address(palettes + kind * 32);
        p[kind * 4] = bus;
        p[kind * 4 + 1] = bus >> 8;
        p[kind * 4 + 3] = bus >> 16;
      }
    }
    if (!upload) continue;
    for (unsigned tile = 0; tile < 6; ++tile) {
      unsigned dest = 0x4000 + (tiles[position] + tile / 3 * 16 + tile % 3) * 32;
      const uint8_t *pixels = source + 0x40000 + slot * 0x8000 + 0x6860 + tile * 32;
      for (unsigned j = 0; j < 16; ++j) {
        unsigned value = id < 12 ? read16(pixels + j * 2) : 0;
        g_ppu->vram[dest / 2 + j] = (uint16_t)value;
        g_ram[0x18000 + dest + j * 2] = value;
        g_ram[0x18001 + dest + j * 2] = value >> 8;
      }
    }
  }
}
static void menu_resources(bool upload) {
  if (!count || !g_snes || !g_snes->cart || !images[0])
    return;
  uint8_t *rom = g_snes->cart->rom;
  static const unsigned tile_base[8] = {0, 0x30, 0x60, 0x0a,
                                        5, 0x35, 0x65, 0x3a};
  static const unsigned tile_source[15] = {
      0x55e0, 0x5600, 0x5620, 0x5640, 0x5660, 0x56e0, 0x5700, 0x5720,
      0x5740, 0x5760, 0x5680, 0x56a0, 0x56c0, 0x5780, 0x57a0};
  for (unsigned position = 0; position < 8; ++position) {
    unsigned id = menu_id(position), slot = id < 12 ? vehicles[id].slot : 0;
    unsigned group = id < 12 ? group_for(id) : 0;
    const uint8_t *source = group ? art[group - 1] : stock_image;
    unsigned bank = MENU_ART + position * 0x8000;
    memcpy(rom + bank, source + 0x40000 + slot * 0x8000, 0x8000);
    unsigned normal = MENU_PALETTES + position * 32, dim = normal + 256;
    memcpy(rom + normal, source + 0x7cd80 + slot * 32, 32);
    memcpy(rom + dim, source + 0x76180 + slot * 32, 32);
    if (id >= 12) {
      memset(rom + bank, 0, 0x8000);
      memset(rom + normal, 0, 32);
      memset(rom + dim, 0, 32);
    }
    /* Palette setup contains the same sources as the selection tables. */
    for (unsigned entry = 0; entry < 16; ++entry) {
      const uint8_t *original = images[0] + 0xf42a2 + entry * 6;
      for (unsigned kind = 0; kind < 2; ++kind) {
        const uint8_t *lookup =
            images[0] + (kind ? 0xf44fc : 0xf44dc) + position * 4;
        if (!memcmp(original, lookup, 3))
          pointer(rom + 0xf42a2 + entry * 6, kind ? dim : normal);
      }
    }
    pointer(rom + 0xf44dc + position * 4, normal);
    pointer(rom + 0xf44fc + position * 4, dim);
    rom[0xf41aa + position] = (uint8_t)(address(bank) >> 16);
    char *label = (char *)rom + 0xf5b20 + position * 16;
    memset(label, ' ', 15);
    label[15] = 0;
    if (id < 12) {
      size_t n = strlen(vehicles[id].name);
      memcpy(label + (15 - n) / 2, vehicles[id].name, n);
      for (unsigned j = 0; j < 15; ++j)
        if (label[j] == '.')
          label[j] = '_';
    }
    unsigned row = slot == 1 ? 2 : slot == 2 ? 1 : slot;
    const uint8_t *card = images[0] + 0xf5ba0 + row * 4;
    unsigned card_offset = (unsigned)card[2] * 0x8000 + (read16(card) & 0x7fff);
    memcpy(rom + MENU_CARDS + position * 0x580, images[0] + card_offset, 0x580);
    pointer(rom + 0xf5ba0 + position * 4, MENU_CARDS + position * 0x580);
    if (!upload)
      continue;
    /* Native static previews are 5x3 BG tiles, with their own authored dim
     * palette. Reuse that layout rather than drawing a host overlay. */
    for (unsigned tile = 0; tile < 15; ++tile) {
      unsigned dest =
          0x4000 + (tile_base[position] + tile / 5 * 16 + tile % 5) * 32;
      const uint8_t *pixels = rom + bank + tile_source[tile];
      memcpy(g_ram + 0x18000 + dest, pixels, 32);
      for (unsigned j = 0; j < 16; ++j)
        g_ppu->vram[dest / 2 + j] = read16(pixels + j * 2);
    }
    const uint8_t *dest = rom + 0xf451c + position * 4;
    unsigned ram_address = (dest[2] == 0x7f ? 0x10000 : 0) + read16(dest);
    memcpy(g_ram + ram_address, rom + dim, 32);
  }
  if (upload && page_count() > 1) {
    /* Add the third column's clipped edge using the same authored 5x3 BG
     * previews. The native two-column slide and selected OBJ remain intact.
     * $8000 VRAM is unused by the native menu, and BG palette 6 is supplied
     * per row alongside the original two column palettes during scanout. */
    unsigned column = (g_ram[0x14c84] & 4) / 4;
    for (unsigned row = 0; row < 4; ++row) {
      unsigned id = preview_id(g_ram, row);
      unsigned group = id < 12 ? group_for(id) : 0;
      unsigned slot = id < 12 ? vehicles[id].slot : 0;
      const uint8_t *source = group ? art[group - 1] : stock_image;
      for (unsigned tile = 0; tile < 15; ++tile) {
        unsigned number = 0x200 + row * 15 + tile;
        unsigned dest = 0x4000 + number * 32;
        const uint8_t *pixels = source + 0x40000 + slot * 0x8000 + tile_source[tile];
        for (unsigned j = 0; j < 16; ++j)
          g_ppu->vram[dest / 2 + j] = id < 12 ? read16(pixels + j * 2) : 0;
      }
      /* Leave the native arrow cells intact. Only two edge tiles of the
       * adjacent ship are visible, clear of the title and selected cursor. */
      for (unsigned y = 0; y < 3; ++y) {
        for (unsigned x = 0; x < 2; ++x) {
          unsigned left = 0x400 + (6 + row * 5 + y) * 32 + x;
          unsigned right = left + 18;
          g_ppu->vram[left] = column ? 0 : 0x1a00 + row * 15 + y * 5 + 3 + x;
          g_ppu->vram[right] = column ? 0x1a00 + row * 15 + y * 5 + x : 0;
          unsigned words[] = {left, right};
          for (unsigned i = 0; i < 2; ++i) {
            unsigned word = words[i];
            g_ram[0x18000 + word * 2] = g_ppu->vram[word];
            g_ram[0x18001 + word * 2] = g_ppu->vram[word] >> 8;
          }
        }
      }
    }
  }
  rival_resources(upload);
}
void FzeroVehiclesRaster(Ppu *ppu, const uint8_t *ram, unsigned line) {
  if (!count || page_count() < 2 || ram[0x54] != 1 || ram[0x55] != 1 || ram[0x56] != 0 || line < 48)
    return;
  unsigned row = (line - 48) / 40;
  if (row >= 4) return;
  unsigned id = preview_id(ram, row), group = id < 12 ? group_for(id) : 0;
  unsigned slot = id < 12 ? vehicles[id].slot : 0;
  const uint8_t *palette = (group ? art[group - 1] : stock_image) + 0x76180 + slot * 32;
  for (unsigned i = 0; i < 16; ++i)
    ppu->cgram[96 + i] = id < 12 ? (uint16_t)read16(palette + i * 2) : 0;
}
static void menu_hook(CpuState *cpu, uint32_t pc) {
  if (pc == 0x1edd01) {
    g_ram[MENU_STATE] = 1;
    unsigned index = 0;
    while (index + 1 < count && roster[index] != FzeroVehicleSelected())
      ++index;
    g_ram[MENU_STATE + 1] = (uint8_t)(index / 4);
    g_ram[MENU_STATE + 2] = (uint8_t)((index / 4 + 1) % page_count());
    g_ram[RIVAL_ID] = (uint8_t)roster[0];
    g_ram[RIVAL_FIRST] = 0;
    g_ram[RIVAL_VERSION] = 1;
    g_ram[0x14c84] = (uint8_t)(index % 4);
    FzeroVehiclesSync();
    menu_resources(true);
  } else if (pc == 0x1ec76e) {
    unsigned index = 0;
    while (index + 1 < count && roster[index] != FzeroVehicleSelected())
      ++index;
    g_ram[0x14c84] = (uint8_t)(index % 4);
    g_ram[0x14c85] = 0;
    g_ram[0x14c86] = g_ram[0x14c87] = 0;
    g_ram[0x14c88] = g_ram[0x14c89] = 0;
    g_ram[0x14c8a] = g_ram[0x14c8b] = 0;
  } else if (pc == 0x1ed90a) {
    unsigned old = read16(g_ram + 0x14c84) & 7;
    unsigned next = cpu->A & 7, column = old / 4,
             rows = count - page(column) * 4;
    if (rows > 4)
      rows = 4;
    unsigned buttons = read16(g_ram + 0x67);
    if ((buttons & 0x2f00) == 0x2000) {
      unsigned index = (page(column) * 4 + old % 4 + 1) % count;
      if (index / 4 != page(column))
        column ^= 1;
      g_ram[MENU_STATE + 1 + column] = (uint8_t)(index / 4);
      next = column * 4 + index % 4;
    } else if ((buttons & 0x300) == 0x100 || (buttons & 0x300) == 0x200) {
      if (page_count() > 1) {
        unsigned target =
            (page(column) + page_count() + ((buttons & 0x200) ? -1 : 1)) %
            page_count();
        column ^= 1;
        g_ram[MENU_STATE + 1 + column] = (uint8_t)target;
        rows = count - target * 4;
        if (rows > 4)
          rows = 4;
        next = column * 4 + ((old % 4 < rows) ? old % 4 : rows - 1);
      } else
        next = old;
    } else {
      next = column * 4 + ((next % 4 < rows) ? next % 4 : rows - 1);
    }
    const char *test = getenv("FZERO_TEST_VEHICLE");
    if (test)
      for (unsigned i = 0; i < count; ++i)
        if (!strcmp(test, vehicles[roster[i]].id)) {
          if (page(column) != i / 4)
            column ^= 1;
          g_ram[MENU_STATE + 1 + column] = (uint8_t)(i / 4);
          next = column * 4 + i % 4;
        }
    cpu->A = (uint16_t)next;
    unsigned id = menu_id(next);
    g_ram[ID_ADDRESS] = (uint8_t)id;
    FzeroVehiclesSync();
    if (next != old) {
      g_ram[0x14c84] = (uint8_t)next;
      menu_resources(true);
      g_ram[0x14c84] = (uint8_t)old;
    }
  } else if (pc == 0x1edb0c) {
    /* Native info/handling record lookup uses the physical racing slot. */
    cpu->A = (uint16_t)vehicles[FzeroVehicleSelected()].slot;
  } else if (pc == 0x1ec81b) {
    cpu->A = (cpu->A & 0xff00) | vehicles[FzeroVehicleSelected()].slot;
  } else if (pc == 0x1ed04f) {
    unsigned old = g_ram[0x14c8a], next = old;
    unsigned index = g_ram[RIVAL_FIRST] + old;
    unsigned buttons = read16(g_ram + 0x67);
    if ((buttons & 0x2f00) == 0x2000) {
      if (old >= 0xfe) index = old == 0xff ? count + 1 : 0;
      else ++index;
    } else if (old < 8) {
      if ((buttons & 0x300) == 0x100) index = index + 2 < count ? index + 2 : index;
      if ((buttons & 0x300) == 0x200) index = index >= 2 ? index - 2 : index;
      if ((buttons & 0xc00) == 0x800) index = index & 1 ? index - 1 : count;
      if ((buttons & 0xc00) == 0x400) index = !(index & 1) && index + 1 < count ? index + 1 : count;
    } else {
      /* Preserve native no-rival/ghost navigation, resolving a car through
       * the current viewport rather than truncating it to the first four. */
      unsigned candidate = cpu->A & 255;
      index = candidate < 8 ? g_ram[RIVAL_FIRST] + candidate
                           : candidate == 0xfe ? count + 1 : count;
    }
    if (index < count) {
      while (index < g_ram[RIVAL_FIRST]) g_ram[RIVAL_FIRST] -= 2;
      while (index >= g_ram[RIVAL_FIRST] + 8u) g_ram[RIVAL_FIRST] += 2;
      next = index - g_ram[RIVAL_FIRST];
      g_ram[RIVAL_ID] = (uint8_t)roster[index];
    } else {
      next = index == count ? 0xff : 0xfe;
      g_ram[RIVAL_ID] = (uint8_t)next;
    }
    cpu->A = (uint16_t)next;
    FzeroVehiclesSync();
    rival_resources(true);
  } else if (pc == 0x1ec831) {
    unsigned rival = g_ram[RIVAL_ID];
    cpu_write_a_m(cpu, (uint16_t)(rival < 12 ? g_ram[RIVAL_SLOT] : rival));
  } else if (pc == 0x00d54c) {
    /* Native rival difficulty follows its original identity, independently
     * of the spare physical slot used for this player/rival pair. */
    unsigned source = selected_rival < 12 ? vehicles[selected_rival].slot : 99;
    unsigned value = source < 4 ? g_snes->cart->rom[0x17fda + source] : 0;
    cpu_write_a_m(cpu, (uint16_t)value);
    cpu->_flag_Z = !(cpu->A & 255);
    cpu->_flag_N = (cpu->A & 128) != 0;
    interp_bridge_pre_opcode_redirect(0x00d54f);
  }
}
void FzeroVehiclesInstallHooks(void) {
  if (!count)
    return;
  const unsigned sites[] = {0x1edd01, 0x1ec76e, 0x1ed90a, 0x1edb0c, 0x1ec81b,
                            0x1ed04f, 0x1ec831, 0x00d54c};
  for (unsigned i = 0; i < sizeof(sites) / sizeof(*sites); ++i)
    interp_bridge_set_pre_opcode_hook(sites[i], menu_hook);
}
