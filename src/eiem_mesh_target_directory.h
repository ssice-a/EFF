#pragma once
#include "eiem_resource_directory.h"
#include <unordered_map>
#include <unordered_set>

// Match declarations only. Reading this directory never opens compiled.bin,
// a game archive, a resource payload, or a list of Prefab consumers.
namespace EiemMeshTargetDirectory {
using namespace EiemDirectory;
struct Target {
  std::string mod, rule;
  std::filesystem::path root;
  std::vector<std::string> partIds;
  unsigned lod = 0;
  Identity source;
  std::vector<std::filesystem::path> resources;
  size_t parts = 0;
  bool skip = false;
  std::string bundle;
};
inline std::string BundleName(std::string value) {
  std::replace(value.begin(),value.end(),'\\','/');
  std::transform(value.begin(),value.end(),value.begin(),[](unsigned char c){return char(std::tolower(c));});
  const std::string prefix="bundles/windows/";
  if(value.rfind(prefix,0)==0)value.erase(0,prefix.size());
  Require(!value.empty()&&value.front()!='/'&&value.find(':')==value.npos,
    "invalid target Bundle logical path");
  for(const auto& segment:std::filesystem::u8path(value))Require(segment!=".."&&segment!=".","target Bundle path escapes game Bundle directory");
  Require(value.size()>3&&value.substr(value.size()-3)==".ab","target Bundle must be a logical .ab path");return value;
}
inline std::vector<Target> Read(const std::filesystem::path& root,
                                const std::vector<Section>& sections) {
  std::map<std::string, const Section*> index;
  for (const auto& s : sections) index.emplace(s.id, &s);
  Require(index.count("Mod"), "Mesh target directory lacks Mod section");
  const auto& mod = index.at("Mod")->values;
  Require(mod.count("id"), "Mesh target directory lacks Mod id");
  if (mod.count("type") && mod.at("type") == "effect") return {};
  auto resource = [&](const std::string& section) {
    Require(index.count(section), "unresolved target resource " + section);
    const auto& values = index.at(section)->values;
    Require(values.count("path"), "target resource path absent: " + section);
    auto relative = std::filesystem::u8path(values.at("path"));
    Require(!relative.is_absolute() && !relative.has_root_name(), "target resource path is absolute");
    for (const auto& segment : relative) Require(segment != "..", "target resource path escapes Mod");
    return root / relative;
  };
  std::vector<Target> result;
  for (const auto& s : sections) {
    if (!Begins(s.id, "Replace")) continue;
    Keys(s, {"target.path", "target.asset"}, {"parts", "handling", "target.bundle"}, {"lod."});
    Target target; target.mod = mod.at("id"); target.rule = s.id; target.root=root;
    target.source = Source(s.values, "target."); target.skip = s.values.count("handling") != 0;
    if(s.values.count("target.bundle"))target.bundle=BundleName(s.values.at("target.bundle"));
    Require(target.skip != bool(s.values.count("parts")), "target requires parts or skip");
    if (target.skip) Require(s.values.at("handling") == "skip", "unknown target handling");
    else for (const auto& part : Split(s.values.at("parts"))) {
      Require(index.count(part), "unresolved target part " + part);
      const auto& values = index.at(part)->values;
      Require(values.count("mesh") && values.count("material"), "target part lacks Mesh/Material");
      target.resources.push_back(resource(values.at("mesh")));
      target.resources.push_back(resource(values.at("material")));
      target.partIds.push_back(part);
      ++target.parts;
    }
    const auto basePath=target.source.path,baseBundle=target.bundle;
    result.push_back(target);
    std::map<unsigned, Settings> lods;
    for (const auto& row : s.values) if (Begins(row.first, "lod.")) {
      auto end = row.first.find('.', 4);
      Require(end != std::string::npos, "invalid Mesh target LOD declaration");
      auto lod = Integer(row.first.substr(4, end - 4));
      Require(lod > 0, "Mesh target additional LOD must be positive");
      lods[lod][row.first.substr(end + 1)] = row.second;
    }
    for (const auto& lod : lods) {
      Require((lod.second.size()==2||(lod.second.size()==3&&lod.second.count("bundle")))&&lod.second.count("path")&&lod.second.count("asset"), "incomplete Mesh target LOD");
      target.lod = lod.first; target.source = Source(lod.second, "");
      target.bundle=lod.second.count("bundle")?BundleName(lod.second.at("bundle")):
        target.source.path==basePath?baseBundle:std::string{};
      result.push_back(target);
    }
  }
  return result;
}
inline std::vector<Target> Read(const std::filesystem::path& root) {
  const auto sections = ParseIni(EiemDirectory::Read(root / "mod.ini"));
  return Read(root, sections);
}
// The caller supplies only the dependency list already requested by the game.
// Include self: a Prefab and its target Mesh can occupy the same Bundle.
inline bool Candidate(uint64_t self, const std::vector<uint64_t>& dependencies,
                      const std::unordered_set<uint64_t>& targets) {
  if (targets.count(self)) return true;
  for (auto dependency : dependencies) if (targets.count(dependency)) return true;
  return false;
}
} // namespace EiemMeshTargetDirectory
