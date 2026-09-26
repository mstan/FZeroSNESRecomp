#include "fzero_course_file.h"
#include "fzero_packs.h"
#include "fzero_tracks.h"
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
#define MKDIR(p) mkdir(p, 0755)
#define CHDIR(p) chdir(p)
#define RMDIR(p) rmdir(p)
#endif
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      fprintf(stderr, "%d: %s (%s)\n", __LINE__, #x, FzeroTracksError());      \
      exit(1);                                                                 \
    }                                                                          \
  } while (0)
static void write_file(const char *path, const void *bytes, size_t n) {
  FILE *f = fopen(path, "wb");
  CHECK(f);
  CHECK(fwrite(bytes, 1, n, f) == n);
  CHECK(!fclose(f));
}
static void set_packs(const char *path) {
#ifdef _WIN32
  _putenv_s("FZERO_PACKS_DIR", path);
#else
  setenv("FZERO_PACKS_DIR", path, 1);
#endif
}
int main(void) {
  const char *root = "test-pack-discovery";
  MKDIR(root);
  MKDIR("test-pack-discovery/a");
  MKDIR("test-pack-discovery/b");
  remove("test-pack-discovery/settings/loader.cfg");
  remove("test-pack-discovery/b/pack.json");
  set_packs(root);
  FzeroCourse c = {0};
  c.block_size = 544;
  c.grid_size = 36;
  c.shortcuts[1] = 0x80; /* Empty, terminated native shortcut table. */
  for (unsigned i = 0; i < 16; ++i)
    c.blocks[512 + i * 2 + 1] = 0x70;
  char error[256];
  CHECK(FzeroCourseFileWrite("test-pack-discovery/a/course.fzc", &c, error,
                             sizeof(error)));
  const char *manifest =
      "{\"format\":1,\"id\":\"one\",\"name\":\"One\",\"author\":\"Test\","
      "\"cups\":[{\"id\":\"solo\",\"name\":\"Solo\",\"courses\":[\"course\"]}],"
      "\"courses\":[{\"id\":\"course\",\"name\":\"Course\",\"source\":\"course."
      "fzc\"}]}";
  write_file("test-pack-discovery/a/pack.json", manifest, strlen(manifest));
  CHECK(FzeroTracksInit("test-pack-discovery/settings", true));
  const CpPack *one = cp_catalog_find(FzeroTracksCatalog(), "one");
  CHECK(one);
  CHECK(FzeroTrackLoaderEnabled() && FzeroTracksAvailable(one));
  FzeroTrackLoaderEnable(false);
  CHECK(!FzeroTracksAvailable(one));
  CHECK(FzeroTracksSave());
  CHECK(FzeroTracksInit("test-pack-discovery/settings", true));
  one = cp_catalog_find(FzeroTracksCatalog(), "one");
  CHECK(!FzeroTrackLoaderEnabled() && !FzeroTracksAvailable(one));
  FzeroTrackLoaderEnable(true);
  CHECK(FzeroTracksAvailable(one));
  CHECK(FzeroTracksSave());
  CHECK(FzeroTracksInit("test-pack-discovery/settings", true));
  CHECK(FzeroTrackLoaderEnabled());
  FzeroCourse copy;
  CHECK(FzeroPacksLoadCourse("one", 0, &copy, error, sizeof(error)));
  CHECK(FzeroCourseValidate(&copy, error, sizeof(error)));
  /* A pack-wide module and a course-only module compose without making an
   * undeclared course in another pack inherit those terrain semantics. */
  const char *scoped =
    "{\"format\":1,\"id\":\"scoped\",\"name\":\"Scoped\",\"author\":\"Test\","
    "\"mechanics\":[\"grip.json\"],\"cups\":[{\"id\":\"cup\",\"name\":\"Cup\",\"courses\":[\"a\",\"b\"]}],"
    "\"courses\":[{\"id\":\"a\",\"name\":\"A\",\"source\":\"course.fzc\",\"mechanics\":[\"rainbow.json\"]},"
    "{\"id\":\"b\",\"name\":\"B\",\"source\":\"course.fzc\"}]}";
  const char *grip = "{\"format\":1,\"id\":\"grip\",\"engine\":\"fzero-course-v1\",\"requires\":[\"grip-magnets\",\"up-magnets\"]}";
  const char *rainbow = "{\"format\":1,\"id\":\"rainbow\",\"engine\":\"fzero-course-v1\",\"requires\":[\"rainbow-road\"]}";
  write_file("test-pack-discovery/b/grip.json",grip,strlen(grip));
  write_file("test-pack-discovery/b/rainbow.json",rainbow,strlen(rainbow));
  CHECK(FzeroCourseFileWrite("test-pack-discovery/b/course.fzc", &c,error,sizeof(error)));
  write_file("test-pack-discovery/b/pack.json",scoped,strlen(scoped));
  CHECK(FzeroTracksInit("test-pack-discovery/settings",true));
  CHECK(FzeroPacksLoadCourse("scoped",0,&copy,error,sizeof(error)) && copy.required==7);
  CHECK(FzeroPacksLoadCourse("scoped",1,&copy,error,sizeof(error)) && copy.required==3);
  CHECK(FzeroPacksLoadCourse("one",0,&copy,error,sizeof(error)) && copy.required==0);
  const char *bad_module="{\"format\":2,\"id\":\"grip\",\"engine\":\"fzero-course-v1\",\"requires\":[]}";
  write_file("test-pack-discovery/b/grip.json",bad_module,strlen(bad_module));
  CHECK(FzeroTracksInit("test-pack-discovery/settings",true));
  CHECK(!cp_catalog_find(FzeroTracksCatalog(),"scoped") && FzeroTracksDiagnosticCount());
  CHECK(FzeroCourseFileWrite("test-pack-discovery/b/course.fzc", &c, error,
                             sizeof(error)));
  write_file("test-pack-discovery/b/pack.json", manifest, strlen(manifest));
  CHECK(FzeroTracksInit("test-pack-discovery/settings", false));
  CHECK(!cp_catalog_find(FzeroTracksCatalog(), "one"));
  CHECK(FzeroTracksDiagnosticCount() == 2);
  CHECK(FzeroTracksAvailable(cp_catalog_find(FzeroTracksCatalog(), "retail")));
  CHECK(!FzeroTracksAvailable(
      cp_catalog_find(FzeroTracksCatalog(), "bs-deluxe")));
  CHECK(!remove("test-pack-discovery/b/pack.json"));
  const char *invalid = "{\"format\":1,\"id\":\"bad\",\"name\":\"Bad\","
                        "\"author\":\"Test\",\"courses\":[]}";
  write_file("test-pack-discovery/b/pack.json", invalid, strlen(invalid));
  CHECK(FzeroTracksInit("test-pack-discovery/settings", true));
  CHECK(cp_catalog_find(FzeroTracksCatalog(), "one"));
  CHECK(!cp_catalog_find(FzeroTracksCatalog(), "bad"));
  CHECK(FzeroTracksDiagnosticCount());
  puts("Pack discovery, atomic rejection, duplicate quarantine and loader "
       "persistence passed");
  return 0;
}
