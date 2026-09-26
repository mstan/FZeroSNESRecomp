/* Author-side conversion. Players do not need donor ROMs or this tool. */
#include "fzero_course_file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc, char **argv) {
  if (argc != 5) {
    fprintf(
        stderr,
        "usage: FZeroExportCourses donor.sfc layout source-slot output.fzc\n");
    return 2;
  }
  char *end;
  unsigned long slot = strtoul(argv[3], &end, 10);
  if (!*argv[3] || *end || slot >= 128)
    return 2;
  FILE *f = fopen(argv[1], "rb");
  if (!f)
    return 2;
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  rewind(f);
  if (n <= 0 || n > 0x1000000) {
    fclose(f);
    return 2;
  }
  uint8_t *rom = malloc((size_t)n);
  if (!rom) {
    fclose(f);
    return 2;
  }
  bool ok = fread(rom, 1, (size_t)n, f) == (size_t)n;
  fclose(f);
  FzeroCourseLayout layout;
  char error[256] = "Cannot read donor";
  FzeroCourse *a = calloc(1, sizeof(*a)), *b = calloc(1, sizeof(*b));
  ok = ok && a && b &&
       FzeroCourseLayoutRead(argv[2], &layout, error, sizeof(error)) &&
       FzeroCourseExtract(rom, (size_t)n, &layout, (unsigned)slot, a, error,
                          sizeof(error)) &&
       FzeroCourseFileWrite(argv[4], a, error, sizeof(error)) &&
       FzeroCourseFileRead(argv[4], b, error, sizeof(error));
  if (ok && memcmp(a, b, sizeof(*a))) {
    snprintf(error, sizeof(error),
             "Resource roundtrip differs from extraction");
    ok = false;
  }
  if (ok) {
    for (unsigned i = 0; i < 32; ++i)
      printf("%02x", a->hash[i]);
    puts("");
  } else
    fprintf(stderr, "%s\n", error);
  free(a);
  free(b);
  free(rom);
  return ok ? 0 : 1;
}
