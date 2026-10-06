#pragma once
#include "eiem_resource_directory.h"
namespace EiemDirectory {
// Serialized provenance for a controlled authored part. Source identity and
// resource digest are included; matching a game Mesh name alone is insufficient.
inline std::string RuntimeShapeName(const Package& package, const std::string& partId,
                                    const Identity& target) {
  const auto& part = package.parts.at(partId);
  const auto& mesh = package.meshes.at(part.mesh);
  const auto& resource = *package.resources.at(mesh.resource);
  auto digest = Sha(resource.bytes.data(), resource.bytes.size());
  auto owner=package.partOwners.find(partId);
  const auto& mod=owner==package.partOwners.end()?package.id:owner->second.first;
  const auto& partName=owner==package.partOwners.end()?partId:owner->second.second;
  auto label = mod + "\n" + partName + "\n" + target.path + "\n" + target.asset + "\n" + Hex(digest.data(),digest.size());
  auto token = Sha((const unsigned char*)label.data(),label.size());
  return target.asset + "__EFF_SHAPE_" + Hex(token.data(),token.size());
}
// Dynamic visibility needs the same provenance even for shape-free Meshes.
inline bool RuntimeVisibility(const EiemExpression& expression) {
  return expression.op==EiemExpression::Variable || (expression.left && RuntimeVisibility(*expression.left)) ||
      (expression.right && RuntimeVisibility(*expression.right));
}
inline bool RuntimeControlled(const Part& part) { return !part.shapes.empty() || RuntimeVisibility(*part.visible); }
inline std::string RuntimePartName(const Package& package,const std::string& partId,const Identity& target) {
  if (!package.meshes.at(package.parts.at(partId).mesh).shapes.empty()) return RuntimeShapeName(package,partId,target);
  auto name=RuntimeShapeName(package,partId,target);
  auto start=name.find("__EFF_SHAPE_");Require(start!=std::string::npos,"runtime part provenance prefix");
  name.replace(start,12,"__EFF_PART_");return name;
}
}
