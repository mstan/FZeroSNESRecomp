#include "fzero_content_ui.h"
#include "data_pack_io.hpp"
#include "fzero_content_import.h"
#include <algorithm>
#include <cstdio>
#include <map>
#include <mutex>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <set>
#include <thread>
#include <vector>
namespace fs = std::filesystem;
namespace {
struct Review {
  std::string source, stock, title, targetHash, inputHash, description;
  std::vector<RecompLauncherCCustomContentField> fields;
  std::vector<std::vector<RecompLauncherCCustomContentOption>> options;
};
struct Context {
  std::mutex lock;
  std::thread worker;
  fs::path mods, helpers;
  std::vector<RecompLauncherCCustomContentEntry> rows;
  RecompLauncherCCustomContentStatus status{};
  bool changed = false;
  std::string error;
  Review review;
  ~Context() {
    if (worker.joinable())
      worker.join();
  }
} context;
template <size_t N> void copy(char (&dest)[N], const std::string &s) {
  snprintf(dest, N, "%s", s.c_str());
}
void require(bool ok, const std::string &message) {
  if (!ok)
    throw std::runtime_error(message);
}
Review readReview(const fs::path &report, const std::string &source,
                  const std::string &stock, const std::string &title) {
  using snesrecomp::data_pack::string;
  auto bytes = snesrecomp::data_pack::read(report, 8 * 1024 * 1024);
  rapidjson::Document d;
  d.Parse<rapidjson::kParseIterativeFlag>(bytes.c_str());
  require(!d.HasParseError() && d.IsObject(), "Invalid import review report.");
  snesrecomp::data_pack::validate_json(d);
  require(d.HasMember("review") && d["review"].IsObject(),
          "Missing import review.");
  auto &form = d["review"];
  require(form.HasMember("fields") && form["fields"].IsArray() &&
              form["fields"].Size() > 0 && form["fields"].Size() <= 256,
          "Invalid import review fields.");
  Review review;
  review.source = source;
  review.stock = stock;
  review.title = title;
  review.targetHash = string(d, "target_sha256");
  review.inputHash = string(d, "input_sha256");
  require(review.targetHash.size() == 64 && review.inputHash.size() == 64,
          "Import review has no source identity.");
  review.description = form.HasMember("description")
                           ? string(form, "description")
                           : "Check the cup names and course assignments, then "
                             "choose Import courses.";
  std::set<std::string> ids;
  for (auto &item : form["fields"].GetArray()) {
    require(item.IsObject(), "Invalid import review field.");
    RecompLauncherCCustomContentField field{};
    auto id = string(item, "id"), value = string(item, "value");
    require(!id.empty() && id.size() < sizeof(field.id) &&
                ids.insert(id).second,
            "Invalid or repeated review field.");
    if (id == "pack_name" && !title.empty())
      value = title;
    require(value.size() < sizeof(field.value), "Review value is too long.");
    copy(field.id, id);
    copy(field.label, string(item, "label"));
    copy(field.value, value);
    if (item.HasMember("description"))
      copy(field.description, string(item, "description"));
    auto type = string(item, "type");
    require(type == "text" || type == "choice", "Unsupported review control.");
    field.type = type == "choice" ? RECOMP_CONTENT_FIELD_CHOICE
                                  : RECOMP_CONTENT_FIELD_TEXT;
    std::vector<RecompLauncherCCustomContentOption> options;
    if (field.type == RECOMP_CONTENT_FIELD_CHOICE) {
      require(item.HasMember("options") && item["options"].IsArray() &&
                  item["options"].Size() > 0 && item["options"].Size() <= 256,
              "Invalid import review choices.");
      std::set<std::string> choices;
      for (auto &choice : item["options"].GetArray()) {
        require(choice.IsObject(), "Invalid import review choice.");
        RecompLauncherCCustomContentOption option{};
        auto key = string(choice, "value");
        require(key.size() < sizeof(option.value) && choices.insert(key).second,
                "Invalid import review choice.");
        copy(option.value, key);
        copy(option.label, string(choice, "label"));
        options.push_back(option);
      }
      require(choices.count(value) != 0, "Unknown default review choice.");
    }
    field.option_count = int(options.size());
    review.fields.push_back(field);
    review.options.push_back(std::move(options));
  }
  return review;
}
void musicNotice(const fs::path &path,
                 RecompLauncherCCustomContentEntry &entry) {
  auto report = path / "conversion-report.json";
  if (!fs::is_regular_file(report))
    return;
  try {
    auto bytes = snesrecomp::data_pack::read(report, 8 * 1024 * 1024);
    rapidjson::Document d;
    d.Parse<rapidjson::kParseIterativeFlag>(bytes.c_str());
    if (d.HasParseError() || !d.IsObject())
      return;
    snesrecomp::data_pack::validate_json(d);
    if (!d.HasMember("audio_inventory") || !d["audio_inventory"].IsObject())
      return;
    auto &audio = d["audio_inventory"];
    if (!audio.HasMember("unmapped_pcm_count") ||
        !audio["unmapped_pcm_count"].IsUint())
      return;
    const auto count = audio["unmapped_pcm_count"].GetUint();
    if (!count)
      return;
    copy(entry.notice,
         std::to_string(count) + (count == 1 ? " song wasn't imported"
                                             : " songs weren't imported"));
    copy(entry.notice_tooltip,
         "These recordings could not be matched or read during import. "
         "Add the songs you want manually; your original ZIP is unchanged.\n\n"
         "For a course, match its ZIP filename in this pack's music folder: "
         "courses/example.zip uses music/example.pcm.\n\n"
         "For menu and event songs, use Mods > Menu and event music. "
         "See MODS.md and this pack's conversion-report.json for details. "
         "This notice describes the import; songs added afterward are not "
         "listed here.");
  } catch (const std::exception &) {
    // An optional historical report cannot make playable courses invalid.
  }
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
          musicNotice(path, e);
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
int queueImport(const std::string &source, const std::string &stock,
                const std::string &title, const std::string &answers = {}) {
  if (context.worker.joinable())
    context.worker.join();
  {
    std::lock_guard guard(context.lock);
    context.status = {};
    context.status.state = RECOMP_CONTENT_BUSY;
    context.status.progress = -1;
    context.error.clear();
    copy(context.status.message, "Checking and importing your content...");
  }
  try {
    context.worker = std::thread([source, stock, title, answers] {
      try {
        auto result = FzeroContentImport(fs::u8path(source), context.mods,
                                         fs::u8path(stock), context.helpers,
                                         title, answers);
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
        context.review = {};
        context.status.state = RECOMP_CONTENT_SUCCEEDED;
        context.status.progress = 100;
        copy(
            context.status.message,
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
      } catch (const FzeroContentNeedsInput &e) {
        try {
          auto review = readReview(e.report, source, stock, title);
          std::lock_guard guard(context.lock);
          context.review = std::move(review);
          context.status = {};
          context.status.state = RECOMP_CONTENT_NEEDS_INPUT;
          copy(context.status.message, "Review your courses");
          copy(context.status.detail, context.review.description);
        } catch (const std::exception &error) {
          std::lock_guard guard(context.lock);
          context.status.state = RECOMP_CONTENT_FAILED;
          copy(context.status.message, "Could not prepare the import details.");
          copy(context.status.detail, error.what());
        }
      } catch (const std::exception &e) {
        std::lock_guard guard(context.lock);
        context.status.state =
            !answers.empty() && !context.review.fields.empty()
                ? RECOMP_CONTENT_NEEDS_INPUT
                : RECOMP_CONTENT_FAILED;
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
int start(void *, const char *, const char *path, const char *image,
          const char *name) {
  if (!path || !*path) {
    context.error = "Choose a content file first.";
    return 0;
  }
  {
    std::lock_guard guard(context.lock);
    if (context.status.state == RECOMP_CONTENT_BUSY ||
        context.status.state == RECOMP_CONTENT_NEEDS_INPUT) {
      context.error = "Finish or cancel the current import first.";
      return 0;
    }
    context.review = {};
  }
  return queueImport(path, image ? image : "", name ? name : "");
}
int status(void *, RecompLauncherCCustomContentStatus *out) {
  if (!out)
    return 0;
  std::lock_guard guard(context.lock);
  *out = context.status;
  return 1;
}
int reviewCount(void *) {
  std::lock_guard guard(context.lock);
  return int(context.review.fields.size());
}
int reviewField(void *, int index, RecompLauncherCCustomContentField *out) {
  std::lock_guard guard(context.lock);
  if (!out || index < 0 || size_t(index) >= context.review.fields.size())
    return 0;
  *out = context.review.fields[index];
  return 1;
}
int reviewOption(void *, const char *id, int index,
                 RecompLauncherCCustomContentOption *out) {
  std::lock_guard guard(context.lock);
  if (!id || !out || index < 0)
    return 0;
  for (size_t i = 0; i < context.review.fields.size(); ++i)
    if (std::string(context.review.fields[i].id) == id &&
        size_t(index) < context.review.options[i].size()) {
      *out = context.review.options[i][index];
      return 1;
    }
  return 0;
}
int reviewSubmit(void *, const RecompLauncherCCustomContentValue *input,
                 int count) {
  try {
    Review review;
    std::map<std::string, std::string> values;
    {
      std::lock_guard guard(context.lock);
      require(context.status.state == RECOMP_CONTENT_NEEDS_INPUT,
              "There is no import waiting for details.");
      review = context.review;
    }
    require(input && count == int(review.fields.size()),
            "Complete the import details first.");
    for (int i = 0; i < count; ++i) {
      require(std::find(std::begin(input[i].id), std::end(input[i].id), '\0') !=
                      std::end(input[i].id) &&
                  std::find(std::begin(input[i].value),
                            std::end(input[i].value),
                            '\0') != std::end(input[i].value),
              "An import value is too long.");
      auto text = std::string(input[i].value);
      auto begin = text.find_first_not_of(" \t\r\n");
      text = begin == text.npos
                 ? ""
                 : text.substr(begin,
                               text.find_last_not_of(" \t\r\n") - begin + 1);
      require(values.emplace(input[i].id, text).second,
              "Repeated import field.");
    }
    std::map<std::string, int> cupCounts;
    for (size_t i = 0; i < review.fields.size(); ++i) {
      auto &field = review.fields[i];
      std::string id(field.id);
      require(values.count(id) != 0, "An import field is missing.");
      auto &text = values[id];
      if (id == "author" && text.empty())
        text = "Unknown author";
      require(
          !text.empty() && text.size() < 96 &&
              std::none_of(text.begin(), text.end(),
                           [](unsigned char c) { return c < 32 || c == '|'; }),
          std::string(field.label) +
              ": use a name shorter than 96 bytes without | or line breaks.");
      if (field.type == RECOMP_CONTENT_FIELD_CHOICE)
        require(std::any_of(
                    review.options[i].begin(), review.options[i].end(),
                    [&](const auto &option) { return text == option.value; }),
                "Choose one of the available options for " +
                    std::string(field.label) + ".");
      if (id.rfind("cup_", 0) == 0 && id.size() > 9 &&
          id.compare(id.size() - 5, 5, "_name") == 0)
        cupCounts.emplace("cup-" + id.substr(4, id.size() - 9), 0);
      copy(field.value, text);
    }
    for (const auto &[id, value] : values)
      if (id.rfind("cup_for_slot_", 0) == 0) {
        require(cupCounts.count(value) != 0,
                "Choose a named cup for every course.");
        ++cupCounts[value];
      }
    for (const auto &[id, amount] : cupCounts)
      require(amount >= 1 && amount <= 5,
              "Each cup needs between one and five courses. Check the course "
              "assignments.");
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    writer.StartObject();
    writer.Key("target_sha256");
    writer.String(review.targetHash.c_str());
    writer.Key("input_sha256");
    writer.String(review.inputHash.c_str());
    writer.Key("values");
    writer.StartObject();
    for (auto &[id, value] : values) {
      writer.Key(id.c_str());
      writer.String(value.c_str());
    }
    writer.EndObject();
    writer.EndObject();
    {
      std::lock_guard guard(context.lock);
      context.review =
          review; // Retain edits if conversion reports another issue.
    }
    return queueImport(review.source, review.stock, "", buffer.GetString());
  } catch (const std::exception &e) {
    context.error = e.what();
    return 0;
  }
}
int reviewCancel(void *) {
  std::lock_guard guard(context.lock);
  if (context.status.state != RECOMP_CONTENT_NEEDS_INPUT) {
    context.error = "There is no pending import to cancel.";
    return 0;
  }
  context.review = {};
  context.status = {};
  context.error.clear();
  copy(context.status.message, "Import cancelled. No content was installed.");
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
    lastError,
    reviewCount,
    reviewField,
    reviewOption,
    reviewSubmit,
    reviewCancel};
} // namespace
const RecompLauncherCCustomContentProvider *
FzeroContentProvider(const char *mods, const char *helpers) {
  FzeroContentShutdown();
  context.mods = fs::absolute(fs::u8path(mods));
  context.helpers = fs::absolute(fs::u8path(helpers));
  context.changed = false;
  context.status = {};
  context.error.clear();
  context.review = {};
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
