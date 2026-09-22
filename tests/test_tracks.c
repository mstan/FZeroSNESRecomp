#include "fzero_tracks.h"
#include "sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#define MKDIR(p) mkdir(p,0755)
#endif
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%d: %s (%s)\n",__LINE__,#x,FzeroTracksError());exit(1); } } while (0)
static void write_file(const char *path,const void *bytes,size_t n) {
    FILE *f=fopen(path,"wb");CHECK(f);CHECK(fwrite(bytes,1,n,f)==n);CHECK(!fclose(f));
}
static void manifest(const char *path,const char *id) {
    uint8_t hash[32];char hex[65];sha256_compute((const uint8_t *)"abc",3,hash);cp_hash_format(hash,hex);
    FILE *f=fopen(path,"wb");CHECK(f);
    fprintf(f,"format=1\nid=%s\nname=Single Course\nauthor=Test\nadapter=fzero-course-v1\n"
        "source_sha256=%s\ntarget_sha256=%s\ncup=solo|Solo Cup|0\ntrack=one|One|solo|0\n",id,hex,hex);
    CHECK(!fclose(f));
}
int main(void) {
    const char *root="test-course-discovery";MKDIR(root);
    remove("test-course-discovery/duplicate.ini");remove("test-course-discovery/a.disabled");
    remove("test-course-discovery/library.disabled");remove("test-course-discovery/a.path");
    remove("test-course-discovery/b.path");
    manifest("test-course-discovery/a.ini","a");manifest("test-course-discovery/b.ini","b");
    write_file("test-course-discovery/arbitrary-name.IPS","PATCHEOF",8);
    write_file("test-course-discovery/broken.bps","BPS1bad!",8);
    CHECK(FzeroTracksInit(root,true));
    FzeroTracksDiscover((const uint8_t *)"abc",3);
    const CpPack *a=cp_catalog_find(FzeroTracksCatalog(),"a"),*b=cp_catalog_find(FzeroTracksCatalog(),"b");
    CHECK(a && b && FzeroTracksAvailable(a) && FzeroTracksAvailable(b));
    CHECK(FzeroTracksDiagnosticCount());
    CHECK(FzeroTracksEnable(a,false) && FzeroTracksSave());
    CHECK(FzeroTracksInit(root,true));FzeroTracksDiscover((const uint8_t *)"abc",3);
    a=cp_catalog_find(FzeroTracksCatalog(),"a");b=cp_catalog_find(FzeroTracksCatalog(),"b");
    CHECK(!FzeroTracksEnabled(a) && FzeroTracksAvailable(b));
    CHECK(!remove("test-course-discovery/arbitrary-name.IPS"));CHECK(!FzeroTracksAvailable(b));
    write_file("test-course-discovery/arbitrary-name.IPS","PATCHEOF",8);CHECK(FzeroTracksAvailable(b));
    manifest("test-course-discovery/duplicate.ini","a");CHECK(FzeroTracksInit(root,false));
    CHECK(!cp_catalog_find(FzeroTracksCatalog(),"a"));CHECK(cp_catalog_find(FzeroTracksCatalog(),"b"));
    CHECK(FzeroTracksAvailable(cp_catalog_find(FzeroTracksCatalog(),"retail")));
    CHECK(!FzeroTracksAvailable(cp_catalog_find(FzeroTracksCatalog(),"bs-deluxe")));
    FzeroTracksLibraryEnable(false);CHECK(FzeroTracksSave());
    CHECK(FzeroTracksInit(root,true) && !FzeroTracksLibraryEnabled());
    remove("test-course-discovery/duplicate.ini");
    puts("Folder discovery, missing inputs, independent toggles and duplicate quarantine passed");return 0;
}
