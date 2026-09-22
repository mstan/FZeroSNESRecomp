#ifdef _WIN32
#include <windows.h>
#undef FORCEINLINE
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#define MKDIR(p) mkdir(p, 0755)
#endif
#include "fzero_course_runtime.h"
#include "common_rtl.h"
#include "snes/saveload.h"
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
uint8 g_ram[0x20000];
static uint8 sram[0x8000];
uint8 *g_sram = sram;
int g_sram_size = 0x800;
static char root[96] = "test-course-records";
const char *RtlSaveRoot(void) {
  return root;
}
void RtlSetSaveRoot(const char *s) {
  snprintf(root, sizeof(root), "%s", s);
}
void RtlEnsureSaveDir(void) {
  MKDIR(root);
}
bool FzeroDeluxeActive(void) {
  return true;
}
void FzeroTracksReport(const char *s) {
  fprintf(stderr, "%s\n", s);
}
int main(void) {
  uint8_t a[32] = {1}, b[32] = {2}, original[0x800], mirror[0x200];
  MKDIR(root);
  FzeroTracksSavesInit();
  memset(sram, 0x42, sizeof(sram));
  memset(g_ram + 0x14800, 0x73, 0x200);
  memcpy(original, sram, sizeof(original));
  memcpy(mirror, g_ram + 0x14800, sizeof(mirror));
  remove("test-course-records/courses/01000000000000000000000000000000/records.bin");
  remove("test-course-records/courses/02000000000000000000000000000000/records.bin");
  CHECK(FzeroTracksRecordsSelect(a));
  CHECK(!memcmp(sram + 5, "\x09\x59\x99", 3) && sram[0xaa] == 0xed && sram[0xab] == 0x35);
  CHECK(sram[0x1fa] == 0x42);
  sram[5] = 0x81;
  g_ram[0x14805] = 0x81;
  CHECK(FzeroTracksRecordsSelect(b) && sram[5] == 9);
  sram[5] = 0x82;
  g_ram[0x14805] = 0x82;
  CHECK(FzeroTracksRecordsSelect(a) && sram[5] == 0x81 && g_ram[0x14805] == 0x81);
  FzeroTracksSavesFinish();
  CHECK(!memcmp(original, sram, sizeof(original)) &&
        !memcmp(mirror, g_ram + 0x14800, sizeof(mirror)));
  CHECK(!strcmp(root, "test-course-records"));
  CHECK(FzeroTracksRecordsSelect(b) && sram[5] == 0x82);
  FzeroTracksSavesFinish();
  const char *path = "test-course-records/courses/01000000000000000000000000000000/records.bin";
  FILE *f = fopen(path, "wb");
  CHECK(f);
  fputs("damaged", f);
  fclose(f);
  CHECK(FzeroTracksRecordsSelect(a) && sram[5] == 9);
  FzeroTracksSavesFinish();
  f = fopen(path, "rb");
  CHECK(f);
  char saved[8] = {0};
  CHECK(fread(saved, 1, 8, f) == 7);
  fclose(f);
  CHECK(!strcmp(saved, "damaged") && !memcmp(original, sram, sizeof(original)));
  puts("Independent cup records, clean defaults, base restoration and corrupt-file preservation "
       "passed");
  return 0;
}
