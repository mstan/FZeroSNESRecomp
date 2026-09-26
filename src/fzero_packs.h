#pragma once
#ifdef __cplusplus
extern "C" {
#endif
#include "content_pack.h"
#include "fzero_course.h"
#include "fzero_music_sources.h"
/* Catalog owns copied CpPack entries; this module owns their resource paths. */
void FzeroPacksDiscover(CpCatalog *catalog, const char *directory);
bool FzeroPacksContains(const char *id);
bool FzeroPacksLoadCourse(const char *id, unsigned index, FzeroCourse *out,
                          char *error, size_t cap);
/* courses/name.fzc or name.fzm -> this pack's music/name.pcm. */
bool FzeroPacksCourseMusic(const char *id, unsigned index, char *out, size_t cap);
bool FzeroPacksMusicSources(FzeroMusicSources *out);
bool FzeroPacksResolveMusic(const char *source, unsigned track, char *out,
                            size_t cap);
bool FzeroPacksHasMusic(void);
const char *FzeroPacksMenuMusicId(unsigned index);
const char *FzeroPacksMenuMusicName(const char *id);
bool FzeroPacksMenuMusic(const char *id, const char *cue, char *out, size_t cap);
#ifdef __cplusplus
}
#endif
