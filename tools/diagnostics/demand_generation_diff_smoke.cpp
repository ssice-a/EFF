#include "../../src/eiem_demand_generation_diff.h"
#include <iostream>
#include <stdexcept>

using namespace EiemDemandGenerationDiff;
void Require(bool value, const char* message) {
  if (!value) throw std::runtime_error(message);
}
Target TargetRow(const char* path, const char* asset) {
  Target target;
  target.rule = "Replace";
  target.lod = 0;
  target.path = path;
  target.asset = asset;
  target.parts.insert("body");
  return target;
}
int main() {
  try {
    Snapshot old;
    old["chen"].root = "mods/chen";
    old["chen"].files["mod.ini"] = "1/10/a";
    old["chen"].files["meshes/body.effmesh"] = "2/20/b";
    old["chen"].targets.insert(TargetRow("assets/mesh/body", "MeshBody"));

    Require(!Compare(old, old).changed, "unchanged generation was marked changed");

    auto texturePath = old;
    texturePath["chen"].files["mod.ini"] = "1/11/edited-dds-path";
    auto texturePathDelta = Compare(old, texturePath);
    Require(texturePathDelta.changed &&
                texturePathDelta.modifiedPackages.count("chen") &&
                !texturePathDelta.targetDeclarationsChanged &&
                texturePathDelta.targets.empty(),
            "an INI-only DDS path change did not mark its Mod resource generation modified");

    auto added = old;
    added["lino"].root = "mods/lino";
    added["lino"].files["mod.ini"] = "1/11/c";
    added["lino"].targets.insert(TargetRow("assets/mesh/hair", "MeshHair"));
    auto addDelta = Compare(old, added);
    Require(addDelta.changed && addDelta.packages.count("lino") &&
                addDelta.addedPackages.count("lino") &&
                addDelta.targetDeclarationsChanged &&
                addDelta.targets.count({"assets/mesh/hair", "MeshHair"}),
            "added Mod did not contribute its targets");

    auto removed = Compare(added, old);
    Require(removed.changed && removed.packages.count("lino") &&
                removed.removedPackages.count("lino") &&
                removed.targetDeclarationsChanged &&
                removed.targets.count({"assets/mesh/hair", "MeshHair"}),
            "removed Mod targets were omitted from the affected set");

    auto replaced = old;
    replaced["chen"].files["meshes/body.effmesh"] = "2/21/new-file-id";
    replaced["chen"].targets.clear();
    replaced["chen"].targets.insert(TargetRow("assets/mesh/body-v2", "MeshBodyV2"));
    auto replaceDelta = Compare(old, replaced);
    Require(replaceDelta.changed && replaceDelta.packages.count("chen") &&
                replaceDelta.modifiedPackages.count("chen") &&
                replaceDelta.targetDeclarationsChanged &&
                replaceDelta.targets.count({"assets/mesh/body", "MeshBody"}) &&
                replaceDelta.targets.count({"assets/mesh/body-v2", "MeshBodyV2"}),
            "replacement did not include both previous and next targets");

    auto sameName = old;
    sameName["chen"].targets.clear();
    sameName["chen"].targets.insert(TargetRow("assets/other/body", "MeshBody"));
    auto sameNameDelta = Compare(old, sameName);
    Require(sameNameDelta.targets.size() == 2 &&
                sameNameDelta.targets.count({"assets/other/body", "MeshBody"}) &&
                sameNameDelta.targets.count({"assets/mesh/body", "MeshBody"}) &&
                sameNameDelta.targetDeclarationsChanged,
            "same asset name on a different path was conflated or previous identity omitted");

    auto payloadOnly = old;
    payloadOnly["chen"].files["textures/body.dds"] = "4/40/new-file-id";
    auto payloadDelta = Compare(old, payloadOnly);
    Require(payloadDelta.changed &&
                payloadDelta.modifiedPackages.count("chen") &&
                !payloadDelta.targetDeclarationsChanged &&
                payloadDelta.targets.empty(),
            "resource-only edit was treated as a target-graph change");

    std::cout << "PASS Format 3 generation delta: no-op, INI DDS path edit, add, remove, replace\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "FAIL " << error.what() << '\n';
    return 1;
  }
}
