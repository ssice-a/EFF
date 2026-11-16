#include "../../src/eiem_demand_author_resources.h"
#include "../../src/eiem_control_panel.h"
#include "mod_animation_identity.h"
#include <iostream>

// Read the actual legacy package through the current game-side demand reader.
// Python metadata inspection alone does not establish runtime compatibility.
int main(int argc,char** argv){try{
 using namespace EiemDirectory;
 Require(argc==2,"verify_mod_shape_experiment <Format 3 Mod>");
 EiemDemandAuthorResources::Catalog catalog(std::filesystem::u8path(argv[1]));
 const auto& d=catalog.Directory();size_t channels=0;
 for(const auto& m:d.meshes){auto mesh=catalog.GetMesh(m.part);Require(bool(mesh),"Mesh failed to open");}
 for(const auto& m:d.materials)Require(bool(catalog.GetMaterial(m.id)),"Material failed to open");
 for(const auto& t:d.textures)Require(bool(catalog.GetTexture(t.id)),"Texture failed to open");
 for(const auto& clip:d.runtime.animations){
  channels+=clip.second.tracks.size();
  for(const auto& track:clip.second.tracks){size_t uses=0;
   for(const auto& part:d.runtime.parts)for(const auto& shape:part.second.shapes)uses+=shape.second==track.first;
   Require(uses>0,"Unbound animation variable: "+track.first);
  }
 }
 Require(!d.runtime.animations.empty(),"No source animation");
 Package instance;instance.root=catalog.Root();instance.authorId=d.mod;
 instance.id=d.mod+"_INSTANCE_"+std::string(64,'A');
 auto other=instance;other.root=instance.root.parent_path();other.id=d.mod+"_INSTANCE_"+std::string(64,'B');
 std::vector<Package> installed{other,instance};
 Require(EiemModAnimationExperiment::Installed(installed,instance.root,d.mod).id==instance.id,
         "native animation resolved author ID instead of the installed copy");
 Require(EiemControlPanel::ModLabel(instance).find("_INSTANCE_")==std::string::npos,
         "panel exposed runtime identity digest");
 auto labelled=instance;labelled.root=instance.root/"Readable Mod";
 Require(EiemControlPanel::ModLabel(labelled)=="Readable Mod  ("+d.mod+")","panel lost the readable author identity");
 std::cout<<"PASS installed Mod animation identity and panel label: copies remain isolated without exposing hashes\n";
 std::cout<<"PASS current runtime catalog: mod="<<d.mod<<" meshes="<<d.meshes.size()<<" materials="<<d.materials.size()
          <<" textures="<<d.textures.size()<<" clips="<<d.runtime.animations.size()<<" animatedChannels="<<channels<<'\n';
 return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
