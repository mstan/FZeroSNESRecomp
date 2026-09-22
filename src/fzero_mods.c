#include "fzero_mods.h"
#include "fzero_tracks_mods.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FzeroVideoSettings *video;
static const char *config_path;
static char error_text[128];
static const char *const aspects[] = {"16:9", "21:9", "32:9", "Fit"};
static const char *const rates[] = {"Auto", "60", "90", "120", "144", "165", "240", "360"};
#define COPY(field, value) snprintf(field, sizeof(field), "%s", value)
static const char *const packages[] = {"fzero-widescreen", "fzero-presentation-fps", "bs-deluxe", "fzero-hd-mode7", "fzero-diagnostics"};
static const char *const features[] = {"widescreen", "presentation-fps", "bs-deluxe", "hd-mode7", "diagnostics"};
static const char *const names[] = {"Widescreen", "Presentation FPS", "BS Deluxe", "HD Mode 7", "Diagnostics"};
static const char *const descriptions[] = {
  "Expand the race view and anchor the HUD at its outer edges.",
  "Choose the presentation rate independently of widescreen and game speed.",
  "Full BS Deluxe v1.1: original and BS courses, eight vehicles, alternate cups and Practice ghosts. Uses separate saves.",
  "Render the track at higher resolution with smoother scanline geometry. Works independently of widescreen and presentation FPS.",
  "Record hardware, active video settings and frame timings in the diagnostics folder beside the game (beside the AppImage on Linux). Off by default. Enable, play through a slowdown, then attach the newest performance JSONL file to your report. Logs stay on your machine; no ROM or save data is included."
};
static int count(void *ctx) { (void)ctx; return 5 + FzeroTrackModsProvider()->feature_count(ctx); }
static int identity(const char *package, const char *feature) {
  if (package && feature) for (int i = 0; i < 5; ++i)
    if (!strcmp(package, packages[i]) && !strcmp(feature, features[i])) return i + 1;
  return 0;
}
static int package_get(void *ctx, int index, RecompLauncherCModPackage *out) {
  if (index >= 5) return FzeroTrackModsProvider()->package_get(ctx, index-5, out);
  (void)ctx;
  if (index < 0 || index > 4 || !out) return 0;
  memset(out, 0, sizeof(*out));
  COPY(out->id, packages[index]); COPY(out->version, "1");
  COPY(out->name, names[index]); COPY(out->author, index == 2 ? "GuyPerfect, PowerPanda, Porthor, Catador" : "FZeroSNESRecomp contributors");
  COPY(out->description, descriptions[index]);
  out->enabled = index == 4 ? video->diagnostics : index == 3 ? video->hd_mode7 : index == 2 ? video->bs_deluxe : index ? video->fps_enabled : video->enhanced;
  return 1;
}
static int feature_get(void *ctx, int index, RecompLauncherCModFeature *out) {
  if (index >= 5) return FzeroTrackModsProvider()->feature_get(ctx, index-5, out);
  (void)ctx;
  if (index < 0 || index > 4 || !out) return 0;
  memset(out, 0, sizeof(*out));
  COPY(out->id, features[index]); COPY(out->package_id, packages[index]);
  COPY(out->package_name, names[index]); COPY(out->package_version, "1");
  COPY(out->name, names[index]); COPY(out->group, index == 4 ? "Support" : index == 2 ? "Content" : "Presentation");
  COPY(out->author, index == 2 ? "GuyPerfect, PowerPanda, Porthor, Catador" : "FZeroSNESRecomp contributors");
  COPY(out->description, descriptions[index]);
  out->enabled = index == 4 ? video->diagnostics : index == 3 ? video->hd_mode7 : index == 2 ? video->bs_deluxe : index ? video->fps_enabled : video->enhanced;
  COPY(out->status, out->enabled ? "Enabled" : "Disabled");
  if (index == 3 && video->hd_scale > 4) {
    snprintf(out->description, sizeof(out->description),
        "%s\n\nWarning: %ux is extremely demanding and can cause severe slowdown, "
        "especially with ultrawide views or high Presentation FPS. Use at your own risk. "
        "Try 2x and 60 FPS if performance drops.", descriptions[index], video->hd_scale);
    COPY(out->status, "Warning: high CPU and memory use above 4x");
  }
  out->option_count = index == 2 || index == 4 ? 0 : 1;
  return 1;
}
static int option_get(void *ctx, const char *package, const char *feature, int index,
                      RecompLauncherCModOption *out) {
  if (!identity(package, feature)) return FzeroTrackModsProvider()->feature_option_get(ctx, package, feature, index, out);
  (void)ctx;
  int kind = identity(package, feature);
  if (!kind || kind == 3 || kind == 5 || index != 0 || !out) return 0;
  memset(out, 0, sizeof(*out)); out->type = RECOMP_MOD_OPTION_CHOICE; out->step = 1;
  if (kind == 1) {
    COPY(out->id, "aspect"); COPY(out->label, "Aspect ratio");
    COPY(out->description, "Fit follows the window from 4:3 through 32:9.");
    COPY(out->value, FzeroAspectName(video->aspect));
    /* Must track FzeroVideoDefaults, or the launcher marks the wrong choice. */
    COPY(out->default_value, FzeroAspectName(FZERO_ASPECT_FIT));
    out->choice_count = 4;
  } else if (kind == 4) {
    COPY(out->id, "scale"); COPY(out->label, "Resolution multiplier (2-10)");
    COPY(out->description, "Whole numbers from 2 to 10 per dimension. 2x recommended; above 4x can cause severe slowdown. Use at your own risk.");
    snprintf(out->value, sizeof(out->value), "%u", video->hd_scale);
    COPY(out->default_value, "2");
    out->type = RECOMP_MOD_OPTION_INTEGER;
    out->min_value = FZERO_HD_SCALE_MIN; out->max_value = FZERO_HD_SCALE_MAX;
  } else {
    COPY(out->id, "fps"); COPY(out->label, "Presentation FPS");
    COPY(out->description, "Auto follows display refresh, up to 360 FPS.");
    if (video->fps) snprintf(out->value, sizeof(out->value), "%u", video->fps);
    else COPY(out->value, "Auto");
    COPY(out->default_value, "Auto"); out->choice_count = 8;
  }
  return 1;
}
static int choice_get(void *ctx, const char *package, const char *feature,
                      const char *option, int index, RecompLauncherCModChoice *out) {
  if (!identity(package, feature)) return FzeroTrackModsProvider()->feature_choice_get(ctx, package, feature, option, index, out);
  (void)ctx;
  if (!identity(package, feature) || !option || !out || index < 0) return 0;
  const char *value = NULL;
  if (identity(package, feature) == 1 && !strcmp(option, "aspect") && index < 4) value = aspects[index];
  if (identity(package, feature) == 2 && !strcmp(option, "fps") && index < 8) value = rates[index];
  if (!value) return 0;
  memset(out, 0, sizeof(*out)); COPY(out->value, value);
  COPY(out->label, !strcmp(value, "Fit") ? "Fit to window" : value);
  return 1;
}
static int enable(void *ctx, const char *package, const char *feature, int enabled) {
  if (!identity(package, feature)) return FzeroTrackModsProvider()->feature_enable(ctx, package, feature, enabled);
  (void)ctx;
  if (!identity(package, feature)) return 0;
  if (identity(package, feature) == 5) video->diagnostics = enabled != 0;
  else if (identity(package, feature) == 4) video->hd_mode7 = enabled != 0;
  else if (identity(package, feature) == 3) video->bs_deluxe = enabled != 0;
  else if (identity(package, feature) == 2) video->fps_enabled = enabled != 0;
  else {
    video->enhanced = enabled != 0;
    if (video->aspect == FZERO_ASPECT_STOCK) video->aspect = FZERO_ASPECT_16_9;
  }
  return 1;
}
static int set_option(void *ctx, const char *package, const char *feature,
                      const char *option, const char *value) {
  if (!identity(package, feature)) return FzeroTrackModsProvider()->feature_set_option(ctx, package, feature, option, value);
  (void)ctx;
  if (!identity(package, feature) || !option || !value) return 0;
  if (identity(package, feature) == 1 && !strcmp(option, "aspect")) {
    for (unsigned i = 0; i < 4; ++i) if (!strcmp(value, aspects[i]))
      return FzeroParseAspect(value, &video->aspect);
  } else if (identity(package, feature) == 4 && !strcmp(option, "scale")) {
    if (FzeroParseHdScale(value, &video->hd_scale)) { error_text[0] = 0; return 1; }
    COPY(error_text, "HD Mode 7 resolution must be a whole number from 2 to 10.");
  } else if (identity(package, feature) == 2 && !strcmp(option, "fps")) {
    for (unsigned i = 0; i < 8; ++i) if (!strcmp(value, rates[i])) {
      video->fps = i ? (unsigned)atoi(value) : 0; return 1;
    }
  }
  return 0;
}
static int commit(void *ctx, const char *image) {
  (void)ctx; (void)image;
  error_text[0] = 0;
  if (!FzeroTrackModsProvider()->commit(ctx, image)) return 0;
  if (FzeroVideoSave(video, config_path)) return 1;
  COPY(error_text, "Unable to save fzero-video.ini"); return 0;
}
static const char *last_error(void *ctx) { (void)ctx; return *error_text ? error_text : FzeroTrackModsProvider()->last_error(ctx); }

const RecompLauncherCModProvider *FzeroModsProvider(FzeroVideoSettings *settings, const char *path) {
  static RecompLauncherCModProvider provider;
  video = settings; config_path = path; error_text[0] = 0;
  memset(&provider, 0, sizeof(provider));
  provider.package_count = count; provider.package_get = package_get;
  provider.feature_count = count; provider.feature_get = feature_get;
  provider.feature_option_get = option_get; provider.feature_choice_get = choice_get;
  provider.feature_enable = enable; provider.feature_set_option = set_option;
  provider.commit = commit; provider.last_error = last_error;
  const RecompLauncherCModProvider *tracks = FzeroTrackModsProvider();
  provider.feature_resource_count = tracks->feature_resource_count;
  provider.feature_resource_get = tracks->feature_resource_get;
  provider.feature_resource_set_path = tracks->feature_resource_set_path;
  provider.catalog_diagnostic_count = tracks->catalog_diagnostic_count;
  provider.catalog_diagnostic_get = tracks->catalog_diagnostic_get;
  return &provider;
}
