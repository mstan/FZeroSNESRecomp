#include "fzero_content_import.h"
#include <cstdio>
int main(int argc, char **argv) {
  if (argc < 3 || argc > 6) {
    fprintf(stderr, "usage: FZeroImportContent source mods-directory [title] "
                    "[stock-rom] [helpers-directory]\n");
    return 2;
  }
  try {
    auto r =
        FzeroContentImport(argv[1], argv[2], argc > 4 ? argv[4] : "",
                           argc > 5 ? argv[5] : ".", argc > 3 ? argv[3] : "");
    printf("Imported %s [%s], %u courses: %s\n", r.name.c_str(), r.id.c_str(),
           r.courses, r.installed.string().c_str());
    return 0;
  } catch (const std::exception &e) {
    fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
