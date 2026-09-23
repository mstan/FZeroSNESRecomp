#include "fzero_tracks.h"
#include "fzero_course_runtime.h"
#include "fzero_deluxe.h"
#include "fzero_gameplay.h"
#include "fzero_title.h"
#include "cpu_state.h"
#include "common_rtl.h"
#include "snes/interp_bridge.h"
#include "sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct ImportedCourses {
  const CpPack *pack;
  FzeroCourse *courses;
} ImportedCourses;
static ImportedCourses imported[CP_PACKS];
static unsigned imported_count;
static const CpPack *active_pack;
static const CpCup *active_cup;
static unsigned menu_index;
static uint8_t identity[32];
static uint16_t menu_last_input;
static unsigned menu_hold;
static const FzeroCourse *course;
static uint8_t cup_hash[32];
bool FzeroTracksActive(void) {
  return imported_count != 0 || FzeroGameplayActive();
}
const uint8_t *FzeroTracksActiveHash(void) {
  return identity;
}
const char *FzeroTracksActiveId(void) {
  return "f_zero_library";
}
const FzeroCourse *FzeroTracksCurrentCourse(void) {
  return course;
}
static bool imported_pack(const CpPack *p) {
  for (unsigned i = 0; i < imported_count; ++i)
    if (imported[i].pack == p)
      return true;
  return false;
}
const CpCup *FzeroTracksRuntimeCup(unsigned n, const CpPack **pack) {
  const CpCatalog *cat = FzeroTracksCatalog();
  const CpPack *base = cp_catalog_find(cat, FzeroDeluxeActive() ? "bs-deluxe" : "retail");
  unsigned base_count=FzeroBsTracks() ? 5 : 3;
  if (base && n < base_count) {
    if (pack)
      *pack = base;
    return &base->cups[n];
  }
  if (base)
    n -= base_count;
  for (unsigned i = 0; i < imported_count; ++i) {
    const CpPack *p = imported[i].pack;
    if (n < p->cup_count) {
      if (pack)
        *pack = p;
      return &p->cups[n];
    }
    n -= p->cup_count;
  }
  return NULL;
}
unsigned FzeroTracksRuntimeCount(void) {
  unsigned n = FzeroBsTracks() ? 5 : 3;
  for (unsigned i = 0; i < imported_count; ++i)
    n += imported[i].pack->cup_count;
  return n;
}
bool FzeroTracksRuntimeSelect(unsigned n) {
  const CpPack *p = NULL;
  const CpCup *cup = FzeroTracksRuntimeCup(n, &p);
  if (!cup)
    return false;
  menu_index = n;
  active_pack = p;
  active_cup = cup;
  course = NULL;
  if (imported_pack(p)) {
    uint8_t key[CP_ID * 2 + CP_TRACKS * (CP_ID + 32)] = {0};
    size_t pos = CP_ID * 2;
    memcpy(key, p->id, strlen(p->id));
    memcpy(key + CP_ID, cup->id, strlen(cup->id));
    for (unsigned i = 0; i < imported_count; ++i)
      if (imported[i].pack == p)
        for (unsigned t = 0; t < p->track_count; ++t)
          if (!strcmp(p->tracks[t].cup, cup->id)) {
            memcpy(key + pos, p->tracks[t].id, strlen(p->tracks[t].id));
            pos += CP_ID;
            memcpy(key + pos, imported[i].courses[t].hash, 32);
            pos += 32;
          }
    sha256_compute(key, pos, cup_hash);
  }
  return true;
}
unsigned FzeroTracksCurrentCupSize(void) {
  if (!active_cup || !imported_pack(active_pack) || g_ram[0x58])
    return 0;
  unsigned count = 0;
  for (unsigned i = 0; i < active_pack->track_count; ++i)
    count += !strcmp(active_pack->tracks[i].cup, active_cup->id);
  return count;
}
static void current_course(void) {
  course = NULL;
  if (!active_cup || !imported_pack(active_pack) || g_ram[0x54] == 0 || g_ram[0x58])
    return;
  unsigned order = g_ram[0x53];
  const char *test = getenv("FZERO_TEST_COURSE");
  if (test)
    order = (unsigned)strtoul(test, NULL, 10);
  for (unsigned i = 0; i < imported_count; ++i)
    if (imported[i].pack == active_pack)
      for (unsigned t = 0; t < active_pack->track_count; ++t)
        if (!strcmp(active_pack->tracks[t].cup, active_cup->id)) {
          if (!order--) {
            course = &imported[i].courses[t];
            return;
          }
        }
}
bool FzeroTracksPrepare(uint8_t **rom, size_t *size, bool deluxe, const char *deluxe_path) {
  for (unsigned i = 0; i < imported_count; ++i)
    free(imported[i].courses);
  imported_count = 0;
  active_pack = NULL;
  active_cup = NULL;
  course = NULL;
  menu_index = 0;
  FzeroTitleReset();
  FzeroTracksDiscover(*rom, *size);
  const CpCatalog *cat = FzeroTracksCatalog();
  char error[256], path[CP_PATH];
  for (unsigned i = 0; i < cat->count; ++i) {
    const CpPack *p = cat->packs[i];
    if (strcmp(p->adapter, "fzero-course-v1") ||
        !FzeroTracksAvailable(p))
      continue;
    uint8_t *donor = NULL;
    size_t donor_size = 0;
    FzeroCourseLayout layout;
    snprintf(path, sizeof(path), "%s/%s.layout", FzeroTracksRoot(), p->id);
    FILE *layout_file = fopen(path, "rb");
    if (layout_file)
      fclose(layout_file);
    else
      snprintf(path, sizeof(path), "assets/track-packs/%s.layout", p->id);
    bool ok = cp_pack_apply(p, *rom, *size, FzeroTracksPatch(p), &donor, &donor_size, error,
                            sizeof(error)) &&
              FzeroCourseLayoutRead(path, &layout, error, sizeof(error));
    FzeroCourse *courses = ok ? calloc(p->track_count, sizeof(*courses)) : NULL;
    if (ok && !courses) {
      snprintf(error, sizeof(error), "Out of memory");
      ok = false;
    }
    for (unsigned j = 0; ok && j < p->cup_count; ++j) {
      unsigned count = 0;
      for (unsigned t = 0; t < p->track_count; ++t)
        count += !strcmp(p->tracks[t].cup, p->cups[j].id);
      if (count > 5) {
        snprintf(error, sizeof(error), "This GP adapter supports 1 to 5 courses per cup");
        ok = false;
      }
    }
    for (unsigned t = 0; ok && t < p->track_count; ++t)
      ok = FzeroCourseExtract(donor, donor_size, &layout, p->tracks[t].slot, &courses[t], error,
                              sizeof(error));
    if (ok && FzeroTracksTitleEnabled(p) &&
        !FzeroTitlePrepare(*rom, *size, "assets/track-packs/presentation/fzero-55.ips", error, sizeof(error))) {
      char message[256];
      snprintf(message, sizeof(message), "CGP title: %.160s; using the original title", error);
      FzeroTracksReport(message);
    }
    free(donor);
    if (!ok) {
      free(courses);
      char message[256];
      snprintf(message, sizeof(message), "%.80s: %.160s", p->name, error);
      FzeroTracksReport(message);
      continue;
    }
    imported[imported_count++] = (ImportedCourses){p, courses};
    fprintf(stderr, "[track-library] extracted %s: %u courses; donor code discarded\n", p->id,
            p->track_count);
  }
  (void)deluxe;
  if (!FzeroDeluxePrepare(rom, size, FzeroBsCars() || FzeroBsTracks(), deluxe_path))
    return false;
  if (!FzeroGameplayPrepare(rom,size)) return false;
  if (FzeroTracksActive()) {
    /* The canonical native interrupt module remains selected. Resource
     * callbacks run at the same loader sites for every imported pack. */
    interp_bridge_set_scheduler_aot_policy(0);
    /* Snapshot indices depend on both resource bytes and stable IDs/order.
     * Hash only initialized manifest records, never a pointer or file path. */
    uint8_t hashes[CP_PACKS * 32 + 73] = {0};
    hashes[0] = (uint8_t)FzeroDeluxeActive();
    const FzeroGameplaySettings *rules=FzeroGameplaySettingsCurrent();
    for (unsigned j=0;j<4;++j) hashes[1+j]=(uint8_t)(rules->enabled>>(j*8));
    hashes[5]=FzeroRuleEnabled(FZERO_RULE_TUNING)?(uint8_t)rules->tuning:0;
    hashes[6]=FzeroRuleEnabled(FZERO_RULE_BOOST)?(uint8_t)rules->boost:0;
    hashes[7]=FzeroRuleEnabled(FZERO_RULE_EXHAUST)?(uint8_t)rules->exhaust:0;
    hashes[8]=(uint8_t)(FzeroBsCars() | (FzeroBsTracks()<<1));
    memcpy(hashes+9,FzeroGameplaySignature(),32);
    for (unsigned i = 0; i < imported_count; ++i) {
      const CpPack *p = imported[i].pack;
      size_t length =
          CP_ID + p->cup_count * sizeof(CpCup) + p->track_count * (sizeof(CpTrack) + 32);
      uint8_t *data = calloc(1, length);
      if (!data)
        return false;
      memcpy(data, p->id, CP_ID);
      size_t pos = CP_ID;
      memcpy(data + pos, p->cups, p->cup_count * sizeof(CpCup));
      pos += p->cup_count * sizeof(CpCup);
      for (unsigned t = 0; t < p->track_count; ++t) {
        memcpy(data + pos, &p->tracks[t], sizeof(CpTrack));
        pos += sizeof(CpTrack);
        memcpy(data + pos, imported[i].courses[t].hash, 32);
        pos += 32;
      }
      sha256_compute(data, length, hashes + 41 + i * 32);
      free(data);
    }
    size_t length = 41 + imported_count * 32;
    /* Title assets affect snapshot compatibility, never course records or the
     * gameplay signature. Preserve existing snapshot identity when off. */
    if (FzeroTitleHash()) {
      memcpy(hashes + length, FzeroTitleHash(), 32);
      length += 32;
      fprintf(stderr, "[track-library] F-Zero 55 title artwork enabled\n");
    }
    sha256_compute(hashes, length, identity);
  }
  FzeroTracksRuntimeSelect(0);
  const char *key = getenv("FZERO_CUP");
  if (key && *key) {
    bool found = false;
    for (unsigned i = 0; i < FzeroTracksRuntimeCount(); ++i) {
      const CpPack *p;
      const CpCup *c = FzeroTracksRuntimeCup(i, &p);
      char k[CP_ID * 2];
      snprintf(k, sizeof(k), "%s/%s", p->id, c->id);
      if (!strcmp(k, key)) {
        FzeroTracksRuntimeSelect(i);
        found = true;
        break;
      }
    }
    if (!found) {
      FzeroTracksReport("Requested FZERO_CUP is unavailable in this library");
      return false;
    }
  }
  return true;
}
bool FzeroTracksSelectSaveRoot(void) {
  if (!FzeroDeluxeSelectSaveRoot())
    return false;
  if (FzeroGameplaySettingsCurrent()->enabled) {
    char hex[65],root[96]; cp_hash_format(FzeroGameplaySignature(),hex);
    if(snprintf(root,sizeof(root),"%s/rules-%.16s",RtlSaveRoot(),hex)>=(int)sizeof(root))return false;
    RtlEnsureSaveDir();RtlSetSaveRoot(root);RtlEnsureSaveDir();
  }
  if (FzeroTracksActive() && strlen(RtlSaveRoot()) + 41 >= 96) {
    FzeroTracksReport(
        "Save root is too long for course records; use a shorter SNESRECOMP_SAVE_ROOT");
    return false;
  }
  FzeroTracksSavesInit();
  return true;
}
static bool cup_menu(const uint8_t *ram) {
  return FzeroDeluxeActive() ? ram[0x54] == 1 && ram[0x55] == 1 && ram[0x56] == 2 && !ram[0x58] && !ram[0x14c98]
                             : ram[0x54] == 1 && ram[0x55] == 5 && !ram[0x58];
}
void FzeroTracksMenuState(uint8_t state[2], bool load) {
  if (load) {
    FzeroTracksRuntimeSelect(state[0] | (unsigned)state[1] << 8);
    menu_last_input = 0;
    menu_hold = 0;
    current_course();
  } else {
    state[0] = (uint8_t)menu_index;
    state[1] = (uint8_t)(menu_index >> 8);
  }
}
void FzeroTracksMenuReset(void) {
  menu_last_input = 0;
  menu_hold = 0;
  course = NULL;
}
void FzeroTracksMenuTick(uint8_t *ram, uint32_t previous_scene) {
  (void)ram;
  (void)previous_scene;
  if (g_ram[0x54] == 0)
    FzeroTracksRecordsSelect(NULL);
  current_course();
}
uint16_t FzeroTracksMenuInput(uint16_t input, const uint8_t *ram) {
  if (getenv("FZERO_TRACE_LIBRARY") && input)
    fprintf(stderr, "[library-input] %u scene=%u,%u,%u active=%u deluxe=%u index=%u\n", input,
            ram[0x54], ram[0x55], ram[0x56], imported_count, FzeroDeluxeActive(), menu_index);
  if (!FzeroTracksActive() || !active_cup)
    return input;
  if (!cup_menu(ram)) {
    menu_last_input = input;
    menu_hold = 0;
    return input;
  }
  uint16_t direction = input & 0xf0;
  bool step = direction &&
              (direction != (menu_last_input & 0xf0) || (++menu_hold >= 24 && menu_hold % 6 == 0));
  if (direction != (menu_last_input & 0xf0))
    menu_hold = 0;
  if (step) {
    unsigned count = FzeroTracksRuntimeCount();
    bool back = (direction & (16 | 64)) != 0;
    FzeroTracksRuntimeSelect((menu_index + count + (back ? -1 : 1)) % count);
    fprintf(stderr, "[track-library] menu %u/%u: %s\n", menu_index + 1, count, active_cup->name);
  }
  menu_last_input = input;
  unsigned slot = imported_pack(active_pack) ? 0 : active_cup->slot;
  g_ram[FzeroDeluxeActive() ? 0x90 : 0x5a] = (uint8_t)slot;
  return input & ~0xf0;
}
unsigned FzeroTracksMenuIndex(void) {
  return menu_index;
}
bool FzeroTracksMenuVisible(void) {
  return FzeroTracksActive() && g_ram[0x54] == 1 && !g_ram[0x58] &&
         (FzeroDeluxeActive() ? g_ram[0x55] == 1 && g_ram[0x56] == 2
                              : (g_ram[0x55] == 5 || g_ram[0x55] == 6));
}
bool FzeroTracksClassSelected(void) {
  return FzeroDeluxeActive() ? g_ram[0x14c98] != 0 : g_ram[0x55] == 6;
}
void FzeroTracksRefreshCourse(void) {
  current_course();
  if (FzeroTracksActive() && !FzeroTracksRecordsSelect(course ? cup_hash : NULL))
    course = NULL;
}
