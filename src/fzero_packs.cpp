// Game-owned course semantics over snesrecomp folder/ZIP transport.
#include "fzero_packs.h"
#include "fzero_fzedit.h"
extern "C" {
#include "fzero_course_file.h"
#include "fzero_title.h"
#include "fzero_tracks.h"
#include "fzero_menu_music.h"
#include "sha256.h"
}
#include <algorithm>
#include "data_pack.h"
#include "data_pack_io.hpp"
#include <memory>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <rapidjson/document.h>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
namespace fs = std::filesystem;
using rapidjson::Value;
namespace {
struct Title {
  std::string id, name;
  fs::path file;
};
struct Pack {
  CpPack info{};
  fs::path root;
  std::vector<fs::path> courses;
  std::vector<FzeroCourse> decoded;
  std::vector<Title> titles;
  std::map<std::string, fs::path> menu_music;
  int order = 0;
};
struct Sound {
  std::string id, prefix;
  fs::path root;
};
std::vector<Pack> packs;
std::vector<Sound> sounds;
std::string primary;
std::string lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return char(std::tolower(c)); });
  return s;
}
using snesrecomp::data_pack::inside;
using snesrecomp::data_pack::read;
using snesrecomp::data_pack::validate_json;
using snesrecomp::data_pack::string;
void require(bool yes, const std::string &message) {
  if (!yes)
    throw std::runtime_error(message);
}
void ident(const std::string &s) {
  require(!s.empty() && s.size() < CP_ID &&
              s.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789-_") ==
                  std::string::npos,
          "Invalid stable ID: " + s);
}
template <size_t N> void copy(char (&out)[N], const std::string &s) {
  require(s.size() < N, "Metadata string too long");
  std::copy(s.begin(), s.end(), out);
  out[s.size()] = 0;
}
fs::path courseMusic(const Pack &pack, unsigned index) {
  auto name = pack.courses[index].filename();
  name.replace_extension(".pcm");
  return inside(pack.root, "music/" + name.string());
}
unsigned requiredFeatures(const Value &features) {
  require(features.IsArray(), "requires must be an array");
  unsigned bits = 0;
  for (auto &feature : features.GetArray()) {
    require(feature.IsString(), "Invalid required feature");
    std::string name(feature.GetString(), feature.GetStringLength());
    if (name == "grip-magnets")
      bits |= FZERO_COURSE_GRIP_MAGNETS;
    else if (name == "up-magnets")
      bits |= FZERO_COURSE_UP_MAGNETS;
    else if (name == "rainbow-road")
      bits |= FZERO_COURSE_RAINBOW;
    else
      require(false, "Unsupported required mechanic: " + name);
  }
  return bits;
}
unsigned mechanics(const fs::path &root, const Value &owner) {
  unsigned bits = 0;
  if (!owner.HasMember("mechanics"))
    return bits;
  require(owner["mechanics"].IsArray(),
          "mechanics must be an array of module paths");
  for (auto &entry : owner["mechanics"].GetArray()) {
    require(entry.IsString(), "Expected mechanics module path");
    auto file =
        inside(root, std::string(entry.GetString(), entry.GetStringLength()));
    auto bytes = read(file, 16384);
    rapidjson::Document module;
    module.Parse<rapidjson::kParseIterativeFlag |
                 rapidjson::kParseValidateEncodingFlag>(bytes.data(),
                                                        bytes.size());
    require(!module.HasParseError() && module.IsObject(),
            "Invalid mechanics module");
    validate_json(module);
    require(module.HasMember("format") && module["format"].IsInt() &&
                module["format"].GetInt() == 1,
            "Unsupported mechanics module format");
    ident(string(module, "id"));
    require(string(module, "engine") == "fzero-course-v1",
            "Unsupported mechanics engine");
    require(module.HasMember("requires"),
            "Mechanics module has no requirements");
    bits |= requiredFeatures(module["requires"]);
  }
  return bits;
}
Pack parse(const fs::path &root, const SnesDataPack &entry, std::vector<Sound> &audio,
           std::string &newPrimary, bool courseArchive = false) {
  rapidjson::Document d;
  std::string text(reinterpret_cast<const char *>(entry.payload), entry.payload_size);
  d.Parse<rapidjson::kParseIterativeFlag |
          rapidjson::kParseValidateEncodingFlag>(text.data(), text.size());
  require(!d.HasParseError() && d.IsObject(), "Invalid courses.json");
  validate_json(d);
  require(d.HasMember("format") && d["format"].IsInt() &&
              d["format"].GetInt() == 1,
          "Unsupported pack format");
  Pack p;
  p.root = root;
  auto id = string(d, "id");
  require(id == entry.id && string(d, "name") == entry.title,
          "Course index identity differs from pack envelope");
  ident(id);
  copy(p.info.id, id);
  copy(p.info.name, string(d, "name"));
  copy(p.info.author, string(d, "author"));
  copy(p.info.adapter, "fzero-course-v1");
  if (d.HasMember("order")) {
    require(d["order"].IsInt(), "order must be an integer");
    p.order = d["order"].GetInt();
  }
  if (d.HasMember("titles")) {
    require(d["titles"].IsArray(), "Invalid title list");
    for (auto &t : d["titles"].GetArray()) {
      auto tid = string(t, "id");
      ident(tid);
      auto file = inside(root, string(t, "source"));
      auto resource = read(file);
      require(resource.size() == 0x1489 &&
                  !memcmp(resource.data(), "FZTITLE\1\0", 9),
              "Invalid title resource");
      p.titles.push_back({tid, string(t, "name"), file});
    }
  }
  if (d.HasMember("soundtracks")) {
    require(d["soundtracks"].IsArray(), "soundtracks must be an array");
    for (auto &s : d["soundtracks"].GetArray()) {
      auto sid = string(s, "id");
      ident(sid);
      auto prefix = string(s, "prefix");
      require(!prefix.empty() && prefix.size() < 96 &&
                  prefix.find_first_of("/\\:") == std::string::npos,
              "Invalid PCM prefix");
      auto dir = inside(root, string(s, "directory"));
      audio.push_back({sid, prefix, dir});
      if (s.HasMember("primary")) {
        require(s["primary"].IsBool(), "primary must be boolean");
        if (s["primary"].GetBool()) {
          require(newPrimary.empty() || newPrimary == sid,
                  "Conflicting primary soundtracks");
          newPrimary = sid;
        }
      }
    }
  }
  if (d.HasMember("menu_music")) {
    auto &menu = d["menu_music"];
    require(menu.IsObject(), "menu_music must be an object");
    for (auto it = menu.MemberBegin(); it != menu.MemberEnd(); ++it) {
      std::string cue(it->name.GetString(), it->name.GetStringLength());
      bool known = false;
      for (auto &event : FzeroMenuCues) known |= cue == event.id;
      require(known && it->value.IsString(), "Unknown menu_music cue: " + cue);
      auto file = inside(root, string(menu, cue.c_str()));
      require(lower(file.extension().string()) == ".pcm", "Menu music must be PCM");
      p.menu_music.emplace(cue, file);
    }
  }
  if (!d.HasMember("courses")) {
    require(!audio.empty() || !p.menu_music.empty(), "Pack has no courses or audio");
    return p;
  }
  require(d["courses"].IsArray() && d["courses"].Size() <= CP_TRACKS,
          "Invalid course list");
  require(d.HasMember("cups") && d["cups"].IsArray() &&
              d["cups"].Size() <= CP_CUPS,
          "Invalid cups");
  std::map<std::string, const Value *> definitions;
  unsigned packMechanics = mechanics(root, d);
  for (auto &c : d["courses"].GetArray()) {
    auto cid = string(c, "id");
    ident(cid);
    require(definitions.emplace(cid, &c).second, "Duplicate course ID");
  }
  std::set<std::string> cupids;
  for (auto &cup : d["cups"].GetArray()) {
    auto cid = string(cup, "id");
    ident(cid);
    require(cupids.insert(cid).second, "Duplicate cup ID");
    auto &dest = p.info.cups[p.info.cup_count++];
    copy(dest.id, cid);
    copy(dest.name, string(cup, "name"));
    dest.slot = p.info.cup_count - 1;
    require(cup.HasMember("courses") && cup["courses"].IsArray() &&
                cup["courses"].Size() > 0 && cup["courses"].Size() <= 5,
            "Cups need 1 to 5 courses");
    for (auto &ref : cup["courses"].GetArray()) {
      require(ref.IsString() && definitions.count(ref.GetString()),
              "Unknown cup course");
      require(p.info.track_count < CP_TRACKS, "Too many cup entries");
      auto &c = *definitions.at(ref.GetString());
      auto &t = p.info.tracks[p.info.track_count++];
      copy(t.id, string(c, "id"));
      copy(t.name, string(c, "name"));
      copy(t.cup, cid);
      t.slot = (unsigned)p.courses.size();
      p.courses.push_back(inside(root, string(c, "source")));
      FzeroCourse decoded{};
      char error[256] = {0};
      auto source = p.courses.back();
      bool ok = false;
      if (lower(source.extension().string()) == ".zip") {
        require(!courseArchive, "A course ZIP cannot contain another course ZIP");
        // A one-course pack can also supply a course in a larger league.
        // Use the shared ZIP validator/cache, exactly as for installed packs.
        struct Errors { std::string source, message; } errors{source.string(), {}};
        auto report = [](void *user, const char *file, const char *reason) {
          auto &errors = *static_cast<Errors *>(user);
          if (fs::path(file) == fs::path(errors.source)) errors.message = reason;
        };
        uint8_t base[32];
        cp_hash_parse("bf16c3c867c58e2ab061c70de9295b6930d63f29f81cc986f5ecae03e0ad18d2", base);
        const char *caps[] = {"fzero-course-v1"};
        std::unique_ptr<SnesDataPacks, decltype(&snes_data_packs_destroy)> nested(
            snes_data_packs_scan(source.parent_path().string().c_str(), "f-zero",
                                "fzero.course-index", base, caps, 1, report, &errors),
            snes_data_packs_destroy);
        for (size_t i = 0; i < snes_data_packs_count(nested.get()); ++i) {
          const auto *item = snes_data_packs_get(nested.get(), i);
          if (fs::path(item->source) != source) continue;
          const char *directory = snes_data_pack_directory(
              nested.get(), i, "mods/packs/.cache/sources", report, &errors);
          require(directory != nullptr, errors.message.empty() ?
                  "Cannot open course ZIP" : errors.message);
          std::vector<Sound> unusedAudio;
          std::string unusedPrimary;
          auto project = parse(fs::path(reinterpret_cast<const char8_t *>(directory)),
                               *item, unusedAudio, unusedPrimary, true);
          require(project.decoded.size() == 1,
                  "A course ZIP must contain exactly one course");
          decoded = project.decoded.front();
          // Music follows the actual .fzm/.fzc filename, never the ZIP name.
          p.courses.back() = project.courses.front();
          ok = true;
          break;
        }
        require(ok, errors.message.empty() ? "Course ZIP has no valid pack manifest" :
                                            errors.message);
      } else if (source.extension() == ".fzc")
        ok = FzeroCourseFileRead(source.string().c_str(), &decoded, error,
                                 sizeof(error));
      else if (source.extension() == ".fzm")
        ok = FzeroFzeditRead(root.string().c_str(), source.string().c_str(),
                             &decoded, error, sizeof(error));
      else
        snprintf(error, sizeof(error), "Expected .fzm, .fzc or single-course .zip source");
      require(ok, string(c, "id") + ": " + error);
      if (c.HasMember("requires") && lower(source.extension().string()) != ".zip")
        decoded.required = uint8_t(requiredFeatures(c["requires"]));
      else if (c.HasMember("requires"))
        decoded.required |= uint8_t(requiredFeatures(c["requires"]));
      decoded.required |= uint8_t(packMechanics | mechanics(root, c));
      if (c.HasMember("music")) {
        auto &m = c["music"];
        require(m.IsObject(), "Invalid music mapping");
        if (m.HasMember("spc")) {
          require(m["spc"].IsUint() && m["spc"].GetUint() < 10,
                  "SPC song index must be 0..9");
          decoded.has_music = 1;
          decoded.music = uint8_t(m["spc"].GetUint() * 9);
        }
        if (m.HasMember("soundtrack")) {
          auto id = string(m, "soundtrack");
          ident(id);
          copy(decoded.msu_source, id);
          require(m.HasMember("track") && m["track"].IsUint() &&
                      m["track"].GetUint() > 0 && m["track"].GetUint() < 256,
                  "PCM track number must be 1..255");
          decoded.msu_track = uint8_t(m["track"].GetUint());
        }
      }
      FzeroCourseHash(&decoded);
      p.decoded.push_back(decoded);
    }
  }
  require(p.info.track_count > 0, "Empty course pack");
  return p;
}
} // namespace
void FzeroPacksDiscover(CpCatalog *cat, const char *directory) {
  packs.clear();
  sounds.clear();
  primary.clear();
  std::vector<Pack> candidates;
  std::vector<std::vector<Sound>> audios;
  std::vector<std::string> primaries;
  try {
    auto report = [](void *, const char *source, const char *reason) {
      FzeroTracksReport((fs::path(reinterpret_cast<const char8_t *>(source)).filename().string() + ": " + reason).c_str());
    };
    uint8_t base[32];
    cp_hash_parse("bf16c3c867c58e2ab061c70de9295b6930d63f29f81cc986f5ecae03e0ad18d2", base);
    const char *caps[] = {"fzero-course-v1"};
    std::unique_ptr<SnesDataPacks, decltype(&snes_data_packs_destroy)> shared(
        snes_data_packs_scan(directory, "f-zero", "fzero.course-index", base, caps, 1,
                             report, nullptr), snes_data_packs_destroy);
    auto cache = (fs::path(reinterpret_cast<const char8_t *>(directory)) / ".cache").string();
    for (size_t i = 0; i < snes_data_packs_count(shared.get()); ++i) {
      const auto *entry = snes_data_packs_get(shared.get(), i);
      const char *root = snes_data_pack_directory(shared.get(), i, cache.c_str(), report, nullptr);
      if (!root) continue;
      try {
        std::vector<Sound> audio;
        std::string prim;
        auto pack = parse(fs::path(reinterpret_cast<const char8_t *>(root)), *entry, audio, prim);
        candidates.push_back(std::move(pack));
        audios.push_back(std::move(audio));
        primaries.push_back(prim);
      } catch (const std::exception &e) {
        report(nullptr, entry->source, e.what());
      }
    }
    std::vector<unsigned> order;
    for (unsigned i = 0; i < candidates.size(); ++i)
      order.push_back(i);
    std::stable_sort(order.begin(), order.end(), [&](unsigned a, unsigned b) {
      return candidates[a].order < candidates[b].order;
    });
    for (auto i : order) {
      auto &p = candidates[i];
      char error[256];
      if (p.info.track_count &&
          !cp_catalog_add(cat, &p.info, error, sizeof(error))) {
        FzeroTracksReport(error);
        continue;
      }
      for (auto &t : p.titles)
        if (!FzeroTitleRegister(t.id.c_str(), t.name.c_str(),
                                t.file.string().c_str(), false))
          FzeroTracksReport(("Conflicting title ID: " + t.id).c_str());
      if (!primaries[i].empty()) {
        if (primary.empty())
          primary = primaries[i];
        else if (primary != primaries[i])
          FzeroTracksReport("Conflicting primary soundtrack declarations");
      }
      sounds.insert(sounds.end(), audios[i].begin(), audios[i].end());
      packs.push_back(std::move(p));
    }
    auto rank = [&](const CpPack *p) {
      for (unsigned i = 0; i < packs.size(); ++i)
        if (!strcmp(p->id, packs[i].info.id))
          return int(i);
      return -1;
    };
    std::stable_sort(
        cat->packs, cat->packs + cat->count,
        [&](const CpPack *a, const CpPack *b) { return rank(a) < rank(b); });
  } catch (const std::exception &e) {
    FzeroTracksReport(e.what());
  }
}
bool FzeroPacksContains(const char *id) {
  for (auto &p : packs)
    if (!strcmp(id, p.info.id))
      return true;
  return false;
}
bool FzeroPacksLoadCourse(const char *id, unsigned index, FzeroCourse *out,
                          char *error, size_t cap) {
  for (auto &p : packs)
    if (!strcmp(id, p.info.id) && index < p.courses.size()) {
      *out = p.decoded[index];
      return true;
    }
  snprintf(error, cap, "Course not found in installed pack");
  return false;
}
bool FzeroPacksCourseMusic(const char *id, unsigned index, char *out, size_t cap) {
  try {
    for (auto &pack : packs)
      if (!strcmp(pack.info.id, id) && index < pack.courses.size()) {
        auto file = courseMusic(pack, index);
        auto path = file.string();
        if (!fs::is_regular_file(file) || path.size() >= cap)
          return false;
        strcpy(out, path.c_str());
        return true;
      }
  } catch (const std::exception &) {
  }
  return false;
}
bool FzeroPacksMusicSources(FzeroMusicSources *out) {
  FzeroMusicSources s{};
  if (primary.size() >= sizeof(s.primary))
    return false;
  strcpy(s.primary, primary.c_str());
  for (auto &a : sounds) {
    bool found = false;
    for (unsigned i = 0; i < s.count; ++i)
      if (lower(s.sources[i].prefix) == lower(a.prefix)) {
        if (strcmp(s.sources[i].id, a.id.c_str()))
          return false;
        found = true;
        break;
      }
    if (found)
      continue;
    if (s.count == 64)
      return false;
    strcpy(s.sources[s.count].id, a.id.c_str());
    strcpy(s.sources[s.count++].prefix, a.prefix.c_str());
  }
  *out = s;
  return true;
}
bool FzeroPacksResolveMusic(const char *source, unsigned track, char *out,
                            size_t cap) {
  try {

    if (!source)
      return false;
    std::string wanted = *source ? source : primary;
    std::set<std::string> matches;
    // Known prefixes may coexist in any installed soundtrack directory. This
    // supports authors putting Astra and CGP files together without renaming.
    for (auto &s : sounds)
      if (s.id == wanted)
        for (auto &location : sounds) {
          auto p =
              location.root / (s.prefix + "-" + std::to_string(track) + ".pcm");
          if (fs::is_regular_file(p))
            matches.insert(p.string());
        }
    if (matches.size() != 1)
      return false;
    auto &p = *matches.begin();
    if (p.size() >= cap)
      return false;
    strcpy(out, p.c_str());
    return true;
  } catch (const std::exception &) {
    return false;
  }
}

bool FzeroPacksHasMusic(void) {
  try {
    for (auto &pack : packs)
      for (auto &[cue, path] : pack.menu_music)
        if (fs::is_regular_file(path)) return true;
    for (auto &pack : packs)
      for (unsigned i = 0; i < pack.courses.size(); ++i)
        if (fs::is_regular_file(courseMusic(pack, i)))
          return true;
    for (auto &s : sounds)
      if (fs::is_directory(s.root))
        for (auto &f : fs::directory_iterator(s.root))
          if (f.is_regular_file() && f.path().extension() == ".pcm" &&
              f.path().filename().string().starts_with(s.prefix + "-"))
            return true;
  } catch (const fs::filesystem_error &) {
  }
  return false;
}

const char *FzeroPacksMenuMusicId(unsigned index) {
  for (auto &pack : packs)
    if (!pack.menu_music.empty() && index-- == 0) return pack.info.id;
  return nullptr;
}
const char *FzeroPacksMenuMusicName(const char *id) {
  if (!id) return nullptr;
  for (auto &pack : packs)
    if (!pack.menu_music.empty() && id == std::string(pack.info.id)) return pack.info.name;
  return nullptr;
}
bool FzeroPacksMenuMusic(const char *id, const char *cue, char *out, size_t cap) {
  if (!id || !cue || !out || !cap) return false;
  out[0] = 0;
  std::string wanted = *id ? id : primary;
  if (!*id && !FzeroPacksMenuMusicName(wanted.c_str())) {
    const char *first = FzeroPacksMenuMusicId(0);
    wanted = first ? first : "";
  }
  for (auto &pack : packs) {
    if (wanted != pack.info.id) continue;
    auto found = pack.menu_music.find(cue);
    if (found == pack.menu_music.end()) return false;
    auto path = found->second.string();
    if (path.size() >= cap) return false;
    strcpy(out, path.c_str());
    return true; // Return the template path even in a download without audio.
  }
  return false;
}
