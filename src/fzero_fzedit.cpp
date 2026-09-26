// FZEdit v1.2 exported file decoder. Only data formats are accepted, never ASM.
#include "fzero_fzedit.h"
extern "C" {
#include "fzero_course_file.h"
#include "sha256.h"
}
#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <gif_lib.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <map>
#include <png.h>
#include <sstream>
#include <stdexcept>
#include <vector>
namespace fs = std::filesystem;
namespace {
void check(bool b, const std::string &s) {
  if (!b)
    throw std::runtime_error(s);
}
void word(uint8_t *p, unsigned v) {
  p[0] = uint8_t(v);
  p[1] = uint8_t(v >> 8);
}
unsigned u16(const uint8_t *p) { return p[0] | unsigned(p[1]) << 8; }
unsigned u32(const uint8_t *p) { return u16(p) | u16(p + 2) << 16; }
std::string text(const fs::path &p) {
  check(fs::file_size(p) < 16 * 1024 * 1024, "Oversized editor source");
  std::ifstream f(p, std::ios::binary);
  check(bool(f), "Cannot read " + p.string());
  return {std::istreambuf_iterator<char>(f), {}};
}
std::string trim(std::string s) {
  auto a = s.find_first_not_of(" \t\r\n");
  if (a == s.npos)
    return {};
  return s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
}
std::map<std::string, std::string> props(const std::string &s,
                                         char separator = '=') {
  std::map<std::string, std::string> m;
  std::istringstream in(s);
  std::string line;
  while (std::getline(in, line)) {
    line = trim(line);
    if (line.empty())
      continue;
    auto n = line.find(separator);
    check(n != line.npos, "Invalid property line");
    check(m.emplace(line.substr(0, n), line.substr(n + 1)).second,
          "Duplicate property");
  }
  return m;
}
unsigned number(const std::string &s, unsigned max = 65535) {
  size_t end = 0;
  auto n = std::stoul(trim(s), &end, 10);
  check(end == trim(s).size() && n <= max, "Numeric value out of range");
  return unsigned(n);
}
std::vector<unsigned> csv(const std::string &s, unsigned max = 65535) {
  std::vector<unsigned> v;
  std::istringstream in(s);
  std::string a;
  while (std::getline(in, a, ',')) {
    if (!trim(a).empty())
      v.push_back(number(a, max));
  }
  return v;
}
struct Files {
  fs::path root, dir;
  fs::path get(std::string name) const {
    std::replace(name.begin(), name.end(), '\\', '/');
    fs::path rel(name);
    check(!rel.is_absolute() && name.find(':') == name.npos,
          "Absolute component path");
    auto p = fs::weakly_canonical(dir / rel);
    auto r = p.lexically_relative(root);
    check(!r.empty() && *r.begin() != "..", "Component escapes pack");
    return p;
  }
};
std::string attr(xmlNode *n, const char *k) {
  xmlChar *p = xmlGetProp(n, BAD_CAST k);
  std::string s = p ? reinterpret_cast<char *>(p) : "";
  xmlFree(p);
  return s;
}
bool tag(xmlNode *n, const char *s) {
  return n->type == XML_ELEMENT_NODE && !xmlStrcmp(n->name, BAD_CAST s);
}
struct Xml {
  xmlDocPtr doc;
  explicit Xml(const fs::path &p) {
    auto s = text(p);
    doc = xmlReadMemory(s.data(), int(s.size()), p.string().c_str(), nullptr,
                        XML_PARSE_NONET);
    check(doc != nullptr, "Invalid Tiled XML");
    if (doc->intSubset || doc->extSubset) {
      xmlFreeDoc(doc);
      doc = nullptr;
      check(false, "XML DTDs are not supported");
    }
  }
  ~Xml() {
    if (doc)
      xmlFreeDoc(doc);
  }
  xmlNode *root() { return xmlDocGetRootElement(doc); }
};
struct Image {
  unsigned w = 0, h = 0;
  std::vector<uint8_t> indices;
  std::vector<std::array<uint8_t, 3>> colors;
  std::vector<std::array<uint8_t, 3>> rgb;
};
Image image(const fs::path &p) {
  Image im;
  auto bytes = text(p);
  const auto *b = reinterpret_cast<const uint8_t *>(bytes.data());
  if (bytes.starts_with("GIF")) {
    int error = 0;
    GifFileType *g = DGifOpenFileName(p.string().c_str(), &error);
    check(g != nullptr, "Cannot decode GIF");
    try {
      check(g->SWidth > 0 && g->SWidth <= 512 && g->SHeight > 0 &&
                g->SHeight <= 512,
            "GIF dimensions exceed limit");
      GifRecordType record;
      bool decoded = false;
      do {
        check(DGifGetRecordType(g, &record) == GIF_OK, "Invalid GIF record");
        if (record == IMAGE_DESC_RECORD_TYPE) {
          check(!decoded && DGifGetImageDesc(g) == GIF_OK,
                "Expected one GIF frame");
          decoded = true;
          im.w = g->Image.Width;
          im.h = g->Image.Height;
          check(im.w && im.h && im.w <= 512 && im.h <= 512,
                "Invalid GIF frame dimensions");
          auto cmap = g->Image.ColorMap ? g->Image.ColorMap : g->SColorMap;
          check(cmap != nullptr, "Missing GIF palette");
          for (int i = 0; i < cmap->ColorCount; ++i) {
            auto c = cmap->Colors[i];
            im.colors.push_back({c.Red, c.Green, c.Blue});
          }
          im.indices.resize(im.w * im.h);
          if (g->Image.Interlace) {
            const unsigned starts[] = {0, 4, 2, 1}, steps[] = {8, 8, 4, 2};
            for (unsigned pass = 0; pass < 4; ++pass)
              for (unsigned y = starts[pass]; y < im.h; y += steps[pass])
                check(DGifGetLine(g, im.indices.data() + y * im.w, int(im.w)) ==
                          GIF_OK,
                      "Truncated GIF");
          } else
            for (unsigned y = 0; y < im.h; ++y)
              check(DGifGetLine(g, im.indices.data() + y * im.w, int(im.w)) ==
                        GIF_OK,
                    "Truncated GIF");
        } else if (record == EXTENSION_RECORD_TYPE) {
          int kind;
          GifByteType *block = nullptr;
          check(DGifGetExtension(g, &kind, &block) == GIF_OK,
                "Invalid GIF extension");
          while (block)
            check(DGifGetExtensionNext(g, &block) == GIF_OK,
                  "Invalid GIF extension block");
        }
      } while (record != TERMINATE_RECORD_TYPE);
      check(decoded, "GIF has no image");
    } catch (...) {
      DGifCloseFile(g, &error);
      throw;
    }
    DGifCloseFile(g, &error);
  } else if (bytes.starts_with("BM")) {
    check(bytes.size() >= 54, "Truncated BMP");
    unsigned off = u32(b + 10), header = u32(b + 14), bits = u16(b + 28);
    im.w = u32(b + 18);
    int height = int(u32(b + 22));
    im.h = height < 0 ? unsigned(-int64_t(height)) : unsigned(height);
    check(header >= 40 && uint64_t(header) + 14 <= bytes.size() &&
              off >= uint64_t(header) + 14 && im.w <= 512 && im.h <= 512 &&
              im.w && im.h && u32(b + 30) == 0 &&
              (bits == 8 || bits == 24 || bits == 32),
          "Unsupported BMP encoding");
    unsigned stride = ((im.w * bits + 31) / 32) * 4;
    check(uint64_t(off) + uint64_t(stride) * im.h <= bytes.size(),
          "Truncated BMP pixels");
    if (bits == 8) {
      unsigned count = u32(b + 46);
      if (!count)
        count = 256;
      check(count <= 256 && uint64_t(14) + header + count * 4 <= off,
            "Invalid BMP palette");
      for (unsigned i = 0; i < count; ++i) {
        auto c = b + 14 + header + i * 4;
        im.colors.push_back({c[2], c[1], c[0]});
      }
    }
    for (unsigned y = 0; y < im.h; ++y)
      for (unsigned x = 0; x < im.w; ++x) {
        auto c =
            b + off + (height > 0 ? im.h - 1 - y : y) * stride + x * (bits / 8);
        if (bits == 8)
          im.indices.push_back(c[0]);
        else
          im.rgb.push_back({c[2], c[1], c[0]});
      }
  } else {
    // Preserve PNG palette indices: the pixel index, not RGB similarity, is
    // game data.
    FILE *f = fopen(p.string().c_str(), "rb");
    check(f != nullptr, "Cannot open PNG");
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr,
                                             nullptr, nullptr);
    png_infop info = png ? png_create_info_struct(png) : nullptr;
    if (!png || !info) {
      if (png)
        png_destroy_read_struct(&png, nullptr, nullptr);
      fclose(f);
      check(false, "PNG allocation failed");
    }
    // Construct C++ storage before setjmp so libpng errors cannot bypass it.
    std::vector<png_bytep> rows;
    if (setjmp(png_jmpbuf(png))) {
      png_destroy_read_struct(&png, &info, nullptr);
      fclose(f);
      check(false, "Invalid PNG");
    }
    png_init_io(png, f);
    png_read_info(png, info);
    im.w = png_get_image_width(png, info);
    im.h = png_get_image_height(png, info);
    if (!im.w || !im.h || im.w > 512 || im.h > 512) {
      png_destroy_read_struct(&png, &info, nullptr);
      fclose(f);
      check(false, "Expected bounded PNG");
    }
    bool indexed = png_get_color_type(png, info) == PNG_COLOR_TYPE_PALETTE;
    if (indexed) {
      png_colorp colors;
      int count = 0;
      png_get_PLTE(png, info, &colors, &count);
      for (int i = 0; i < count; ++i)
        im.colors.push_back({colors[i].red, colors[i].green, colors[i].blue});
      png_set_packing(png);
    } else {
      png_set_strip_16(png);
      png_set_strip_alpha(png);
      png_set_expand_gray_1_2_4_to_8(png);
      png_set_gray_to_rgb(png);
    }
    png_set_interlace_handling(png);
    png_read_update_info(png, info);
    rows.resize(im.h);
    if (indexed) {
      im.indices.resize(im.w * im.h);
      for (unsigned y = 0; y < im.h; ++y)
        rows[y] = im.indices.data() + y * im.w;
    } else {
      im.rgb.resize(im.w * im.h);
      for (unsigned y = 0; y < im.h; ++y)
        rows[y] = im.rgb[y * im.w].data();
    }
    png_read_image(png, rows.data());
    png_read_end(png, nullptr);
    png_destroy_read_struct(&png, &info, nullptr);
    fclose(f);
  }
  if (im.rgb.empty())
    for (auto i : im.indices) {
      check(i < im.colors.size(), "Invalid image palette index");
      im.rgb.push_back(im.colors[i]);
    }
  return im;
}
std::vector<uint32_t> layer(xmlNode *root, const std::string &name, unsigned w,
                            unsigned h) {
  std::vector<uint32_t> result(w * h);
  bool found = false;
  for (auto n = root->children; n; n = n->next)
    if (tag(n, "layer") && (name.empty() || attr(n, "name") == name)) {
      check(number(attr(n, "width")) == w && number(attr(n, "height")) == h,
            "Tiled layer dimensions differ");
      found = true;
      for (auto d = n->children; d; d = d->next)
        if (tag(d, "data")) {
          check(attr(d, "encoding") == "csv" && attr(d, "compression").empty(),
                "Use CSV Tiled layers");
          xmlChar *s = xmlNodeGetContent(d);
          auto values = csv(reinterpret_cast<char *>(s), UINT32_MAX);
          xmlFree(s);
          check(values.size() == result.size(),
                "Tiled layer pixel count differs");
          for (size_t i = 0; i < values.size(); ++i)
            if (values[i])
              result[i] = values[i];
        }
    }
  check(found, "Missing Tiled layer: " + name);
  return result;
}
void checkpoints(const std::string &s, FzeroCourse &c) {
  auto main = s.find("MAIN_PATH:");
  check(main == 0, "Missing main AI path");
  auto split = s.find("BRANCH_PATH:");
  unsigned count = 0;
  for (unsigned branch = 0; branch < 2; ++branch) {
    if (branch && split == s.npos)
      break;
    auto chunk =
        branch ? s.substr(split + 12)
               : s.substr(10, split == s.npos ? s.size() - 10 : split - 10);
    auto m = props(chunk, ':');
    auto origin = csv(m.at("ORIGIN")), xy = csv(m.at("XY")),
         path = csv(m.at("PATH"), 255), car = csv(m.at("MAIN"), 255),
         green = csv(m.at("GREEN"), 255), purple = csv(m.at("PURPLE"), 255);
    if (branch && path.empty())
      continue;
    check(origin.size() == 2 && !path.empty() && xy.size() == path.size() * 2 &&
              car.size() == path.size() && green.size() == path.size() &&
              purple.size() == path.size(),
          "Invalid AI path arrays");
    unsigned at = count + (branch ? 1 : 0);
    check(at + path.size() <= 254, "Too many AI checkpoints");
    if (branch)
      c.finish_checkpoint = uint8_t(at);
    unsigned x = origin[0], y = origin[1], sourceX = x, sourceY = y;
    int remX = 0, remY = 0;
    word(c.path + at * 2, x);
    word(c.path + 0x200 + at * 2, y);
    for (unsigned i = 0; i < path.size(); ++i, ++at) {
      int dx = int(xy[i * 2]) - int(sourceX) + remX,
          dy = int(xy[i * 2 + 1]) - int(sourceY) + remY;
      check(dx / 8 >= -128 && dx / 8 <= 127 && dy / 8 >= -128 && dy / 8 <= 127,
            "AI checkpoint delta cannot be represented");
      remX = dx % 8;
      remY = dy % 8;
      sourceX = xy[i * 2];
      sourceY = xy[i * 2 + 1];
      x = (x + dx / 8 * 8) & 65535;
      y = (y + dy / 8 * 8) & 65535;
      word(c.path + 2 + at * 2, x);
      word(c.path + 0x202 + at * 2, y);
      c.path[0x502 + at] = uint8_t(path[i]);
      c.path[0x602 + at] = uint8_t(car[i]);
      c.path[0x702 + at] = uint8_t(green[i]);
      c.path[0x802 + at] = uint8_t(purple[i]);
      if (!c.has_pit && (path[i] & 32)) {
        c.has_pit = 1;
        c.pit_checkpoint = uint8_t(at);
      }
    }
    if (!branch)
      count = at;
  }
  check(count > 0, "Empty AI path");
  c.last_checkpoint = uint8_t(count - 1);
}
} // namespace
bool FzeroFzeditRead(const char *pack_root, const char *path, FzeroCourse *out,
                     char *error, size_t cap) {
  try {
    Files files{fs::weakly_canonical(pack_root), fs::path(path).parent_path()};
    auto properties = props(text(path));
    check(properties.at("Version") == "FZEdit Version 0.9",
          "Unsupported FZM revision; export using FZEdit 1.2.0");
    uint8_t digest[32] = {5};
    auto hashPart = [&](const std::string &data) {
      std::vector<uint8_t> bytes(digest, digest + 32);
      bytes.insert(bytes.end(), data.begin(), data.end());
      sha256_compute(bytes.data(), bytes.size(), digest);
    };
    hashPart(text(path));
    for (const char *key :
         {"AiPathFile", "PaletteFile", "TilesetFile", "HorizonTileSetFile",
          "HorizonTileMapFile", "MiniMapFile", "TilesetTSX", "TrackFile"})
      hashPart(text(files.get(properties.at(key))));
    char key[65];
    for (unsigned i = 0; i < 32; ++i)
      snprintf(key + i * 2, 3, "%02x", digest[i]);
    fs::path cache =
        fs::path("mods/packs/.cache/courses") / (std::string(key) + ".fzc");
    char cacheError[256];
    if (FzeroCourseFileRead(cache.string().c_str(), out, cacheError,
                            sizeof(cacheError)))
      return true;
    FzeroCourse c{};
    checkpoints(text(files.get(properties.at("AiPathFile"))), c);
    auto palette = image(files.get(properties.at("PaletteFile")));
    check(palette.w == 16 && palette.h == 7, "Expected 16x7 palette");
    for (unsigned i = 0; i < 112; ++i) {
      auto rgb = palette.rgb[i];
      word(c.palette + i * 2,
           (rgb[0] >> 3) | ((rgb[1] >> 3) << 5) | ((rgb[2] >> 3) << 10));
    }
    auto tiles = image(files.get(properties.at("TilesetFile")));
    check(tiles.w == 128 && tiles.h == 128 && tiles.indices.size() == 16384,
          "Expected indexed 128x128 track tileset");
    for (unsigned t = 0; t < 256; ++t)
      for (unsigned p = 0; p < 64; ++p)
        c.graphics[t * 64 + p] =
            tiles.indices[((t / 16) * 8 + p / 8) * 128 + (t % 16) * 8 + p % 8];
    auto horizon = image(files.get(properties.at("HorizonTileSetFile")));
    check(horizon.w == 128 && horizon.h == 128 &&
              horizon.indices.size() == 16384,
          "Expected indexed 128x128 horizon tileset");
    for (unsigned t = 0; t < 256; ++t)
      for (unsigned y = 0; y < 8; ++y)
        for (unsigned x = 0; x < 8; ++x) {
          unsigned v =
              horizon.indices[((t / 16) * 8 + y) * 128 + (t % 16) * 8 + x] & 15;
          for (unsigned bit = 0; bit < 4; ++bit)
            if (v & (1u << bit))
              c.sky_graphics[t * 32 + (bit / 2) * 16 + y * 2 + bit % 2] |=
                  uint8_t(128 >> x);
        }
    Xml sky(files.get(properties.at("HorizonTileMapFile")));
    for (unsigned front = 0; front < 2; ++front) {
      unsigned width = front ? 96 : 112, target = front ? 96 : 128;
      auto values =
          layer(sky.root(), front ? "Background" : "Foreground", width, 7);
      auto dest = front ? c.sky_front : c.sky_back;
      for (unsigned x = 0; x < target; ++x)
        for (unsigned y = 0; y < 7; ++y) {
          auto v = values[y * width + x % width];
          check(!(v & 0x30000000), "Unsupported diagonal horizon flip");
          unsigned gid = v & 0xffff;
          check(gid <= 512, "Horizon tile out of range");
          unsigned tile = gid ? ((gid - 1) & 255) : 384,
                   pal = gid > 256 ? 7 : 6;
          unsigned offset = ((x / 32) * 224 + y * 32 + x % 32) * 2;
          word(dest + offset, tile | (pal << 10) | ((v >> 31) << 14) |
                                  (((v >> 30) & 1) << 15));
        }
    }
    auto mini = image(files.get(properties.at("MiniMapFile")));
    check(mini.w == 32 && mini.h == 64, "Expected 32x64 minimap");
    const unsigned offsets[] = {0, 256, 64, 320, 128, 384, 192, 448};
    for (unsigned y = 0; y < 64; ++y)
      for (unsigned x = 0; x < 32; ++x) {
        auto rgb = mini.rgb[y * 32 + x];
        bool r = rgb[0] > 128, g = rgb[1] > 128, b = rgb[2] > 128;
        unsigned color = (!r && !g && !b) ? 1 : (r && g && b) ? 2 : g ? 3 : 0;
        unsigned at = (x / 8) * 16 + offsets[y / 8] + (y % 8) * 2;
        for (unsigned bit = 0; bit < 2; ++bit)
          if (color & (1u << bit))
            c.minimap[at + bit] |= uint8_t(128 >> (x % 8));
      }
    auto signedOffset = [](const std::string &s) {
      size_t end = 0;
      long n = std::stol(s, &end);
      check(end == s.size() && n >= -32768 && n <= 65535,
            "Minimap offset out of range");
      return uint16_t(n);
    };
    c.map_x = signedOffset(properties.at("MiniMapXOffset"));
    c.map_y = signedOffset(properties.at("MiniMapYOffset"));
    const unsigned songs[] = {1, 3, 2, 4, 9, 5, 8, 0, 6, 7};
    unsigned music = number(properties.at("Music"), 9);
    c.setting = uint8_t(0xc0 | music);
    c.has_music = 1;
    c.music = uint8_t(songs[music] * 9);
    auto shade = properties.at("HorizonShade");
    check(shade == "Light" || shade == "Dark" || shade == "Shadeless",
          "Unknown horizon shade");
    c.gradient = shade == "Light" ? 35 : shade == "Dark" ? 163 : 0;
    auto cycles = properties.at("CycleData");
    check(cycles.size() == 14, "Invalid palette cycle declaration");
    c.has_palette_cycles = 1;
    for (unsigned i = 0; i < 14; ++i) {
      check(cycles[i] == '0' || cycles[i] == '1', "Invalid palette cycle bit");
      if (cycles[i] == '1')
        c.palette_cycles[c.palette_cycle_count++] = uint8_t(i * 16);
    }
    const char *freq[] = {"BeginnerExplosiveFrequency",
                          "StandardExplosiveFrequency",
                          "ExpertExplosiveFrequency"};
    for (unsigned i = 0; i < 3; ++i)
      c.opponents[i] = uint8_t(number(properties.at(freq[i]), 255));
    auto name = properties.at("InGameMapName");
    check(name.size() <= 26, "Intro name exceeds supported width");
    static const uint8_t alphabet[] = {0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a,
                                       0x6b, 0x8e, 0x6c, 0x6e, 0x6f, 0x8a, 0x8b,
                                       0x8c, 0x8d, 0x6d, 0x8f, 0xa0, 0xa1, 0xa2,
                                       0xa3, 0xa4, 0xa6, 0xa5, 0xa7};
    c.name[0] = uint8_t(68 - (name.size() > 10 ? (name.size() - 10) * 4 : 0));
    c.name[1] = 0x53;
    c.name[2] = 1;
    c.name[3] = 0x82;
    c.name[4] = 0x1b;
    c.name[5] = 0xff;
    const std::map<unsigned, uint8_t> punctuation = {
        {' ', 0xff}, {'.', 0x1b}, {'\'', 0x28}, {'"', 0x38}, {'?', 0xa9},
        {'(', 0x2a}, {')', 0x3a}, {'^', 0x2b},  {'$', 0xfe}};
    for (unsigned i = 0; i < name.size(); ++i) {
      unsigned ch = unsigned(toupper((unsigned char)name[i]));
      if (ch >= 'A' && ch <= 'Z')
        c.name[6 + i] = alphabet[ch - 'A'];
      else if (ch >= '0' && ch <= '9')
        c.name[6 + i] = uint8_t(0x80 + ch - '0');
      else {
        check(punctuation.count(ch) > 0,
              "Unsupported intro glyph; provide an explicit glyph resource");
        c.name[6 + i] = punctuation.at(ch);
      }
    }
    // FZEdit's R/Z differ from the stock OBJ atlas. Preserve its lettering
    // using the reviewed two-plane glyphs (also used by the Astra importer).
    static const uint8_t extraCodes[] = {0x8f, 0xa7};
    static const uint8_t extraPixels[][32] = {
        {0x00, 0x00, 0x00, 0x00, 0x7e, 0xfc, 0x77, 0xee, 0x77, 0xee, 0x77,
         0xee, 0x77, 0xee, 0x7f, 0xfc, 0x76, 0xec, 0x77, 0xee, 0x77, 0xee,
         0x77, 0xee, 0x77, 0xee, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00},
        {0x00, 0x00, 0x00, 0x00, 0x7f, 0xfe, 0x77, 0xce, 0x6f, 0xdc, 0x6e,
         0xdc, 0xee, 0x18, 0x1c, 0x38, 0x1c, 0x38, 0x3c, 0x70, 0x3b, 0x76,
         0x7b, 0xe6, 0x7f, 0xfe, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00}};
    for (unsigned g = 0; g < 2; ++g) {
      if (std::find(c.name + 6, c.name + 6 + name.size(), extraCodes[g]) !=
          c.name + 6 + name.size()) {
        auto &glyph = c.intro_glyphs[c.intro_glyph_count++];
        glyph.code = extraCodes[g];
        memcpy(glyph.pixels, extraPixels[g], 32);
      }
    }
    Xml tsx(files.get(properties.at("TilesetTSX")));
    const std::map<std::string, std::pair<unsigned, unsigned>> bits = {
        {"MAIN", {0, 128}},
        {"BRANCH", {0, 64}},
        {"DIRT", {256, 128}},
        {"JUMP", {256, 64}},
        {"LANDMINE", {256, 32}},
        {"DASH", {256, 16}},
        {"DOWNPULL_MAGNET", {256, 8}},
        {"MAGNET", {256, 4}},
        {"ICE", {512, 128}},
        {"RIGHT_PUSH", {512, 64}},
        {"LEFT_PUSH", {512, 32}},
        {"PIT", {512, 16}},
        {"CUSTOM1", {512, 1}},
        {"CUSTOM2", {512, 2}},
        {"CUSTOM3", {512, 4}},
        {"CUSTOM4", {512, 8}},
        {"BARRIER", {768, 16}},
        {"WALL", {768, 32}},
        {"BACKGROUND", {768, 128}}};
    for (auto tile = tsx.root()->children; tile; tile = tile->next)
      if (tag(tile, "tile")) {
        unsigned id = number(attr(tile, "id"), 255);
        for (auto propertiesNode = tile->children; propertiesNode;
             propertiesNode = propertiesNode->next)
          if (tag(propertiesNode, "properties"))
            for (auto prop = propertiesNode->children; prop; prop = prop->next)
              if (tag(prop, "property")) {
                auto key = attr(prop, "name");
                check(bits.count(key) > 0, "Unsupported tile property: " + key);
                if (number(attr(prop, "value"), 1)) {
                  auto [off, mask] = bits.at(key);
                  c.terrain[off + id] |= uint8_t(mask);
                }
              }
      }
    for (unsigned i = 0; i < 256; ++i)
      if (c.terrain[i] & 128)
        c.terrain[i] = 128;
    Xml map(files.get(properties.at("TrackFile")));
    auto values = layer(map.root(), "", 1024, 512);
    for (auto v : values)
      check(v <= 256, "Unsupported flipped or out-of-range track tile");
    std::vector<uint8_t> pool, rows, blocks(512);
    std::map<std::array<uint8_t, 4>, unsigned> quadIndex;
    std::map<std::array<uint8_t, 32>, unsigned> rowIndex, blockIndex;
    auto append = [](std::vector<uint8_t> &out, const uint8_t *data,
                     unsigned size, unsigned align) {
      for (unsigned overlap = size - align; overlap > 0; overlap -= align)
        if (out.size() >= overlap &&
            std::equal(data, data + overlap, out.end() - overlap)) {
          unsigned at = unsigned(out.size() - overlap);
          out.insert(out.end(), data + overlap, data + size);
          return at;
        }
      unsigned at = unsigned(out.size());
      out.insert(out.end(), data, data + size);
      return at;
    };
    for (unsigned cy = 0; cy < 16; ++cy)
      for (unsigned cx = 0; cx < 32; ++cx) {
        std::array<uint8_t, 32> pointers{};
        bool mineChunk = false;
        for (unsigned y = 0; y < 16; ++y) {
          std::array<uint8_t, 32> row{};
          bool mine = false;
          for (unsigned x = 0; x < 16; ++x) {
            unsigned at = (cy * 32 + y * 2) * 1024 + cx * 32 + x * 2;
            std::array<uint8_t, 4> q{};
            unsigned indices[] = {at, at + 1024, at + 1, at + 1025};
            for (unsigned k = 0; k < 4; ++k) {
              q[k] = uint8_t(values[indices[k]] ? values[indices[k]] - 1 : 0);
              mine |= q[k] >= 200 && q[k] <= 203;
            }
            auto it = quadIndex.find(q);
            unsigned offset;
            if (it == quadIndex.end()) {
              offset = append(pool, q.data(), 4, 1);
              quadIndex[q] = offset;
            } else
              offset = it->second;
            check(offset + 4 <= sizeof(c.pool),
                  "Track exceeds tile pool limit");
            word(row.data() + x * 2, offset);
          }
          unsigned at;
          auto it = rowIndex.find(row);
          if (mine || it == rowIndex.end()) {
            at = mine ? unsigned(rows.size()) : append(rows, row.data(), 32, 2);
            if (mine)
              rows.insert(rows.end(), row.begin(), row.end());
            else
              rowIndex[row] = at;
          } else
            at = it->second;
          word(pointers.data() + y * 2, at + 0x7000);
          mineChunk |= mine;
        }
        unsigned offset;
        auto it = blockIndex.find(pointers);
        if (mineChunk || it == blockIndex.end()) {
          offset = unsigned(blocks.size() - 512);
          blocks.insert(blocks.end(), pointers.begin(), pointers.end());
          if (!mineChunk)
            blockIndex[pointers] = offset;
        } else
          offset = it->second;
        check(offset / 32 < 256, "Track exceeds 256 unique chunks");
        blocks[cy * 32 + cx] = uint8_t(offset / 32);
      }
    while (rows.size() % 18)
      rows.push_back(0xff);
    check(blocks.size() <= sizeof(c.blocks) && rows.size() <= sizeof(c.grid),
          "Track exceeds engine layout capacity");
    memcpy(c.pool, pool.data(), pool.size());
    memcpy(c.blocks, blocks.data(), blocks.size());
    memcpy(c.grid, rows.data(), rows.size());
    c.block_size = uint16_t(blocks.size());
    c.grid_size = uint16_t(rows.size());
    // Preserve authored shortcut landing rectangles; pair J_n and L_n objects.
    struct Rect {
      unsigned x, y, w, h, checkpoint;
    };
    std::map<unsigned, Rect> jumps, lands;
    for (auto group = map.root()->children; group; group = group->next)
      if (tag(group, "objectgroup"))
        for (auto o = group->children; o; o = o->next)
          if (tag(o, "object")) {
            auto label = attr(o, "name");
            if (label.empty())
              continue;
            auto split = label.find('_');
            check(split != label.npos, "Shortcut rectangle needs J_n/L_n name");
            unsigned id = number(label.substr(split + 1));
            auto coordinate = [&](const char *key) {
              double v = std::stod(attr(o, key)) / 2;
              check(v >= 0 && v <= 65535, "Shortcut rectangle out of range");
              return unsigned(v);
            };
            Rect r{coordinate("x"), coordinate("y"), coordinate("width"),
                   coordinate("height"), 255};
            for (auto ps = o->children; ps; ps = ps->next)
              if (tag(ps, "properties"))
                for (auto prop = ps->children; prop; prop = prop->next)
                  if (tag(prop, "property"))
                    r.checkpoint = number(attr(prop, "value"), 255);
            auto &rects = label.find('J') != label.npos ? jumps : lands;
            check(rects.emplace(id, r).second, "Duplicate shortcut rectangle");
          }
    unsigned shortcut = 0;
    for (auto &[id, a] : jumps) {
      check(lands.count(id) && shortcut < 16, "Unpaired or excess shortcuts");
      auto b = lands.at(id);
      unsigned bounds[] = {a.x, a.y, a.x + a.w, a.y + a.h,
                           b.x, b.y, b.x + b.w, b.y + b.h};
      for (unsigned i = 0; i < 8; ++i) {
        check(bounds[i] <= 65535, "Shortcut bounds overflow");
        word(c.shortcuts + shortcut * 17 + i * 2, bounds[i]);
      }
      c.shortcuts[shortcut * 17 + 16] = uint8_t(b.checkpoint);
      ++shortcut;
    }
    check(lands.size() == jumps.size(), "Unpaired shortcut landing");
    c.shortcuts[shortcut * 17] = c.shortcuts[shortcut * 17 + 1] = 0xff;
    char validation[256] = {0};
    check(FzeroCourseValidate(&c, validation, sizeof(validation)), validation);
    FzeroCourseHash(&c);
    *out = c;
    std::error_code ec;
    fs::create_directories(cache.parent_path(), ec);
    if (!ec) {
      auto temp = cache;
      temp += ".tmp";
      if (FzeroCourseFileWrite(temp.string().c_str(), &c, cacheError,
                               sizeof(cacheError)))
        fs::rename(temp, cache, ec);
    }
    return true;
  } catch (const std::exception &e) {
    snprintf(error, cap, "%s", e.what());
    return false;
  }
}
