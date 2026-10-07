#pragma once
// The format-2 resource graph is independent from Unity object creation.
#ifndef NOMINMAX
#define NOMINMAX
#endif
// bcrypt.h depends on the Windows type declarations; keep this include first.
#include <windows.h>

#include <bcrypt.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <vector>

#include "eiem_expression.h"
#include "eiem_keys.h"
#pragma comment(lib, "bcrypt.lib")

namespace EiemDirectory {
using Bytes = std::vector<unsigned char>;
using Digest = std::array<unsigned char, 32>;
inline void Require(bool ok, const std::string& message) {
  if (!ok) throw std::runtime_error(message);
}
inline size_t AddSize(size_t a,size_t b) {
  Require(b<=SIZE_MAX-a,"byte extent overflow");return a+b;
}
inline size_t MultiplySize(size_t a,size_t b) {
  Require(!a||b<=SIZE_MAX/a,"byte extent overflow");return a*b;
}
inline size_t AlignSize(size_t value,size_t alignment) {
  return AddSize(value,alignment-1)&~(alignment-1);
}
inline uint64_t Le(const Bytes& b, size_t p, size_t n) {
  Require(n <= 8 && p <= b.size() && n <= b.size() - p, "truncated integer");
  uint64_t v = 0;
  for (size_t i = 0; i < n; ++i) v |= uint64_t(b[p + i]) << (i * 8);
  return v;
}
inline std::string Text(const unsigned char* p, size_t n) {
  if (n == 0) return {};
  // The Windows conversion API takes an int length. Validate in chunks at
  // codepoint boundaries instead of imposing that API's width on the field.
  for(size_t offset=0;offset<n;) {
    size_t chunk=std::min(n-offset,size_t(1024*1024));
    if(chunk<n-offset)while(chunk&&(p[offset+chunk]&0xC0)==0x80)--chunk;
    Require(chunk&&MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,
                (const char*)p+offset,int(chunk),nullptr,0)>0,"invalid UTF-8");
    offset+=chunk;
  }
  std::string s((const char*)p, n);
  Require(s.find('\0') == s.npos, "embedded NUL");
  return s;
}
// Editable image sources and the manual converter may sit beside .tex files.
// They are never executable Mod inputs and cannot trigger a native rebuild.
inline bool AuthorAuxiliary(const std::filesystem::path& path) {
  auto extension=path.extension().u8string(),name=path.filename().u8string();
  for(auto& ch:extension)if(ch>='A'&&ch<='Z')ch+=32;
  for(auto& ch:name)if(ch>='A'&&ch<='Z')ch+=32;
  return extension==".png"||extension==".psd"||name=="eff_texture_convert.exe";
}
inline Bytes Read(const std::filesystem::path& p) {
  std::ifstream f(p, std::ios::binary | std::ios::ate);
  Require(bool(f), "cannot open " + p.u8string());
  auto n = f.tellg();
  Require(n >= 0 && uint64_t(n) <= Bytes{}.max_size(), "file size cannot be represented " + p.u8string());
  Bytes b((size_t)n);
  f.seekg(0);
  if (!b.empty()) f.read((char*)b.data(), b.size());
  Require(bool(f), "short file read " + p.u8string());
  return b;
}
inline Digest Sha(const unsigned char* p, size_t n) {
  BCRYPT_ALG_HANDLE alg = nullptr;
  BCRYPT_HASH_HANDLE hash = nullptr;
  Digest d{};
  Require(BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0,
          "SHA256 provider failed");
  NTSTATUS status = BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0);
  if (status >= 0) {
    size_t i = 0;
    while (i < n && status >= 0) {
      ULONG step = (ULONG)std::min<size_t>(n - i, 0x10000000);
      status = BCryptHashData(hash, (PUCHAR)(p + i), step, 0);
      i += step;
    }
  }
  if (status >= 0) status = BCryptFinishHash(hash, d.data(), (ULONG)d.size(), 0);
  if (hash) BCryptDestroyHash(hash);
  BCryptCloseAlgorithmProvider(alg, 0);
  Require(status >= 0, "SHA256 calculation failed");
  return d;
}
inline std::string Hex(const unsigned char* p, size_t n) {
  std::string s;
  static const char digits[] = "0123456789ABCDEF";
  for (size_t i = 0; i < n; ++i) {
    s += digits[p[i] >> 4];
    s += digits[p[i] & 15];
  }
  return s;
}
inline bool EqualDigest(const Digest& d, const Bytes& b, size_t p) {
  return p <= b.size() && 32 <= b.size() - p && std::equal(d.begin(), d.end(), b.begin() + p);
}
struct Block {
  size_t offset = 0, size = 0;
  Digest digest{};
};
struct Resource {
  uint32_t kind = 0;
  std::array<unsigned char, 16> type{};
  Bytes bytes;
  std::map<std::string, Block> blocks;
};
inline Resource Decode(Bytes bytes, unsigned expected) {
  Require(bytes.size() >= 80, "resource header truncated");
  Require(memcmp(bytes.data(), "EFFRES", 6) == 0 && bytes[7] == 0,
          "unsupported resource container");
  Resource r;
  r.kind = (uint32_t)Le(bytes, 12, 4);
  Require((r.kind == 1 || r.kind == 2 || r.kind == 4) && r.kind == expected,
          "resource kind mismatch");
  Require(Le(bytes, 40, 8) == bytes.size(), "resource total size mismatch");
  Require(EqualDigest(Sha(bytes.data() + 80, bytes.size() - 80), bytes, 48),
          "resource body SHA256 mismatch");
  std::copy_n(bytes.begin() + 24, 16, r.type.begin());
  size_t count = (size_t)Le(bytes, 16, 4), directory = (size_t)Le(bytes, 20, 4);
  Require(directory <= bytes.size() - 80 && count <= directory / 51, "invalid block directory");
  size_t stop = 80 + directory, p = 80, end = stop;
  for (size_t i = 0; i < count; ++i) {
    Require(p <= stop && stop - p >= 2, "truncated block directory");
    size_t n = (size_t)Le(bytes, p, 2);
    p += 2;
    Require(n && n <= stop - p && stop - p - n >= 48, "truncated block name");
    auto name = Text(bytes.data() + p, n);
    p += n;
    uint64_t off = Le(bytes, p, 8), size = Le(bytes, p + 8, 8);
    Require(off == ((end + 15) & ~size_t(15)) && off <= bytes.size() && size <= bytes.size() - off,
            "overlapping/unreferenced block");
    Require(std::all_of(bytes.begin() + end, bytes.begin() + (size_t)off,
                        [](unsigned char v) { return v == 0; }),
            "nonzero block alignment");
    Block block;
    block.offset = (size_t)off;
    block.size = (size_t)size;
    std::copy_n(bytes.begin() + p + 16, 32, block.digest.begin());
    Require(EqualDigest(Sha(bytes.data() + block.offset, block.size), bytes, p + 16),
            "block SHA256 mismatch: " + name);
    Require(r.blocks.emplace(name, block).second, "duplicate block: " + name);
    p += 48;
    end = block.offset + block.size;
  }
  Require(p == stop && end == bytes.size(), "unreferenced directory/suffix");
  r.bytes = std::move(bytes);
  return r;
}
inline std::vector<std::string> Strings(const Resource& r, const char* name) {
  const auto& b = r.blocks.at(name);
  size_t end = b.offset + b.size, p = b.offset;
  Require(b.size >= 4, "binding table truncated");
  size_t count = (size_t)Le(r.bytes, p, 4);
  p += 4;
  Require(count <= (end - p) / 2, "invalid binding count");
  std::vector<std::string> out;
  std::set<std::string> seen;
  for (size_t i = 0; i < count; ++i) {
    Require(end - p >= 2, "truncated binding length");
    size_t n = (size_t)Le(r.bytes, p, 2);
    p += 2;
    Require(n && n <= end - p, "truncated/empty binding");
    auto text = Text(r.bytes.data() + p, n);
    p += n;
    Require(seen.insert(text).second, "duplicate binding");
    out.push_back(text);
  }
  Require(p == end, "binding table suffix");
  return out;
}
using Settings = std::map<std::string, std::string>;
struct Section {
  std::string id;
  Settings values;
};
inline bool Begins(const std::string& s, const char* prefix) {
  return s.compare(0, strlen(prefix), prefix) == 0;
}
inline std::string Trim(std::string s) {
  EiemKeyTrim(s);
  return s;
}
inline bool TableIdentity(const std::string& row,const char* name,const std::string& key) {
  auto first=row.find('\t');if(first==row.npos||row.substr(0,first)!=name)return false;
  auto second=row.find('\t',first+1);
  return second!=row.npos&&second>first+1&&row.substr(second+1)==key;
}
inline bool Identifier(const std::string& s) {
  return !s.empty() && std::all_of(s.begin(), s.end(), [](unsigned char c) {
    return std::isalnum(c) || c == '_' || c == '.' || c == '-';
  });
}
inline double Number(const std::string& s) {
  double n = 0;
  Require(s != "true" && s != "false" && EiemNumber(s, &n), "invalid finite number: " + s);
  return n;
}
inline unsigned Integer(const std::string& s) {
  Require(!s.empty() && s.size() <= 10 &&
              std::all_of(s.begin(), s.end(), [](unsigned char c) { return c >= '0' && c <= '9'; }),
          "invalid integer: " + s);
  auto v = std::stoull(s);
  Require(v <= UINT_MAX, "integer overflow");
  return (unsigned)v;
}
inline std::vector<std::string> Split(const std::string& s) {
  std::vector<std::string> out;
  size_t p = 0;
  do {
    size_t e = s.find(',', p);
    auto v = Trim(s.substr(p, e == s.npos ? e : e - p));
    Require(!v.empty(), "empty list item");
    out.push_back(v);
    if (e == s.npos) break;
    p = e + 1;
  } while (true);
  return out;
}
inline std::vector<Section> ParseIni(const Bytes& bytes) {
  auto all = Text(bytes.data(), bytes.size());
  if (Begins(all, "\xEF\xBB\xBF")) all.erase(0, 3);
  std::istringstream input(all);
  std::string line;
  std::vector<Section> out;
  std::set<std::string> seen;
  size_t number = 0;
  while (std::getline(input, line)) {
    ++number;
    line = Trim(line);
    if (line.empty() || line[0] == ';' || line[0] == '#') continue;
    if (line[0] == '[' && line.back() == ']') {
      auto id = line.substr(1, line.size() - 2);
      Require(Identifier(id) && seen.insert(id).second,
              "invalid/duplicate section at line " + std::to_string(number));
      out.push_back({id, {}});
      continue;
    }
    auto equals = line.find('=');
    Require(!out.empty() && equals != line.npos,
            "expected key=value at line " + std::to_string(number));
    auto key = Trim(line.substr(0, equals)), value = Trim(line.substr(equals + 1));
    Require(!key.empty() && !value.empty() && out.back().values.emplace(key, value).second,
            "empty/duplicate setting at line " + std::to_string(number));
  }
  return out;
}
inline void Keys(const Section& s, std::initializer_list<const char*> required,
                 std::initializer_list<const char*> optional = {},
                 std::initializer_list<const char*> prefixes = {}) {
  for (auto k : required) Require(s.values.count(k) != 0, "missing " + s.id + "." + k);
  for (const auto& v : s.values) {
    bool ok = false;
    for (auto k : required) ok |= v.first == k;
    for (auto k : optional) ok |= v.first == k;
    for (auto k : prefixes) ok |= Begins(v.first, k);
    Require(ok, "unknown setting " + s.id + "." + v.first);
  }
}
struct Identity {
  std::string path, asset;
  bool operator<(const Identity& b) const {
    return std::tie(path, asset) < std::tie(b.path, b.asset);
  }
};
inline Identity Source(const Settings& v, const std::string& prefix) {
  Identity id{v.at(prefix + "path"), v.at(prefix + "asset")};
  Require(Begins(id.path, "assets/") && id.path.find('\\') == id.path.npos &&
              id.path.find("..") == id.path.npos,
          "invalid logical resource path " + id.path);
  std::transform(id.path.begin(),id.path.end(),id.path.begin(),[](unsigned char c){return char(std::tolower(c));});
  return id;
}
struct Mesh {
  std::string resource;
  unsigned vertices = 0, indices = 0;
  std::vector<std::string> bones, shapes;
};
struct Material {
  std::string resource;
  Identity source;
  std::map<std::string, std::string> textures;
};
struct Texture {
  std::string resource;
  Identity source;
  unsigned width = 0, height = 0, mips = 0, filter = 0, wrap = 0, aniso = 0;
  bool linear = false;
  double bias = 0;
};
struct Part {
  std::string mesh, material;
  std::shared_ptr<EiemExpression> visible;
  std::map<std::string, std::string> shapes;
};
struct Rule {
  std::vector<std::string> parts;
  std::map<unsigned, Identity> targets;
  bool skip = false;
};
struct Control {
  std::string label;
  double minimum = 0, maximum = 1;
};
struct Binding {
  std::string id, variable, type;
  EiemKeyChord key;
  std::vector<double> values;
  double speed = 1;
};
struct Package {
  std::filesystem::path root;
  std::string id;
  Digest iniHash{};
  std::map<std::string, std::shared_ptr<const Resource>> resources;
  std::map<std::string, Mesh> meshes;
  std::map<std::string, Material> materials;
  std::map<std::string, Texture> textures;
  std::map<std::string, Part> parts;
  std::map<std::string, Rule> rules;
  std::map<Identity, std::string> targetIndex;
  EiemVariables defaults;
  std::set<std::string> persistent;
  std::map<std::string, Control> controls;
  std::vector<Binding> bindings;
  // Compiler-only ownership metadata, reconstructed from the original authors
  // when a composed manifest is decoded. Never authored in mod.ini.
  bool composed=false;
  std::map<std::string,std::pair<std::string,std::string>> partOwners;
};
inline std::filesystem::path DiskPath(const std::filesystem::path& root,
                                      const std::string& relative, unsigned kind) {
  const char* folder = kind == 1 ? "meshes/" : kind == 2 ? "materials/" : "textures/";
  Require(Begins(relative, folder) && relative.find('\\') == relative.npos &&
              relative.find(':') == relative.npos,
          "resource folder/kind mismatch");
  auto r = std::filesystem::u8path(relative);
  for (const auto& c : r) Require(c != ".." && c != "." && !c.empty(), "unsafe resource path");
  auto path = std::filesystem::weakly_canonical(root / r), rel = path.lexically_relative(root);
  Require(!rel.empty() && !rel.is_absolute() && *rel.begin() != "..",
          "resource escaped Mod folder");
  return path;
}
inline Package Load(const std::filesystem::path& directory) {
  Package p;
  p.root = std::filesystem::canonical(directory);
  auto ini = Read(p.root / "mod.ini");
  auto sections = ParseIni(ini);
  p.iniHash = Sha(ini.data(), ini.size());
  std::set<std::string> allowed{"mod.ini"};
  bool mod = false, constants = false;
  for (const auto& s : sections) {
    if (s.id == "Mod") {
      Keys(s, {"format", "id"});
      Require(Identifier(s.values.at("id")),
              "unsupported Mod format/id");
      p.id = s.values.at("id");
      mod = true;
    }
    if (s.id == "Constants") {
      constants = true;
      for (const auto& row : s.values) {
        bool persist = Begins(row.first, "persist ");
        auto var = persist ? row.first.substr(8) : row.first;
        Require(EiemVariableName(var) && p.defaults.emplace(var, Number(row.second)).second,
                "invalid/duplicate state");
        if (persist) p.persistent.insert(var);
      }
    }
  }
  Require(mod && constants, "Mod and Constants required; format 2 only");
  auto load = [&](const std::string& relative, unsigned kind) -> const Resource& {
    auto found = p.resources.find(relative);
    if (found != p.resources.end()) {
      Require(found->second->kind == kind, "resource alias kind");
      return *found->second;
    }
    auto resource =
        std::make_shared<Resource>(Decode(Read(DiskPath(p.root, relative, kind)), kind));
    allowed.insert(relative);
    p.resources.emplace(relative, resource);
    return *resource;
  };
  static const std::set<std::string> geometry = {"m_SubMeshes",
                                                 "m_Shapes",
                                                 "m_BindPose",
                                                 "m_BoneNameHashes",
                                                 "m_BonesAABB",
                                                 "m_VariableBoneCountWeights",
                                                 "m_MeshCompression",
                                                 "m_IndexFormat",
                                                 "m_IndexBuffer",
                                                 "m_VertexData",
                                                 "m_CompressedMesh",
                                                 "m_LocalAABB",
                                                 "m_StreamData",
                                                 "m_BonesPerVertex",
                                                 "@geometry",
                                                 "@bones",
                                                 "@shapes"};
  for (const auto& s : sections) {
    const auto& v = s.values;
    if (s.id == "Mod" || s.id == "Constants") continue;
    if (Begins(s.id, "Mesh")) {
      Keys(s, {"path"});
      const auto& r = load(v.at("path"), 1);
      std::set<std::string> fields;
      for (const auto& b : r.blocks) fields.insert(b.first);
      Require(fields == geometry, "incomplete/unknown Mesh fields");
      auto g = r.blocks.at("@geometry");
      Require(g.size == 8, "invalid geometry metadata");
      Mesh m;
      m.resource = v.at("path");
      m.vertices = (unsigned)Le(r.bytes, g.offset, 4);
      m.indices = (unsigned)Le(r.bytes, g.offset + 4, 4);
      Require(m.vertices && m.indices && m.indices % 3 == 0, "invalid geometry count");
      m.bones = Strings(r, "@bones");
      m.shapes = Strings(r, "@shapes");
      p.meshes.emplace(s.id, std::move(m));
    } else if (Begins(s.id, "Material")) {
      Keys(s, {"path", "source.path", "source.asset"}, {}, {"texture."});
      const auto& r = load(v.at("path"), 2);
      Material m;
      m.resource = v.at("path");
      m.source = Source(v, "source.");
      for (const auto& b : r.blocks) Require(!Begins(b.first, "@"), "invalid material metadata");
      for (const auto& row : v)
        if (Begins(row.first, "texture.")) {
          Require(row.first.size() > 8, "empty texture property");
          m.textures.emplace(row.first.substr(8), row.second);
        }
      p.materials.emplace(s.id, std::move(m));
    } else if (Begins(s.id, "Texture")) {
      Keys(s, {"path"}, {"source.path", "source.asset", "width", "height", "format", "mip_count",
                        "linear", "filter", "wrap", "aniso", "mip_bias"});
      const auto& r = load(v.at("path"), 4);
      Require(r.blocks.count("pixels") &&
                  (r.blocks.size() == 1 || (r.blocks.size() == 2 && r.blocks.count("@texture"))) &&
                  std::all_of(r.type.begin(), r.type.end(), [](unsigned char c) { return !c; }),
              "invalid Pixels resource");
      Texture t;
      t.resource = v.at("path");
      if (v.count("source.path") || v.count("source.asset")) t.source = Source(v, "source.");
      auto setting = [&](const char* key, const char* fallback) {
        auto it = v.find(key); return it == v.end() ? std::string(fallback) : it->second;
      };
      if (r.blocks.count("@texture")) {
        const auto& spec = r.blocks.at("@texture");
        Require(spec.size == 20 &&
                    Le(r.bytes, spec.offset + 12, 4) == 4, "unsupported TEX metadata structure/pixel encoding");
        t.width = (unsigned)Le(r.bytes, spec.offset + 4, 4);
        t.height = (unsigned)Le(r.bytes, spec.offset + 8, 4);
        t.mips = (unsigned)Le(r.bytes, spec.offset + 16, 4);
      } else {
        Require(v.count("width") && v.count("height") && v.count("mip_count") &&
                    v.count("format") && v.at("format") == "RGBA32", "legacy TEX needs INI dimensions; re-export once");
        t.width = Integer(v.at("width")); t.height = Integer(v.at("height")); t.mips = Integer(v.at("mip_count"));
      }
      t.filter = Integer(setting("filter", "1"));
      t.wrap = Integer(setting("wrap", "0"));
      t.aniso = Integer(setting("aniso", "1"));
      t.bias = Number(setting("mip_bias", "0"));
      Require(setting("linear", "false") == "true" || setting("linear", "false") == "false",
              "unsupported texture encoding/color");
      t.linear = setting("linear", "false") == "true";
      Require(t.width && t.height && t.mips && t.filter <= 2 && t.wrap <= 3 && t.aniso <= 16 &&
                  std::abs(t.bias) <= 3.402823466e38,
              "invalid texture declaration range");
      size_t size = 0;
      unsigned w = t.width, h = t.height, maxMips = 1;
      for (unsigned n = std::max(w, h); n > 1; n >>= 1) ++maxMips;
      Require(t.mips <= maxMips, "too many mip levels");
      for (unsigned i = 0; i < t.mips; ++i) {
        Require(uint64_t(w) * h <= UINT_MAX / 4, "texture layer exceeds native size field");
        size += (size_t)w * h * 4;
        Require(size <= UINT_MAX, "texture exceeds native size field");
        w = std::max(1u, w / 2);
        h = std::max(1u, h / 2);
      }
      Require(size == r.blocks.at("pixels").size,
              "pixel size does not match TEX dimensions/mips");
      p.textures.emplace(s.id, std::move(t));
    } else if (Begins(s.id, "Part")) {
      Keys(s, {"mesh", "material", "visible"}, {}, {"shape."});
      Part part;
      part.mesh = v.at("mesh");
      part.material = v.at("material");
      std::string error;
      part.visible = EiemExpressionParser(v.at("visible"), error).Parse();
      Require(part.visible && part.visible->Validate(p.defaults, error),
              "invalid visibility: " + s.id + ": " + error);
      for (const auto& row : v)
        if (Begins(row.first, "shape.")) {
          Require(row.first.size() > 6, "empty shape channel");
          part.shapes.emplace(row.first.substr(6), row.second);
        }
      p.parts.emplace(s.id, std::move(part));
    } else if (Begins(s.id, "ShapeControl")) {
      Keys(s, {"variable", "label", "min", "max"});
      auto var = v.at("variable");
      Control c{v.at("label"), Number(v.at("min")), Number(v.at("max"))};
      Require(p.defaults.count(var) && c.minimum < c.maximum && p.defaults.at(var) >= c.minimum &&
                  p.defaults.at(var) <= c.maximum && p.controls.emplace(var, c).second,
              "invalid shape control/default");
    } else if (Begins(s.id, "Key")) {
      Keys(s, {"key", "type"}, {"speed"}, {"$"});
      Binding b;
      b.id = s.id;
      b.type = v.at("type");
      Require(EiemParseKeyChord(v.at("key"), &b.key), "unknown key chord");
      Require(b.type == "cycle" || b.type == "hold" || b.type == "press", "unsupported key type");
      size_t assignments = 0;
      for (const auto& row : v)
        if (Begins(row.first, "$")) {
          ++assignments;
          b.variable = row.first;
          for (const auto& text : Split(row.second)) b.values.push_back(Number(text));
        }
      Require(assignments == 1 && p.defaults.count(b.variable),
              "key requires one declared state assignment");
      if (v.count("speed")) b.speed = Number(v.at("speed"));
      Require(b.speed > 0 && (b.type == "hold" || !v.count("speed")), "invalid speed declaration");
      if (b.type == "cycle") {
        std::set<double> values(b.values.begin(), b.values.end());
        Require(b.values.size() >= 2 && values.size() == b.values.size() &&
                    values.count(p.defaults.at(b.variable)),
                "invalid cycle values/default");
      } else
        Require(b.values.size() == 1, "press/hold requires one target");
      p.bindings.push_back(std::move(b));
    } else if (Begins(s.id, "Replace")) {
      Keys(s, {"target.path", "target.asset"}, {"parts", "handling"}, {"lod."});
      Rule rule;
      rule.skip = v.count("handling") != 0;
      Require(!rule.skip || v.at("handling") == "skip", "unknown handling");
      Require(rule.skip != bool(v.count("parts")), "rule requires parts or skip");
      if (!rule.skip) {
        rule.parts = Split(v.at("parts"));
        Require(
            std::set<std::string>(rule.parts.begin(), rule.parts.end()).size() == rule.parts.size(),
            "duplicate replacement part");
      }
      rule.targets.emplace(0, Source(v, "target."));
      std::map<unsigned, Settings> lods;
      for (const auto& row : v)
        if (Begins(row.first, "lod.")) {
          std::smatch match;
          Require(
              std::regex_match(row.first, match, std::regex("lod\\.([1-9][0-9]*)\\.(path|asset)")),
              "invalid LOD key");
          lods[Integer(match[1])][match[2]] = row.second;
        }
      for (const auto& lod : lods) {
        Require(lod.second.size() == 2, "incomplete LOD identity");
        rule.targets.emplace(lod.first, Source(lod.second, ""));
      }
      for (const auto& target : rule.targets)
        Require(p.targetIndex.emplace(target.second, s.id).second, "duplicate path+asset target");
      p.rules.emplace(s.id, std::move(rule));
    } else
      throw std::runtime_error("unknown INI section: " + s.id);
  }
  Require(!p.parts.empty() && !p.rules.empty(), "Mod requires parts and replacement rules");
  std::set<std::string> usedMesh, usedMaterial, usedTexture, usedControl, usedPart;
  for (const auto& row : p.parts) {
    const auto& part = row.second;
    Require(p.meshes.count(part.mesh) && p.materials.count(part.material),
            "unresolved part resources " + row.first);
    usedMesh.insert(part.mesh);
    usedMaterial.insert(part.material);
    for (const auto& shape : part.shapes) {
      const auto& channels = p.meshes.at(part.mesh).shapes;
      Require(std::find(channels.begin(), channels.end(), shape.first) != channels.end() &&
                  p.controls.count(shape.second),
              "unresolved shape binding " + row.first);
      usedControl.insert(shape.second);
    }
  }
  for (const auto& row : p.materials)
    for (const auto& tex : row.second.textures) {
      Require(p.textures.count(tex.second), "unresolved material texture");
      usedTexture.insert(tex.second);
    }
  for (const auto& row : p.rules)
    for (const auto& part : row.second.parts) {
      Require(p.parts.count(part), "unresolved replacement part");
      usedPart.insert(part);
    }
  Require(usedMesh.size() == p.meshes.size() && usedMaterial.size() == p.materials.size() &&
              usedTexture.size() == p.textures.size() && usedControl.size() == p.controls.size() &&
              usedPart.size() == p.parts.size(),
          "unreferenced declaration");
  for (const auto& state : p.defaults) {
    const Binding* cycle = nullptr;
    for (const auto& b : p.bindings)
      if (b.variable == state.first && b.type == "cycle") {
        if (!cycle)
          cycle = &b;
        else
          Require(std::set<double>(cycle->values.begin(), cycle->values.end()) ==
                      std::set<double>(b.values.begin(), b.values.end()),
                  "shared-state cycle domains differ");
      }
    for (const auto& b : p.bindings)
      if (b.variable == state.first)
        for (double target : b.values) {
          if (cycle)
            Require(std::find(cycle->values.begin(), cycle->values.end(), target) !=
                        cycle->values.end(),
                    "key exceeds cycle domain");
          if (p.controls.count(state.first)) {
            const auto& c = p.controls.at(state.first);
            Require(target >= c.minimum && target <= c.maximum, "shape key exceeds control range");
          }
        }
  }
  if(std::filesystem::is_regular_file(p.root/"source-inputs.bin"))allowed.insert("source-inputs.bin");
  if(std::filesystem::is_regular_file(p.root/"compiled.bin"))allowed.insert("compiled.bin");
  std::set<std::string> actual;
  for (const auto& entry : std::filesystem::recursive_directory_iterator(p.root)) {
    Require(!entry.is_symlink(), "symlink/reparse resources forbidden");
    if (entry.is_regular_file()) {
      auto relative=entry.path().lexically_relative(p.root).generic_u8string();
      // Player preferences are not author resources and are excluded from the
      // package digest. A concurrent atomic save can expose its temporary file.
      if (relative=="state.ini" || std::regex_match(relative,std::regex("state\\.ini\\.tmp-[0-9]+"))) continue;
      if (AuthorAuxiliary(entry.path())) continue;
      actual.insert(std::move(relative));
    }
  }
  Require(actual == allowed, "unreferenced/source files in Mod directory");
  return p;
}
}  // namespace EiemDirectory
