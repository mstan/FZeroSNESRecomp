#pragma once
#include <filesystem>
#include <string>

struct FzeroContentImportResult {
  std::string id, name;
  std::filesystem::path installed;
  unsigned courses = 0;
  bool plain_editor_project = false;
  std::string warnings;
};
// Staging writes this ownership index outside user pack manifests.
bool FzeroContentIsBundled(const std::filesystem::path &mods,
                           const std::string &id);
// Throws a readable error. Never overwrites an existing pack or save file.
FzeroContentImportResult FzeroContentImport(
    const std::filesystem::path &source, const std::filesystem::path &mods,
    const std::filesystem::path &stock, const std::filesystem::path &helpers,
    const std::string &displayName);
