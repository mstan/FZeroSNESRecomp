#include "fzero_tracks_mods.h"
#include "fzero_tracks.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define COPY(field, value) snprintf(field, sizeof(field), "%s", value)
static const CpPack *external(unsigned index) {
    const CpCatalog *c = FzeroTracksCatalog();
    for (unsigned i = 0; i < c->count; ++i) if (!strcmp(c->packs[i]->adapter, "fzero-max-v1")) {
        if (!index--) return c->packs[i];
    }
    return NULL;
}
static const CpPack *find(const char *id) {
    const CpPack *p = id ? cp_catalog_find(FzeroTracksCatalog(), id) : NULL;
    return p && !strcmp(p->adapter, "fzero-max-v1") ? p : NULL;
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
        COPY(out->description, "Choose a cup from your combined library. Adding patches expands this list and preserves existing records. The selected cup is highlighted in its game's Grand Prix menu. Return to the launcher to change packs.");
        out->option_count = 1; out->enabled = *FzeroTracksSelection() != 0;
        const CpPack *p = NULL; const CpCup *cup = FzeroTracksSelected(&p);
        if (cup) {
            size_t at = (size_t)snprintf(out->description, sizeof(out->description),
                "%.90s / %.90s\nCourses: ", p->name, cup->name);
            for (unsigned t = 0; t < p->track_count && at < sizeof(out->description)-1; ++t)
                if (!strcmp(p->tracks[t].cup, cup->id)) {
                    int n = snprintf(out->description+at, sizeof(out->description)-at, "%s; ", p->tracks[t].name);
                    if (n > 0) at += (size_t)n;
                }
        }
        if (out->enabled && (!cup || !FzeroTracksAvailable(p))) {
            out->has_error = 1; COPY(out->status, "Selected cup unavailable: restore its patch or select another cup");
        } else COPY(out->status, out->enabled ? "Cup selected" : "Uses the BS Deluxe setting and original menus");
        return 1;
    }
    const CpPack *p = external((unsigned)i-1); if (!p) return 0;
    COPY(out->id, "tracks"); COPY(out->package_id, p->id); COPY(out->name, p->name);
    COPY(out->package_name, p->name); COPY(out->author, p->author);
    snprintf(out->description, sizeof(out->description), "%u cup(s), %u course(s). Supply your own IPS or BPS patch. The patch is checked against your original USA ROM when you play this pack. Records stay separate from other packs.", p->cup_count, p->track_count);
    out->enabled = FzeroTracksEnabled(p);
    COPY(out->status, FzeroTracksAvailable(p) ? "Patch supplied; cup added to Track Library" : "Locate a patch to add these cups");
    return 1;
}
static int package_get(void *ctx, int i, RecompLauncherCModPackage *out) {
    RecompLauncherCModFeature f;
    if (!out || !feature_get(ctx, i, &f)) return 0;
    memset(out, 0, sizeof(*out)); COPY(out->id, f.package_id); COPY(out->name, f.name);
    COPY(out->author, f.author); COPY(out->description, f.description); COPY(out->version, "1"); out->enabled = f.enabled; return 1;
}
static int option_get(void *ctx, const char *package, const char *feature, int i, RecompLauncherCModOption *out) {
    (void)ctx; if (!library(package, feature) || i || !out) return 0;
    memset(out, 0, sizeof(*out)); COPY(out->id, "cup"); COPY(out->label, "Cup");
    COPY(out->description, "Original and installed cups. Each pack retains its own records and rules.");
    COPY(out->value, FzeroTracksSelection()); out->type = RECOMP_MOD_OPTION_CHOICE;
    out->choice_count = (int)FzeroTracksCupCount()+1; return 1;
}
static int choice_get(void *ctx, const char *package, const char *feature, const char *option, int i, RecompLauncherCModChoice *out) {
    (void)ctx; if (!library(package, feature) || !option || strcmp(option, "cup") || i < 0 || !out) return 0;
    memset(out, 0, sizeof(*out));
    if (!i) { COPY(out->label, "Use original menus / BS Deluxe setting"); return 1; }
    const CpPack *p = NULL; const CpCup *c = FzeroTracksCupAt((unsigned)i-1, &p);
    if (!c) return 0;
    snprintf(out->value, sizeof(out->value), "%s/%s", p->id, c->id);
    if (p->cup_count == 1) COPY(out->label, p->name);
    else snprintf(out->label, sizeof(out->label), "%.70s / %.50s", p->name, c->name);
    return 1;
}
static int set_option(void *ctx, const char *package, const char *feature, const char *option, const char *value) {
    (void)ctx; return library(package, feature) && option && !strcmp(option, "cup") && FzeroTracksSelect(value);
}
static int enable(void *ctx, const char *package, const char *feature, int enabled) {
    (void)ctx;
    if (library(package, feature)) return FzeroTracksSelect(enabled ? (*FzeroTracksSelection() ? FzeroTracksSelection() : "retail/knight") : "");
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
    COPY(out->description, "Extract the patch from its ZIP first. Only the selected cup's pack is applied to a fresh copy of your original ROM.");
    COPY(out->file_patterns, "*.ips,*.bps"); COPY(out->file_description, "ROM patches");
    COPY(out->status, *FzeroTracksPatch(p) ? "Selected; exact output verified on Play" : "Not supplied");
    return 1;
}
static int resource_set(void *ctx, const char *package, const char *feature, const char *resource, const char *path) {
    return resources(ctx, package, feature) && resource && !strcmp(resource, "patch") && FzeroTracksSetPatch(find(package), path);
}
static int commit(void *ctx, const char *image) {
    (void)ctx;
    if (*FzeroTracksSelection()) {
        FILE *f = image ? fopen(image, "rb") : NULL; if (!f) return 0;
        uint8_t *rom = malloc(0x80200); if (!rom) { fclose(f); return 0; }
        size_t n = fread(rom, 1, 0x80200, f); int extra = fgetc(f); fclose(f);
        size_t skip = n == 0x80200 ? 512 : 0;
        bool ok = extra == EOF && (n == 0x80000 || n == 0x80200) && FzeroTracksValidate(rom+skip, n-skip);
        free(rom); if (!ok) return 0;
    }
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
        .feature_option_get=option_get, .feature_choice_get=choice_get, .feature_enable=enable,
        .feature_set_option=set_option, .feature_resource_count=resources, .feature_resource_get=resource_get,
        .feature_resource_set_path=resource_set, .commit=commit, .last_error=error,
        .catalog_diagnostic_count=diagnostics, .catalog_diagnostic_get=diagnostic};
    return &provider;
}
