#include "fzero_content_import.h"
#include <cstdio>
#include <fstream>
#include <iterator>
int main(int argc, char **argv) {
  if (argc < 3 || argc > 7) {
    fprintf(stderr, "usage: FZeroImportContent source mods-directory [title] "
                    "[stock-rom] [helpers-directory] [answers-json]\n");
    return 2;
  }
  try {
    std::string answers;
    if (argc > 6) {
      if (std::filesystem::file_size(argv[6]) > 256 * 1024)
        throw std::runtime_error("Import answers exceed the size limit.");
      std::ifstream input(argv[6], std::ios::binary);
      if (!input)
        throw std::runtime_error("Cannot read import answers.");
      answers.assign(std::istreambuf_iterator<char>(input), {});
    }
    auto r = FzeroContentImport(argv[1], argv[2], argc > 4 ? argv[4] : "",
                                argc > 5 ? argv[5] : ".",
                                argc > 3 ? argv[3] : "", answers);
    printf("Imported %s [%s], %u courses: %s\n", r.name.c_str(), r.id.c_str(),
           r.courses, r.installed.string().c_str());
    if (!r.warnings.empty())
      printf("%s\n", r.warnings.c_str());
    return 0;
  } catch (const FzeroContentNeedsInput &e) {
    fprintf(stderr, "%s\nReview: %s\n", e.what(), e.report.string().c_str());
    return 3;
  } catch (const std::exception &e) {
    fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
