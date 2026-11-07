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
        "[MaterialBody]\npath=author/materials/body.mat\nsource.path=packages/com.hg.render-pipelines/runtime/renderpipelineresources/material/defaultmaterial.mat\nsource.asset=Body\ntexture._BaseMap=TextureBody\n\n"
        "[TextureBody]\npath=author/textures/body.texmeta\ndds=editable/custom/body.dds\n"
        "source.path=packages/com.hg.render-pipelines/runtime/renderpipelineresources/texture/defaulttexture.tex\nsource.asset=DefaultTexture\n"
        "linear=true\nfilter=2\nwrap=1\naniso=8\nmip_bias=-0.5\n\n"
        "[ResourceEffect_0123456789ABCDEF]\ntype=effect\npath=effects/spark.effres\ntargets=ReplaceBody\n\n"
        "[ResourceEffectAuthor_Spark]\ntype=effect-author\npath=effects/author/spark.effres\n\n"
        "[ResourceControls]\ntype=controls\npath=author/controls.effres\n\n"
        "[PartBody]\nmesh=MeshBody\nmaterial=MaterialBody\nvisible=1\n\n"
        "[ReplaceBody]\ntarget.path=packages/com.hg.game/runtime/characters/body.asset\ntarget.asset=Body\nparts=PartBody\n";
    { std::ofstream output(root / "mod.ini", std::ios::binary); output << ini; }

    auto manifest = EiemRuntimeAuthorManifest::Read(root);
    Require(manifest.id == "FormatThree" && manifest.resources.size() == 9 &&
                manifest.parts.size() == 1 && manifest.targets.size() == 1,
            "format 3 relationships differ");
    Require(manifest.targets.front().source.path ==
                "packages/com.hg.game/runtime/characters/body.asset",
            "Format 3 Mesh package target identity was not preserved");
    {
      auto routed=ini;auto pos=routed.find("target.asset=Body\n");
      routed.insert(pos+std::string("target.asset=Body\n").size(),
        "target.bundle=Bundles/Windows/main/weapon.ab\nlod.1.path=assets/weapon.fbx\nlod.1.asset=LOD1\nlod.1.bundle=Bundles/Windows/main/weapon-lod.ab\n");
      {std::ofstream output(root/"mod.ini",std::ios::binary);output<<routed;}
      auto targets=EiemRuntimeAuthorManifest::Read(root).targets;
      Require(targets.size()==2,"dependency-only Mesh route lost an LOD target");
      Require(targets[0].bundle=="main/weapon.ab"&&targets[1].bundle=="main/weapon-lod.ab",
        "dependency-only Mesh route changed its logical Bundle identity");
      {std::ofstream output(root/"mod.ini",std::ios::binary);output<<ini;}
    }
    Require(manifest.resources.at("TextureBody").dds == "editable/custom/body.dds",
            "user-selected DDS path was not preserved");
    Require(manifest.resources.at("MaterialBody").source.path ==
                "packages/com.hg.render-pipelines/runtime/renderpipelineresources/material/defaultmaterial.mat" &&
                manifest.resources.at("TextureBody").source.path ==
                "packages/com.hg.render-pipelines/runtime/renderpipelineresources/texture/defaulttexture.tex",
            "Format 3 Material/Texture package source identities were not preserved");
    const auto& sampling = manifest.resources.at("TextureBody").textureSettings;
    Require(sampling.linear && sampling.filter == 2 && sampling.wrap == 1 &&
                sampling.aniso == 8 && sampling.mipBias == -0.5,
            "Format 3 Texture semantic/sampling settings were not parsed");
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

    auto inheritedMaterial = ini;
    const auto oldPart = std::string("[PartBody]\nmesh=MeshBody\nmaterial=MaterialBody\nvisible=1\n");
    const auto partAt = inheritedMaterial.find(oldPart);
    Require(partAt != inheritedMaterial.npos, "smoke Part fixture is absent");
    inheritedMaterial.replace(partAt, oldPart.size(),
        "[PartBody]\nmesh=MeshBody\nmaterial_slot=2\nvisible=1\n");
    { std::ofstream output(root / "mod.ini", std::ios::binary | std::ios::trunc);
      output << inheritedMaterial; }
    auto inheritedManifest = EiemRuntimeAuthorManifest::Read(root);
    Require(inheritedManifest.parts.at("PartBody").material.empty(),
            "Format 3 Part without material did not request source material inheritance");
    Require(inheritedManifest.parts.at("PartBody").materialSlot == 2 &&
                inheritedManifest.targets.front().resources.size() == 1,
            "Format 3 inherited material slot or target resource set differs");

    bool traversalRejected = false;
    { std::ofstream output(root / "mod.ini", std::ios::binary | std::ios::trunc);
      output << ini.substr(0, ini.find("author/characters/body.mesh"))
             << "../outside.mesh\n" << ini.substr(ini.find("\n\n[MaterialBody]")); }
    try { (void)EiemRuntimeAuthorManifest::Read(root); } catch (const std::exception&) { traversalRejected = true; }
    Require(traversalRejected, "format 3 accepted a resource path outside the Mod root");

    { std::ofstream output(root / "mod.ini", std::ios::binary | std::ios::trunc);
      auto invalidTarget = ini;
      const auto oldTarget = std::string("target.path=packages/com.hg.game/runtime/characters/body.asset");
      const auto targetAt = invalidTarget.find(oldTarget);
      Require(targetAt != invalidTarget.npos, "smoke target path fixture is absent");
      invalidTarget.replace(targetAt, oldTarget.size(),
          "target.path=packages/../assets/body.asset");
      output << invalidTarget; }
    bool traversalTargetRejected = false;
    try { (void)EiemRuntimeAuthorManifest::Read(root); }
    catch (const std::exception&) { traversalTargetRejected = true; }
    Require(traversalTargetRejected, "format 3 accepted a traversing Mesh target path");

    { std::ofstream output(root / "mod.ini", std::ios::binary | std::ios::trunc);
      output << ini << "\n[TextureAlias]\npath=author/textures/body.texmeta\n"
                "dds=editable/other.dds\nlinear=true\n"; }
    bool textureAliasRejected = false;
    try { (void)EiemRuntimeAuthorManifest::Read(root); }
    catch (const std::exception&) { textureAliasRejected = true; }
    Require(textureAliasRejected, "format 3 accepted conflicting semantics for one Texture file");

    { std::ofstream output(root / "mod.ini", std::ios::binary | std::ios::trunc);
      output << "[Mod]\nformat=2\nid=Legacy\n\n[Constants]\n"; }
    bool legacyRejected = false;
    try { (void)EiemRuntimeAuthorManifest::Read(root); } catch (const std::exception&) { legacyRejected = true; }
    Require(legacyRejected, "runtime accepted a Mod Manager migration input");

    std::filesystem::remove_all(root);
    std::cout << "PASS Format 3 manifest validates relationships only; payloads remain unopened, traversal and legacy format are rejected\n";
    return 0;
  } catch (const std::exception& error) {
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
  }
}
