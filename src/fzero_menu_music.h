#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#define FZERO_MENU_CUES 6
typedef struct FzeroMenuCue {
  const char *id, *name;
  unsigned command;
} FzeroMenuCue;
extern const FzeroMenuCue FzeroMenuCues[FZERO_MENU_CUES];
void FzeroMenuMusicInit(void);
bool FzeroMenuMusicEnabled(void);
void FzeroMenuMusicEnable(bool enabled);
const char *FzeroMenuMusicPack(void);
bool FzeroMenuMusicSetPack(const char *id);
bool FzeroMenuMusicPath(unsigned cue, char *out, size_t cap);
bool FzeroMenuMusicCustom(unsigned cue);
bool FzeroMenuMusicSetPath(unsigned cue, const char *path);
bool FzeroMenuMusicValid(const char *path);
bool FzeroMenuMusicResolve(unsigned command, char *out, size_t cap);
void FzeroMenuMusicRead(const char *line);
bool FzeroMenuMusicWrite(FILE *file);
