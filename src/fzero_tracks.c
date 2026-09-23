#include "fzero_tracks.h"
#include "sha256.h"
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
static char root_path[CP_PATH], error_text[256];
static char patches[CP_PACKS][CP_PATH];
static bool disabled[CP_PACKS];
static bool bundled[CP_PACKS];
static bool hidden[CP_PACKS];
static bool custom_title[CP_PACKS];
static char diagnostics[CP_PACKS][256];
static unsigned diagnostic_count;
static char ambiguous[CP_PACKS][CP_ID];
static unsigned ambiguous_count;
static bool has_deluxe;
const CpCatalog *FzeroTracksCatalog(void) { return &catalog; }
const char *FzeroTracksError(void) { return error_text; }
const char *FzeroTracksRoot(void) { return root_path; }
void FzeroTracksReport(const char *message) {
    if (diagnostic_count < CP_PACKS) snprintf(diagnostics[diagnostic_count++],256,"%s",message);
    fprintf(stderr,"[track-library] %s\n",message);
}
unsigned FzeroTracksDiagnosticCount(void) { return diagnostic_count; }
const char *FzeroTracksDiagnostic(unsigned i) { return i < diagnostic_count ? diagnostics[i] : ""; }
static bool fail(const char *text) { snprintf(error_text, sizeof(error_text), "%s", text); return false; }
static int index_of(const CpPack *p) {
    for (unsigned i = 0; i < catalog.count; ++i) if (catalog.packs[i] == p) return (int)i;
    return -1;
}
const char *FzeroTracksPatch(const CpPack *p) { int i = index_of(p); return i < 0 ? "" : patches[i]; }
bool FzeroTracksBundled(const CpPack *p) { int i = index_of(p); return i >= 0 && bundled[i]; }
bool FzeroTracksHidden(const CpPack *p) { int i = index_of(p); return i >= 0 && hidden[i]; }
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
bool FzeroTracksEnabled(const CpPack *p) { int i = index_of(p); return i >= 0 && !hidden[i] && !disabled[i]; }
bool FzeroTracksEnable(const CpPack *p, bool enabled) {
    int i = index_of(p); if (i < 0 || builtin(p) || hidden[i]) return false;
    disabled[i] = !enabled; return true;
}
bool FzeroTracksHasTitle(const CpPack *p) {
    return p && !strcmp(p->id, "cgp") && !strcmp(p->adapter, "fzero-course-v1");
}
bool FzeroTracksTitleEnabled(const CpPack *p) {
    int i = index_of(p);
    return i >= 0 && FzeroTracksHasTitle(p) && custom_title[i];
}
bool FzeroTracksSetTitle(const CpPack *p, bool enabled) {
    int i = index_of(p);
    if (i < 0 || !FzeroTracksHasTitle(p)) return false;
    custom_title[i] = enabled;
    return true;
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
static void load_manifest_at(const char *directory, const char *name, bool fallback) {
    size_t n = strlen(name); char path[CP_PATH], error[256] = "Manifest path is too long";
    if (n < 5 || strcmp(name+n-4, ".ini") || strchr(name, '/') || strchr(name, '\\')) return;
    CpPack *p = malloc(sizeof(*p));
    if (!p) { fail("Out of memory"); return; }
    bool ok = snprintf(path,sizeof(path),"%s/%s",directory,name)<(int)sizeof(path) && cp_manifest_read(path, p, error, sizeof(error));
    if (ok && strcmp(p->adapter, "fzero-course-v1")) {
        snprintf(error, sizeof(error), "Unsupported game adapter: %s", p->adapter); ok = false;
    }
    if (ok && fallback) {
        bool conflict=cp_catalog_find(&catalog,p->id)!=NULL;
        for(unsigned i=0;i<ambiguous_count;++i)conflict|=!strcmp(ambiguous[i],p->id);
        if(conflict){free(p);return;}
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
static void scan_manifests(const char *directory,bool fallback) {
#ifdef _WIN32
    char pattern[CP_PATH];WIN32_FIND_DATAA data;
    if(snprintf(pattern,sizeof(pattern),"%s/*.ini",directory)>=(int)sizeof(pattern))return;
    HANDLE h=FindFirstFileA(pattern,&data);
    if(h!=INVALID_HANDLE_VALUE) {
        do {if(!(data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY))load_manifest_at(directory,data.cFileName,fallback);}while(FindNextFileA(h,&data));
        FindClose(h);
    }
#else
    DIR *dir=opendir(directory);
    if(dir){struct dirent *entry;while((entry=readdir(dir)))load_manifest_at(directory,entry->d_name,fallback);closedir(dir);}
#endif
}
static void select_companion_patch(const CpPack *pack, const char *directory) {
    static const char *const suffixes[] = {".ips", ".bps"};
    char path[CP_PATH];
    for (unsigned i=0;i<sizeof(suffixes)/sizeof(suffixes[0]);++i) {
        int n=snprintf(path,sizeof(path),"%s/%s%s",directory,pack->id,suffixes[i]);
        if(n<0||n>=(int)sizeof(path))continue;
        FILE *file=fopen(path,"rb");
        if(file){fclose(file);strcpy(patches[index_of(pack)],path);return;}
    }
}
bool FzeroTracksInit(const char *root, bool deluxe_available) {
    cp_catalog_free(&catalog); memset(patches, 0, sizeof(patches));
    memset(disabled, 0, sizeof(disabled));
    memset(bundled, 0, sizeof(bundled));
    memset(hidden, 0, sizeof(hidden));
    memset(custom_title, 0, sizeof(custom_title));
    error_text[0] = 0; diagnostic_count = ambiguous_count = 0; has_deluxe = deluxe_available;
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
    scan_manifests(root_path,false);
    /* Registry defaults are read-only. Local identities override defaults;
     * ambiguous local IDs remain quarantined rather than falling back. */
    scan_manifests("assets/track-packs",true);
    char path[CP_PATH];
    for (unsigned i = 0; i < catalog.count; ++i) {
        /* Included packs have a fixed input; old picker settings cannot replace
         * it. Exact source/target verification still happens on Play. */
        if (!builtin(catalog.packs[i])) {
            select_companion_patch(catalog.packs[i],"assets/track-packs");
            bundled[i] = patches[i][0] != 0;
            if (!bundled[i]) {
                path_for(path, sizeof(path), catalog.packs[i]->id, ".path");
                if (!read_line(path, patches[i], sizeof(patches[i]))) patches[i][0] = 0;
                if (!patches[i][0])select_companion_patch(catalog.packs[i],root_path);
            }
        }
        char flag[8] = {0}; path_for(path, sizeof(path), catalog.packs[i]->id, ".disabled");
        if (read_line(path, flag, sizeof(flag))) disabled[i] = !strcmp(flag, "1");
        path_for(path, sizeof(path), catalog.packs[i]->id, ".title");
        if (FzeroTracksHasTitle(catalog.packs[i]) && read_line(path, flag, sizeof(flag)))
            custom_title[i] = !strcmp(flag, "1");
        snprintf(path,sizeof(path),"assets/track-packs/%s.hidden",catalog.packs[i]->id);
        if (read_line(path,flag,sizeof(flag)) && !strcmp(flag,"1")) {
            hidden[i] = true;
            disabled[i] = true;
        }
    }
    return catalog.count >= 2;
}
bool FzeroTracksSetPatch(const CpPack *p, const char *path) {
    int i = index_of(p);
    if (i < 0 || builtin(p) || !path || strlen(path) >= CP_PATH || strchr(path, '\n') || strchr(path, '\r')) return fail("Invalid patch path");
    if (bundled[i]) return fail("Bundled packs use their included patch");
    if (*path) {
        FILE *f = fopen(path, "rb"); uint8_t magic[5] = {0};
        if (!f) return fail("Cannot open the selected patch");
        size_t n = fread(magic, 1, 5, f); fclose(f);
        if (n != 5 || (memcmp(magic, "PATCH", 5) && memcmp(magic, "BPS1", 4))) return fail("Choose an IPS or BPS patch (extract ZIP archives first)");
    }
    strcpy(patches[i], path); error_text[0] = 0; return true;
}
bool FzeroTracksSave(void) {
#ifdef _WIN32
    if (_mkdir(root_path) && errno != EEXIST) return fail("Cannot create track library directory");
#else
    if (mkdir(root_path, 0755) && errno != EEXIST) return fail("Cannot create track library directory");
#endif
    for (unsigned i = 0; i < catalog.count; ++i) if (!builtin(catalog.packs[i])) {
        if ((!bundled[i] && !write_line(catalog.packs[i]->id, ".path", patches[i])) ||
            !write_line(catalog.packs[i]->id, ".disabled", disabled[i] ? "1" : "0")) return false;
        if (FzeroTracksHasTitle(catalog.packs[i]) &&
            !write_line(catalog.packs[i]->id, ".title", custom_title[i] ? "1" : "0")) return false;
    }
    return true;
}

enum { MAX_PATCH_FILES=256 };
static char patch_files[MAX_PATCH_FILES][CP_PATH];
static unsigned patch_file_count;
static void candidate(const char *directory, const char *name) {
    const char *ext=strrchr(name,'.');if(!ext)return;
    char suffix[8]={0};if(strlen(ext)>=sizeof(suffix))return;
    for(unsigned i=0;ext[i];++i)suffix[i]=ext[i]>='A'&&ext[i]<='Z'?ext[i]+32:ext[i];
    if(strcmp(suffix,".ips")&&strcmp(suffix,".bps")&&strcmp(suffix,".patch"))return;
    if(patch_file_count>=MAX_PATCH_FILES){FzeroTracksReport("Patch directory limit reached (256 files)");return;}
    if(snprintf(patch_files[patch_file_count],CP_PATH,"%s/%s",directory,name)<CP_PATH)++patch_file_count;
}
static int compare_paths(const void *a,const void *b) { return strcmp(a,b); }
static void scan_patches(const char *directory) {
    unsigned first=patch_file_count;
#ifdef _WIN32
    char pattern[CP_PATH];WIN32_FIND_DATAA data;
    if(snprintf(pattern,sizeof(pattern),"%s/*",directory)>=(int)sizeof(pattern))return;
    HANDLE h=FindFirstFileA(pattern,&data);
    if(h!=INVALID_HANDLE_VALUE){do{if(!(data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY))candidate(directory,data.cFileName);}while(FindNextFileA(h,&data));FindClose(h);}
#else
    DIR *dir=opendir(directory);if(dir){struct dirent *entry;while((entry=readdir(dir)))candidate(directory,entry->d_name);closedir(dir);}
#endif
    qsort(patch_files+first,patch_file_count-first,sizeof(patch_files[0]),compare_paths);
}
void FzeroTracksDiscover(const uint8_t *stock,size_t size) {
    patch_file_count=0;
    scan_patches(root_path);
    /* Shipped inputs use the same verified manifest path as user packs, but
     * retain their fixed patch. Only per-pack settings control activation. */
    if(strcmp(root_path,"assets/track-packs"))scan_patches("assets/track-packs");
    uint8_t source_hash[32];sha256_compute(stock,size,source_hash);
    bool found[CP_PACKS];memcpy(found,bundled,sizeof(found));
    for(unsigned f=0;f<patch_file_count;++f){
        FILE *file=fopen(patch_files[f],"rb");if(!file)continue;
        bool ok=!fseek(file,0,SEEK_END);long length=ok?ftell(file):-1;
        ok=ok&&length>=8&&length<=CP_ROM_LIMIT*2&&!fseek(file,0,SEEK_SET);
        uint8_t *patch=ok?malloc((size_t)length):NULL,*target=NULL;size_t target_size=0;
        char error[256]="Patch exceeds input limit or cannot be read";
        ok=patch&&fread(patch,1,(size_t)length,file)==(size_t)length;fclose(file);
        if(ok)ok=cp_patch_apply(stock,size,patch,(size_t)length,&target,&target_size,error,sizeof(error));
        free(patch);
        if(!ok){char msg[256];snprintf(msg,sizeof(msg),"%.100s: %.140s",patch_files[f],error);FzeroTracksReport(msg);continue;}
        uint8_t hash[32];sha256_compute(target,target_size,hash);free(target);bool matched=false;
        for(unsigned i=0;i<catalog.count;++i){const CpPack *p=catalog.packs[i];
            if(builtin(p)||memcmp(source_hash,p->source_hash,32))continue;
            bool same=!memcmp(hash,p->target_hash,32);
            for(unsigned j=0;j<p->alternate_target_count;++j)same|=!memcmp(hash,p->alternate_target_hash[j],32);
            if(same){matched=true;if(!found[i]){strcpy(patches[i],patch_files[f]);found[i]=true;}}
        }
        if(!matched){char msg[256];snprintf(msg,sizeof(msg),"%.150s: no matching course manifest; see PARSE_MANIFEST.md",patch_files[f]);FzeroTracksReport(msg);}
    }
}
