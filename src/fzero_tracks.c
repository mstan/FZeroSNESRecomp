#include "fzero_tracks.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

static CpCatalog catalog;
static char root_path[CP_PATH], selection[CP_ID * 2], error_text[256];
static char patches[CP_PACKS][CP_PATH];
static bool disabled[CP_PACKS];
static char diagnostics[CP_PACKS][256];
static unsigned diagnostic_count;
static char ambiguous[CP_PACKS][CP_ID];
static unsigned ambiguous_count;
static bool has_deluxe;
const CpCatalog *FzeroTracksCatalog(void) { return &catalog; }
const char *FzeroTracksError(void) { return error_text; }
const char *FzeroTracksSelection(void) { return selection; }
unsigned FzeroTracksDiagnosticCount(void) { return diagnostic_count; }
const char *FzeroTracksDiagnostic(unsigned i) { return i < diagnostic_count ? diagnostics[i] : ""; }
static bool fail(const char *text) { snprintf(error_text, sizeof(error_text), "%s", text); return false; }
static int index_of(const CpPack *p) {
    for (unsigned i = 0; i < catalog.count; ++i) if (catalog.packs[i] == p) return (int)i;
    return -1;
}
const char *FzeroTracksPatch(const CpPack *p) { int i = index_of(p); return i < 0 ? "" : patches[i]; }
static bool builtin(const CpPack *p) { return !strcmp(p->adapter, "retail") || !strcmp(p->adapter, "bs-deluxe"); }
bool FzeroTracksAvailable(const CpPack *p) {
    if (!p) return false;
    if (!strcmp(p->adapter, "retail")) return true;
    if (!strcmp(p->adapter, "bs-deluxe")) return has_deluxe;
    if (!FzeroTracksEnabled(p)) return false;
    const char *path = FzeroTracksPatch(p);
    FILE *f = *path ? fopen(path, "rb") : NULL;
    if (!f) return false;
    fclose(f); return true;
}
bool FzeroTracksEnabled(const CpPack *p) { int i = index_of(p); return i >= 0 && !disabled[i]; }
bool FzeroTracksEnable(const CpPack *p, bool enabled) {
    int i = index_of(p); if (i < 0 || builtin(p)) return false;
    disabled[i] = !enabled; return true;
}
static bool path_for(char *out, size_t cap, const char *id, const char *suffix) {
    return snprintf(out, cap, "%s/%s%s", root_path, id, suffix) < (int)cap;
}
static bool read_line(const char *path, char *out, size_t cap) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    bool ok = fgets(out, (int)cap, f) != NULL;
    if (ok) {
        size_t n = strlen(out);
        if (n == cap-1 && out[n-1] != '\n') ok = false;
        while (n && (out[n-1] == '\n' || out[n-1] == '\r')) out[--n] = 0;
    }
    fclose(f); return ok;
}
static bool write_line(const char *id, const char *suffix, const char *value) {
    char path[CP_PATH], temp[CP_PATH];
    if (!path_for(path, sizeof(path), id, suffix) || snprintf(temp, sizeof(temp), "%s.tmp", path) >= (int)sizeof(temp))
        return fail("Track library path is too long");
    FILE *f = fopen(temp, "wb");
    if (!f) return fail("Cannot write track library settings");
    bool ok = fprintf(f, "%s\n", value) >= 0;
    if (fclose(f)) ok = false;
#ifdef _WIN32
    if (ok) ok = MoveFileExA(temp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    if (ok) ok = rename(temp, path) == 0;
#endif
    return ok ? true : fail("Cannot replace track library settings");
}
static void add_builtin(const char *id, const char *name, const char *adapter,
                         const char *const *cups, unsigned count, const char *const *tracks) {
    CpPack *p = calloc(1, sizeof(*p));
    if (!p) { fail("Out of memory"); return; }
    snprintf(p->id, sizeof(p->id), "%s", id); snprintf(p->name, sizeof(p->name), "%s", name);
    snprintf(p->adapter, sizeof(p->adapter), "%s", adapter);
    snprintf(p->author, sizeof(p->author), "%s", !strcmp(id, "retail") ? "Nintendo" : "GuyPerfect, PowerPanda, Porthor, Catador");
    p->cup_count = count; p->track_count = count * 5;
    static const char *const labels[] = {"Knight League", "Queen League", "King League", "BS-1 League", "BS-2 League"};
    for (unsigned i = 0; i < count; ++i) {
        snprintf(p->cups[i].id, CP_ID, "%s", cups[i]);
        snprintf(p->cups[i].name, CP_NAME, "%s", labels[i]); p->cups[i].slot = i;
        for (unsigned j = 0; j < 5; ++j) {
            CpTrack *t = &p->tracks[i*5+j];
            snprintf(t->id, CP_ID, "%s-%u", cups[i], j+1);
            snprintf(t->name, CP_NAME, "%s", tracks[i*5+j]);
            snprintf(t->cup, CP_ID, "%s", cups[i]); t->slot = j;
        }
    }
    cp_catalog_add(&catalog, p, error_text, sizeof(error_text)); free(p);
}
static void load_manifest(const char *name) {
    size_t n = strlen(name); char path[CP_PATH], error[256] = "Manifest path is too long";
    if (n < 5 || strcmp(name+n-4, ".ini") || strchr(name, '/') || strchr(name, '\\')) return;
    CpPack *p = malloc(sizeof(*p));
    if (!p) { fail("Out of memory"); return; }
    bool ok = path_for(path, sizeof(path), name, "") && cp_manifest_read(path, p, error, sizeof(error));
    if (ok && strcmp(p->adapter, "fzero-max-v1")) {
        snprintf(error, sizeof(error), "Unsupported game adapter: %s", p->adapter); ok = false;
    }
    if (ok) {
        for (unsigned i = 0; i < ambiguous_count; ++i) if (!strcmp(ambiguous[i], p->id)) {
            snprintf(error, sizeof(error), "Ambiguous pack ID: %s", p->id); ok = false;
        }
        const CpPack *existing = cp_catalog_find(&catalog, p->id);
        if (existing && !builtin(existing)) {
            /* Reject both files. Directory enumeration order must never pick
             * a winner for a duplicated persistent identity. */
            if (ambiguous_count < CP_PACKS) strcpy(ambiguous[ambiguous_count++], p->id);
            int index = index_of(existing);
            free(catalog.packs[index]);
            for (unsigned i = (unsigned)index; i+1 < catalog.count; ++i) catalog.packs[i] = catalog.packs[i+1];
            --catalog.count;
            snprintf(error, sizeof(error), "Ambiguous pack ID: %s (all copies excluded)", p->id); ok = false;
        }
    }
    if (ok) ok = cp_catalog_add(&catalog, p, error, sizeof(error));
    if (!ok && diagnostic_count < CP_PACKS)
        snprintf(diagnostics[diagnostic_count++], sizeof(diagnostics[0]), "%.80s: %.160s", name, error);
    free(p);
}
bool FzeroTracksInit(const char *root, bool deluxe_available) {
    cp_catalog_free(&catalog); memset(patches, 0, sizeof(patches));
    memset(disabled, 0, sizeof(disabled));
    selection[0] = error_text[0] = 0; diagnostic_count = ambiguous_count = 0; has_deluxe = deluxe_available;
    if (!root || !*root || strlen(root) >= sizeof(root_path)-CP_ID-16) return fail("Track library path is too long");
    strcpy(root_path, root);
    static const char *const cups[] = {"knight", "queen", "king", "bs-1", "bs-2"};
    static const char *const tracks[] = {
        "Mute City I", "Big Blue", "Sand Ocean", "Death Wind I", "Silence",
        "Mute City II", "Port Town I", "Red Canyon I", "White Land I", "White Land II",
        "Mute City III", "Death Wind II", "Port Town II", "Red Canyon II", "Fire Field",
        "Forest I", "Big Blue II", "Sand Storm I", "Forest II", "Silence II",
        "Mute City IV", "Forest III", "Sand Storm II", "Metal Fort I", "Metal Fort II"};
    add_builtin("retail", "F-Zero", "retail", cups, 3, tracks);
    add_builtin("bs-deluxe", "BS Deluxe", "bs-deluxe", cups, 5, tracks);
#ifdef _WIN32
    char pattern[CP_PATH]; WIN32_FIND_DATAA data;
    path_for(pattern, sizeof(pattern), "*", ".ini");
    HANDLE h = FindFirstFileA(pattern, &data);
    if (h != INVALID_HANDLE_VALUE) {
        do { if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) load_manifest(data.cFileName); } while (FindNextFileA(h, &data));
        FindClose(h);
    }
#else
    DIR *dir = opendir(root_path);
    if (dir) { struct dirent *entry; while ((entry = readdir(dir))) load_manifest(entry->d_name); closedir(dir); }
#endif
    char path[CP_PATH];
    for (unsigned i = 0; i < catalog.count; ++i) {
        path_for(path, sizeof(path), catalog.packs[i]->id, ".path");
        if (!read_line(path, patches[i], sizeof(patches[i]))) patches[i][0] = 0;
        char flag[8] = {0}; path_for(path, sizeof(path), catalog.packs[i]->id, ".disabled");
        if (read_line(path, flag, sizeof(flag))) disabled[i] = !strcmp(flag, "1");
    }
    path_for(path, sizeof(path), "selection", ".txt");
    if (!read_line(path, selection, sizeof(selection))) selection[0] = 0;
    /* A missing persisted key stays missing. Never bind it to another index. */
    const char *override = getenv("FZERO_CUP");
    if (override) {
        if (strlen(override) >= sizeof(selection)) return fail("Cup key is too long");
        strcpy(selection, override);
    }
    return catalog.count >= 2;
}
bool FzeroTracksSetPatch(const CpPack *p, const char *path) {
    int i = index_of(p);
    if (i < 0 || builtin(p) || !path || strlen(path) >= CP_PATH || strchr(path, '\n') || strchr(path, '\r')) return fail("Invalid patch path");
    if (*path) {
        FILE *f = fopen(path, "rb"); uint8_t magic[5] = {0};
        if (!f) return fail("Cannot open the selected patch");
        size_t n = fread(magic, 1, 5, f); fclose(f);
        if (n != 5 || (memcmp(magic, "PATCH", 5) && memcmp(magic, "BPS1", 4))) return fail("Choose an IPS or BPS patch (extract ZIP archives first)");
    }
    strcpy(patches[i], path); error_text[0] = 0; return true;
}
bool FzeroTracksSelect(const char *key) {
    const CpPack *p = NULL;
    if (!key || strlen(key) >= sizeof(selection)) return fail("Invalid cup key");
    if (*key && (!cp_catalog_cup(&catalog, key, &p) || !FzeroTracksAvailable(p))) return fail("Cup unavailable; locate its patch first");
    strcpy(selection, key); error_text[0] = 0; return true;
}
const CpCup *FzeroTracksSelected(const CpPack **p) { return cp_catalog_cup(&catalog, selection, p); }
unsigned FzeroTracksCupCount(void) {
    unsigned count = 0;
    for (unsigned i = 0; i < catalog.count; ++i) if (FzeroTracksAvailable(catalog.packs[i])) count += catalog.packs[i]->cup_count;
    return count;
}
const CpCup *FzeroTracksCupAt(unsigned index, const CpPack **pack) {
    for (unsigned i = 0; i < catalog.count; ++i) {
        const CpPack *p = catalog.packs[i];
        if (!FzeroTracksAvailable(p)) continue;
        if (index < p->cup_count) { if (pack) *pack = p; return &p->cups[index]; }
        index -= p->cup_count;
    }
    return NULL;
}
bool FzeroTracksValidate(const uint8_t *stock, size_t size) {
    if (!*selection) return true;
    const CpPack *p = NULL;
    if (!FzeroTracksSelected(&p) || !FzeroTracksAvailable(p)) return fail("Selected cup is unavailable. Restore its patch or choose another cup in Track Library.");
    if (builtin(p)) return true;
    uint8_t *target = NULL; size_t target_size = 0;
    bool ok = cp_pack_apply(p, stock, size, FzeroTracksPatch(p), &target, &target_size, error_text, sizeof(error_text));
    free(target); return ok;
}
bool FzeroTracksSave(void) {
#ifdef _WIN32
    if (_mkdir(root_path) && errno != EEXIST) return fail("Cannot create track library directory");
#else
    if (mkdir(root_path, 0755) && errno != EEXIST) return fail("Cannot create track library directory");
#endif
    for (unsigned i = 0; i < catalog.count; ++i) if (!builtin(catalog.packs[i])) {
        if (!write_line(catalog.packs[i]->id, ".path", patches[i]) ||
            !write_line(catalog.packs[i]->id, ".disabled", disabled[i] ? "1" : "0")) return false;
    }
    return write_line("selection", ".txt", selection);
}
