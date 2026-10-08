#pragma once
#include <filesystem>
#include <stdexcept>
#include <string>

struct FzeroContentImportResult {
  std::string id, name;
  std::filesystem::path installed;
  unsigned courses = 0;
  bool plain_editor_project = false;
  std::string warnings;
};
// A usable donor needs the user's labels/grouping before publication.
struct FzeroContentNeedsInput : std::runtime_error {
  std::filesystem::path report;
  FzeroContentNeedsInput(const std::string &message,
                         const std::filesystem::path &reportPath)
      : std::runtime_error(message), report(reportPath) {}
};
// Staging writes this ownership index outside user pack manifests.
bool FzeroContentIsBundled(const std::filesystem::path &mods,
                           const std::string &id);
// Throws a readable error. Never overwrites an existing pack or save file.
FzeroContentImportResult FzeroContentImport(
    const std::filesystem::path &source, const std::filesystem::path &mods,
    const std::filesystem::path &stock, const std::filesystem::path &helpers,
    const std::string &displayName, const std::string &answersJson = {});
