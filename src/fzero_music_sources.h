#pragma once
#include <stdbool.h>
#include <stddef.h>
typedef struct FzeroMusicSources {
  char primary[64];
  unsigned count;
  struct {
    char id[64], prefix[96];
  } sources[64];
} FzeroMusicSources;
/* NULL source forbids substitution. Empty source means the selected primary;
 * race=true prevents another known pack's files from impersonating it. */
bool FzeroMusicResolve(const FzeroMusicSources *sources, const char *base,
                       const char *source, bool race, unsigned track, char *out,
                       size_t cap);
