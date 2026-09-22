/* Private extraction inspection: output contains ROM-derived data. */
#include "fzero_course.h"
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char **argv) {
  if (argc != 3 && argc != 4) {
    fprintf(stderr, "usage: inspect_courses donor.sfc pack.layout [private-output-prefix]\n");
    return 2;
  }
  FILE *f = fopen(argv[1], "rb");
  if (!f)
    return 2;
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  rewind(f);
  if (n < 0 || n > 0x1000000) {
    fclose(f);
    return 2;
  }
  unsigned char *r = malloc((size_t)n);
  if (!r)
    return 2;
  if (fread(r, 1, (size_t)n, f) != (size_t)n)
    return 2;
  fclose(f);
  char error[256];
  FzeroCourseLayout l;
  if (!FzeroCourseLayoutRead(argv[2], &l, error, sizeof(error))) {
    puts(error);
    return 1;
  }
  FzeroCourse *c = malloc(sizeof(*c));
  if (!c)
    return 2;
  for (unsigned i = 0; i < l.count; ++i) {
    if (!FzeroCourseExtract(r, (size_t)n, &l, i, c, error, sizeof(error))) {
      printf("%u: %s\n", i, error);
      return 1;
    }
    if (argc == 4) {
      char path[1024];
      if (snprintf(path, sizeof(path), "%s-%u.bin", argv[3], i) >= (int)sizeof(path))
        return 2;
      f = fopen(path, "wb");
      if (!f)
        return 2;
      bool ok = fwrite(c, 1, sizeof(*c), f) == sizeof(*c);
      if (fclose(f))
        ok = false;
      if (!ok)
        return 2;
    }
    printf("course %u: setting=%02x blocks=%u grid=%u last=%u finish=%u pit=%u hash=", i,
           c->setting, c->block_size, c->grid_size, c->last_checkpoint, c->finish_checkpoint,
           c->pit_checkpoint);
    for (unsigned j = 0; j < 32; ++j)
      printf("%02x", c->hash[j]);
    puts("");
  }
  free(c);
  free(r);
  return 0;
}
