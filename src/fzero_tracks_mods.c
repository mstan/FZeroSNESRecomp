#include "fzero_tracks_mods.h"
#include "fzero_tracks.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define COPY(field, value) snprintf(field, sizeof(field), "%s", value)
static const CpPack *external(unsigned index) {
    const CpCatalog *c = FzeroTracksCatalog();
    for (unsigned i = 0; i < c->count; ++i) if (!strcmp(c->packs[i]->adapter, "fzero-course-v1") && !FzeroTracksHidden(c->packs[i])) {
        if (!index--) return c->packs[i];
    }
    return NULL;
}
static const CpPack *find(const char *id) {
    const CpPack *p = id ? cp_catalog_find(FzeroTracksCatalog(), id) : NULL;
    return p && !strcmp(p->adapter, "fzero-course-v1") && !FzeroTracksHidden(p) ? p : NULL;
}
static int count(void *ctx) { (void)ctx; unsigned n = 0; while (external(n)) ++n; return (int)n; }
static int feature_get(void *ctx, int i, RecompLauncherCModFeature *out) {
    (void)ctx; if (!out || i < 0) return 0;
    memset(out, 0, sizeof(*out));
    COPY(out->group, "Track Packs"); COPY(out->package_version, "1");
    const CpPack *p = external((unsigned)i); if (!p) return 0;
    COPY(out->id, "tracks"); COPY(out->package_id, p->id); COPY(out->name, p->name);
    COPY(out->package_name, p->name); COPY(out->author, p->author);
    snprintf(out->description, sizeof(out->description), "%u cup(s), %u course(s). Enable this pack to add its cups to the in-game Grand Prix league list.", p->cup_count, p->track_count);
    if (!strcmp(p->id,"cgp"))
        COPY(out->description,"30 new courses plus 10 corrected BS courses in 8 cups. Enabling this turns off the original BS Satellaview track pack. Vehicles and gameplay rules are separate mods.");
    out->enabled = FzeroTracksEnabled(p);
    out->option_count = FzeroTracksHasTitle(p) ? 1 : 0;
    COPY(out->status, !out->enabled ? "Disabled" : FzeroTracksBundled(p) ? "Enabled" : FzeroTracksAvailable(p) ? "Enabled; patch checked on Play" : "Supply the patch to add these cups");
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
    const CpPack *p = find(package);
    return p && feature && !strcmp(feature, "tracks") && FzeroTracksEnable(p, enabled != 0);
}
static bool title_option(const char *package, const char *feature) {
    return feature && !strcmp(feature, "tracks") && FzeroTracksHasTitle(find(package));
}
static int option_get(void *ctx, const char *package, const char *feature, int index,
                      RecompLauncherCModOption *out) {
    (void)ctx;
    if (!out || index || !title_option(package, feature)) return 0;
    memset(out, 0, sizeof(*out));
    COPY(out->id, "title-screen"); COPY(out->label, "Title screen");
    COPY(out->description, "Use the F-Zero 55 title artwork while Community Grand Prix is enabled.");
    out->type = RECOMP_MOD_OPTION_CHOICE; out->choice_count = 2; out->step = 1;
    COPY(out->default_value, "original");
    COPY(out->value, FzeroTracksTitleEnabled(find(package)) ? "fzero-55" : "original");
    return 1;
}
static int choice_get(void *ctx, const char *package, const char *feature, const char *option,
                      int index, RecompLauncherCModChoice *out) {
    (void)ctx;
    if (!out || !option || strcmp(option, "title-screen") || index < 0 || index > 1 ||
        !title_option(package, feature)) return 0;
    memset(out, 0, sizeof(*out));
    COPY(out->value, index ? "fzero-55" : "original");
    COPY(out->label, index ? "F-Zero 55" : "Original");
    return 1;
}
static int set_option(void *ctx, const char *package, const char *feature, const char *option,
                      const char *value) {
    (void)ctx;
    if (!title_option(package, feature) || !option || strcmp(option, "title-screen") ||
        !value || (strcmp(value, "original") && strcmp(value, "fzero-55"))) return 0;
    return FzeroTracksSetTitle(find(package), !strcmp(value, "fzero-55"));
}
static int resources(void *ctx, const char *package, const char *feature) {
    (void)ctx; const CpPack *p=find(package);
    return p && !FzeroTracksBundled(p) && feature && !strcmp(feature, "tracks");
}
static int resource_get(void *ctx, const char *package, const char *feature, int i, RecompLauncherCModResource *out) {
    if (!out || i || !resources(ctx, package, feature)) return 0;
    const CpPack *p = find(package); memset(out, 0, sizeof(*out));
    COPY(out->id, "patch"); COPY(out->label, "IPS or BPS patch"); COPY(out->path, FzeroTracksPatch(p));
    COPY(out->description, "Extract the IPS or BPS from its ZIP and select it here.");
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
    COPY(out->resource, "Track Packs"); COPY(out->message, FzeroTracksDiagnostic((unsigned)index)); return 1;
}
const RecompLauncherCModProvider *FzeroTrackModsProvider(void) {
    static const RecompLauncherCModProvider provider = {
        .package_count=count, .package_get=package_get, .feature_count=count, .feature_get=feature_get,
        .feature_enable=enable,
        .feature_option_get=option_get, .feature_choice_get=choice_get, .feature_set_option=set_option,
        .feature_resource_count=resources, .feature_resource_get=resource_get,
        .feature_resource_set_path=resource_set, .commit=commit, .last_error=error,
        .catalog_diagnostic_count=diagnostics, .catalog_diagnostic_get=diagnostic};
    return &provider;
}
