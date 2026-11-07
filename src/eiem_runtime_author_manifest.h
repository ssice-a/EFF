#pragma once
#include "eiem_mesh_target_directory.h"
#include "eiem_runtime_author_directory.h"
#include <cerrno>
#include <cstdlib>

// Startup manifest view. It validates relationships and paths but deliberately
// does not stat, hash, or read the resource payloads.
namespace EiemRuntimeAuthorManifest {
using namespace EiemDirectory;
namespace D = EiemRuntimeAuthorDirectory;
struct TextureSettings {
  bool linear = false;
  unsigned filter = 1, wrap = 0, aniso = 1;
  double mipBias = 0.0;
};
struct Resource {
  std::string id;
  unsigned kind = 0;
  std::string type;
  std::string path;
  std::string dds;
  std::string author;
  std::string skeleton;
  Identity source;
  TextureSettings textureSettings;
  std::map<std::string, std::string> textureBindings;
  std::vector<std::string> targetRules;
};
struct Part {
  std::string mesh;
  std::string material;
  unsigned materialSlot = 0;
};
struct Manifest {
  std::filesystem::path root;
  std::string id;
  std::map<std::string, Resource> resources;
  std::map<std::string, Part> parts;
  std::vector<EiemMeshTargetDirectory::Target> targets;
};

inline void ResourcePath(const std::string& value) {
  D::Relative(value);
}

inline unsigned TextureInteger(const std::map<std::string, std::string>& values,
                               const char* name, unsigned fallback, unsigned maximum) {
  auto found = values.find(name);
  if (found == values.end()) return fallback;
  const auto& text = found->second;
  Require(!text.empty() && text.find_first_not_of("0123456789") == text.npos,
          std::string("invalid Texture setting: ") + name);
  auto parsed = std::stoul(text);
  Require(parsed <= maximum, std::string("Texture setting out of range: ") + name);
  return unsigned(parsed);
}

inline double TextureNumber(const std::map<std::string, std::string>& values,
                            const char* name, double fallback) {
  auto found = values.find(name);
  if (found == values.end()) return fallback;
  const auto& text = found->second;
  errno = 0;
  char* end = nullptr;
  auto parsed = std::strtod(text.c_str(), &end);
  Require(end == text.c_str() + text.size() && end != text.c_str() &&
              errno != ERANGE && std::isfinite(parsed),
          std::string("invalid Texture setting: ") + name);
  return parsed;
}

inline unsigned PartMaterialSlot(const std::map<std::string, std::string>& values) {
  auto found = values.find("material_slot");
  if (found == values.end()) return 0;
  const auto& text = found->second;
  Require(!text.empty() && text.find_first_not_of("0123456789") == text.npos,
          "invalid Part material_slot");
  auto parsed = std::stoull(text);
  Require(parsed <= UINT_MAX, "Part material_slot is out of range");
  return unsigned(parsed);
}

inline Manifest Read(const std::filesystem::path& root,
                     const std::vector<Section>& sections) {
  Manifest result;
  result.root = std::filesystem::canonical(root);
  std::map<std::string, const Section*> index;
  for (const auto& section : sections)
    Require(index.emplace(section.id, &section).second, "duplicate author manifest section");

  auto mod = index.find("Mod");
  Require(mod != index.end(), "format 3 Mod section absent");
  Keys(*mod->second, {"format", "id"}, {"key_switch_enabled", "type"});
  Require(mod->second->values.at("format") == "3", "runtime accepts format 3 Mods only");
  Require(Identifier(mod->second->values.at("id")), "invalid format 3 Mod id");
  result.id = mod->second->values.at("id");

  std::string controlsPath;
  for (const auto& section : sections) {
    const auto& values = section.values;
    Resource resource;
    resource.id = section.id;
    bool isResource = false;
    if (Begins(section.id, "Mesh")) {
      Keys(section, {"path"}, {"skeleton"});
      resource.kind = 1;
      resource.path = values.at("path");
      if(values.count("skeleton"))resource.skeleton=values.at("skeleton");
      isResource = true;
    } else if (Begins(section.id, "Skeleton")) {
      Keys(section, {"type", "path"});
      Require(section.id.size()>strlen("Skeleton")&&values.at("type")=="skeleton",
        "format 3 Skeleton declaration identity/type differs");
      resource.kind=8;resource.type="skeleton";resource.path=values.at("path");
      resource.skeleton=section.id.substr(strlen("Skeleton"));
      Require(Identifier(resource.skeleton),"invalid format 3 Skeleton id");
      isResource=true;
    } else if (Begins(section.id, "ShapeAnimation")) {
      Keys(section, {"path"});
      Require(section.id.size() > strlen("ShapeAnimation"), "empty ShapeAnimation id");
      resource.kind = 6;
      resource.type = "shape-animation";
      resource.path = values.at("path");
      isResource = true;
    } else if (Begins(section.id, "MaterialAnimation")) {
      Keys(section, {"path"});
      Require(section.id.size() > strlen("MaterialAnimation"), "empty MaterialAnimation id");
      resource.kind = 6;
      resource.type = "material-animation";
      resource.path = values.at("path");
      isResource = true;
    } else if (Begins(section.id, "EffectAttachment")) {
      Keys(section, {"path"}, {"author"});
      Require(section.id.size() > strlen("EffectAttachment"), "empty EffectAttachment id");
      resource.kind = 6;
      resource.type = "effect-attachment";
      resource.path = values.at("path");
      if(values.count("author"))resource.author=values.at("author");
      isResource = true;
    } else if (Begins(section.id, "Material")) {
      Keys(section, {"path", "source.path", "source.asset"}, {}, {"texture."});
      resource.kind = 2;
      resource.path = values.at("path");
      resource.source = Source(values, "source.");
      for (const auto& row : values)
        if (Begins(row.first, "texture.")) {
          Require(row.first.size() > 8, "empty Material texture property");
          resource.textureBindings.emplace(row.first.substr(8), row.second);
        }
      isResource = true;
    } else if (Begins(section.id, "Texture")) {
      Keys(section, {"path", "dds"}, {"source.path", "source.asset", "linear", "filter", "wrap", "aniso", "mip_bias"});
      resource.kind = 3;
      resource.path = values.at("path");
      resource.dds = values.at("dds");
      if (auto linear = values.find("linear"); linear != values.end()) {
        Require(linear->second == "true" || linear->second == "false" ||
                    linear->second == "1" || linear->second == "0",
                "Texture linear setting is invalid");
        resource.textureSettings.linear = linear->second == "true" || linear->second == "1";
      }
      resource.textureSettings.filter = TextureInteger(values, "filter", 1, 2);
      resource.textureSettings.wrap = TextureInteger(values, "wrap", 0, 3);
      resource.textureSettings.aniso = TextureInteger(values, "aniso", 1, 16);
      resource.textureSettings.mipBias = TextureNumber(values, "mip_bias", 0.0);
      if (values.count("source.path") || values.count("source.asset")) {
        Require(values.count("source.path") && values.count("source.asset"), "Texture source identity is incomplete");
        resource.source = Source(values, "source.");
      }
      isResource = true;
    } else if (Begins(section.id, "Resource")) {
      Keys(section, {"type", "path"}, {"targets"});
      const auto& type = values.at("type");
      Require(type == "physics" || type == "effect" || type == "controls" || type == "effect-author",
              "unknown format 3 resource type");
      resource.kind = type == "physics" ? 4 : type == "effect" ? 5 : type == "effect-author" ? 7 : 6;
      resource.type = type;
      resource.path = values.at("path");
      if (type == "controls") {
        Require(section.id == "ResourceControls" && controlsPath.empty(),
                "format 3 Controls resource must be declared once as ResourceControls");
        controlsPath = resource.path;
      }
      if (values.count("targets")) resource.targetRules = Split(values.at("targets"));
      isResource = true;
    } else if (Begins(section.id, "Part")) {
      Keys(section, {"mesh", "visible"}, {"material", "material_slot"}, {"shape."});
      auto material = values.find("material");
      result.parts.emplace(section.id, Part{values.at("mesh"),
          material == values.end() ? std::string{} : material->second,
          PartMaterialSlot(values)});
    }
    if (!isResource) continue;
    ResourcePath(resource.path);
    if (resource.kind == 3) ResourcePath(resource.dds);
    Require(result.resources.emplace(resource.id, std::move(resource)).second,
            "duplicate format 3 resource id");
  }

  Require(!controlsPath.empty(), "format 3 Controls resource is absent");
  std::map<std::string, Resource> textureFiles;
  for (const auto& row : result.resources) {
    const auto& resource = row.second;
    if (resource.kind != 3) continue;
    auto inserted = textureFiles.emplace(resource.path, resource);
    if (!inserted.second) {
      const auto& prior = inserted.first->second;
      const auto& a = prior.textureSettings;
      const auto& b = resource.textureSettings;
      Require(prior.dds == resource.dds && a.linear == b.linear && a.filter == b.filter &&
                  a.wrap == b.wrap && a.aniso == b.aniso && a.mipBias == b.mipBias,
              "shared Texture resource path has conflicting DDS or sampling semantics: " + resource.path);
    }
  }

  std::set<std::string> skeletonIds;
  for(const auto& row:result.resources)if(row.second.type=="skeleton")
    Require(skeletonIds.insert(row.second.skeleton).second,"duplicate format 3 Skeleton id");
  for(const auto& row:result.resources)if(row.second.kind==1&&!row.second.skeleton.empty())
    Require(skeletonIds.count(row.second.skeleton),"Mesh Skeleton is unresolved: "+row.first);
  for (const auto& row : result.resources) {
    const auto& resource = row.second;
    if(resource.type=="effect-attachment"&&!resource.author.empty()){
      auto author=result.resources.find(resource.author);
      Require(author!=result.resources.end()&&author->second.type=="effect-author",
        "format 3 Effect author resource is unresolved: "+resource.author);
    }
    if (resource.type == "shape-animation" || resource.type == "material-animation" ||
        resource.type == "effect-attachment")
      Require(resource.path == controlsPath,
              "format 3 control declaration does not reference ResourceControls: " + row.first);
  }

  for (const auto& row : result.parts) {
    auto mesh = result.resources.find(row.second.mesh);
    Require(mesh != result.resources.end() && mesh->second.kind == 1,
            "Part Mesh resource is unresolved: " + row.first);
    if (!row.second.material.empty()) {
      auto material = result.resources.find(row.second.material);
      Require(material != result.resources.end() && material->second.kind == 2,
              "Part Material resource is unresolved: " + row.first);
      for (const auto& binding : material->second.textureBindings) {
        auto texture = result.resources.find(binding.second);
        Require(texture != result.resources.end() && texture->second.kind == 3,
                "Material Texture resource is unresolved: " + row.first);
      }
    }
  }

  result.targets = EiemMeshTargetDirectory::Read(result.root, sections);
  std::set<std::string> rules;
  for (const auto& target : result.targets) rules.insert(target.rule);
  for (const auto& row : result.resources)
    for (const auto& rule : row.second.targetRules)
      Require(rules.count(rule), "Resource target rule is unresolved: " + rule);
  for (const auto& target : result.targets)
    for (const auto& part : target.partIds)
      Require(result.parts.count(part), "Replace target Part is unresolved: " + target.rule);
  for (const auto& row : result.parts)
    Require(result.resources.count(row.second.mesh) &&
                (row.second.material.empty() || result.resources.count(row.second.material)),
            "unreferenced Part has missing resources: " + row.first);
  return result;
}
inline Manifest Read(const std::filesystem::path& root) {
  const auto canonicalRoot = std::filesystem::canonical(root);
  const auto sections = ParseIni(EiemDirectory::Read(canonicalRoot / "mod.ini"));
  return Read(canonicalRoot, sections);
}
} // namespace EiemRuntimeAuthorManifest
