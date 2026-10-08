// Host review lifecycle with the external converter boundary replaced by a
// source-bound fixture. No ROM, graphics backend or installed packs required.
#include "fzero_content_import.h"
#include "fzero_content_ui.h"
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <rapidjson/document.h>
#include <stdexcept>
#include <thread>
#include <vector>
namespace fs = std::filesystem;
static fs::path report;
static std::string submitted;
static void check(bool ok, const char *message) {
  if (!ok)
    throw std::runtime_error(message);
}
bool FzeroContentIsBundled(const fs::path &, const std::string &) {
  return false;
}
FzeroContentImportResult FzeroContentImport(const fs::path &,
                                            const fs::path &mods,
                                            const fs::path &, const fs::path &,
                                            const std::string &,
                                            const std::string &answers) {
  if (answers.empty())
    throw FzeroContentNeedsInput("Review needed", report);
  submitted = answers;
  return {"fixture", "Chosen pack", mods / "packs/fixture",
          2,         false,         "Music needs attention."};
}
static RecompLauncherCCustomContentStatus
wait(const RecompLauncherCCustomContentProvider *p) {
  for (int i = 0; i < 500; ++i) {
    RecompLauncherCCustomContentStatus status{};
    check(p->import_status(p->ctx, &status) != 0, "status");
    if (status.state != RECOMP_CONTENT_BUSY)
      return status;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  throw std::runtime_error("Review worker timed out");
}
static std::vector<RecompLauncherCCustomContentValue>
defaults(const RecompLauncherCCustomContentProvider *p) {
  std::vector<RecompLauncherCCustomContentValue> values;
  for (int i = 0; i < p->review_field_count(p->ctx); ++i) {
    RecompLauncherCCustomContentField field{};
    check(p->review_field_get(p->ctx, i, &field) != 0, "field");
    RecompLauncherCCustomContentValue value{};
    std::strcpy(value.id, field.id);
    std::strcpy(value.value, field.value);
    values.push_back(value);
  }
  return values;
}
int main() {
  auto root =
      fs::temp_directory_path() /
      ("fzero-review-test-" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  try {
    check(fs::create_directory(root), "Temporary fixture already exists");
    report = root / "report.json";
    const std::string digest(64, 'a');
    std::ofstream out(report);
    out << "{\"target_sha256\":\"" << digest << "\",\"input_sha256\":\""
        << digest << R"(","review":{
      "description":"Confirm the two cup names.","fields":[
      {"id":"pack_name","type":"text","label":"Pack name","value":"Source name"},
      {"id":"author","type":"text","label":"Author","value":"Unknown author"},
      {"id":"cup_1_name","type":"text","label":"Cup 1 name","value":"Cup 1"},
      {"id":"cup_2_name","type":"text","label":"Cup 2 name","value":"Cup 2"},
      {"id":"cup_for_slot_0","type":"choice","label":"Course A","value":"cup-1",
       "options":[{"value":"cup-1","label":"Cup 1"},{"value":"cup-2","label":"Cup 2"}]},
      {"id":"cup_for_slot_1","type":"choice","label":"Course B","value":"cup-2",
       "options":[{"value":"cup-1","label":"Cup 1"},{"value":"cup-2","label":"Cup 2"}]}
    ]}})";
    out.close();
    auto mods = root / "mods";
    auto p = FzeroContentProvider(mods.string().c_str(), root.string().c_str());
    check(p->type_count(p->ctx) == 1 && !p->open_folder, "Single import flow");
    check(p->import_start(p->ctx, "file", "source.zip", "stock.sfc",
                          "My pack") != 0,
          "start");
    check(wait(p).state == RECOMP_CONTENT_NEEDS_INPUT, "Needs review");
    check(!FzeroContentShutdown() && !fs::exists(mods / "packs"),
          "Review must not publish content");
    check(!p->import_start(p->ctx, "file", "second.zip", "stock.sfc", "second"),
          "No second pending import");
    auto values = defaults(p);
    check(values.size() == 6 && std::string(values[0].value) == "My pack",
          "Keep initial user title");
    RecompLauncherCCustomContentOption option{};
    check(p->review_option_get(p->ctx, "cup_for_slot_0", 1, &option) != 0 &&
              std::string(option.value) == "cup-2",
          "Choice identity");
    std::strcpy(values[0].value, "Chosen pack");
    std::strcpy(values[2].value, "First league");
    std::strcpy(values[3].value, "Second league");
    std::strcpy(values[5].value, "cup-1");
    check(!p->review_submit(p->ctx, values.data(), int(values.size())),
          "Empty cup rejected");
    check(wait(p).state == RECOMP_CONTENT_NEEDS_INPUT,
          "Invalid input leaves review editable");
    std::strcpy(values[5].value, "cup-2");
    check(p->review_submit(p->ctx, values.data(), int(values.size())) != 0,
          "Submit corrected input");
    auto done = wait(p);
    check(done.state == RECOMP_CONTENT_SUCCEEDED &&
              std::strstr(done.message, "attention"),
          "Partial music success");
    check(FzeroContentShutdown() != 0,
          "Successful import refreshes game content");
    rapidjson::Document answers;
    answers.Parse(submitted.c_str());
    check(!answers.HasParseError() &&
              answers["target_sha256"] == digest.c_str() &&
              answers["input_sha256"] == digest.c_str(),
          "Review is source-bound");
    check(std::string(answers["values"]["pack_name"].GetString()) ==
                  "Chosen pack" &&
              std::string(answers["values"]["cup_1_name"].GetString()) ==
                  "First league",
          "Edited labels submitted");
    p = FzeroContentProvider(mods.string().c_str(), root.string().c_str());
    check(p->import_start(p->ctx, "file", "source.zip", "stock.sfc",
                          "Cancel test") != 0,
          "Restart");
    check(wait(p).state == RECOMP_CONTENT_NEEDS_INPUT, "Second review");
    check(p->review_cancel(p->ctx) != 0 && wait(p).state == RECOMP_CONTENT_IDLE,
          "Cancel clears pending import");
    check(!FzeroContentShutdown() && p->review_field_count(p->ctx) == 0,
          "Cancelled import changes nothing");
    // Music warnings must survive reopening the launcher without making a
    // valid pack look broken. Reports describe import history, not live files.
    auto installed = mods / "packs/music-fixture";
    fs::create_directories(installed);
    std::ofstream(installed / "pack.json")
        << R"({"id":"music-fixture","title":"Music fixture"})";
    std::ofstream(installed / "conversion-report.json")
        << R"({"audio_inventory":{"unmapped_pcm_count":4}})";
    p = FzeroContentProvider(mods.string().c_str(), root.string().c_str());
    RecompLauncherCCustomContentEntry entry{};
    check(p->entry_count(p->ctx) == 1 && p->entry_get(p->ctx, 0, &entry),
          "Installed music fixture");
    check(!entry.has_error && std::strstr(entry.notice, "4 songs") &&
              std::strstr(entry.notice_tooltip, "music/example.pcm"),
          "Persistent music placement notice");
    std::ofstream(installed / "conversion-report.json") << "invalid report";
    p = FzeroContentProvider(mods.string().c_str(), root.string().c_str());
    check(p->entry_get(p->ctx, 0, &entry) && !entry.has_error &&
              !entry.notice[0],
          "Optional damaged report does not invalidate courses");
    fs::remove_all(root);
    std::puts("Custom content host review passed");
    return 0;
  } catch (const std::exception &e) {
    FzeroContentShutdown();
    std::fprintf(stderr, "%s\nFixture retained: %s\n", e.what(),
                 root.string().c_str());
    return 1;
  }
}
