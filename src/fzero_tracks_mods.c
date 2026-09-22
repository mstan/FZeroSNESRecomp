#include "fzero_tracks_mods.h"
#include "fzero_tracks.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define COPY(field, value) snprintf(field, sizeof(field), "%s", value)
static const CpPack *external(unsigned index) {
    const CpCatalog *c = FzeroTracksCatalog();
    for (unsigned i = 0; i < c->count; ++i) if (!strcmp(c->packs[i]->adapter, "fzero-course-v1")) {
        if (!index--) return c->packs[i];
    }
    return NULL;
}
static const CpPack *find(const char *id) {
    const CpPack *p = id ? cp_catalog_find(FzeroTracksCatalog(), id) : NULL;
    return p && !strcmp(p->adapter, "fzero-course-v1") ? p : NULL;
}
static bool library(const char *package, const char *feature) {
    return package && feature && !strcmp(package, "track-library") && !strcmp(feature, "cups");
}
static int count(void *ctx) { (void)ctx; unsigned n = 0; while (external(n)) ++n; return (int)n+1; }
static int feature_get(void *ctx, int i, RecompLauncherCModFeature *out) {
    (void)ctx; if (!out || i < 0) return 0;
    memset(out, 0, sizeof(*out));
    COPY(out->group, "Track Packs"); COPY(out->package_version, "1");
    if (!i) {
        COPY(out->id, "cups"); COPY(out->package_id, "track-library");
        COPY(out->name, "Track Library"); COPY(out->package_name, "Track Library");
        COPY(out->author, "FZeroSNESRecomp contributors");
        COPY(out->description, "Add installed course packs to the in-game Grand Prix league menu. Put IPS/BPS patches and their manifests in mods/track-packs. Known MAX League patches are recognized automatically. Existing cars, cups and rendering remain available.");
        out->option_count = 0; out->enabled = FzeroTracksLibraryEnabled();
        COPY(out->status, out->enabled ? "Choose all cups inside the game" : "Original game menus and content");
        return 1;
    }
    const CpPack *p = external((unsigned)i-1); if (!p) return 0;
    COPY(out->id, "tracks"); COPY(out->package_id, p->id); COPY(out->name, p->name);
    COPY(out->package_name, p->name); COPY(out->author, p->author);
    snprintf(out->description, sizeof(out->description), "%u cup(s), %u course(s). Supply your own IPS or BPS patch. Course assets are extracted at launch and added to the game. Donor rules and executable code are not imported.", p->cup_count, p->track_count);
    out->enabled = FzeroTracksEnabled(p);
    COPY(out->status, FzeroTracksAvailable(p) ? "Patch supplied; courses checked at launch" : "Drop the patch in mods/track-packs, or locate it here");
    return 1;
}
static int package_get(void *ctx, int i, RecompLauncherCModPackage *out) {
    RecompLauncherCModFeature f;
    if (!out || !feature_get(ctx, i, &f)) return 0;
    memset(out, 0, sizeof(*out)); COPY(out->id, f.package_id); COPY(out->name, f.name);
    COPY(out->author, f.author); COPY(out->description, f.description); COPY(out->version, "1"); out->enabled = f.enabled; return 1;
}
static int enable(void *ctx, const char *package, const char *feature, int enabled) {
    (void)ctx;
    if (library(package, feature)) { FzeroTracksLibraryEnable(enabled != 0); return 1; }
    const CpPack *p = find(package);
    return p && feature && !strcmp(feature, "tracks") && FzeroTracksEnable(p, enabled != 0);
}
static int resources(void *ctx, const char *package, const char *feature) {
    (void)ctx; return find(package) && feature && !strcmp(feature, "tracks");
}
static int resource_get(void *ctx, const char *package, const char *feature, int i, RecompLauncherCModResource *out) {
    if (!out || i || !resources(ctx, package, feature)) return 0;
    const CpPack *p = find(package); memset(out, 0, sizeof(*out));
    COPY(out->id, "patch"); COPY(out->label, "Your IPS or BPS patch"); COPY(out->path, FzeroTracksPatch(p));
    COPY(out->description, "Extract the patch from its ZIP first. Each pack is parsed independently from a fresh copy of the original ROM.");
    COPY(out->file_patterns, "*.ips,*.bps"); COPY(out->file_description, "ROM patches");
    COPY(out->status, *FzeroTracksPatch(p) ? "Selected; exact output verified on Play" : "Not supplied");
    return 1;
}
static int resource_set(void *ctx, const char *package, const char *feature, const char *resource, const char *path) {
    return resources(ctx, package, feature) && resource && !strcmp(resource, "patch") && FzeroTracksSetPatch(find(package), path);
}
static int commit(void *ctx, const char *image) {
    (void)ctx; (void)image;
    return FzeroTracksSave();
}
static const char *error(void *ctx) { (void)ctx; return FzeroTracksError(); }
static int diagnostics(void *ctx) { (void)ctx; return (int)FzeroTracksDiagnosticCount(); }
static int diagnostic(void *ctx, int index, RecompLauncherCModDiagnostic *out) {
    (void)ctx; if (!out || index < 0 || (unsigned)index >= FzeroTracksDiagnosticCount()) return 0;
    memset(out, 0, sizeof(*out)); out->severity = RECOMP_MOD_DIAGNOSTIC_ERROR;
    COPY(out->resource, "Track Library"); COPY(out->message, FzeroTracksDiagnostic((unsigned)index)); return 1;
}
const RecompLauncherCModProvider *FzeroTrackModsProvider(void) {
    static const RecompLauncherCModProvider provider = {
        .package_count=count, .package_get=package_get, .feature_count=count, .feature_get=feature_get,
        .feature_enable=enable,
        .feature_resource_count=resources, .feature_resource_get=resource_get,
        .feature_resource_set_path=resource_set, .commit=commit, .last_error=error,
        .catalog_diagnostic_count=diagnostics, .catalog_diagnostic_get=diagnostic};
    return &provider;
}
