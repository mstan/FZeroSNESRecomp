#include "fzero_mods.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FzeroVideoSettings *video;
static const char *config_path;
static char error_text[128];
static const char *const aspects[] = {"16:9", "21:9", "32:9", "Fit"};
static const char *const rates[] = {"Auto", "60", "90", "120", "144", "165", "240", "360"};
#define COPY(field, value) snprintf(field, sizeof(field), "%s", value)
static const char *const packages[] = {"fzero-widescreen", "fzero-presentation-fps", "bs-deluxe", "fzero-dlss5"};
static const char *const features[] = {"widescreen", "presentation-fps", "bs-deluxe", "dlss5"};
static const char *const names[] = {"Widescreen", "Presentation FPS", "BS Deluxe", "DLSS5"};
static const char *const descriptions[] = {
  "Expand the race view and anchor the HUD at its outer edges.",
  "Choose the presentation rate independently of widescreen and game speed.",
  "Full BS Deluxe: original and BS courses, eight vehicles, alternate cups and Practice ghosts. Uses separate saves.",
  "DLSS5 neural rendering with Vulkan presentation. Uses the installed NVIDIA neural rendering runtime."
};
static int count(void *ctx) { (void)ctx; return 4; }
static int identity(const char *package, const char *feature) {
  if (package && feature) for (int i = 0; i < 4; ++i)
    if (!strcmp(package, packages[i]) && !strcmp(feature, features[i])) return i + 1;
  return 0;
}
static int package_get(void *ctx, int index, RecompLauncherCModPackage *out) {
  (void)ctx;
  if (index < 0 || index > 3 || !out) return 0;
  memset(out, 0, sizeof(*out));
  COPY(out->id, packages[index]); COPY(out->version, "1");
  COPY(out->name, names[index]); COPY(out->author, index == 2 ? "GuyPerfect, PowerPanda, Porthor, Catador" : "FZeroSNESRecomp contributors");
  COPY(out->description, descriptions[index]);
  out->enabled = index == 3 ? video->dlss : index == 2 ? video->bs_deluxe : index ? video->fps_enabled : video->enhanced;
  return 1;
}
static int feature_get(void *ctx, int index, RecompLauncherCModFeature *out) {
  (void)ctx;
  if (index < 0 || index > 3 || !out) return 0;
  memset(out, 0, sizeof(*out));
  COPY(out->id, features[index]); COPY(out->package_id, packages[index]);
  COPY(out->package_name, names[index]); COPY(out->package_version, "1");
  COPY(out->name, names[index]); COPY(out->group, index == 2 ? "Content" : "Presentation");
  COPY(out->author, index == 2 ? "GuyPerfect, PowerPanda, Porthor, Catador" : "FZeroSNESRecomp contributors");
  COPY(out->description, descriptions[index]);
  out->enabled = index == 3 ? video->dlss : index == 2 ? video->bs_deluxe : index ? video->fps_enabled : video->enhanced;
  COPY(out->status, out->enabled ? "Enabled" : "Disabled");
  out->option_count = index >= 2 ? 0 : 1;
  return 1;
}
static int option_get(void *ctx, const char *package, const char *feature, int index,
                      RecompLauncherCModOption *out) {
  (void)ctx;
  int kind = identity(package, feature);
  if (!kind || kind >= 3 || index != 0 || !out) return 0;
  memset(out, 0, sizeof(*out)); out->type = RECOMP_MOD_OPTION_CHOICE; out->step = 1;
  if (kind == 1) {
    COPY(out->id, "aspect"); COPY(out->label, "Aspect ratio");
    COPY(out->description, "Fit follows the window from 4:3 through 32:9.");
    COPY(out->value, FzeroAspectName(video->aspect)); COPY(out->default_value, "16:9");
    out->choice_count = 4;
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
  (void)ctx;
  if (!identity(package, feature)) return 0;
  if (identity(package, feature) == 4) {
    video->dlss = enabled != 0;
    if (video->dlss) video->vulkan = true;
  }
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
  (void)ctx;
  if (!identity(package, feature) || !option || !value) return 0;
  if (identity(package, feature) == 1 && !strcmp(option, "aspect")) {
    for (unsigned i = 0; i < 4; ++i) if (!strcmp(value, aspects[i]))
      return FzeroParseAspect(value, &video->aspect);
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
  if (FzeroVideoSave(video, config_path)) return 1;
  COPY(error_text, "Unable to save fzero-video.ini"); return 0;
}
static const char *last_error(void *ctx) { (void)ctx; return error_text; }

const RecompLauncherCModProvider *FzeroModsProvider(FzeroVideoSettings *settings, const char *path) {
  static RecompLauncherCModProvider provider;
  video = settings; config_path = path; error_text[0] = 0;
  memset(&provider, 0, sizeof(provider));
  provider.package_count = count; provider.package_get = package_get;
  provider.feature_count = count; provider.feature_get = feature_get;
  provider.feature_option_get = option_get; provider.feature_choice_get = choice_get;
  provider.feature_enable = enable; provider.feature_set_option = set_option;
  provider.commit = commit; provider.last_error = last_error;
  return &provider;
}
