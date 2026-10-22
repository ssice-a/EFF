#include "../../src/eiem_resource_compiler.h"
#include "../../src/eiem_compiled_mod_writer.h"
#include <iostream>
using namespace EiemCompiledModWriter;
int wmain(int argc,wchar_t** argv){try{
 Require(argc==5,"incremental_mesh_smoke <game> <before-Mod> <incremental-Mod> <full-Mod>");
 const auto game=std::filesystem::canonical(argv[1]);
 auto before=EiemCompiledMod::Read(argv[2],game),fast=EiemCompiledMod::Read(argv[3],game),full=EiemCompiledMod::Read(argv[4],game);
 const auto& author=fast.manifest.package;
 auto rebound=before;auto selection=EiemStaticSources::LoadMod(argv[2],game);EiemCompiledExportCarryover::RebindSources(rebound,selection);
 Require(rebound.verifiedSources.size()==before.verifiedSources.size()&&!rebound.verifiedSources.empty(),"identical source rebind discarded verified stamps");
 auto moved=selection;for(auto& row:moved.inputs)row.second.offset++;
 EiemCompiledExportCarryover::RebindSources(rebound,moved);Require(rebound.verifiedSources.empty(),"relocated source retained old verified stamps");
 Require(!MeshResourceChanges(author,before.manifest.package).empty(),"real Mesh edit did not select incremental path");
 Require(MeshResourceChanges(author,author).empty(),"unchanged Mesh falsely classified as changed");
 auto unsafe=author;unsafe.meshes.begin()->second.vertices++;
 Require(MeshResourceChanges(unsafe,before.manifest.package).empty(),"Mesh vertex-count change accepted as unchanged structure");
 unsafe=author;auto material=unsafe.materials.begin()->second.resource;auto replacement=std::make_shared<Resource>(*unsafe.resources.at(material));replacement->contentHash=std::string(64,'A');unsafe.resources[material]=replacement;
 Require(MeshResourceChanges(unsafe,before.manifest.package).empty(),"combined material change accepted as Mesh-only");
 Require(fast.views.size()==full.views.size(),"Mesh incremental source coverage differs");
 size_t changed=0;
 for(size_t i=0;i<full.views.size();++i){
  Require(fast.manifest.plan.originals[i].hash==full.manifest.plan.originals[i].hash,"Mesh original source differs");
  Require(fast.views[i].startupHash==full.views[i].startupHash&&fast.views[i].size==full.views[i].size,"Mesh incremental native view differs from full compile at input "+std::to_string(i));
  Require(fast.manifest.plan.startupHashes[i]==full.manifest.plan.startupHashes[i],"Mesh incremental member content differs from full compile");
  if(fast.views[i].startupHash!=before.views.at(i).startupHash)++changed;
 }
 Require(changed,"Mesh edit changed no native input");
 auto replay=std::make_shared<const Plan>(fast.manifest.plan);EiemPrefabRules::Context context(replay,author);
 for(size_t i=0;i<fast.views.size();++i)Require(Hash(context.Materialize(i,false))==fast.manifest.plan.startupHashes[i],"Mesh member recipe replay differs");
 std::cout<<"PASS incremental Mesh: "<<fast.views.size()<<" native views exactly equal full build; "<<changed<<" changed inputs; renderer bounds, recipe replay and structural fallback verified\n";
 return 0;
 }catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}}
