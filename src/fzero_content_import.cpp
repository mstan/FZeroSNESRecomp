#include "fzero_content_import.h"
#include "data_pack.h"
#include "data_pack_io.hpp"
#include "fzero_packs.h"
#include "sha256.h"
#include <algorithm>
#include <archive.h>
#include <archive_entry.h>
#include <array>
#include <atomic>
#include <chrono>
#include <fstream>
#include <memory>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include <set>
#include <stdexcept>
#include <vector>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace fs = std::filesystem;
using namespace snesrecomp::data_pack;
namespace {
constexpr uint64_t maxFile = 256ull * 1024 * 1024;
constexpr uint64_t maxTotal = 2ull * 1024 * 1024 * 1024;
constexpr const char *baseHash =
    "bf16c3c867c58e2ab061c70de9295b6930d63f29f81cc986f5ecae03e0ad18d2";
void need(bool b, const std::string &s) {
  if (!b)
    throw std::runtime_error(s);
}
std::string lower(std::string s) {
  for (char &c : s)
    c = char(std::tolower(static_cast<unsigned char>(c)));
  return s;
}
bool donorExtension(const std::string &ext) {
  return ext == ".ips" || ext == ".bps" || ext == ".sfc" || ext == ".smc" ||
         ext == ".rom" || ext == ".fig";
}
std::string hash(const std::string &bytes) {
  uint8_t digest[32];
  sha256_compute(reinterpret_cast<const uint8_t *>(bytes.data()), bytes.size(),
                 digest);
  char hex[65];
  for (unsigned i = 0; i < 32; ++i)
    snprintf(hex + i * 2, 3, "%02x", digest[i]);
  return hex;
}
bool allowed(const fs::path &p) {
  static const std::set<std::string> ext = {
      ".fzm",  ".aip", ".tmx", ".tsx", ".png",     ".gif", ".bmp",
      ".json", ".bin", ".fzc", ".fzt", ".fztitle", ".txt", ".md",
      ".pcm",  ".msu", ".zip", ".ips", ".bps",     ".sfc", ".smc"};
  auto suffix = lower(p.extension().string());
  return ext.count(suffix) != 0 || donorExtension(suffix);
}
void safeRelative(const fs::path &p) {
  need(!p.empty() && !p.is_absolute() && !p.has_root_name(),
       "The archive contains an absolute path.");
  need(p.generic_string().size() <= 768 &&
           std::distance(p.begin(), p.end()) <= 32,
       "The project has an excessively long or deeply nested path.");
  for (auto &part : p) {
    auto s = part.string();
    auto stem = lower(s.substr(0, s.find('.')));
    if (s.empty())
      continue; // A normal ZIP directory name ends with '/'.
    need(s != ".." && s != "." && s.find_first_of(":\\<>\"|?*") == s.npos &&
             s.back() != '.' && s.back() != ' ' &&
             std::none_of(s.begin(), s.end(),
                          [](unsigned char c) { return c < 32; }),
         "The archive contains an unsafe path.");
    need(stem != "con" && stem != "prn" && stem != "aux" && stem != "nul" &&
             !(stem.size() == 4 &&
               (stem.starts_with("com") || stem.starts_with("lpt")) &&
               stem[3] >= '0' && stem[3] <= '9'),
         "The archive contains a reserved filename.");
  }
}
bool linked(const fs::path &p) {
#ifdef _WIN32
  DWORD a = GetFileAttributesW(p.c_str());
  return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_REPARSE_POINT);
#else
  return fs::is_symlink(fs::symlink_status(p));
#endif
}
struct Budget {
  uint64_t total = 0;
  unsigned count = 0;
  std::set<std::string> paths;
  void add(const fs::path &relative, uint64_t size) {
    safeRelative(relative);
    need(++count <= 20000 && size <= maxFile && size <= maxTotal - total,
         "This project exceeds the import size limit.");
    total += size;
    need(paths.insert(lower(relative.generic_string())).second,
         "The archive contains duplicate filenames.");
  }
};
struct ZipContents {
  bool donor = false, editor = false;
};
ZipContents unzip(const fs::path &source, const fs::path &dest,
                  bool inspectOnly = false) {
  std::unique_ptr<archive, decltype(&archive_read_free)> ar(archive_read_new(),
                                                            archive_read_free);
  archive_read_support_format_zip(ar.get());
  need(archive_read_open_filename(ar.get(), source.string().c_str(), 65536) ==
           ARCHIVE_OK,
       "Cannot open this ZIP.");
  Budget budget;
  ZipContents contents;
  archive_entry *entry;
  int status;
  while ((status = archive_read_next_header(ar.get(), &entry)) == ARCHIVE_OK) {
    auto name = archive_entry_pathname_utf8(entry);
    need(name != nullptr, "A ZIP filename is not valid UTF-8.");
    auto rel = fs::path(reinterpret_cast<const char8_t *>(name));
    safeRelative(rel);
    need(!archive_entry_symlink(entry) && !archive_entry_hardlink(entry),
         "Linked files cannot be imported.");
    bool hidden = false;
    for (auto &component : rel)
      if (component.string().starts_with('.'))
        hidden = true;
    if (hidden) {
      budget.add(rel, 0);
      archive_read_data_skip(ar.get());
      continue;
    }
    if (archive_entry_filetype(entry) == AE_IFDIR) {
      budget.add(rel, 0);
      if (!inspectOnly)
        fs::create_directories(dest / rel);
      continue;
    }
    need(archive_entry_filetype(entry) == AE_IFREG,
         "Unsupported ZIP entry: " + rel.generic_string());
    auto size = archive_entry_size(entry);
    need(size >= 0, "ZIP entry has an invalid size.");
    budget.add(rel, uint64_t(size));
    auto ext = lower(rel.extension().string());
    contents.donor |= donorExtension(ext);
    contents.editor |= ext == ".fzm" || rel.filename() == "pack.json";
    if (inspectOnly) {
      archive_read_data_skip(ar.get());
      continue;
    }
    need(allowed(rel), "Unsupported file in ZIP: " + rel.generic_string());
    fs::create_directories((dest / rel).parent_path());
    std::ofstream out(dest / rel, std::ios::binary);
    need(bool(out), "Cannot create an imported file.");
    std::array<char, 65536> buffer;
    uint64_t written = 0;
    la_ssize_t got;
    while ((got = archive_read_data(ar.get(), buffer.data(), buffer.size())) >
           0) {
      written += uint64_t(got);
      need(written <= uint64_t(size), "ZIP entry exceeds its declared size.");
      out.write(buffer.data(), got);
      need(bool(out), "Cannot write the imported project.");
    }
    need(got == 0 && written == uint64_t(size),
         "ZIP data is damaged or incomplete.");
  }
  need(status == ARCHIVE_EOF, "The ZIP directory is damaged.");
  return contents;
}
void copyProject(const fs::path &source, const fs::path &dest) {
  need(!linked(source),
       "Choose the actual project folder, not a linked folder.");
  Budget budget;
  for (fs::recursive_directory_iterator it(source), end; it != end; ++it) {
    auto rel = fs::relative(it->path(), source);
    if (it->path().filename().string().starts_with('.')) {
      if (it->is_directory())
        it.disable_recursion_pending();
      continue;
    }
    need(!linked(it->path()), "Project contains a linked file or folder.");
    if (it->is_directory())
      continue;
    need(it->is_regular_file(), "Project contains a special file.");
    if (!allowed(it->path()))
      continue; // Do not bring tools/executables along with editor assets.
    budget.add(rel, it->file_size());
    fs::create_directories((dest / rel).parent_path());
    fs::copy_file(it->path(), dest / rel, fs::copy_options::none);
  }
}
// Bound expansion of per-course archives before the pack decoder opens them.
// They share one budget with the surrounding unpacked files.
void preflight(const fs::path &root) {
  Budget budget;
  for (auto &e : fs::recursive_directory_iterator(root)) {
    if (!e.is_regular_file())
      continue;
    auto rel = fs::relative(e.path(), root);
    budget.add(rel, e.file_size());
    if (lower(e.path().extension().string()) != ".zip")
      continue;
    std::unique_ptr<archive, decltype(&archive_read_free)> ar(
        archive_read_new(), archive_read_free);
    archive_read_support_format_zip(ar.get());
    need(archive_read_open_filename(ar.get(), e.path().string().c_str(),
                                    65536) == ARCHIVE_OK,
         "Cannot open a course ZIP.");
    archive_entry *entry;
    int status;
    while ((status = archive_read_next_header(ar.get(), &entry)) ==
           ARCHIVE_OK) {
      auto name = archive_entry_pathname_utf8(entry);
      need(name != nullptr, "Invalid course ZIP path.");
      auto path = fs::path(reinterpret_cast<const char8_t *>(name));
      safeRelative(path);
      auto size = archive_entry_size(entry);
      need(size >= 0, "Invalid course ZIP size.");
      budget.add(rel / path, uint64_t(size));
      need(!archive_entry_symlink(entry) && !archive_entry_hardlink(entry),
           "Linked course files are not supported.");
      need(archive_entry_filetype(entry) == AE_IFDIR ||
               archive_entry_filetype(entry) == AE_IFREG,
           "Unsupported course ZIP entry.");
      auto ext = lower(path.extension().string());
      need(ext != ".zip" && !donorExtension(ext),
           "A course ZIP must contain editor assets, not another archive, ROM "
           "or patch.");
      archive_read_data_skip(ar.get());
    }
    need(status == ARCHIVE_EOF, "Damaged course ZIP.");
  }
}
rapidjson::Document json(const fs::path &p) {
  auto bytes = read(p);
  rapidjson::Document d;
  d.Parse<rapidjson::kParseIterativeFlag>(bytes.c_str());
  need(!d.HasParseError() && d.IsObject(),
       "Invalid JSON: " + p.filename().string());
  validate_json(d);
  return d;
}
void writeJson(const fs::path &p, const rapidjson::Value &d) {
  rapidjson::StringBuffer b;
  rapidjson::PrettyWriter<rapidjson::StringBuffer> w(b);
  d.Accept(w);
  std::ofstream o(p, std::ios::binary);
  o.write(b.GetString(), b.GetSize());
  need(bool(o), "Cannot write the pack manifest.");
}
// Inspect identities individually: a catalog deliberately drops both members
// of an existing duplicate-ID set, which must not permit a third copy to
// install.
std::string installedId(const fs::path &path) {
  try {
    if (fs::is_directory(path))
      return string(json(path / "pack.json"), "id");
    if (lower(path.extension().string()) != ".zip")
      return {};
    std::unique_ptr<archive, decltype(&archive_read_free)> ar(
        archive_read_new(), archive_read_free);
    archive_read_support_format_zip(ar.get());
    if (archive_read_open_filename(ar.get(), path.string().c_str(), 65536) !=
        ARCHIVE_OK)
      return {};
    archive_entry *entry;
    unsigned count = 0;
    while (++count <= 20000 &&
           archive_read_next_header(ar.get(), &entry) == ARCHIVE_OK) {
      auto name = archive_entry_pathname_utf8(entry);
      if (!name)
        return {};
      auto rel = fs::path(reinterpret_cast<const char8_t *>(name));
      if (rel.filename() != "pack.json" ||
          std::distance(rel.begin(), rel.end()) > 2)
        continue;
      auto size = archive_entry_size(entry);
      if (size < 0 || size > 4 * 1024 * 1024)
        return {};
      std::string bytes(static_cast<size_t>(size), '\0');
      size_t offset = 0;
      while (offset < bytes.size()) {
        auto n = archive_read_data(ar.get(), bytes.data() + offset,
                                   bytes.size() - offset);
        if (n <= 0)
          return {};
        offset += size_t(n);
      }
      rapidjson::Document d;
      d.Parse<rapidjson::kParseIterativeFlag>(bytes.c_str());
      if (d.HasParseError() || !d.IsObject())
        return {};
      validate_json(d);
      return string(d, "id");
    }
  } catch (const std::exception &) {
  }
  return {};
}
void set(rapidjson::Document &d, const char *key, const std::string &s) {
  auto &a = d.GetAllocator();
  rapidjson::Value v(s.c_str(), rapidjson::SizeType(s.size()), a);
  if (d.HasMember(key))
    d[key] = std::move(v);
  else
    d.AddMember(rapidjson::Value(key, a), v, a);
}
void refreshManifest(const fs::path &root, const std::string &title) {
  auto envelope = json(root / "pack.json");
  need(envelope.HasMember("payload") && envelope["payload"].IsObject(),
       "Pack has no payload.");
  auto indexPath = inside(root, string(envelope["payload"], "file"));
  auto index = json(indexPath);
  if (!title.empty()) {
    set(index, "name", title);
    set(envelope, "title", title);
  }
  writeJson(indexPath, index);
  auto digest = hash(read(indexPath));
  auto &a = envelope.GetAllocator();
  envelope["payload"]["sha256"].SetString(digest.c_str(), a);
  writeJson(root / "pack.json", envelope);
}
std::string property(const fs::path &p, const std::string &wanted) {
  std::ifstream in(p);
  std::string line;
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    auto eq = line.find('=');
    if (eq == line.npos)
      continue;
    auto key = line.substr(0, eq);
    key.erase(key.find_last_not_of(" \t") + 1);
    key.erase(0, key.find_first_not_of(" \t"));
    if (key == wanted) {
      auto s = line.substr(eq + 1);
      auto start = s.find_first_not_of(" \t");
      return start == s.npos ? "" : s.substr(start);
    }
  }
  return {};
}
void wrapRaw(const fs::path &root, const fs::path &fzm,
             const std::string &title) {
  auto name = property(fzm, "InGameMapName");
  need(!name.empty(), "FZEdit project has no InGameMapName.");
  std::vector<fs::path> files;
  for (auto &e : fs::recursive_directory_iterator(root))
    if (e.is_regular_file())
      files.push_back(e.path());
  std::sort(files.begin(), files.end());
  std::string signature;
  for (auto &p : files)
    signature += fs::relative(p, root).generic_string() + "\n" +
                 hash(read(p, maxFile)) + "\n";
  auto id = "fzedit-" + hash(signature).substr(0, 24);
  rapidjson::Document d;
  d.Parse(
      R"({"format":1,"id":"","name":"","author":"Imported FZEdit project","cups":[{"id":"course","name":"","courses":["course"]}],"courses":[{"id":"course","name":"","source":""}]})");
  auto &a = d.GetAllocator();
  set(d, "id", id);
  set(d, "name", title.empty() ? name : title);
  d["cups"][0]["name"].SetString(name.c_str(), a);
  d["courses"][0]["name"].SetString(name.c_str(), a);
  auto rel = fs::relative(fzm, root).generic_string();
  d["courses"][0]["source"].SetString(rel.c_str(), a);
  writeJson(root / "courses.json", d);
  rapidjson::Document e;
  e.Parse(
      R"({"format":"snesrecomp.data-pack","version":1,"game":"f-zero","id":"","title":"","base_rom_sha256":"","requires":["fzero-course-v1"],"payload":{"format":"fzero.course-index","file":"courses.json","sha256":""}})");
  set(e, "id", id);
  set(e, "title", title.empty() ? name : title);
  set(e, "base_rom_sha256", baseHash);
  auto digest = hash(read(root / "courses.json"));
  e["payload"]["sha256"].SetString(digest.c_str(), e.GetAllocator());
  writeJson(root / "pack.json", e);
}
#ifdef _WIN32
std::wstring quote(const std::wstring &s) {
  std::wstring out = L"\"";
  unsigned slashes = 0;
  for (wchar_t c : s) {
    if (c == L'\\') {
      ++slashes;
      continue;
    }
    out.append(slashes * (c == L'"' ? 2 : 1), L'\\');
    slashes = 0;
    if (c == L'"')
      out += L'\\';
    out += c;
  }
  out.append(slashes * 2, L'\\');
  return out + L'"';
}
#endif
void convert(const fs::path &source, const fs::path &stock,
             const fs::path &helpers, const fs::path &out,
             const fs::path &reports) {
  auto exe = helpers / "FZeroConvertContent.exe";
  need(fs::is_regular_file(exe),
       "Patch conversion tools are not installed in this build. See "
       "mods/CONVERSION.md for the conversion steps.");
  need(fs::is_regular_file(stock),
       "Select your original F-Zero game file on the main page first, then "
       "import the patch.");
#ifdef _WIN32
  std::wstring cmd = quote(exe.wstring());
  for (auto &arg : std::vector<std::wstring>{
           source.wstring(), L"--stock", stock.wstring(), L"--out",
           out.wstring(), L"--exporter",
           (helpers / "FZeroExportCourses.exe").wstring(), L"--inspector",
           (helpers / "FZeroInspectPacks.exe").wstring()})
    cmd += L" " + quote(arg);
  auto log = out.parent_path() / "conversion.log";
  SECURITY_ATTRIBUTES security{sizeof(security), nullptr, TRUE};
  HANDLE output =
      CreateFileW(log.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &security,
                  CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  need(output != INVALID_HANDLE_VALUE, "Cannot create conversion log.");
  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  startup.dwFlags = STARTF_USESTDHANDLES;
  startup.hStdOutput = output;
  startup.hStdError = output;
  PROCESS_INFORMATION process{};
  BOOL started = CreateProcessW(exe.c_str(), cmd.data(), nullptr, nullptr, TRUE,
                                CREATE_NO_WINDOW, nullptr, helpers.c_str(),
                                &startup, &process);
  CloseHandle(output);
  need(started != 0, "Cannot start the conversion tool.");
  CloseHandle(process.hThread);
  DWORD waited = WaitForSingleObject(process.hProcess, 10 * 60 * 1000);
  DWORD code = 1;
  if (waited == WAIT_TIMEOUT)
    TerminateProcess(process.hProcess, 1);
  GetExitCodeProcess(process.hProcess, &code);
  CloseHandle(process.hProcess);
  if (code != 0) {
    std::string message =
        code == 2
            ? "This hack is not a reviewed conversion yet. No courses were "
              "installed."
            : "The patch could not be converted. No courses were installed.";
    auto report = out / "conversion-report.json";
    fs::create_directories(reports);
    if (fs::is_regular_file(log))
      fs::copy_file(log, reports / "conversion.log");
    if (fs::is_regular_file(report)) {
      auto d = json(report);
      for (const char *field : {"message", "reason"})
        if (d.HasMember(field) && d[field].IsString())
          message += "\n" + std::string(d[field].GetString());
      fs::copy_file(report, reports / "conversion-report.json");
      if (fs::is_regular_file(out / "REVIEW.txt"))
        fs::copy_file(out / "REVIEW.txt", reports / "REVIEW.txt");
    } else if (fs::is_regular_file(log)) {
      auto bytes = read(log, 1024 * 1024);
      message += "\n" + bytes.substr(0, 400);
    }
    message += "\nDetails: " + reports.string() +
               "\nSee mods/CONVERSION.md for the next steps.";
    throw std::runtime_error(message);
  }
#else
  (void)source;
  (void)out;
  (void)reports;
  throw std::runtime_error(
      "Patch conversion in this spike is available on Windows. See "
      "mods/CONVERSION.md for the command-line workflow.");
#endif
}
struct Stage {
  fs::path path;
  explicit Stage(fs::path p) : path(std::move(p)) {
    need(fs::create_directory(path), "Cannot create the import workspace.");
  }
  ~Stage() {
    std::error_code ec;
    fs::remove_all(path, ec);
  }
};
} // namespace
bool FzeroContentIsBundled(const fs::path &mods, const std::string &id) {
  auto path = mods / ".bundled-packs.json";
  if (!fs::exists(path))
    return false;
  auto index = json(path);
  need(index.HasMember("packs") && index["packs"].IsArray(),
       "The included-pack index is damaged. Restore it from your download.");
  for (const auto &pack : index["packs"].GetArray())
    if (pack.IsObject() && string(pack, "id") == id)
      return true;
  return false;
}
FzeroContentImportResult FzeroContentImport(const fs::path &input,
                                            const fs::path &modsInput,
                                            const fs::path &stockInput,
                                            const fs::path &helpersInput,
                                            const std::string &displayName) {
  need(displayName.size() < 96 &&
           std::none_of(displayName.begin(), displayName.end(),
                        [](unsigned char c) { return c < 32; }),
       "Choose a name shorter than 96 bytes, without line breaks.");
  need(!linked(input), "Choose the original file, not a linked file.");
  auto source = fs::canonical(input), mods = fs::absolute(modsInput),
       helpers = fs::absolute(helpersInput);
  auto stock = stockInput.empty() ? fs::path() : fs::absolute(stockInput);
  need(!linked(source), "Choose the original file, not a linked file.");
  fs::create_directories(mods / ".imports");
  need(!linked(mods) && !linked(mods / ".imports"),
       "The mods folder must not be a linked folder.");
  static std::atomic<unsigned> sequence{0};
  auto key = std::to_string(
                 std::chrono::steady_clock::now().time_since_epoch().count()) +
             "-" + std::to_string(sequence++);
  Stage stage{mods / ".imports" / key};
  // Nested editor source paths otherwise exceed Windows' path limit when
  // the game itself sits in a deep build or Downloads directory.
  Stage cache{fs::temp_directory_path() / ("fzimport-" + key)};
  auto unpacked = stage.path / "unpacked";
  fs::create_directory(unpacked);
  auto extension = lower(source.extension().string());
  bool patch = donorExtension(extension);
  auto reports = mods / "import-reports" / key;
  if (patch) {
    convert(source, stock, helpers, unpacked / "converted", reports);
  } else if (fs::is_directory(source))
    copyProject(source, unpacked);
  else if (extension == ".fzm")
    copyProject(source.parent_path(), unpacked);
  else if (extension == ".zip") {
    // Identify the submission before extracting potentially large recordings.
    // The converter reads just the selected donor and validated assets; extra
    // documentation or tools in patch downloads are never run or installed.
    auto contents = unzip(source, {}, true);
    if (contents.donor && !contents.editor)
      convert(source, stock, helpers, unpacked / "converted", reports);
    else
      unzip(source, unpacked);
  } else
    throw std::runtime_error(
        "Choose a FZEdit project, ZIP, IPS/BPS patch, or SNES ROM hack.");
  std::vector<fs::path> manifests, projects, donors;
  bool audio = false;
  for (auto &e : fs::recursive_directory_iterator(unpacked))
    if (e.is_regular_file()) {
      auto ext = lower(e.path().extension().string());
      if (e.path().filename() == "pack.json")
        manifests.push_back(e.path());
      if (ext == ".fzm")
        projects.push_back(e.path());
      if (donorExtension(ext))
        donors.push_back(e.path());
      if (ext == ".pcm" || ext == ".msu")
        audio = true;
    }
  if (manifests.empty() && projects.empty() && !donors.empty()) {
    need(extension == ".zip" || donors.size() == 1,
         "Choose one patch or ROM hack, or its ZIP download.");
    auto converted = stage.path / "converted";
    convert(extension == ".zip" ? source : donors.front(), stock, helpers,
            converted, reports);
    manifests.push_back(converted / "pack.json");
  }
  auto catalog = stage.path / "catalog";
  fs::create_directory(catalog);
  auto candidate = catalog / "candidate";
  if (!manifests.empty()) {
    need(manifests.size() == 1,
         "This ZIP contains several packs. Import each pack separately.");
    fs::rename(manifests.front().parent_path(), candidate);
    preflight(candidate);
    // Verify original hashes before an optional display-name change.
    CpPack original{};
    char error[2048]{};
    bool valid = FzeroPacksValidateImport(catalog.string().c_str(),
                                          cache.path.string().c_str(),
                                          &original, error, sizeof(error));
    need(valid, error);
    refreshManifest(candidate, displayName);
  } else {
    need(!projects.empty() || !audio,
         "This ZIP contains music but no courses or patch. Add recordings to "
         "the matching pack's music folder, using its course filenames. See "
         "mods/CONVERSION.md for music mapping.");
    need(projects.size() == 1,
         "Choose a ZIP or folder with one FZEdit project, or a pack that "
         "includes pack.json. Multiple projects need explicit league/order "
         "metadata; see mods/CONVERSION.md.");
    auto relative = fs::relative(projects.front(), unpacked);
    fs::rename(unpacked, candidate);
    wrapRaw(candidate, candidate / relative, displayName);
    preflight(candidate);
  }
  CpPack info{};
  char error[2048]{};
  bool valid = FzeroPacksValidateImport(catalog.string().c_str(),
                                        cache.path.string().c_str(), &info,
                                        error, sizeof(error));
  need(valid, error);
  need(std::string(info.id) != "retail" && std::string(info.id) != "bs-deluxe",
       "This pack uses a reserved built-in game ID. Its author must choose a "
       "unique pack ID.");
  need(!FzeroContentIsBundled(mods, info.id),
       "This pack is included with F-Zero Forever. Import cannot replace an "
       "included pack.");
  for (auto &e : fs::recursive_directory_iterator(candidate))
    if (e.is_regular_file()) {
      auto ext = lower(e.path().extension().string());
      need(!donorExtension(ext),
           "The content includes ROM or patch files alongside its assets. "
           "Import that patch directly, or remove those files from the editor "
           "project.");
    }
  auto installed = mods / "packs";
  fs::create_directories(installed);
  need(!linked(installed), "The packs folder must not be a linked folder.");
  for (auto &e : fs::directory_iterator(installed)) {
    if (e.path().filename().string().starts_with('.'))
      continue;
    need(installedId(e.path()) != info.id,
         "This pack is already installed. Importing will not overwrite it.");
  }
  // The shared transport also recognizes installed ZIPs and rejects ID
  // collisions.
  uint8_t base[32];
  cp_hash_parse(baseHash, base);
  const char *caps[] = {"fzero-course-v1"};
  std::unique_ptr<SnesDataPacks, decltype(&snes_data_packs_destroy)> existing(
      snes_data_packs_scan(installed.string().c_str(), "f-zero",
                           "fzero.course-index", base, caps, 1, nullptr,
                           nullptr),
      snes_data_packs_destroy);
  need(snes_data_packs_count(existing.get()) < CP_PACKS - 2,
       "The installed pack limit has been reached. Remove an unused pack "
       "before importing another.");
  for (size_t i = 0; i < snes_data_packs_count(existing.get()); ++i)
    need(std::string(snes_data_packs_get(existing.get(), i)->id) != info.id,
         "This pack is already installed as a folder or ZIP.");
  auto dest = installed / info.id;
  need(!fs::exists(dest),
       "A folder already exists for this pack. Nothing was overwritten.");
  std::string warnings;
  if (fs::is_regular_file(candidate / "conversion-report.json")) {
    auto report = json(candidate / "conversion-report.json");
    if (report.HasMember("warnings") && report["warnings"].IsArray())
      for (auto &warning : report["warnings"].GetArray())
        if (warning.IsString() && warnings.size() < 768) {
          if (!warnings.empty())
            warnings += "\n";
          warnings += std::string(warning.GetString()).substr(0, 768 - warnings.size());
        }
  }
  fs::rename(candidate, dest);
  return {info.id, info.name, dest, info.track_count, manifests.empty(), warnings};
}
