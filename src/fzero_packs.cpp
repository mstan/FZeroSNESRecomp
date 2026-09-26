// Game-owned pack discovery. Neither filenames nor pack IDs select engine
// behavior.
#include "fzero_packs.h"
#include "fzero_fzedit.h"
extern "C" {
#include "fzero_course_file.h"
#include "fzero_title.h"
#include "fzero_tracks.h"
#include "sha256.h"
}
#include <algorithm>
#include <archive.h>
#include <archive_entry.h>
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
void validateJson(const Value &v, unsigned depth = 0) {
  if (depth > 64)
    throw std::runtime_error("JSON nesting exceeds limit");
  if (v.IsObject()) {
    std::set<std::string> keys;
    for (auto i = v.MemberBegin(); i != v.MemberEnd(); ++i) {
      std::string key(i->name.GetString(), i->name.GetStringLength());
      if (key.find('\0') != std::string::npos || !keys.insert(key).second)
        throw std::runtime_error("Duplicate or invalid JSON key");
      validateJson(i->value, depth + 1);
    }
  } else if (v.IsArray())
    for (auto &item : v.GetArray())
      validateJson(item, depth + 1);
}
void require(bool yes, const std::string &message) {
  if (!yes)
    throw std::runtime_error(message);
}
std::string read(const fs::path &p, size_t limit = 4 * 1024 * 1024) {
  require(fs::is_regular_file(p) && fs::file_size(p) <= limit,
          "Missing or oversized file: " + p.string());
  std::ifstream f(p, std::ios::binary);
  require(bool(f), "Cannot read " + p.string());
  return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}
std::string str(const Value &v, const char *key) {
  require(v.IsObject() && v.HasMember(key) && v[key].IsString(),
          std::string("Missing string: ") + key);
  std::string s(v[key].GetString(), v[key].GetStringLength());
  require(s.find('\0') == std::string::npos, "NUL in string");
  return s;
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
fs::path inside(const fs::path &root, const std::string &relative) {
  require(!relative.empty() && relative.find('\\') == std::string::npos &&
              relative.find(':') == std::string::npos,
          "Expected relative pack path");
  fs::path p(reinterpret_cast<const char8_t *>(relative.c_str()));
  require(!p.is_absolute(), "Absolute pack path");
  for (const auto &part : p)
    require(part != "..", "Pack path escapes its directory");
  auto canonical = fs::weakly_canonical(root / p),
       base = fs::weakly_canonical(root);
  auto rel = canonical.lexically_relative(base);
  require(!rel.empty() && *rel.begin() != "..",
          "Pack symlink escapes its directory");
  return canonical;
}
std::string contentKey(const fs::path &path) {
  // Bounded streaming digest chain, used only as a disposable cache key.
  std::ifstream in(path, std::ios::binary);
  require(bool(in), "Cannot hash pack archive");
  std::vector<uint8_t> buffer(65536 + 32, 0);
  uint8_t digest[32] = {0};
  while (in) {
    in.read(reinterpret_cast<char *>(buffer.data() + 32), 65536);
    auto n = in.gcount();
    if (n > 0) {
      memcpy(buffer.data(), digest, 32);
      sha256_compute(buffer.data(), size_t(n) + 32, digest);
    }
  }
  require(in.eof(), "Cannot read pack archive");
  char hex[65];
  for (unsigned i = 0; i < 32; ++i)
    snprintf(hex + i * 2, 3, "%02x", digest[i]);
  return hex;
}
fs::path unpack(const fs::path &zip, const fs::path &cache) {
  // Marker lives outside the archive-controlled tree. Reuse only a fully
  // extracted content-addressed cache; interrupted extraction is retried.
  auto ready = cache;
  ready += ".ready";
  if (!fs::is_regular_file(ready)) {
    fs::create_directories(cache);
    struct archive *a = archive_read_new();
    archive_read_support_format_zip(a);
    archive_read_support_filter_none(a);
    if (archive_read_open_filename(a, zip.string().c_str(), 65536) !=
        ARCHIVE_OK) {
      archive_read_free(a);
      throw std::runtime_error("Cannot open ZIP");
    }
    uint64_t total = 0;
    unsigned files = 0;
    struct archive_entry *entry = nullptr;
    std::set<std::string> names;
    try {
      int status;
      while ((status = archive_read_next_header(a, &entry)) == ARCHIVE_OK) {
        require(++files <= 20000, "ZIP contains too many entries");
        require(!archive_entry_symlink(entry) && !archive_entry_hardlink(entry),
                "ZIP links are not supported");
        const char *name = archive_entry_pathname(entry);
        require(name != nullptr, "Invalid ZIP name");
        std::string normalized = name;
        std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                       [](unsigned char c) { return char(std::tolower(c)); });
        require(names.insert(normalized).second, "Duplicate ZIP entry");
        auto target = inside(cache, name);
        auto type = archive_entry_filetype(entry);
        if (type == AE_IFDIR) {
          fs::create_directories(target);
          continue;
        }
        require(type == AE_IFREG, "Unsupported ZIP entry type");
        auto size = archive_entry_size(entry);
        require(size >= 0 && size <= INT64_C(2147483648) &&
                    total + size <= UINT64_C(8589934592),
                "ZIP exceeds pack size limits");
        total += size;
        fs::create_directories(target.parent_path());
        std::ofstream out(target, std::ios::binary | std::ios::trunc);
        require(bool(out), "Cannot write pack cache");
        char data[65536];
        int64_t actual = 0;
        la_ssize_t n;
        while ((n = archive_read_data(a, data, sizeof(data))) > 0) {
          actual += n;
          require(actual <= size, "ZIP entry exceeds declared size");
          out.write(data, n);
          require(bool(out), "Pack cache write failed");
        }
        require(n == 0 && actual == size, "Damaged ZIP entry");
      }
      require(status == ARCHIVE_EOF, "Invalid ZIP directory");
    } catch (...) {
      archive_read_free(a);
      throw;
    }
    archive_read_free(a);
    std::ofstream marker(ready, std::ios::binary | std::ios::trunc);
    marker << "1\n";
    require(bool(marker), "Cannot complete ZIP cache");
  }
  if (fs::exists(cache / "pack.json"))
    return cache;
  std::vector<fs::path> roots;
  for (auto &f : fs::directory_iterator(cache))
    if (f.is_directory() && fs::exists(f.path() / "pack.json"))
      roots.push_back(f.path());
  require(roots.size() == 1,
          "ZIP needs one pack.json at its root or one enclosing directory");
  return roots[0];
}
Pack parse(const fs::path &root, std::vector<Sound> &audio,
           std::string &newPrimary) {
  rapidjson::Document d;
  auto text = read(root / "pack.json");
  d.Parse<rapidjson::kParseIterativeFlag |
          rapidjson::kParseValidateEncodingFlag>(text.data(), text.size());
  require(!d.HasParseError() && d.IsObject(), "Invalid pack.json");
  validateJson(d);
  require(d.HasMember("format") && d["format"].IsInt() &&
              d["format"].GetInt() == 1,
          "Unsupported pack format");
  Pack p;
  p.root = root;
  auto id = str(d, "id");
  ident(id);
  copy(p.info.id, id);
  copy(p.info.name, str(d, "name"));
  copy(p.info.author, str(d, "author"));
  copy(p.info.adapter, "fzero-course-v1");
  if (d.HasMember("order")) {
    require(d["order"].IsInt(), "order must be an integer");
    p.order = d["order"].GetInt();
  }
  if (d.HasMember("titles")) {
    require(d["titles"].IsArray(), "Invalid title list");
    for (auto &t : d["titles"].GetArray()) {
      auto tid = str(t, "id");
      ident(tid);
      auto file = inside(root, str(t, "source"));
      auto resource = read(file);
      require(resource.size() == 0x1489 &&
                  !memcmp(resource.data(), "FZTITLE\1\0", 9),
              "Invalid title resource");
      p.titles.push_back({tid, str(t, "name"), file});
    }
  }
  if (d.HasMember("soundtracks")) {
    require(d["soundtracks"].IsArray(), "soundtracks must be an array");
    for (auto &s : d["soundtracks"].GetArray()) {
      auto sid = str(s, "id");
      ident(sid);
      auto prefix = str(s, "prefix");
      require(!prefix.empty() && prefix.size() < 96 &&
                  prefix.find_first_of("/\\:") == std::string::npos,
              "Invalid PCM prefix");
      auto dir = inside(root, str(s, "directory"));
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
  if (!d.HasMember("courses")) {
    require(!audio.empty(), "Pack has no courses or audio");
    return p;
  }
  require(d["courses"].IsArray() && d["courses"].Size() <= CP_TRACKS,
          "Invalid course list");
  require(d.HasMember("cups") && d["cups"].IsArray() &&
              d["cups"].Size() <= CP_CUPS,
          "Invalid cups");
  std::map<std::string, const Value *> definitions;
  for (auto &c : d["courses"].GetArray()) {
    auto cid = str(c, "id");
    ident(cid);
    require(definitions.emplace(cid, &c).second, "Duplicate course ID");
  }
  std::set<std::string> cupids;
  for (auto &cup : d["cups"].GetArray()) {
    auto cid = str(cup, "id");
    ident(cid);
    require(cupids.insert(cid).second, "Duplicate cup ID");
    auto &dest = p.info.cups[p.info.cup_count++];
    copy(dest.id, cid);
    copy(dest.name, str(cup, "name"));
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
      copy(t.id, str(c, "id"));
      copy(t.name, str(c, "name"));
      copy(t.cup, cid);
      t.slot = (unsigned)p.courses.size();
      p.courses.push_back(inside(root, str(c, "source")));
      FzeroCourse decoded{};
      char error[256] = {0};
      auto source = p.courses.back();
      bool ok = false;
      if (source.extension() == ".fzc")
        ok = FzeroCourseFileRead(source.string().c_str(), &decoded, error,
                                 sizeof(error));
      else if (source.extension() == ".fzm")
        ok = FzeroFzeditRead(root.string().c_str(), source.string().c_str(),
                             &decoded, error, sizeof(error));
      else
        snprintf(error, sizeof(error), "Expected .fzm or .fzc source");
      require(ok, str(c, "id") + ": " + error);
      if (c.HasMember("requires")) {
        require(c["requires"].IsArray(), "requires must be an array");
        decoded.required = 0;
        for (auto &feature : c["requires"].GetArray()) {
          require(feature.IsString(), "Invalid required feature");
          std::string name = feature.GetString();
          if (name == "grip-magnets")
            decoded.required |= FZERO_COURSE_GRIP_MAGNETS;
          else if (name == "up-magnets")
            decoded.required |= FZERO_COURSE_UP_MAGNETS;
          else if (name == "rainbow-road")
            decoded.required |= FZERO_COURSE_RAINBOW;
          else
            require(false, "Unsupported required mechanic: " + name);
        }
      }
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
          auto id = str(m, "soundtrack");
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
  std::map<std::string, unsigned> ids;
  std::vector<std::vector<Sound>> audios;
  std::vector<std::string> primaries;
  try {
    fs::path dir(reinterpret_cast<const char8_t *>(directory));
    if (!fs::exists(dir))
      return;
    std::vector<fs::path> entries;
    for (auto &f : fs::directory_iterator(dir))
      entries.push_back(f.path());
    std::sort(entries.begin(), entries.end());
    for (auto &entry : entries) {
      if (entry.filename().string().starts_with('.'))
        continue;
      if (!(fs::is_directory(entry) && fs::exists(entry / "pack.json")) &&
          entry.extension() != ".zip")
        continue;
      try {
        auto root = entry;
        if (entry.extension() == ".zip") {
          auto key = contentKey(entry);
          root = unpack(entry, dir / ".cache" / key);
        }
        std::vector<Sound> audio;
        std::string prim;
        auto pack = parse(root, audio, prim);
        ++ids[pack.info.id];
        candidates.push_back(std::move(pack));
        audios.push_back(std::move(audio));
        primaries.push_back(prim);
      } catch (const std::exception &e) {
        FzeroTracksReport(
            (entry.filename().string() + ": " + e.what()).c_str());
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
      if (ids[p.info.id] != 1) {
        FzeroTracksReport(
            (std::string("Duplicate pack ID: ") + p.info.id).c_str());
        continue;
      }
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
