#include "fzero_content_ui.h"
#include "data_pack_io.hpp"
#include "fzero_content_import.h"
#include <algorithm>
#include <cstdio>
#include <mutex>
#include <thread>
#include <vector>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <shellapi.h>
#endif
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
      // Guides and loader preferences are available through Open folder;
      // they are not installed playable content.
      auto filename = e.path().filename().string();
      if (filename == "CONVERSION.md" || filename == "PARSE_MANIFEST.md" ||
          filename == "README.md" || filename == "track-packs")
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
int types(void *) { return 2; }
int type(void *, int index, RecompLauncherCCustomContentType *out) {
  if (!out || index < 0 || index > 1)
    return 0;
  *out = {};
  copy(out->id, index ? "folder" : "file");
  copy(out->label, index ? "Import folder" : "Import content");
  copy(out->description,
       "FZEdit projects and packs, or supported IPS/BPS patches.");
  copy(out->file_patterns, "*.zip,*.ips,*.bps,*.fzm,*.sfc,*.smc");
  copy(out->file_description, "F-Zero custom content");
  out->directory = index;
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
                 (result.courses == 1 ? " course)." : " courses)."));
        std::string detail =
            "Ready to play with Track Pack Loader enabled in Mods.";
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
int open(void *, const char *id) {
  try {
    fs::path path = context.mods;
    if (id && *id) {
      std::lock_guard guard(context.lock);
      bool found = false;
      for (auto &e : context.rows)
        if (std::string(e.id) == id) {
          path = fs::u8path(e.path);
          found = true;
          break;
        }
      if (!found)
        throw std::runtime_error("This content entry is no longer available.");
      if (!fs::is_directory(path))
        path = path.parent_path();
    }
    fs::create_directories(path);
#ifdef _WIN32
    // Explicit user action; invoke Explorer, never execute the selected
    // content.
    auto argument = L"\"" + path.wstring() + L"\"";
    if (reinterpret_cast<INT_PTR>(
            ShellExecuteW(nullptr, L"open", L"explorer.exe", argument.c_str(),
                          nullptr, SW_SHOWNORMAL)) <= 32)
      throw std::runtime_error("Could not open the content folder.");
    return 1;
#else
    throw std::runtime_error(
        "Open the content folder using your file manager: " + path.string());
#endif
  } catch (const std::exception &e) {
    context.error = e.what();
    return 0;
  }
}
const char *lastError(void *) { return context.error.c_str(); }
const RecompLauncherCCustomContentProvider provider = {
    nullptr,
    "Add courses from a ZIP, FZEdit project, or supported IPS/BPS patch. Your "
    "installed content appears below. Gameplay options remain in Mods.",
    types,
    type,
    count,
    entry,
    start,
    status,
    open,
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
