#include "fzero_music_sources.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static bool same(const char *a, const char *b) {
  while (*a && *b)
    if (tolower((unsigned char)*a++) != tolower((unsigned char)*b++))
      return false;
  return *a == *b;
}
static bool exists(const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f)
    return false;
  fclose(f);
  return true;
}
bool FzeroMusicResolve(const FzeroMusicSources *s, const char *base,
                       const char *source, bool race, unsigned track, char *out,
                       size_t cap) {
  if (!source)
    return false;
  const char *leaf = base, *selected = NULL;
  for (const char *p = base; *p; ++p)
    if (*p == '/' || *p == '\\')
      leaf = p + 1;
  size_t dir = (size_t)(leaf - base);
  for (unsigned i = 0; i < s->count; ++i)
    if (same(leaf, s->sources[i].prefix)) {
      selected = s->sources[i].id;
      break;
    }
  if (!*source && (!race || !selected || !strcmp(selected, s->primary)))
    return snprintf(out, cap, "%s-%u.pcm", base, track) < (int)cap;
  const char *wanted = *source ? source : s->primary;
  if (selected && !strcmp(selected, wanted) &&
      snprintf(out, cap, "%s-%u.pcm", base, track) < (int)cap && exists(out))
    return true;
  for (unsigned i = 0; i < s->count; ++i)
    if (!strcmp(wanted, s->sources[i].id)) {
      if (snprintf(out, cap, "%.*s%s-%u.pcm", (int)dir, base,
                   s->sources[i].prefix, track) >= (int)cap)
        return false;
      if (exists(out))
        return true;
    }
  return false;
}
