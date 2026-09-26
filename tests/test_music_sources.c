#include "fzero_music_sources.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      fprintf(stderr, "line %u: %s\n", __LINE__, #x);                          \
      return 1;                                                                \
    }                                                                          \
  } while (0)
static void file(const char *name, const char *text) {
  FILE *f = fopen(name, "wb");
  if (!f)
    abort();
  fputs(text, f);
  fclose(f);
}
int main(void) {
  FzeroMusicSources s = {0};
  char path[256];
  strcpy(s.primary, "one");
  s.count = 3;
  strcpy(s.sources[0].id, "one");
  strcpy(s.sources[0].prefix, "first");
  strcpy(s.sources[1].id, "two");
  strcpy(s.sources[1].prefix, "second");
  strcpy(s.sources[2].id, "three");
  strcpy(s.sources[2].prefix, "third");
  file("first-10.pcm", "one");
  file("second-10.pcm", "two");
  CHECK(FzeroMusicResolve(&s, "first", "two", true, 10, path, sizeof(path)) &&
        !strcmp(path, "second-10.pcm"));
  CHECK(!FzeroMusicResolve(&s, "first", "three", true, 10, path, sizeof(path)));
  CHECK(!FzeroMusicResolve(&s, "first", NULL, true, 10, path, sizeof(path)));
  file("third-10.pcm", "three");
  CHECK(FzeroMusicResolve(&s, "first", "three", true, 10, path, sizeof(path)) &&
        !strcmp(path, "third-10.pcm"));
  remove("first-10.pcm");
  remove("second-10.pcm");
  remove("third-10.pcm");
  puts("music namespace tests passed");
  return 0;
}
