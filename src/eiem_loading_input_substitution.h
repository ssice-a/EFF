#pragma once
#include "eiem_native_input.h"
#include <iomanip>
#include <locale>

namespace EiemNativeInput {
inline std::string InputPath(std::string value) {
  std::replace(value.begin(), value.end(), '\\', '/');
  for (auto& ch : value) if (ch >= 'A' && ch <= 'Z') ch += 'a' - 'A';
  return value;
}
inline std::string PackageDigest(const Package& package) {
  std::string graph = package.id + ":" + Hex(package.iniHash.data(), package.iniHash.size()) + "\n";
  for (const auto& row : package.resources) {
    graph += row.first + ":" + ResourceContentHash(*row.second) + "\n";
  }
  auto hash = Sha((const unsigned char*)graph.data(), graph.size());
  return Hex(hash.data(), hash.size());
}
// The native input contract excludes runtime-only keys, values, visibility and
// shape bindings. All authored parts have stable provenance and source enabled
// flags; runtime state is applied after deserialization. Resources and topology
// still require rebuilt loading inputs.
inline std::string InputContractDigest(const Package& p) {
  std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(17);
  auto text=[&](const std::string& value) {out<<value.size()<<':'<<value<<'\n';};
  text("EFF_NATIVE_INPUT_CONTRACT_1");text(p.id);
  text("resources");out<<p.resources.size()<<'\n';
  for(const auto& r:p.resources) {text(r.first);text(ResourceContentHash(*r.second));}
  text("meshes");out<<p.meshes.size()<<'\n';
  for(const auto& m:p.meshes) {text(m.first);text(m.second.resource);}
  text("materials");out<<p.materials.size()<<'\n';
  for(const auto& m:p.materials) {text(m.first);text(m.second.resource);text(m.second.source.path);text(m.second.source.asset);
    out<<m.second.textures.size()<<'\n';for(const auto& t:m.second.textures) {text(t.first);text(t.second);}}
  text("textures");out<<p.textures.size()<<'\n';
  for(const auto& t:p.textures) {const auto& v=t.second;text(t.first);text(v.resource);text(v.source.path);text(v.source.asset);
    out<<v.width<<','<<v.height<<','<<v.mips<<','<<v.filter<<','<<v.wrap<<','<<v.aniso<<','<<v.linear<<','<<v.bias<<'\n';}
  text("parts");out<<p.parts.size()<<'\n';
  for(const auto& part:p.parts) {text(part.first);text(part.second.mesh);text(part.second.material);}
  text("rules");out<<p.rules.size()<<'\n';
  for(const auto& r:p.rules) {text(r.first);out<<r.second.skip<<','<<r.second.parts.size()<<'\n';for(const auto& part:r.second.parts) text(part);
    out<<r.second.targets.size()<<'\n';for(const auto& target:r.second.targets) {out<<target.first<<'\n';text(target.second.path);text(target.second.asset);}}
  auto data=out.str();auto h=Sha((const unsigned char*)data.data(),data.size());return Hex(h.data(),h.size());
}
struct InputFileStamp {
  uint64_t bytes = 0, written = 0;
};
inline InputFileStamp FileStamp(const std::filesystem::path& path) {
  WIN32_FILE_ATTRIBUTE_DATA attributes{};
  Require(GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &attributes) &&
              !(attributes.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY), "loading input file stamp unavailable");
  return {(uint64_t(attributes.nFileSizeHigh) << 32) | attributes.nFileSizeLow,
          (uint64_t(attributes.ftLastWriteTime.dwHighDateTime) << 32) | attributes.ftLastWriteTime.dwLowDateTime};
}
inline bool SameStamp(const InputFileStamp& a, const InputFileStamp& b) {
  return a.bytes == b.bytes && a.written == b.written;
}
// Native asynchronous loads may reopen frozen startup/generation paths. Hold
// the verified file against overwrite and deletion for the consumer lifetime.
struct FileHold {
  HANDLE handle=INVALID_HANDLE_VALUE;
  explicit FileHold(const std::filesystem::path& path) {
    handle=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,
                       FILE_ATTRIBUTE_NORMAL|FILE_FLAG_SEQUENTIAL_SCAN,nullptr);
    Require(handle!=INVALID_HANDLE_VALUE,"generation input file lock unavailable");
  }
  ~FileHold(){if(handle!=INVALID_HANDLE_VALUE)CloseHandle(handle);}
  FileHold(const FileHold&)=delete;
  FileHold& operator=(const FileHold&)=delete;
  Bytes BytesLocked() {
    LARGE_INTEGER size{},zero{};
    Require(GetFileSizeEx(handle,&size)&&size.QuadPart>0&&uint64_t(size.QuadPart)<=Bytes{}.max_size(),"generation locked input size cannot be represented");
    Require(SetFilePointerEx(handle,zero,nullptr,FILE_BEGIN),"generation input read seek");
    Bytes bytes(size_t(size.QuadPart));
    for(size_t offset=0;offset<bytes.size();){
      DWORD readBytes=0,chunk=DWORD(std::min(bytes.size()-offset,size_t(1024*1024)));
      Require(ReadFile(handle,bytes.data()+offset,chunk,&readBytes,nullptr)&&readBytes==chunk,"generation locked input read failed");offset+=readBytes;
    }
    return bytes;
  }
};
struct InputSubstitution {
  std::filesystem::path source, output;
  uint64_t offset = 0;
  size_t inputBytes = 0, outputBytes = 0;
  std::string inputHash, outputHash;
  InputFileStamp sourceStamp, outputStamp;
  uint32_t pathHandle = 0;
  std::shared_ptr<FileHold> file;
};
struct InputSubstitutions {
  std::string packageDigest;
  std::string inputContractDigest;
  std::string control;
  unsigned generation = 0;
  bool composed=false;
  std::map<std::pair<std::string, uint64_t>, InputSubstitution> entries;
};
inline std::vector<std::string> TabFields(const std::string& line) {
  std::vector<std::string> fields;
  size_t start = 0;
  for (;;) {
    auto end = line.find('\t', start);
    fields.push_back(line.substr(start, end == line.npos ? end : end - start));
    if (end == line.npos) return fields;
    start = end + 1;
  }
}
inline bool TableHeader(const std::string& text,const char* name) {
  auto fields=TabFields(text);return fields.size()==2&&fields[0]==name&&!fields[1].empty();
}
inline uint64_t Decimal(const std::string& value) {
  Require(!value.empty() && value.find_first_not_of("0123456789") == value.npos, "loading input decimal field");
  return std::stoull(value);
}
inline bool DigestText(const std::string& value) {
  return value.size() == 64 && value.find_first_not_of("0123456789ABCDEF") == value.npos;
}
inline InputSubstitutions ReadInputSubstitutions(const std::filesystem::path& table,
                                                  const std::filesystem::path& game,
                                                  const Package& package) {
  auto data = Read(table);
  std::istringstream lines(std::string(data.begin(), data.end()));
  std::string line;
  Require(bool(std::getline(lines, line)), "loading input table empty");
  if (!line.empty() && line.back() == '\r') line.pop_back();
  auto header = TabFields(line);
  bool replacement = header.size() == 4;
  bool compatible = replacement && header[3] == InputContractDigest(package);
  bool control = header.size() == 5 &&
      (header[4] == "raw-copy" || header[4] == "reencoded-unchanged");
  Require((replacement || compatible || control) && header[0] == "EFF_LOADING_INPUT_SUBSTITUTIONS" &&
              header[2] == package.id && DigestText(header[3]) && header[3] == (compatible?InputContractDigest(package):PackageDigest(package)),
          "loading input table structure/package content mismatch");
  InputSubstitutions result;
  result.composed=package.composed;
  result.packageDigest = PackageDigest(package);
  if (compatible) result.inputContractDigest=header[3];
  if (control) result.control = header[4];
  auto gameRoot = InputPath(std::filesystem::canonical(game).u8string()) + "/";
  auto cacheRoot = gameRoot + "plugin/resource-export-staging/";
  auto offlineRoot = gameRoot + "plugin/resource-reload-cache/generations/";
  while (std::getline(lines, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    auto fields = TabFields(line);
    Require(fields.size() == 7, "loading input table row fields");
    InputSubstitution entry;
    entry.source = std::filesystem::canonical(std::filesystem::u8path(fields[0]));
    entry.offset = Decimal(fields[1]); entry.inputBytes = size_t(Decimal(fields[2])); entry.inputHash = fields[3];
    entry.output = std::filesystem::canonical(std::filesystem::u8path(fields[4]));
    entry.outputBytes = size_t(Decimal(fields[5])); entry.outputHash = fields[6];
    auto sourcePath = InputPath(entry.source.u8string()), outputPath = InputPath(entry.output.u8string());
    Require(sourcePath.rfind(gameRoot + "endfield_data/", 0) == 0 &&
                (outputPath.rfind(cacheRoot, 0) == 0 || outputPath.rfind(offlineRoot, 0) == 0) &&
                entry.inputBytes >= 48 && entry.outputBytes >= 48 &&
                DigestText(entry.inputHash) && DigestText(entry.outputHash), "loading input table path/size/digest bounds");
    entry.sourceStamp = FileStamp(entry.source); entry.outputStamp = FileStamp(entry.output);
    auto before = EndfieldInput::Snapshot(entry.source, entry.offset);
    auto after = Read(entry.output);
    auto beforeHash = Sha(before.data(), before.size()), afterHash = Sha(after.data(), after.size());
    Require(before.size() == entry.inputBytes && after.size() == entry.outputBytes &&
                Hex(beforeHash.data(), beforeHash.size()) == entry.inputHash &&
                Hex(afterHash.data(), afterHash.size()) == entry.outputHash,
            "loading input current source/output checksum mismatch");
    Require(result.control == "raw-copy" ? before == after : before != after,
            "loading input control/replacement byte contract mismatch");
    auto original = EndfieldInput::Decode(before);
    auto rebuilt = EndfieldInput::Decode(after);
    Require(original.members.size() == rebuilt.members.size(), "loading input member count changed");
    for (size_t i = 0; i < original.members.size(); ++i)
      Require(original.members[i].name == rebuilt.members[i].name && original.members[i].flags == rebuilt.members[i].flags,
              "loading input member identity/flags changed");
    if (control)
      for (size_t i = 0; i < original.members.size(); ++i)
        Require(original.members[i].bytes == rebuilt.members[i].bytes,
                "loading input control changed decoded content");
    Require(SameStamp(entry.sourceStamp, FileStamp(entry.source)) && SameStamp(entry.outputStamp, FileStamp(entry.output)),
            "loading input changed during validation");
    Require(result.entries.emplace(std::make_pair(sourcePath, entry.offset), std::move(entry)).second,
            "duplicate loading input file/range");
  }
  Require(package.composed || !result.entries.empty(), "loading input table has no substitutions");
  Require(!control || result.entries.size() == 1, "loading input control requires exactly one range");
  return result;
}
}  // namespace EiemNativeInput
