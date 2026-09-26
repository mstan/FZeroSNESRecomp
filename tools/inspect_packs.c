#include "fzero_packs.h"
#include "fzero_tracks.h"
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char **argv) {
  if (argc != 2)
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
      }
    }
  }
  free(course);
  cp_catalog_free(&catalog);
  return ok ? 0 : 1;
}
