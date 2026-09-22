#ifdef _WIN32
#include <windows.h>
#undef FORCEINLINE
#endif
#include "fzero_course_runtime.h"
#include "fzero_tracks.h"
#include "fzero_deluxe.h"
#include "common_rtl.h"
#include "sha256.h"
#include "snes/saveload.h"
#include <stdio.h>
#include <string.h>

/* Guest records still use their native slots. A cup has a stable private
 * namespace, and the original cartridge's records remain the base context.
 * The backup is part of every library snapshot and rewind frame. */
typedef struct CourseSaveState {
  uint8_t external, backup_valid, read_only, reserved, key[32];
  uint8_t base_sram[0x8000], base_records[0x200];
} CourseSaveState;
static CourseSaveState state;
static char base_root[96];
static bool set_root(const uint8_t *key) {
  if (!key) {
    RtlSetSaveRoot(base_root);
    return true;
  }
  char hex[65], root[96];
  cp_hash_format(key, hex);
  /* 128-bit path component; the full digest is checked in records.bin. */
  if (snprintf(root, sizeof(root), "%s/courses/%.32s", base_root, hex) >= (int)sizeof(root))
    return false;
  char parent[96];
  snprintf(parent, sizeof(parent), "%s/courses", base_root);
  RtlSetSaveRoot(parent);
  RtlEnsureSaveDir();
  RtlSetSaveRoot(root);
  RtlEnsureSaveDir();
  return true;
}
void FzeroTracksSavesInit(void) {
  memset(&state, 0, sizeof(state));
  snprintf(base_root, sizeof(base_root), "%s", RtlSaveRoot());
}
void FzeroTracksFlush(void) {
  if (!state.external || state.read_only)
    return;
  char path[160], temp[164];
  snprintf(path, sizeof(path), "%s/records.bin", RtlSaveRoot());
  snprintf(temp, sizeof(temp), "%s.tmp", path);
  FILE *f = fopen(temp, "wb");
  if (!f) {
    FzeroTracksReport("Cannot save imported cup records");
    return;
  }
  bool ok = fwrite(state.key, 1, 32, f) == 32 &&
            fwrite(g_sram, 1, g_sram_size, f) == (size_t)g_sram_size &&
            fwrite(g_ram + 0x14800, 1, 0x200, f) == 0x200;
  if (fclose(f))
    ok = false;
#ifdef _WIN32
  if (ok)
    ok = MoveFileExA(temp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
  if (ok)
    ok = rename(temp, path) == 0;
#endif
  if (!ok)
    FzeroTracksReport("Could not finish saving imported cup records");
}
/* Canonical record reset: 55 BCD times (9:59.99) and their 16-bit
 * byte-sum checksum per five-course league. Verified against the stock
 * $02c541 and Deluxe $1eb7bc initialization routines. Keep unlock flags. */
static void clear_league(unsigned start, unsigned times) {
  unsigned sum = 0;
  static const uint8_t empty_time[] = {0x09, 0x59, 0x99};
  for (unsigned i = 0; i < times * 3; ++i) {
    uint8_t value = empty_time[i % 3];
    g_sram[start + i] = value;
    sum += value;
  }
  g_sram[start + times * 3] = (uint8_t)sum;
  g_sram[start + times * 3 + 1] = (uint8_t)(sum >> 8);
}
static void fresh_records(void) {
  clear_league(0x005, 55);
  clear_league(0x0ac, 55);
  clear_league(0x153, 55);
  if (FzeroDeluxeActive() && g_sram_size >= 0x400) {
    clear_league(0x205, 55);
    clear_league(0x2ac, 55);
    clear_league(0x353, 22);
  }
  memcpy(g_ram + 0x14800, g_sram, 0x200);
}
bool FzeroTracksRecordsSelect(const uint8_t *key) {
  if ((!key && !state.external) || (key && state.external && !memcmp(key, state.key, 32)))
    return true;
  if (g_sram_size < 0x200 || g_sram_size > 0x8000)
    return false;
  FzeroTracksFlush();
  if (state.external && state.backup_valid) {
    memcpy(g_sram, state.base_sram, g_sram_size);
    memcpy(g_ram + 0x14800, state.base_records, 0x200);
  }
  state.external = 0;
  state.read_only = 0;
  if (!key) {
    set_root(NULL);
    return true;
  }
  memcpy(state.base_sram, g_sram, g_sram_size);
  memcpy(state.base_records, g_ram + 0x14800, 0x200);
  state.backup_valid = 1;
  if (!set_root(key)) {
    set_root(NULL);
    FzeroTracksReport("Save root too long for imported cup records");
    return false;
  }
  memcpy(state.key, key, 32);
  state.external = 1;
  fresh_records();
  char path[160];
  snprintf(path, sizeof(path), "%s/records.bin", RtlSaveRoot());
  FILE *f = fopen(path, "rb");
  if (f) {
    uint8_t hash[32], sram[0x8000], records[0x200];
    bool ok = fread(hash, 1, 32, f) == 32 && !memcmp(hash, key, 32) &&
              fread(sram, 1, g_sram_size, f) == (size_t)g_sram_size &&
              fread(records, 1, 0x200, f) == 0x200 && fgetc(f) == EOF;
    fclose(f);
    if (ok) {
      memcpy(g_sram, sram, g_sram_size);
      memcpy(g_ram + 0x14800, records, 0x200);
    } else {
      state.read_only = 1;
      FzeroTracksReport(
          "Imported cup records failed validation; this session will preserve that file");
    }
  }
  return true;
}
size_t FzeroTracksSaveStateSize(void) {
  return sizeof(state);
}
void FzeroTracksSaveState(SaveLoadInfo *sli, bool load) {
  sli->func(sli, &state, sizeof(state));
  if (load)
    set_root(state.external ? state.key : NULL);
}
void FzeroTracksSavesFinish(void) {
  FzeroTracksRecordsSelect(NULL);
}
