#pragma once
#include "eiem_mesh_target_directory.h"
#include "eiem_runtime_author_directory.h"

// Startup manifest view. It validates relationships and paths but deliberately
// does not stat, hash, or read the resource payloads.
namespace EiemRuntimeAuthorManifest {
using namespace EiemDirectory;
namespace D = EiemRuntimeAuthorDirectory;
struct Resource {
  std::string id;
  unsigned kind = 0;
  std::string type;
  std::string path;
  std::string dds;
  std::string author;
  Identity source;
  std::map<std::string, std::string> textureBindings;
  std::vector<std::string> targetRules;
};
struct Part {
  std::string mesh;
  std::string material;
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

inline Manifest Read(const std::filesystem::path& root) {
  Manifest result;
  result.root = std::filesystem::canonical(root);
  const auto sections = ParseIni(EiemDirectory::Read(result.root / "mod.ini"));
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
      Keys(section, {"path"});
      resource.kind = 1;
      resource.path = values.at("path");
      isResource = true;
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
      Keys(section, {"mesh", "material", "visible"}, {}, {"shape."});
      result.parts.emplace(section.id, Part{values.at("mesh"), values.at("material")});
    }
    if (!isResource) continue;
    ResourcePath(resource.path);
    if (resource.kind == 3) ResourcePath(resource.dds);
    Require(result.resources.emplace(resource.id, std::move(resource)).second,
            "duplicate format 3 resource id");
  }

  Require(!controlsPath.empty(), "format 3 Controls resource is absent");
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
    auto material = result.resources.find(row.second.material);
    Require(mesh != result.resources.end() && mesh->second.kind == 1,
            "Part Mesh resource is unresolved: " + row.first);
    Require(material != result.resources.end() && material->second.kind == 2,
            "Part Material resource is unresolved: " + row.first);
    for (const auto& binding : material->second.textureBindings) {
      auto texture = result.resources.find(binding.second);
      Require(texture != result.resources.end() && texture->second.kind == 3,
              "Material Texture resource is unresolved: " + row.first);
    }
  }

  result.targets = EiemMeshTargetDirectory::Read(result.root);
  std::set<std::string> rules;
  for (const auto& target : result.targets) rules.insert(target.rule);
  for (const auto& row : result.resources)
    for (const auto& rule : row.second.targetRules)
      Require(rules.count(rule), "Resource target rule is unresolved: " + rule);
  for (const auto& target : result.targets)
    for (const auto& part : target.partIds)
      Require(result.parts.count(part), "Replace target Part is unresolved: " + target.rule);
  for (const auto& row : result.parts)
    Require(result.resources.count(row.second.mesh) && result.resources.count(row.second.material),
            "unreferenced Part has missing resources: " + row.first);
  return result;
}
} // namespace EiemRuntimeAuthorManifest
