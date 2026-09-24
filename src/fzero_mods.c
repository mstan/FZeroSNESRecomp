#include "fzero_mods.h"
#include "fzero_tracks_mods.h"
#include "fzero_tracks.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FzeroVideoSettings *video;
static const char *config_path;
static bool has_bundled_music;
static char error_text[128];
static const char *const aspects[] = {"16:9", "21:9", "32:9", "Fit"};
static const char *const rates[] = {"Auto", "60", "90", "120", "144", "165", "240", "360"};
#define COPY(field, value) snprintf(field, sizeof(field), "%s", value)
static const char *const packages[] = {"fzero-widescreen", "fzero-presentation-fps", "bs-cars", "fzero-hd-mode7", "fzero-diagnostics", "bs-tracks"};
static const char *const features[] = {"widescreen", "presentation-fps", "vehicles", "hd-mode7", "diagnostics", "tracks"};
static const char *const names[] = {"Widescreen", "Presentation FPS", "BS Satellaview vehicles", "HD Mode 7", "Diagnostics", "BS Satellaview tracks"};
static const char *const descriptions[] = {
  "Expand the race view and anchor the HUD at its outer edges.",
  "Choose the presentation rate independently of widescreen and game speed.",
  "Add Blue Thunder, Luna Bomber, Green Amazone and Fire Scorpion. Independent of the selected courses: use them with stock, original BS or CGP tracks.",
  "Render the track at higher resolution with smoother scanline geometry. Works independently of widescreen and presentation FPS.",
  "Record hardware, active video settings and frame timings in the diagnostics folder beside the game (beside the AppImage on Linux). Off by default. Enable, play through a slowdown, then attach the newest performance JSONL file to your report. Logs stay on your machine; no ROM or save data is included.",
  "Add the ten original BS courses in two leagues. Enabling this turns off Community Grand Prix, which includes corrected versions of these courses. BS vehicles have their own switch."
};
enum { VEHICLE_START=6+FZERO_RULE_COUNT-4, TRACK_START=VEHICLE_START+5 };
static unsigned visible_rule(unsigned index) {
  unsigned rule = index - 3;
  return rule >= FZERO_RULE_MSU ? rule + 1 : rule;
}
static const char *const vehicle_ids[]={"cgp-cars",
  "cgp-blue-falcon","cgp-wild-goose","cgp-golden-fox","cgp-fire-stingray"};
static const char *const vehicle_names[]={"CGP vehicles",
  "CGP Blue Falcon rebalance","CGP Wild Goose rebalance","CGP Golden Fox rebalance","CGP Fire Stingray rebalance"};
static const char *const vehicle_descriptions[]={
  "Enable all three CGP car groups together: eight additional ships plus the original four, for twelve identities. Keeps each group in its own selection column and Grand Prix rival set. New ships include their matching artwork, handling, boost and exhaust. Original-car rebalances remain separate. Disables stock BS vehicles.",
  "Opt in to CGP's Blue Falcon artwork, handling, energy boost and exhaust. Modifies the existing identity on any course; adds no duplicate ship. Disables stock BS vehicles.",
  "Opt in to CGP's Wild Goose artwork, handling, energy boost and exhaust. Modifies the existing identity on any course; adds no duplicate ship. Disables stock BS vehicles.",
  "Opt in to CGP's Golden Fox artwork, handling, energy boost and exhaust. Modifies the existing identity on any course; adds no duplicate ship. Disables stock BS vehicles.",
  "Opt in to CGP's Fire Stingray artwork, handling, energy boost and exhaust. Modifies the existing identity on any course; adds no duplicate ship. Disables stock BS vehicles."};
static bool vehicle_enabled(unsigned i) {
  return i==0 ? video->gameplay.vehicle_packs!=0 : (video->gameplay.stock_rebalance&(1u<<(i-1)))!=0;
}
static int count(void *ctx) { (void)ctx; return TRACK_START + FzeroTrackModsProvider()->feature_count(ctx); }
static int identity(const char *package, const char *feature) {
  if (package && feature) for (int i = 0; i < 6; ++i)
    if (!strcmp(package, packages[i]) && !strcmp(feature, features[i])) return i + 1;
  if (package && feature && !strcmp(feature,"rules"))
    for (int i=3;i<FZERO_RULE_COUNT;++i) if (i != FZERO_RULE_MSU && !strcmp(package,fzero_rules[i].id)) return 7+i;
  if (package && feature && !strcmp(feature,"vehicles"))
    for (int i=0;i<5;++i) if (!strcmp(package,vehicle_ids[i])) return 100+i;
  return 0;
}
static int package_get(void *ctx, int index, RecompLauncherCModPackage *out) {
  if (index >= TRACK_START) return FzeroTrackModsProvider()->package_get(ctx, index-TRACK_START, out);
  if (index>=VEHICLE_START && out) {
    unsigned i=(unsigned)(index-VEHICLE_START);memset(out,0,sizeof(*out));
    COPY(out->id,vehicle_ids[i]);COPY(out->name,vehicle_names[i]);COPY(out->version,"1");
    COPY(out->author,"Fennor Virastar and the CGP contributors");COPY(out->description,vehicle_descriptions[i]);
    out->enabled=vehicle_enabled(i);return 1;
  }
  if (index>=6 && out) {
    const FzeroRuleInfo *r=&fzero_rules[visible_rule((unsigned)index)]; memset(out,0,sizeof(*out));
    COPY(out->id,r->id); COPY(out->name,r->name); COPY(out->version,"1");
    COPY(out->author,"Fennor Virastar and the CGP contributors"); COPY(out->description,r->description);
    out->enabled=(video->gameplay.enabled>>visible_rule((unsigned)index))&1; return 1;
  }
  (void)ctx;
  if (index < 0 || index > 5 || !out) return 0;
  memset(out, 0, sizeof(*out));
  COPY(out->id, packages[index]); COPY(out->version, "1");
  COPY(out->name, names[index]); COPY(out->author, (index == 2 || index == 5) ? "GuyPerfect, PowerPanda, Porthor, Catador" : "FZeroSNESRecomp contributors");
  COPY(out->description, descriptions[index]);
  out->enabled = index == 5 ? video->bs_tracks : index == 4 ? video->diagnostics : index == 3 ? video->hd_mode7 : index == 2 ? video->bs_deluxe : index ? video->fps_enabled : video->enhanced;
  return 1;
}
static int feature_get(void *ctx, int index, RecompLauncherCModFeature *out) {
  if (index >= TRACK_START) return FzeroTrackModsProvider()->feature_get(ctx, index-TRACK_START, out);
  if (index>=VEHICLE_START && out) {
    unsigned i=(unsigned)(index-VEHICLE_START);memset(out,0,sizeof(*out));
    COPY(out->id,"vehicles");COPY(out->package_id,vehicle_ids[i]);COPY(out->package_name,vehicle_names[i]);
    COPY(out->package_version,"1");COPY(out->name,vehicle_names[i]);COPY(out->group,i==0?"Vehicle Packs":"Vehicle Rebalances");
    COPY(out->author,"Fennor Virastar and the CGP contributors");COPY(out->description,vehicle_descriptions[i]);
    out->enabled=vehicle_enabled(i);COPY(out->status,out->enabled?"Enabled":"Disabled");return 1;
  }
  if (index>=6 && out) {
    const FzeroRuleInfo *r=&fzero_rules[visible_rule((unsigned)index)]; memset(out,0,sizeof(*out));
    COPY(out->id,"rules"); COPY(out->package_id,r->id); COPY(out->package_name,r->name); COPY(out->package_version,"1");
    COPY(out->name,r->name); COPY(out->group,"CGP Rules and Fixes");
    COPY(out->author,"Fennor Virastar and the CGP contributors"); COPY(out->description,r->description);
    out->enabled=(video->gameplay.enabled>>visible_rule((unsigned)index))&1; COPY(out->status,out->enabled ? "Enabled" : "Disabled");
    out->option_count=0; return 1;
  }
  (void)ctx;
  if (index < 0 || index > 5 || !out) return 0;
  memset(out, 0, sizeof(*out));
  COPY(out->id, features[index]); COPY(out->package_id, packages[index]);
  COPY(out->package_name, names[index]); COPY(out->package_version, "1");
  COPY(out->name, names[index]); COPY(out->group, index == 5 ? "Track Packs" : index == 4 ? "Support" : index == 2 ? "Vehicle Packs" : "Presentation");
  COPY(out->author, (index == 2 || index == 5) ? "GuyPerfect, PowerPanda, Porthor, Catador" : "FZeroSNESRecomp contributors");
  COPY(out->description, descriptions[index]);
  out->enabled = index == 5 ? video->bs_tracks : index == 4 ? video->diagnostics : index == 3 ? video->hd_mode7 : index == 2 ? video->bs_deluxe : index ? video->fps_enabled : video->enhanced;
  COPY(out->status, out->enabled ? "Enabled" : "Disabled");
  if (index == 3 && video->hd_scale > 4) {
    snprintf(out->description, sizeof(out->description),
        "%s\n\nWarning: %ux is extremely demanding and can cause severe slowdown, "
        "especially with ultrawide views or high Presentation FPS. Use at your own risk. "
        "Try 2x and 60 FPS if performance drops.", descriptions[index], video->hd_scale);
    COPY(out->status, "Warning: high CPU and memory use above 4x");
  }
  out->option_count = index == 2 || index == 4 || index == 5 ? 0 : 1;
  return 1;
}
static int option_get(void *ctx, const char *package, const char *feature, int index,
                      RecompLauncherCModOption *out) {
  if (!identity(package, feature)) return FzeroTrackModsProvider()->feature_option_get ? FzeroTrackModsProvider()->feature_option_get(ctx, package, feature, index, out) : 0;
  (void)ctx;
  int kind = identity(package, feature);
  if (!kind || kind == 3 || kind == 5 || kind == 6 || index != 0 || !out) return 0;
  if (kind>=7) return 0;
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
  if (!identity(package, feature)) return FzeroTrackModsProvider()->feature_choice_get ? FzeroTrackModsProvider()->feature_choice_get(ctx, package, feature, option, index, out) : 0;
  (void)ctx;
  if (!identity(package, feature) || !option || !out || index < 0) return 0;
  const char *value = NULL;
  if (identity(package,feature)>=7) return 0;
  if (identity(package, feature) == 1 && !strcmp(option, "aspect") && index < 4) value = aspects[index];
  if (identity(package, feature) == 2 && !strcmp(option, "fps") && index < 8) value = rates[index];
  if (!value) return 0;
  memset(out, 0, sizeof(*out)); COPY(out->value, value);
  COPY(out->label, !strcmp(value, "Fit") ? "Fit to window" : value);
  return 1;
}
static int enable(void *ctx, const char *package, const char *feature, int enabled) {
  if (!identity(package, feature)) {
    int ok=FzeroTrackModsProvider()->feature_enable(ctx,package,feature,enabled);
    if (ok && enabled && !strcmp(package,"cgp")) video->bs_tracks=false;
    return ok;
  }
  (void)ctx;
  if (!identity(package, feature)) return 0;
  if (identity(package,feature)>=100) {
    unsigned i=(unsigned)(identity(package,feature)-100);
    unsigned *bits=i==0?&video->gameplay.vehicle_packs:&video->gameplay.stock_rebalance;
    unsigned bit=i==0?7u:1u<<(i-1);
    if(enabled) { *bits|=bit;video->bs_deluxe=false; } else *bits&=~bit;
  } else if (identity(package,feature)>=7) {
    uint32_t bit=1u<<(identity(package,feature)-7);
    if (enabled) video->gameplay.enabled|=bit; else video->gameplay.enabled&=~bit;
  } else if (identity(package,feature)==6) {
    video->bs_tracks=enabled!=0;
    if (enabled) FzeroTracksEnable(cp_catalog_find(FzeroTracksCatalog(),"cgp"),false);
  } else if (identity(package, feature) == 5) video->diagnostics = enabled != 0;
  else if (identity(package, feature) == 4) video->hd_mode7 = enabled != 0;
  else if (identity(package, feature) == 3) {
    video->bs_deluxe = enabled != 0;
    if(enabled)video->gameplay.vehicle_packs=video->gameplay.stock_rebalance=0;
  }
  else if (identity(package, feature) == 2) video->fps_enabled = enabled != 0;
  else {
    video->enhanced = enabled != 0;
    if (video->aspect == FZERO_ASPECT_STOCK) video->aspect = FZERO_ASPECT_16_9;
  }
  return 1;
}
static int set_option(void *ctx, const char *package, const char *feature,
                      const char *option, const char *value) {
  if (!identity(package, feature)) return FzeroTrackModsProvider()->feature_set_option ? FzeroTrackModsProvider()->feature_set_option(ctx, package, feature, option, value) : 0;
  (void)ctx;
  if (!identity(package, feature) || !option || !value) return 0;
  if (identity(package,feature)>=7) return 0;
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

static const RecompLauncherCModPreset presets[] = {
  {"vanilla", "Vanilla", "Original cars, rules, title and SNES music. Turns off CGP and BS content. Unrelated packs and personal settings stay as selected."},
  {"satellaview", "Satellaview", "Original cars plus the four BS cars and ten BS courses, with their stock behavior and SNES music. Turns off CGP. Unrelated choices stay as selected."},
  {"cgp", "Community Grand Prix", "CGP courses, all twelve cars and their authored rebalances, every CGP rule including Legend and credits, F-Zero 55 title and bundled music. Disables conflicting BS content; keeps unrelated choices."}
};
static int preset_count(void *ctx) { (void)ctx; return 3; }
static int preset_get(void *ctx, int i, RecompLauncherCModPreset *out) {
  (void)ctx;
  if (i < 0 || i >= 3 || !out) return 0;
  *out = presets[i];
  if (i == 2 && !has_bundled_music)
    COPY(out->description, "CGP courses, all twelve cars and their rebalances, every CGP rule including Legend and credits, and F-Zero 55 title. Uses your selected custom music, or SNES audio until music is supplied. Disables conflicting BS content; keeps unrelated choices.");
  return 1;
}
static const char *preset_current(void *ctx, const RecompLauncherCSettings *s) {
  (void)ctx;
  if (!s) return "";
  const CpPack *cgp = cp_catalog_find(FzeroTracksCatalog(), "cgp");
  unsigned rules = video->gameplay.enabled & ~(1u << FZERO_RULE_MSU);
  unsigned all = ((1u << FZERO_RULE_COUNT) - 1) & ~7u & ~(1u << FZERO_RULE_MSU);
  bool cgp_music = has_bundled_music ?
      s->msu1_enabled && !strcmp(s->msu1_pack,"cgp") :
      s->msu1_enabled == (s->msu1_dir[0] != 0) && !s->msu1_pack[0];
  if (FzeroTracksEnabled(cgp) && FzeroTracksTitleEnabled(cgp) &&
      !video->bs_deluxe && !video->bs_tracks && video->gameplay.vehicle_packs == 7 &&
      video->gameplay.stock_rebalance == 15 && rules == all && cgp_music) return "cgp";
  if (!FzeroTracksEnabled(cgp) && !rules && !video->gameplay.vehicle_packs &&
      !video->gameplay.stock_rebalance && !s->msu1_enabled) {
    if (video->bs_deluxe && video->bs_tracks) return "satellaview";
    if (!video->bs_deluxe && !video->bs_tracks) return "vanilla";
  }
  return "";
}
static int preset_apply(void *ctx, const char *id, RecompLauncherCSettings *s) {
  (void)ctx;
  if (!s || !id) return 0;
  unsigned index;
  for (index=0; index<3; ++index) if (!strcmp(id,presets[index].id)) break;
  if (index == 3) return 0;
  const CpPack *cgp = cp_catalog_find(FzeroTracksCatalog(), "cgp");
  if (index == 2 && (!cgp || FzeroTracksHidden(cgp) || !FzeroTracksHasTitle(cgp))) {
    COPY(error_text,"Community Grand Prix track pack is unavailable"); return 0;
  }
  if (cgp && !FzeroTracksEnable(cgp,index == 2)) {
    COPY(error_text,"Unable to change the CGP track pack"); return 0;
  }
  if (cgp) FzeroTracksSetTitle(cgp,index == 2);
  video->bs_deluxe = video->bs_tracks = index == 1;
  video->gameplay.vehicle_packs = index == 2 ? 7 : 0;
  video->gameplay.stock_rebalance = index == 2 ? 15 : 0;
  video->gameplay.enabled = index == 2 ? ((1u << FZERO_RULE_COUNT)-1) & ~7u : 0;
  s->msu1_enabled = index == 2 && (has_bundled_music || s->msu1_dir[0]);
  if (index == 2) COPY(s->msu1_pack,has_bundled_music ? "cgp" : "");
  if (!s->msu1_enabled) video->gameplay.enabled &= ~(1u << FZERO_RULE_MSU);
  error_text[0] = 0;
  return 1;
}

const RecompLauncherCModProvider *FzeroModsProvider(FzeroVideoSettings *settings, const char *path, bool bundled_music) {
  static RecompLauncherCModProvider provider;
  video = settings; config_path = path; error_text[0] = 0;
  has_bundled_music = bundled_music;
  video->gameplay.enabled &= ~7u;
  if(video->bs_deluxe)video->gameplay.vehicle_packs=video->gameplay.stock_rebalance=0;
  else if(video->gameplay.vehicle_packs)video->gameplay.vehicle_packs=7;
  if (FzeroTracksEnabled(cp_catalog_find(FzeroTracksCatalog(),"cgp"))) video->bs_tracks=false;
  memset(&provider, 0, sizeof(provider));
  provider.package_count = count; provider.package_get = package_get;
  provider.feature_count = count; provider.feature_get = feature_get;
  provider.feature_option_get = option_get; provider.feature_choice_get = choice_get;
  provider.feature_enable = enable; provider.feature_set_option = set_option;
  provider.commit = commit; provider.last_error = last_error;
  provider.preset_count = preset_count; provider.preset_get = preset_get;
  provider.preset_current = preset_current; provider.preset_apply = preset_apply;
  const RecompLauncherCModProvider *tracks = FzeroTrackModsProvider();
  provider.feature_resource_count = tracks->feature_resource_count;
  provider.feature_resource_get = tracks->feature_resource_get;
  provider.feature_resource_set_path = tracks->feature_resource_set_path;
  provider.catalog_diagnostic_count = tracks->catalog_diagnostic_count;
  provider.catalog_diagnostic_get = tracks->catalog_diagnostic_get;
  return &provider;
}
