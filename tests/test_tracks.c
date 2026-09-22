#include "fzero_tracks.h"
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
static void copy(const char *src, const char *dst) {
    FILE *a=fopen(src,"rb"),*b=fopen(dst,"wb");CHECK(a && b);
    int c;while ((c=fgetc(a))!=EOF) CHECK(fputc(c,b)!=EOF);fclose(a);CHECK(!fclose(b));
}
int main(int argc,char **argv) {
    CHECK(argc==2);MKDIR("test-tracks");char from[1024];
    snprintf(from,sizeof(from),"%s/max-league-classic.ini",argv[1]);copy(from,"test-tracks/classic.ini");
    snprintf(from,sizeof(from),"%s/max-league-modern.ini",argv[1]);copy(from,"test-tracks/modern.ini");
    remove("test-tracks/selection.txt");remove("test-tracks/max-league-classic.path");remove("test-tracks/max-league-modern.path");
    remove("test-tracks/max-league-classic.disabled");remove("test-tracks/max-league-modern.disabled");
    CHECK(FzeroTracksInit("test-tracks",true));CHECK(FzeroTracksCupCount()==8);
    const CpPack *classic=cp_catalog_find(FzeroTracksCatalog(),"max-league-classic");
    const CpPack *modern=cp_catalog_find(FzeroTracksCatalog(),"max-league-modern");CHECK(classic && modern);
    CHECK(!FzeroTracksSelect("max-league-classic/max"));
    FILE *f=fopen("test-tracks/source.ips","wb");CHECK(f);fwrite("PATCHEOF",1,8,f);fclose(f);
    CHECK(FzeroTracksSetPatch(classic,"test-tracks/source.ips"));CHECK(FzeroTracksCupCount()==9);
    CHECK(FzeroTracksSelect("max-league-classic/max"));
    CHECK(!FzeroTracksValidate((const uint8_t*)"wrong ROM",9));
    CHECK(FzeroTracksSetPatch(modern,"test-tracks/source.ips"));CHECK(FzeroTracksCupCount()==10);
    CHECK(!strcmp(FzeroTracksSelection(),"max-league-classic/max"));
    CHECK(FzeroTracksEnable(classic,false));CHECK(FzeroTracksCupCount()==9);
    CHECK(!strcmp(FzeroTracksSelection(),"max-league-classic/max") && !FzeroTracksValidate((const uint8_t*)"wrong ROM",9));
    CHECK(FzeroTracksEnable(classic,true));CHECK(FzeroTracksSave());
    CHECK(!remove("test-tracks/classic.ini"));CHECK(FzeroTracksInit("test-tracks",true));
    CHECK(FzeroTracksCupCount()==9 && !FzeroTracksSelected(NULL));
    CHECK(!strcmp(FzeroTracksSelection(),"max-league-classic/max"));
    CHECK(FzeroTracksSelect("retail/king"));CHECK(FzeroTracksSave());
    snprintf(from,sizeof(from),"%s/max-league-classic.ini",argv[1]);copy(from,"test-tracks/classic.ini");
    CHECK(FzeroTracksInit("test-tracks",false));CHECK(FzeroTracksCupCount()==5);
    classic=cp_catalog_find(FzeroTracksCatalog(),"max-league-classic");
    CHECK(FzeroTracksAvailable(classic)); /* Removing another pack did not erase its path. */
    CHECK(!strcmp(FzeroTracksSelection(),"retail/king"));
    CHECK(!remove("test-tracks/source.ips"));CHECK(FzeroTracksCupCount()==3);
    CHECK(FzeroTracksSelect("retail/knight"));
    copy("test-tracks/classic.ini","test-tracks/duplicate.ini");
    CHECK(FzeroTracksInit("test-tracks",true));
    CHECK(!cp_catalog_find(FzeroTracksCatalog(),"max-league-classic") && FzeroTracksDiagnosticCount());
    CHECK(cp_catalog_find(FzeroTracksCatalog(),"max-league-modern"));
    CHECK(!remove("test-tracks/duplicate.ini"));
    puts("Absent, partial, disabled, removed and restored pack combinations passed");return 0;
}
