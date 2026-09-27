#include "fzero_tracks_mods.h"
#include "fzero_tracks.h"
#include "fzero_packs.h"
#include "fzero_menu_music.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define COPY(field, value) snprintf(field, sizeof(field), "%s", value)
static char music_error[256];
static bool is_music(const char *p, const char *f) {
  return p && f && !strcmp(p, "menu-music") && !strcmp(f, "menu-music");
}
static unsigned music_choices(void) {
  unsigned n = 0;
  while (FzeroPacksMenuMusicId(n)) ++n;
  return n + 1;
}
static int count(void *ctx) {
  (void)ctx;
  return 2;
}
static int feature_get(void *ctx, int index, RecompLauncherCModFeature *out) {
  (void)ctx;
  if (index < 0 || index >= 2 || !out)
    return 0;
  memset(out, 0, sizeof(*out));
  if (index == 1) {
    COPY(out->id, "menu-music");
    COPY(out->package_id, "menu-music");
    COPY(out->name, "Menu and event music");
    COPY(out->package_name, "Menu and event music");
    COPY(out->package_version, "1");
    COPY(out->author, "Installed soundtrack authors");
    COPY(out->group, "Audio");
    COPY(out->description, "Music for the title, menus, countdown, racers ready, lost life and ending. Prefilled from the selected pack; Change file replaces one song, and Clear selection restores its pack default. Enable MSU-1 in Settings > Audio. Missing songs use SNES audio.");
    out->enabled = FzeroMenuMusicEnabled();
    out->option_count = 1;
    COPY(out->status, out->enabled ? "Pack defaults with optional replacements" : "SNES menu and event audio");
    return 1;
  }
  COPY(out->id, "tracks");
  COPY(out->package_id, "track-pack-loader");
  COPY(out->name, "Track Pack Loader");
  COPY(out->package_name, "Track Pack Loader");
  COPY(out->group, "Track Packs");
  COPY(out->author, "F-Zero Forever and pack authors");
  COPY(out->package_version, "1");
  unsigned cups = 0, courses = 0;
  const CpCatalog *c = FzeroTracksCatalog();
  for (unsigned i = 0; i < c->count; ++i)
    if (!strcmp(c->packs[i]->adapter, "fzero-course-v1")) {
      cups += c->packs[i]->cup_count;
      courses += c->packs[i]->track_count;
    }
  snprintf(out->description, sizeof(out->description),
           "Enabled by default. Loads every installed course pack from mods/packs. %u extra cups, "
           "%u course entries installed; the original 15 remain available. Add "
           "or remove folders/ZIPs to change content. Mutually exclusive with "
           "BS Satellaview Tracks. Music is optional.",
           cups, courses);
  out->enabled = FzeroTrackLoaderEnabled();
  COPY(out->status, out->enabled ? "All installed packs enabled" : "Disabled");
  return 1;
}
static int package_get(void *ctx, int i, RecompLauncherCModPackage *out) {
  RecompLauncherCModFeature f;
  if (!out || !feature_get(ctx, i, &f))
    return 0;
  memset(out, 0, sizeof(*out));
  COPY(out->id, f.package_id);
  COPY(out->name, f.name);
  COPY(out->description, f.description);
  COPY(out->author, f.author);
  COPY(out->version, "1");
  out->enabled = f.enabled;
  return 1;
}
static int enable(void *ctx, const char *package, const char *feature,
                  int enabled) {
  (void)ctx;
  if (is_music(package, feature)) {
    FzeroMenuMusicEnable(enabled != 0);
    return 1;
  }
  if (!package || strcmp(package, "track-pack-loader") || !feature ||
      strcmp(feature, "tracks"))
    return 0;
  FzeroTrackLoaderEnable(enabled != 0);
  return 1;
}
static int resources(void *ctx, const char *p, const char *f) {
  (void)ctx;
  return is_music(p, f) ? FZERO_MENU_CUES : 0;
}
static int resource_get(void *ctx, const char *p, const char *f, int i,
                        RecompLauncherCModResource *out) {
  (void)ctx;
  if (!is_music(p, f) || i < 0 || i >= FZERO_MENU_CUES || !out) return 0;
  memset(out, 0, sizeof(*out));
  COPY(out->id, FzeroMenuCues[i].id);
  COPY(out->label, FzeroMenuCues[i].name);
  COPY(out->description, "Choose an MSU-1 PCM recording, or Clear to use the pack's supplied song. This selection changes only this event.");
  COPY(out->file_patterns, "*.pcm");
  COPY(out->file_description, "MSU-1 PCM audio");
  FzeroMenuMusicPath((unsigned)i, out->path, sizeof(out->path));
  out->verified = FzeroMenuMusicValid(out->path);
  COPY(out->status, out->verified ? (FzeroMenuMusicCustom(i) ? "Custom recording" : "Pack default - ready") : "Recording not installed - SNES audio");
  return 1;
}
static int resource_set(void *ctx, const char *p, const char *f, const char *r,
                        const char *path) {
  (void)ctx;
  if (!is_music(p, f) || !r) return 0;
  music_error[0] = 0;
  for (unsigned i = 0; i < FZERO_MENU_CUES; ++i)
    if (!strcmp(r, FzeroMenuCues[i].id)) {
      if (FzeroMenuMusicSetPath(i, path)) return 1;
      COPY(music_error, "Choose a valid MSU-1 .pcm file (with an MSU1 header), or Clear to restore the pack default.");
      return 0;
    }
  return 0;
}
static int option_get(void *ctx, const char *p, const char *f, int i,
                      RecompLauncherCModOption *out) {
  (void)ctx;
  if (!is_music(p, f) || i != 0 || !out) return 0;
  memset(out, 0, sizeof(*out));
  COPY(out->id, "soundtrack");
  COPY(out->label, "Default soundtrack");
  COPY(out->description, "Installed packs supply the default songs below. Individual replacements remain selected when changing this.");
  out->type = RECOMP_MOD_OPTION_CHOICE;
  COPY(out->value, FzeroMenuMusicPack());
  out->choice_count = (int)music_choices();
  return 1;
}
static int choice_get(void *ctx, const char *p, const char *f, const char *o,
                      int i, RecompLauncherCModChoice *out) {
  (void)ctx;
  if (!is_music(p, f) || !o || strcmp(o, "soundtrack") || i < 0 ||
      (unsigned)i >= music_choices() || !out) return 0;
  memset(out, 0, sizeof(*out));
  if (!i) COPY(out->label, "Pack default");
  else {
    const char *id = FzeroPacksMenuMusicId((unsigned)i - 1);
    COPY(out->value, id);
    COPY(out->label, FzeroPacksMenuMusicName(id));
  }
  return 1;
}
static int set_option(void *ctx, const char *p, const char *f, const char *o,
                      const char *value) {
  (void)ctx;
  return is_music(p, f) && o && !strcmp(o, "soundtrack") && FzeroMenuMusicSetPack(value);
}
static int commit(void *ctx, const char *image) {
  (void)ctx;
  (void)image;
  return FzeroTracksSave();
}
static const char *error(void *ctx) {
  (void)ctx;
  return music_error[0] ? music_error : FzeroTracksError();
}
static int diagnostics(void *ctx) {
  (void)ctx;
  return (int)FzeroTracksDiagnosticCount();
}
static int diagnostic(void *ctx, int index, RecompLauncherCModDiagnostic *out) {
  (void)ctx;
  if (!out || index < 0 || (unsigned)index >= FzeroTracksDiagnosticCount())
    return 0;
  memset(out, 0, sizeof(*out));
  out->severity = RECOMP_MOD_DIAGNOSTIC_ERROR;
  COPY(out->resource, "Track Packs");
  COPY(out->message, FzeroTracksDiagnostic((unsigned)index));
  return 1;
}
const RecompLauncherCModProvider *FzeroTrackModsProvider(void) {
  static const RecompLauncherCModProvider provider = {
      .package_count = count,
      .package_get = package_get,
      .feature_count = count,
      .feature_get = feature_get,
      .feature_enable = enable,
      .feature_option_get = option_get,
      .feature_choice_get = choice_get,
      .feature_set_option = set_option,
      .feature_resource_count = resources,
      .feature_resource_get = resource_get,
      .feature_resource_set_path = resource_set,
      .commit = commit,
      .last_error = error,
      .catalog_diagnostic_count = diagnostics,
      .catalog_diagnostic_get = diagnostic};
  return &provider;
}
