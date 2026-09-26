#include "fzero_menu_music.h"
#include "fzero_packs.h"
#include <stdint.h>
#include <string.h>

const FzeroMenuCue FzeroMenuCues[FZERO_MENU_CUES] = {
    {"countdown", "Countdown / race start", 1},
    {"ready", "Racers ready / zoom", 2},
    {"lost-life", "Lost life", 3},
    {"title", "Title screen", 4},
    {"select", "Menus / records", 5},
    {"ending", "Ending / victory", 7}};
static bool enabled;
static char selected_pack[CP_ID], custom[FZERO_MENU_CUES][1024];
void FzeroMenuMusicInit(void) {
  enabled = true;
  selected_pack[0] = 0;
  memset(custom, 0, sizeof(custom));
}
bool FzeroMenuMusicEnabled(void) { return enabled; }
void FzeroMenuMusicEnable(bool value) { enabled = value; }
const char *FzeroMenuMusicPack(void) { return selected_pack; }
bool FzeroMenuMusicSetPack(const char *id) {
  if (!id || strlen(id) >= sizeof(selected_pack)) return false;
  if (*id && !FzeroPacksMenuMusicName(id)) return false;
  strcpy(selected_pack, id);
  return true;
}
bool FzeroMenuMusicCustom(unsigned cue) {
  return cue < FZERO_MENU_CUES && custom[cue][0];
}
bool FzeroMenuMusicPath(unsigned cue, char *out, size_t cap) {
  if (!out || !cap) return false;
  out[0] = 0;
  if (cue >= FZERO_MENU_CUES) return false;
  if (custom[cue][0]) {
    if (strlen(custom[cue]) >= cap) return false;
    strcpy(out, custom[cue]);
    return true;
  }
  return FzeroPacksMenuMusic(selected_pack, FzeroMenuCues[cue].id, out, cap);
}
bool FzeroMenuMusicValid(const char *path) {
  if (!path || !*path) return false;
  FILE *f = fopen(path, "rb");
  if (!f) return false;
  uint8_t header[8];
  bool ok = fread(header, 1, 8, f) == 8 && !memcmp(header, "MSU1", 4);
  if (fseek(f, 0, SEEK_END)) ok = false;
  long bytes = ftell(f);
  if (ok) {
    uint32_t loop = (uint32_t)header[4] | (uint32_t)header[5] << 8 |
                    (uint32_t)header[6] << 16 | (uint32_t)header[7] << 24;
    ok = bytes > 8 && !((bytes - 8) % 4) && loop < (uint64_t)(bytes - 8) / 4;
  }
  fclose(f);
  return ok;
}
bool FzeroMenuMusicSetPath(unsigned cue, const char *path) {
  if (cue >= FZERO_MENU_CUES || !path || strlen(path) >= sizeof(custom[cue]) ||
      strpbrk(path, "\r\n")) return false;
  if (*path && !FzeroMenuMusicValid(path)) return false;
  strcpy(custom[cue], path);
  return true;
}
bool FzeroMenuMusicResolve(unsigned command, char *out, size_t cap) {
  if (!enabled) return false;
  for (unsigned i = 0; i < FZERO_MENU_CUES; ++i)
    if (FzeroMenuCues[i].command == command)
      return FzeroMenuMusicPath(i, out, cap) && FzeroMenuMusicValid(out);
  return false;
}
void FzeroMenuMusicRead(const char *line) {
  if (!strcmp(line, "menu_music=0")) enabled = false;
  if (!strcmp(line, "menu_music=1")) enabled = true;
  if (!strncmp(line, "menu_pack=", 10)) {
    const char *id = line + 10;
    if (strlen(id) < sizeof(selected_pack) &&
        strspn(id, "abcdefghijklmnopqrstuvwxyz0123456789-_") == strlen(id))
      strcpy(selected_pack, id); /* Preserve a temporarily removed pack. */
  }
  for (unsigned i = 0; i < FZERO_MENU_CUES; ++i) {
    char key[64];
    snprintf(key, sizeof(key), "menu_%s=", FzeroMenuCues[i].id);
    size_t n = strlen(key);
    if (!strncmp(line, key, n) && strlen(line + n) < sizeof(custom[i]))
      strcpy(custom[i], line + n); /* Missing files fall back to SNES audio. */
  }
}
bool FzeroMenuMusicWrite(FILE *file) {
  if (fprintf(file, "menu_music=%u\nmenu_pack=%s\n", enabled, selected_pack) < 0)
    return false;
  for (unsigned i = 0; i < FZERO_MENU_CUES; ++i)
    if (fprintf(file, "menu_%s=%s\n", FzeroMenuCues[i].id, custom[i]) < 0)
      return false;
  return true;
}
