#include "fzero_packs.h"
#include "fzero_tracks.h"
#include "fzero_course_file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc, char **argv) {
  if (argc != 2 && !(argc == 4 && !strcmp(argv[2], "--dump")))
    return 2;
  CpCatalog catalog = {0};
  FzeroPacksDiscover(&catalog, argv[1]);
  FzeroCourse *course = malloc(sizeof(*course));
  if (!course)
    return 2;
  char error[256];
  bool ok = FzeroTracksDiagnosticCount() == 0;
  for (unsigned i = 0; i < catalog.count; ++i) {
    const CpPack *p = catalog.packs[i];
    printf("%s: %u cups, %u course entries\n", p->id, p->cup_count,
           p->track_count);
    for (unsigned j = 0; j < p->track_count; ++j) {
      if (!FzeroPacksLoadCourse(p->id, j, course, error, sizeof(error))) {
        fprintf(stderr, "%s/%s: %s\n", p->id, p->tracks[j].id, error);
        ok = false;
      } else {
        printf("%s/%s ", p->id, p->tracks[j].id);
        for (unsigned h = 0; h < 32; ++h)
          printf("%02x", course->hash[h]);
        puts("");
        if (argc == 4) {
          char filename[1024];
          snprintf(filename, sizeof(filename), "%s/%s--%s.fzc", argv[3], p->id, p->tracks[j].id);
          if (!FzeroCourseFileWrite(filename, course, error, sizeof(error))) {
            fprintf(stderr, "%s: %s\n", filename, error);
            ok = false;
          }
        }
      }
    }
  }
  free(course);
  cp_catalog_free(&catalog);
  return ok ? 0 : 1;
}
