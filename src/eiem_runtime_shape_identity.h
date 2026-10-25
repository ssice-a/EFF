#pragma once
#include "eiem_resource_directory.h"
namespace EiemDirectory {
// Serialized identity belongs to the owner, part and exact target. Resource
// versions may change without rebuilding the template containing this name.
inline std::string RuntimeShapeLabel(const Package& package,const std::string& partId,const Identity& target) {
  auto owner=package.partOwners.find(partId);
  const auto& mod=owner==package.partOwners.end()?package.id:owner->second.first;
  const auto& partName=owner==package.partOwners.end()?partId:owner->second.second;
  return mod + "\n" + partName + "\n" + target.path + "\n" + target.asset;
}
inline std::string RuntimeShapeName(const Package& package, const std::string& partId,
                                    const Identity& target) {
  auto label = "EFF_STABLE_PART/1\n" + RuntimeShapeLabel(package,partId,target);
  auto token = Sha((const unsigned char*)label.data(),label.size());
  return target.asset + "__EFF_SHAPE_" + Hex(token.data(),token.size());
}
// Released packages used a content-dependent name. Register it as an alias so
// they retain shape and visibility controls with the new reader.
inline std::string LegacyRuntimeShapeName(const Package& package,const std::string& partId,const Identity& target) {
  const auto& resource=*package.resources.at(package.meshes.at(package.parts.at(partId).mesh).resource);
  auto digest=Sha(resource.bytes.data(),resource.bytes.size());
  auto label=RuntimeShapeLabel(package,partId,target)+"\n"+Hex(digest.data(),digest.size());
  auto token = Sha((const unsigned char*)label.data(),label.size());
  return target.asset + "__EFF_SHAPE_" + Hex(token.data(),token.size());
}
// Dynamic visibility needs the same provenance even for shape-free Meshes.
inline bool RuntimeVisibility(const EiemExpression& expression) {
  return expression.op==EiemExpression::Variable || (expression.left && RuntimeVisibility(*expression.left)) ||
      (expression.right && RuntimeVisibility(*expression.right));
}
inline bool RuntimeControlled(const Part& part) { return !part.shapes.empty() || RuntimeVisibility(*part.visible); }
inline std::string RuntimePartMarker(const Package& package,const std::string& partId,std::string name) {
  if (!package.meshes.at(package.parts.at(partId).mesh).shapes.empty()) return name;
  auto start=name.find("__EFF_SHAPE_");Require(start!=std::string::npos,"runtime part provenance prefix");
  name.replace(start,12,"__EFF_PART_");return name;
}
inline std::string RuntimePartName(const Package& package,const std::string& partId,const Identity& target) {
  return RuntimePartMarker(package,partId,RuntimeShapeName(package,partId,target));
}
inline std::string LegacyRuntimePartName(const Package& package,const std::string& partId,const Identity& target) {
  return RuntimePartMarker(package,partId,LegacyRuntimeShapeName(package,partId,target));
}
}
