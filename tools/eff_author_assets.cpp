#include "../src/eiem_author_mesh_bundle.h"
#include "../src/eiem_author_texture_bundle.h"
#include "../src/eiem_author_material_edits.h"
#include "../src/eiem_runtime_author_directory.h"
#include "../src/eiem_loading_input_substitution.h"
#include <iostream>
#include <chrono>

// Development/author entry for the new native asset directory. It reads only
// explicitly supplied EFFSOURCE templates, never an installed-game index or
// a Prefab-consumer catalog. The output stays separate until runtime migration.
namespace AuthorAssets {
using namespace EiemNativeInput;
struct Template {std::string kind,cab,hash;int64_t id=0;std::filesystem::path file;};
struct Source {Bytes bytes;SerializedLayout layout;std::string hash;};
using Templates=std::map<Identity,Template>;
inline Templates ReadTemplates(const std::filesystem::path& path){
 auto raw=Read(path);std::istringstream lines(Text(raw.data(),raw.size()));std::string line;
 Require(bool(std::getline(lines,line)),"author template table header absent");if(!line.empty()&&line.back()=='\r')line.pop_back();
 Require(line=="EFF_AUTHOR_TEMPLATES/exact-assets+native-snapshots/tsv","author template table structure differs");Templates result;
 while(std::getline(lines,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty())continue;auto f=TabFields(line);Require(f.size()==7,"author template row fields differ");
  Template t{f[0],f[3],f[6],std::stoll(f[4]),std::filesystem::u8path(f[5])};
  Require((t.kind=="Mesh"||t.kind=="Material"||t.kind=="Texture2D")&&t.id&&!t.cab.empty()&&DigestText(t.hash)&&t.file.is_absolute(),"author template identity/digest/path differs");
  Require(result.emplace(Identity{f[1],f[2]},std::move(t)).second,"author template path+asset duplicated");
 }return result;
}
struct Sources {
 const Templates& templates;std::map<std::filesystem::path,Source> files;
 std::pair<const Template&,const Source&> Get(const Identity& identity,const char* kind){
  auto t=templates.find(identity);Require(t!=templates.end()&&t->second.kind==kind,"exact author template absent: "+identity.path+" / "+identity.asset);
  auto found=files.find(t->second.file);if(found==files.end()){
   auto before=FileStamp(t->second.file);Source file;file.bytes=Read(t->second.file);auto hash=Sha(file.bytes.data(),file.bytes.size());file.hash=Hex(hash.data(),hash.size());
   Require(SameStamp(before,FileStamp(t->second.file))&&file.hash==t->second.hash,"author template content changed");file.layout=ParseSerialized(file.bytes);found=files.emplace(t->second.file,std::move(file)).first;
  }Require(found->second.hash==t->second.hash,"shared template digest differs");
  auto asset=SerializedAssets(found->second.bytes,t->second.cab,found->second.layout);
  Require(std::any_of(asset.begin(),asset.end(),[&](const auto& a){return a.path==identity.path&&a.asset==identity.asset&&a.pathId==t->second.id&&a.classId==(!strcmp(kind,"Mesh")?43:!strcmp(kind,"Material")?21:28);}),"author template asset is not its declared path+name");
  return {t->second,found->second};
 }
};
inline void Save(const std::filesystem::path& path,const Bytes& bytes){
 std::filesystem::create_directories(path.parent_path());std::ofstream out(path,std::ios::binary|std::ios::trunc);Require(bool(out),"author asset output open failed");
 out.write((const char*)bytes.data(),std::streamsize(bytes.size()));Require(bool(out),"author asset output write failed");
}
void Export(const std::filesystem::path& mod,const std::filesystem::path& table,const std::filesystem::path& output){
 auto started=std::chrono::steady_clock::now();auto package=Load(mod);auto templates=ReadTemplates(table);Sources sources{templates};
 auto target=std::filesystem::weakly_canonical(output);auto modRoot=InputPath(std::filesystem::weakly_canonical(mod).generic_u8string()),targetRoot=InputPath(target.generic_u8string());
 Require(targetRoot!=modRoot&&targetRoot.rfind(modRoot+"/",0)!=0,"new author assets must be staged separately from the current Mod");
 EiemRuntimeAuthorDirectory::Directory directory;directory.mod=package.id;size_t nativeBytes=0;
 for(const auto& rule:package.rules){if(rule.second.skip)continue;Require(!rule.second.targets.empty(),"author rule has no Mesh target");const auto& identity=rule.second.targets.begin()->second;auto source=sources.Get(identity,"Mesh");
  for(const auto& part:rule.second.parts){auto asset=EiemAuthorMeshBundle::Build(package,identity,part,source.second.bytes,source.first.cab,source.first.id);
   EiemRuntimeAuthorDirectory::MeshAsset m;m.part=part;m.file="meshes/"+part+".mesh";m.path=asset.path;m.marker=asset.meshName;m.material=package.parts.at(part).material;
   auto hash=Sha(asset.bytes.data(),asset.bytes.size());m.hash=Hex(hash.data(),hash.size());m.vertices=asset.vertices;m.indices=asset.indices;
   const auto& mesh=package.meshes.at(package.parts.at(part).mesh);m.bones=mesh.bones;m.shapes=mesh.shapes;
   EiemRuntimeAuthorDirectory::Relative(m.file);Save(target/std::filesystem::u8path(m.file),asset.bytes);nativeBytes+=asset.bytes.size();directory.meshes.push_back(std::move(m));
  }
 }
 for(const auto& rule:package.rules)directory.rules.push_back({rule.first,rule.second.targets,rule.second.parts,rule.second.skip});
 std::map<std::string,std::string> textureFiles;
 for(const auto& texture:package.textures){
  // Texture2D native schema is independent of the material's donor slot. Any
  // explicitly supplied compatible local template may host RGBA32 author data.
  auto chosen=std::find_if(templates.begin(),templates.end(),[](const auto& t){return t.second.kind=="Texture2D";});Require(chosen!=templates.end(),"author Texture2D template absent");
  auto source=sources.Get(chosen->first,"Texture2D");auto asset=EiemAuthorTextureBundle::Build(texture.second,*package.resources.at(texture.second.resource),source.second.bytes,source.first.cab,source.first.id,chosen->first.path,package.id+"/"+texture.second.resource);
  EiemRuntimeAuthorDirectory::TextureAsset t;t.id=texture.first;t.file=texture.second.resource;t.path=asset.path;
  auto hash=Sha(asset.bytes.data(),asset.bytes.size());t.hash=Hex(hash.data(),hash.size());t.width=texture.second.width;t.height=texture.second.height;t.mips=texture.second.mips;
  auto prior=textureFiles.emplace(t.file,t.hash);if(!prior.second&&prior.first->second!=t.hash){t.file="textures/"+texture.first+".tex";Require(textureFiles.emplace(t.file,t.hash).second,"author texture sampling variant file collision");prior.second=true;}
  if(prior.second){EiemRuntimeAuthorDirectory::Relative(t.file);Save(target/std::filesystem::u8path(t.file),asset.bytes);nativeBytes+=asset.bytes.size();}directory.textures.push_back(std::move(t));
 }
 for(const auto& material:package.materials){auto source=sources.Get(material.second.source,"Material");const auto& layout=source.second.layout;const auto& type=layout.types.at(layout.objects.at(source.first.id).type);
  EiemRuntimeAuthorDirectory::MaterialEntry m;m.id=material.first;m.source=material.second.source;
  m.edits=EiemAuthorMaterialEdits::Project(type,ObjectValue(source.second.bytes,layout,source.first.id),*package.resources.at(material.second.resource));m.textures=material.second.textures;directory.materials.push_back(std::move(m));
 }
 auto bytes=EiemRuntimeAuthorDirectory::Encode(directory);auto checked=EiemRuntimeAuthorDirectory::Decode(bytes);
 Require(EiemRuntimeAuthorDirectory::Encode(checked)==bytes,"native author directory is not deterministic");Save(target/"assets.bin",bytes);
 auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
 std::cout<<"AUTHOR-ASSETS meshes="<<directory.meshes.size()<<" materials="<<directory.materials.size()<<" textures="<<directory.textures.size()<<" nativeBytes="<<nativeBytes<<" directoryBytes="<<bytes.size()<<" templateFiles="<<sources.files.size()<<" consumerPrefabs=0 gameIndexRead=0 physicsMigrated=0 milliseconds="<<ms<<'\n';
}
}
int wmain(int argc,wchar_t** argv){try{
 EiemDirectory::Require(argc==4,"usage: eff_author_assets Mod author-templates.tsv separate-output");
 AuthorAssets::Export(std::filesystem::canonical(argv[1]),std::filesystem::canonical(argv[2]),std::filesystem::absolute(argv[3]));return 0;
 }catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}}
