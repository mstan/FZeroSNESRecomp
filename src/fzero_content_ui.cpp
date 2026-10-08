#include "fzero_content_ui.h"
#include "data_pack_io.hpp"
#include "fzero_content_import.h"
#include <algorithm>
#include <cstdio>
#include <mutex>
#include <thread>
#include <vector>
namespace fs = std::filesystem;
namespace {
struct Context {
  std::mutex lock;
  std::thread worker;
  fs::path mods, helpers;
  std::vector<RecompLauncherCCustomContentEntry> rows;
  RecompLauncherCCustomContentStatus status{};
  bool changed = false;
  std::string error;
  ~Context() {
    if (worker.joinable())
      worker.join();
  }
} context;
template <size_t N> void copy(char (&dest)[N], const std::string &s) {
  snprintf(dest, N, "%s", s.c_str());
}
std::vector<RecompLauncherCCustomContentEntry> inventory(const fs::path &mods) {
  std::vector<RecompLauncherCCustomContentEntry> rows;
  auto add = [&](const fs::path &path, bool pack) {
    RecompLauncherCCustomContentEntry e{};
    copy(e.id, fs::relative(path, mods).generic_string());
    copy(e.name, path.filename().string());
    copy(e.path, path.string());
    copy(e.kind, pack ? "Course pack" : "Mod files");
    copy(e.status, pack ? "Installed; checked when the game starts"
                        : "Managed by the game or its Mods settings");
    if (pack && fs::is_regular_file(path / "pack.json")) {
      try {
        auto bytes = snesrecomp::data_pack::read(path / "pack.json");
        rapidjson::Document d;
        d.Parse<rapidjson::kParseIterativeFlag>(bytes.c_str());
        if (d.HasParseError() || !d.IsObject())
          throw std::runtime_error("Invalid pack.json");
        copy(e.name, snesrecomp::data_pack::string(d, "title"));
        if (FzeroContentIsBundled(mods,
                                  snesrecomp::data_pack::string(d, "id"))) {
          copy(e.kind, "Included course pack");
          copy(e.status,
               "Included with F-Zero Forever; cannot be removed here");
        } else {
          copy(e.kind, "Imported course pack");
        }
      } catch (const std::exception &ex) {
        e.has_error = 1;
        copy(e.status, ex.what());
      }
    }
    rows.push_back(e);
  };
  if (fs::exists(mods))
    for (auto &e : fs::directory_iterator(mods)) {
      if (e.path().filename().string().rfind('.', 0) == 0)
        continue;
      // Guides, import reports and preferences are not playable content.
      auto filename = e.path().filename().string();
      if (filename == "CONVERSION.md" || filename == "PARSE_MANIFEST.md" ||
          filename == "README.md" || filename == "track-packs" ||
          filename == "import-reports")
        continue;
      if (e.path().filename() == "packs" && e.is_directory()) {
        for (auto &p : fs::directory_iterator(e.path()))
          if (p.path().filename().string().rfind('.', 0) != 0)
            add(p.path(), true);
      } else
        add(e.path(), false);
    }
  std::sort(rows.begin(), rows.end(),
            [](auto &a, auto &b) { return std::string(a.name) < b.name; });
  return rows;
}
int types(void *) { return 1; }
int type(void *, int index, RecompLauncherCCustomContentType *out) {
  if (!out || index != 0)
    return 0;
  *out = {};
  copy(out->id, "file");
  copy(out->label, "Import");
  copy(out->description,
       "ZIPs, FZEdit projects, IPS/BPS patches or supported ROM hacks.");
  copy(out->file_patterns, "*.zip,*.ips,*.bps,*.fzm,*.sfc,*.smc,*.rom,*.fig");
  copy(out->file_description, "F-Zero custom content");
  return 1;
}
int count(void *) {
  std::lock_guard guard(context.lock);
  return int(context.rows.size());
}
int entry(void *, int i, RecompLauncherCCustomContentEntry *out) {
  std::lock_guard guard(context.lock);
  if (!out || i < 0 || size_t(i) >= context.rows.size())
    return 0;
  *out = context.rows[i];
  return 1;
}
int start(void *, const char *, const char *path, const char *image,
          const char *name) {
  if (!path || !*path) {
    context.error = "Choose a content file first.";
    return 0;
  }
  {
    std::lock_guard guard(context.lock);
    if (context.status.state == RECOMP_CONTENT_BUSY) {
      context.error = "An import is already running.";
      return 0;
    }
  }
  if (context.worker.joinable())
    context.worker.join();
  std::string source(path), stock(image ? image : ""), title(name ? name : "");
  {
    std::lock_guard guard(context.lock);
    context.status = {};
    context.status.state = RECOMP_CONTENT_BUSY;
    context.status.progress = -1;
    copy(context.status.message, "Checking and importing your content...");
  }
  try {
    context.worker = std::thread([source, stock, title] {
      try {
        auto result =
            FzeroContentImport(fs::u8path(source), context.mods,
                               fs::u8path(stock), context.helpers, title);
        std::vector<RecompLauncherCCustomContentEntry> rows;
        bool refreshed = true;
        try {
          rows = inventory(context.mods);
        } catch (const std::exception &) {
          refreshed = false;
        }
        std::lock_guard guard(context.lock);
        if (refreshed)
          context.rows = std::move(rows);
        context.changed = true;
        context.status.state = RECOMP_CONTENT_SUCCEEDED;
        context.status.progress = 100;
        copy(context.status.message,
             "Imported " + result.name + " (" + std::to_string(result.courses) +
                 (result.courses == 1 ? " course)." : " courses).") +
                 (result.warnings.empty() ? "" : " Some files need attention."));
        std::string detail =
            "Ready to play with Track Pack Loader enabled in Mods.";
        if (!result.warnings.empty())
          detail += "\n" + result.warnings + "\nDetails: mods/packs/" +
                    result.id + "/conversion-report.json";
        if (result.plain_editor_project)
          detail +=
              " This editor project uses standard F-Zero rules. Special rules "
              "need an author-provided pack; see mods/CONVERSION.md.";
        if (!refreshed)
          detail += " Reopen the launcher to refresh the content list.";
        copy(context.status.detail, detail);
      } catch (const std::exception &e) {
        std::lock_guard guard(context.lock);
        context.status.state = RECOMP_CONTENT_FAILED;
        context.status.progress = 0;
        copy(context.status.message, "This file could not be imported.");
        copy(context.status.detail, e.what());
      }
    });
    return 1;
  } catch (const std::exception &e) {
    std::lock_guard guard(context.lock);
    context.status.state = RECOMP_CONTENT_FAILED;
    context.error = e.what();
    return 0;
  }
}
int status(void *, RecompLauncherCCustomContentStatus *out) {
  if (!out)
    return 0;
  std::lock_guard guard(context.lock);
  *out = context.status;
  return 1;
}
const char *lastError(void *) { return context.error.c_str(); }
const RecompLauncherCCustomContentProvider provider = {
    nullptr,
    "Add courses from a ZIP, FZEdit project, patch or supported ROM hack. Your "
    "installed content appears below. Gameplay options remain in Mods.",
    types,
    type,
    count,
    entry,
    start,
    status,
    nullptr,
    lastError};
} // namespace
const RecompLauncherCCustomContentProvider *
FzeroContentProvider(const char *mods, const char *helpers) {
  FzeroContentShutdown();
  context.mods = fs::absolute(fs::u8path(mods));
  context.helpers = fs::absolute(fs::u8path(helpers));
  context.changed = false;
  context.status = {};
  context.error.clear();
  try {
    context.rows = inventory(context.mods);
  } catch (const std::exception &e) {
    context.rows.clear();
    context.status.state = RECOMP_CONTENT_FAILED;
    copy(context.status.detail, e.what());
  }
  return &provider;
}
int FzeroContentShutdown(void) {
  if (context.worker.joinable())
    context.worker.join();
  return context.changed ? 1 : 0;
}
