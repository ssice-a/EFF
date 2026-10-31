#include "../../src/eiem_runtime_author_manifest.h"
#include <fstream>
#include <iostream>

using namespace EiemDirectory;
using namespace EiemRuntimeAuthorManifest;

int main() {
  const auto root = std::filesystem::temp_directory_path() /
      ("eff-format3-manifest-" + std::to_string(GetTickCount64()));
  try {
    std::filesystem::create_directories(root);
    const std::string ini =
        "[Mod]\nformat=3\nid=FormatThree\n\n"
        "[Constants]\n$outfit=0\n\n"
        "[MeshBody]\npath=author/characters/body.mesh\n\n"
        "[ShapeAnimationWalk]\npath=author/controls.effres\n\n"
        "[MaterialAnimationGlow]\npath=author/controls.effres\n\n"
        "[EffectAttachmentSpark]\npath=author/controls.effres\nauthor=ResourceEffectAuthor_Spark\n\n"
        "[MaterialBody]\npath=author/materials/body.mat\nsource.path=assets/base/body.mat\nsource.asset=Body\ntexture._BaseMap=TextureBody\n\n"
        "[TextureBody]\npath=author/textures/body.texmeta\ndds=editable/custom/body.dds\n\n"
        "[ResourceEffect_0123456789ABCDEF]\ntype=effect\npath=native/effects/spark.effres\ntargets=ReplaceBody\n\n"
        "[ResourceEffectAuthor_Spark]\ntype=effect-author\npath=native/effect-author/spark.effres\n\n"
        "[ResourceControls]\ntype=controls\npath=author/controls.effres\n\n"
        "[PartBody]\nmesh=MeshBody\nmaterial=MaterialBody\nvisible=1\n\n"
        "[ReplaceBody]\ntarget.path=assets/base/body.asset\ntarget.asset=Body\nparts=PartBody\n";
    { std::ofstream output(root / "mod.ini", std::ios::binary); output << ini; }

    auto manifest = EiemRuntimeAuthorManifest::Read(root);
    Require(manifest.id == "FormatThree" && manifest.resources.size() == 9 &&
                manifest.parts.size() == 1 && manifest.targets.size() == 1,
            "format 3 relationships differ");
    Require(manifest.resources.at("TextureBody").dds == "editable/custom/body.dds",
            "user-selected DDS path was not preserved");
    Require(manifest.resources.at("ShapeAnimationWalk").type == "shape-animation" &&
                manifest.resources.at("MaterialAnimationGlow").type == "material-animation" &&
                manifest.resources.at("EffectAttachmentSpark").type == "effect-attachment" &&
                manifest.resources.at("EffectAttachmentSpark").author == "ResourceEffectAuthor_Spark" &&
                manifest.resources.at("ResourceEffectAuthor_Spark").type == "effect-author" &&
                manifest.resources.at("ResourceEffect_0123456789ABCDEF").targetRules ==
                    std::vector<std::string>{"ReplaceBody"},
            "format 3 control/effect declarations were not parsed");
    Require(!std::filesystem::exists(root / "author/characters/body.mesh") &&
                !std::filesystem::exists(root / "author/materials/body.mat") &&
                !std::filesystem::exists(root / "editable/custom/body.dds"),
            "startup manifest validation opened or required resource payloads");

    bool traversalRejected = false;
    { std::ofstream output(root / "mod.ini", std::ios::binary | std::ios::trunc);
      output << ini.substr(0, ini.find("author/characters/body.mesh"))
             << "../outside.mesh\n" << ini.substr(ini.find("\n\n[MaterialBody]")); }
    try { (void)EiemRuntimeAuthorManifest::Read(root); } catch (const std::exception&) { traversalRejected = true; }
    Require(traversalRejected, "format 3 accepted a resource path outside the Mod root");

    std::filesystem::remove_all(root);
    std::cout << "PASS format 3 manifest validates relationships only; missing payloads remain unopened and traversal is rejected\n";
    return 0;
  } catch (const std::exception& error) {
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
  }
}
