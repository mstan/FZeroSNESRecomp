#include "fzero_tracks_mods.h"
#include "fzero_tracks.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define COPY(field, value) snprintf(field, sizeof(field), "%s", value)
static int count(void *ctx) {
  (void)ctx;
  return 1;
}
static int feature_get(void *ctx, int index, RecompLauncherCModFeature *out) {
  (void)ctx;
  if (index || !out)
    return 0;
  memset(out, 0, sizeof(*out));
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
  if (!package || strcmp(package, "track-pack-loader") || !feature ||
      strcmp(feature, "tracks"))
    return 0;
  FzeroTrackLoaderEnable(enabled != 0);
  return 1;
}
static int resources(void *ctx, const char *p, const char *f) {
  (void)ctx;
  (void)p;
  (void)f;
  return 0;
}
static int resource_get(void *ctx, const char *p, const char *f, int i,
                        RecompLauncherCModResource *out) {
  (void)ctx;
  (void)p;
  (void)f;
  (void)i;
  (void)out;
  return 0;
}
static int resource_set(void *ctx, const char *p, const char *f, const char *r,
                        const char *path) {
  (void)ctx;
  (void)p;
  (void)f;
  (void)r;
  (void)path;
  return 0;
}
static int commit(void *ctx, const char *image) {
  (void)ctx;
  (void)image;
  return FzeroTracksSave();
}
static const char *error(void *ctx) {
  (void)ctx;
  return FzeroTracksError();
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
      .feature_resource_count = resources,
      .feature_resource_get = resource_get,
      .feature_resource_set_path = resource_set,
      .commit = commit,
      .last_error = error,
      .catalog_diagnostic_count = diagnostics,
      .catalog_diagnostic_get = diagnostic};
  return &provider;
}
