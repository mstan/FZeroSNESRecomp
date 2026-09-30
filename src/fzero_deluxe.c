#include "fzero_deluxe.h"
#include "cpu_state.h"
#include "common_rtl.h"
#include "program_module.h"
#include "snes/interp_bridge.h"
#include "sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool active;
static char error[160];
bool FzeroDeluxeActive(void) { return active; }
const char *FzeroDeluxeError(void) { return error; }

bool FzeroDeluxeSelectSaveRoot(void) {
  if (!active) return true;
  /* Shared SRAM filenames are generic save.srm; a different title/prefix
   * isolates snapshots only. Give the content module its own directory too. */
  char root[96];
  if (snprintf(root, sizeof(root), "%s/bs-deluxe", RtlSaveRoot()) >= (int)sizeof(root)) {
    snprintf(error, sizeof(error), "BS Deluxe save directory is too long; choose a shorter save root.");
    return false;
  }
  RtlEnsureSaveDir();
  RtlSetSaveRoot(root);
  RtlEnsureSaveDir();
  fprintf(stderr, "[bs-deluxe] saves: %s/save.srm\n", RtlSaveRoot());
  return true;
}

#ifdef FZERO_HAS_DELUXE
extern SnesProgramModule deluxe_g_program_module;
static const uint8_t stock_hash[32] = {
  0xbf,0x16,0xc3,0xc8,0x67,0xc5,0x8e,0x2a,0xb0,0x61,0xc7,0x0d,0xe9,0x29,0x5b,0x69,
  0x30,0xd6,0x3f,0x29,0xf8,0x1c,0xc9,0x86,0xf5,0xec,0xae,0x03,0xe0,0xad,0x18,0xd2};
/* BS F-Zero Deluxe USA v1.1 (April 1, 2025). Must match DELUXE_SHA256 in
 * tools/import_bs_deluxe.py and the native module in FZERO_DELUXE_GEN_DIR. */
static const uint8_t target_hash[32] = {
  0x55,0x21,0x59,0xa1,0x99,0x55,0xe8,0x8a,0x83,0x37,0xf7,0xc4,0x73,0xcc,0xc5,0x3e,
  0x5d,0xce,0xf1,0x5b,0x87,0xda,0xab,0x8e,0x89,0xe1,0x89,0x46,0x94,0x54,0x2f,0xe6};
static uint32_t u32(const uint8_t *p) {
  return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24;
}
#endif

#ifdef FZERO_HAS_DELUXE
/* Compiled in by tools/embed_payload.py: the payload ships inside the
 * executable so a download can never be missing it. */
extern const uint8_t fzero_deluxe_payload[];
extern const size_t fzero_deluxe_payload_size;

/* Apply one payload image to the verified stock ROM. The bytes are checked the
 * same way whether they came from a file or from the executable: magic,
 * declared sizes, the stock digest they were built against, ordered
 * non-overlapping records, and the digest of the result. */
static uint8_t *deluxe_apply(const uint8_t *data, size_t size,
                             const uint8_t *rom, size_t rom_size) {
  if (size < 80 || memcmp(data, "BSDELX1\0", 8) || u32(data + 8) != 0x100000 ||
      !u32(data + 12) || u32(data + 12) > 0x100000 ||
      memcmp(data + 16, stock_hash, 32) || memcmp(data + 48, target_hash, 32))
    return NULL;
  uint8_t *mapped = calloc(1, 0x100000);
  if (!mapped) return NULL;
  memcpy(mapped, rom, rom_size);
  size_t at = 80;
  uint32_t end = 0;
  for (uint32_t i = 0; i < u32(data + 12); ++i) {
    if (size - at < 8) goto failed;
    uint32_t offset = u32(data + at), length = u32(data + at + 4);
    at += 8;
    if (!length || offset < end || offset >= 0x100000 || length > 0x100000 - offset ||
        size - at < length) goto failed;
    memcpy(mapped + offset, data + at, length);
    at += length;
    end = offset + length;
  }
  if (at != size) goto failed;
  uint8_t actual[32];
  sha256_compute(mapped, 0x100000, actual);
  if (memcmp(actual, target_hash, 32)) goto failed;
  return mapped;
failed:
  free(mapped);
  return NULL;
}

/* A file beside the executable, or FZERO_DELUXE_DATA, stays supported so an
 * importer run can be tried without rebuilding. Both are development inputs:
 * the embedded copy is what ships and what is used when they are absent. */
static uint8_t *deluxe_read_file(const char *path, size_t *size) {
  FILE *f = fopen(path, "rb");
  if (!f) return NULL;
  uint8_t *data = NULL;
  if (fseek(f, 0, SEEK_END)) goto done;
  long length = ftell(f);
  if (length <= 0 || length > 0x200000 || fseek(f, 0, SEEK_SET)) goto done;
  data = malloc((size_t)length);
  if (data && fread(data, 1, (size_t)length, f) == (size_t)length) *size = (size_t)length;
  else { free(data); data = NULL; }
done:
  fclose(f);
  return data;
}
#endif

bool FzeroDeluxePrepare(uint8_t **rom, size_t *size, bool enabled, const char *path) {
  active = false;
  error[0] = 0;
  snes_program_module_select(NULL);
  interp_bridge_set_scheduler_aot_policy(-1);
  if (!enabled) return true;
#ifndef FZERO_HAS_DELUXE
  (void)rom; (void)size; (void)path;
  snprintf(error, sizeof(error), "This build does not include the BS Deluxe native module.");
  return false;
#else
  uint8_t actual[32];
  if (!rom || !*rom || !size || *size != 0x80000) {
    snprintf(error, sizeof(error), "BS Deluxe needs the verified stock ROM.");
    return false;
  }
  sha256_compute(*rom, *size, actual);
  if (memcmp(actual, stock_hash, 32)) {
    snprintf(error, sizeof(error), "BS Deluxe needs the verified stock ROM.");
    return false;
  }
  uint8_t *mapped = NULL;
  size_t file_size = 0;
  uint8_t *file_data = path ? deluxe_read_file(path, &file_size) : NULL;
  if (file_data) {
    mapped = deluxe_apply(file_data, file_size, *rom, *size);
    free(file_data);
    if (!mapped)
      fprintf(stderr, "[bs-deluxe] %s failed verification; using the embedded copy\n", path);
  }
  if (!mapped) mapped = deluxe_apply(fzero_deluxe_payload, fzero_deluxe_payload_size, *rom, *size);
  if (!mapped) {
    snprintf(error, sizeof(error), "BS Deluxe data verification failed; nothing activated.");
    return false;
  }
  free(*rom);
  *rom = mapped;
  *size = 0x100000;
  snes_program_module_select(&deluxe_g_program_module);
  active = true;
  /* Loading paths need interpreter parity before further AOT promotion.
   * Keep native interrupt helpers; the main scheduler uses the faithful floor.
   * No process environment override or effect on the stock module. */
  interp_bridge_set_scheduler_aot_policy(0);
  fprintf(stderr, "[bs-deluxe] USA 1.1 native module active; separate 32 KiB saves\n");
  return true;
#endif
}
