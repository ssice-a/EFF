#include "../../src/eiem_demand_author_resources.h"
#include <iostream>

using namespace EiemNativeInput;

namespace {
std::vector<std::filesystem::path> ModRoots(const std::filesystem::path& root) {
  if (std::filesystem::is_regular_file(root / "mod.ini")) return {root};
  std::vector<std::filesystem::path> result;
  if (!std::filesystem::is_directory(root)) return result;
  for (const auto& row : std::filesystem::directory_iterator(root))
    if (row.is_directory() && std::filesystem::is_regular_file(row.path() / "mod.ini"))
      result.push_back(row.path());
  std::sort(result.begin(), result.end());
  return result;
}
}

int wmain(int argc,wchar_t** argv){try{
  EiemDirectory::Require(argc==2,"audit_author_assets <Format 3 Mod or mods directory>");
  const auto root=std::filesystem::canonical(std::filesystem::path(argv[1]));size_t mods=0,totalMeshes=0,totalMaterials=0,totalTextures=0,totalPhysics=0,totalEffects=0;
  for(const auto& modRoot:ModRoots(root)){
    EiemDemandAuthorResources::Catalog catalog(modRoot);const auto& directory=catalog.Directory();
    std::set<Identity> targets;for(const auto& rule:directory.rules)for(const auto& target:rule.targets)targets.insert(target.second);
    size_t meshes=0,materials=0,textures=0,physics=0,effects=0;
    for(const auto& asset:directory.textures){try{auto value=catalog.GetTexture(asset.id);EiemDirectory::Require(value->input&&value->replacement.bytes>0&&value->cab.size()>0,"Texture scaffold/DDS did not produce a runtime input");++textures;}catch(const std::exception& error){throw std::runtime_error("Texture "+asset.id+" ("+asset.file+"): "+error.what());}}
    for(const auto& asset:directory.meshes){try{auto value=catalog.GetMesh(asset.part);EiemDirectory::Require(value->declaration.part==asset.part&&value->declaration.file==asset.file&&value->declaration.material==asset.material&&value->declaration.nativeBytes>0&&value->declaration.authorBytes>0&&value->metadataReadBytes>0,"Mesh resource identity or native/author payload is incomplete");++meshes;}catch(const std::exception& error){throw std::runtime_error("Mesh "+asset.part+" ("+asset.file+"): "+error.what());}}
    for(const auto& asset:directory.materials){try{auto value=catalog.GetMaterial(asset.id);EiemDirectory::Require(value->declaration.id==asset.id&&value->declaration.file==asset.file&&value->declaration.source.path==asset.source.path&&value->declaration.source.asset==asset.source.asset&&value->metadataReadBytes>0,"Material resource identity or author edits are incomplete");++materials;}catch(const std::exception& error){throw std::runtime_error("Material "+asset.id+" ("+asset.file+"): "+error.what());}}
    physics=catalog.GetPhysics(targets).size();
    Package controls;EiemAuthorControlMetadata::Apply(directory.runtime,controls);catalog.BindEffects(controls);
    for(const auto& effect:controls.effects)for(const auto& bundle:effect.second.native->bundles){auto view=bundle.open();EiemDirectory::Require(view&&view->Size()==bundle.Size(),"effect native input failed to open");++effects;}
    EiemDirectory::Require(meshes==directory.meshes.size()&&materials==directory.materials.size(),"declared Mesh or Material resources were not opened");
    EiemDirectory::Require(textures==directory.textures.size(),"declared Texture resources were not opened");
    std::cout<<directory.mod<<" meshes="<<meshes<<" materials="<<materials<<" textures="<<textures
      <<" physics="<<physics<<" effectBundles="<<effects<<" rules="<<directory.rules.size()<<'\n';
    totalMeshes+=meshes;totalMaterials+=materials;totalTextures+=textures;totalPhysics+=physics;totalEffects+=effects;++mods;
  }
  EiemDirectory::Require(mods>0,"no Format 3 Mod directories found");
  std::cout<<"PASS author assets mods="<<mods<<" meshes="<<totalMeshes<<" materials="<<totalMaterials
    <<" textures="<<totalTextures<<" physics="<<totalPhysics<<" effectBundles="<<totalEffects<<'\n';return 0;
}catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}}
