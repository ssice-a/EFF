#include "../../src/eiem_demand_author_resources.h"
#include <iostream>

using namespace EiemNativeInput;

int wmain(int argc,wchar_t** argv){try{
  EiemDirectory::Require(argc==2,"audit_author_assets <author-assets-root>");
  const auto root=std::filesystem::canonical(std::filesystem::path(argv[1]));size_t mods=0,totalMeshes=0,totalMaterials=0,totalTextures=0,totalPhysics=0,totalEffects=0;
  for(const auto& row:std::filesystem::directory_iterator(root)){
    if(!row.is_directory()||!std::filesystem::is_regular_file(row.path()/"assets.bin"))continue;
    EiemDemandAuthorResources::Catalog catalog(row.path());const auto& directory=catalog.Directory();
    std::set<Identity> targets;for(const auto& rule:directory.rules)for(const auto& target:rule.targets)targets.insert(target.second);
    size_t meshes=0,materials=0,textures=0,physics=0,effects=0;
    for(const auto& asset:directory.meshes){auto value=catalog.GetMesh(asset.part);EiemDirectory::Require(value->declaration.hash==asset.hash,"Mesh declaration changed while opening");++meshes;}
    for(const auto& asset:directory.materials){auto value=catalog.GetMaterial(asset.id);EiemDirectory::Require(value->declaration.hash==asset.hash&&!value->native.dependencies.empty(),"Material native input/dependencies absent");++materials;}
    for(const auto& asset:directory.textures){auto value=catalog.GetTexture(asset.id);EiemDirectory::Require(value->declaration.hash==asset.hash,"Texture declaration changed while opening");++textures;}
    physics=catalog.GetPhysics(targets).size();
    Package controls;EiemAuthorControlMetadata::Apply(directory.runtime,controls);catalog.BindEffects(controls);
    for(const auto& effect:controls.effects)for(const auto& bundle:effect.second.native->bundles){auto view=bundle.open();EiemDirectory::Require(view&&view->Size()==bundle.Size(),"effect native input failed to open");++effects;}
    EiemDirectory::Require(materials==directory.materials.size()&&materials>0,"complete native Material resources absent");
    std::cout<<directory.mod<<" meshes="<<meshes<<" materials="<<materials<<" textures="<<textures
      <<" physics="<<physics<<" effectBundles="<<effects<<" rules="<<directory.rules.size()<<'\n';
    totalMeshes+=meshes;totalMaterials+=materials;totalTextures+=textures;totalPhysics+=physics;totalEffects+=effects;++mods;
  }
  EiemDirectory::Require(mods>0,"no author Mod directories found");
  std::cout<<"PASS author assets mods="<<mods<<" meshes="<<totalMeshes<<" materials="<<totalMaterials
    <<" textures="<<totalTextures<<" physics="<<totalPhysics<<" effectBundles="<<totalEffects<<'\n';return 0;
}catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}}
