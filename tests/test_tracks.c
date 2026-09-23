#include "fzero_tracks.h"
#include "sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#define CHDIR(p) _chdir(p)
#define RMDIR(p) _rmdir(p)
#else
#include <sys/stat.h>
#include <unistd.h>
#define MKDIR(p) mkdir(p,0755)
#define CHDIR(p) chdir(p)
#define RMDIR(p) rmdir(p)
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
    /* A clean portable install need not already contain mods/track-packs.
     * Remove only these empty test directories; never recurse over a root. */
    RMDIR("test-new-library/mods/track-packs");
    RMDIR("test-new-library/mods");
    RMDIR("test-new-library");
    CHECK(FzeroTracksInit("test-new-library/mods/track-packs",true));
    CHECK(FzeroTracksSave());
    CHECK(!CHDIR("test-new-library/mods/track-packs"));
    CHECK(!CHDIR("../../.."));
    write_file("test-library-file","not a directory",15);
    CHECK(FzeroTracksInit("test-library-file/track-packs",true));
    CHECK(!FzeroTracksSave());
    CHECK(!remove("test-library-file"));
    const char *root="test-course-discovery";MKDIR(root);
    remove("test-course-discovery/duplicate.ini");remove("test-course-discovery/a.disabled");
    remove("test-course-discovery/library.disabled");remove("test-course-discovery/a.path");
    remove("test-course-discovery/b.path");remove("test-course-discovery/b.disabled");
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
    /* Retired master settings cannot hide an individually enabled pack. */
    write_file("test-course-discovery/library.disabled","1\n",2);
    CHECK(FzeroTracksInit(root,true));FzeroTracksDiscover((const uint8_t *)"abc",3);
    b=cp_catalog_find(FzeroTracksCatalog(),"b");CHECK(FzeroTracksAvailable(b));
    CHECK(FzeroTracksEnable(b,false) && FzeroTracksSave());
    CHECK(FzeroTracksInit(root,true));FzeroTracksDiscover((const uint8_t *)"abc",3);
    CHECK(!FzeroTracksAvailable(cp_catalog_find(FzeroTracksCatalog(),"b")));
    remove("test-course-discovery/duplicate.ini");
    /* Bundled patches use the same catalog with an empty user directory. */
    CHECK(!CHDIR(root));MKDIR("assets");MKDIR("assets/track-packs");MKDIR("user");
    remove("user/bundled.disabled");remove("user/bundled.path");remove("user/override.ips");
    remove("assets/track-packs/bundled.hidden");
    write_file("user/library.disabled","1\n",2);
    manifest("assets/track-packs/bundled.ini","bundled");
    remove("assets/track-packs/included.ips");
    write_file("assets/track-packs/bundled.ips","PATCHEOF",8);
    CHECK(FzeroTracksInit("user",true));
    const CpPack *bundled=cp_catalog_find(FzeroTracksCatalog(),"bundled");
    /* The launcher can show the supplied input before selecting a ROM. */
    CHECK(bundled && FzeroTracksAvailable(bundled));
    CHECK(FzeroTracksBundled(bundled));
    CHECK(!FzeroTracksSetPatch(bundled,""));
    CHECK(!strcmp(FzeroTracksPatch(bundled),"assets/track-packs/bundled.ips"));
    FzeroTracksDiscover((const uint8_t *)"abc",3);
    CHECK(!strcmp(FzeroTracksPatch(bundled),"assets/track-packs/bundled.ips"));
    write_file("user/override.ips","PATCHEOF",8);FzeroTracksDiscover((const uint8_t *)"abc",3);
    CHECK(!strcmp(FzeroTracksPatch(bundled),"assets/track-packs/bundled.ips"));
    CHECK(!FzeroTracksSetPatch(bundled,"user/override.ips"));
    write_file("user/bundled.path","user/override.ips\n",18);
    CHECK(FzeroTracksEnable(bundled,false) && FzeroTracksSave());
    CHECK(FzeroTracksInit("user",true));FzeroTracksDiscover((const uint8_t *)"abc",3);
    bundled=cp_catalog_find(FzeroTracksCatalog(),"bundled");
    CHECK(!FzeroTracksAvailable(bundled));
    CHECK(!strcmp(FzeroTracksPatch(bundled),"assets/track-packs/bundled.ips"));
    /* A hidden shipped pack stays present but old enabled settings cannot
     * activate it or expose it as an available cup. */
    write_file("assets/track-packs/bundled.hidden","1\n",2);
    write_file("user/bundled.disabled","0\n",2);
    CHECK(FzeroTracksInit("user",true));FzeroTracksDiscover((const uint8_t *)"abc",3);
    bundled=cp_catalog_find(FzeroTracksCatalog(),"bundled");
    CHECK(bundled && FzeroTracksHidden(bundled) && FzeroTracksBundled(bundled));
    CHECK(!FzeroTracksEnabled(bundled) && !FzeroTracksAvailable(bundled));
    CHECK(!FzeroTracksEnable(bundled,true));
    CHECK(FzeroTracksSave());
    CHECK(!remove("assets/track-packs/bundled.hidden"));
    CHECK(!CHDIR(".."));
    puts("User/bundled discovery, per-pack toggles, obsolete settings and duplicate quarantine passed");return 0;
}
