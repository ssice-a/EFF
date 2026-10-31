#include "../src/eiem_author_mesh_bundle.h"
#include "../src/eiem_author_texture_bundle.h"
#include "../src/eiem_author_texture_dds.h"
#include "../src/eiem_author_resource_file.h"
#include "../src/eiem_author_resource_metadata.h"
#include "../src/eiem_runtime_author_directory.h"
#include "../src/eiem_runtime_author_manifest.h"
#include "../src/eiem_loading_input_substitution.h"
#include "../src/eiem_physics_input_builder.h"
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
struct TemplateView {const wchar_t* kind;const wchar_t* path;const wchar_t* asset;const wchar_t* cab;int64_t id;const wchar_t* file;const wchar_t* hash;};
struct InputFileView {const wchar_t* path;const unsigned char* data;size_t size;};
using Files=std::map<std::string,InputBytes>;
inline std::string Utf8(const wchar_t* value){
 Require(value!=nullptr,"author template string is null");auto length=int(wcslen(value));if(!length)return {};
 auto count=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value,length,nullptr,0,nullptr,nullptr);
 Require(count>0,"author template string is not valid Unicode");std::string result(size_t(count),'\0');
 Require(WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value,length,result.data(),count,nullptr,nullptr)==count,
         "author template string conversion failed");return result;
}
inline Templates ReadTemplates(const TemplateView* values,size_t count){
 Require((values||!count)&&count<=100000,"author template array extent invalid");Templates result;
 for(size_t i=0;i<count;++i){const auto& row=values[i];Require(row.kind&&row.path&&row.asset&&row.cab&&row.file&&row.hash,"author template field is null");
  Template value{Utf8(row.kind),Utf8(row.cab),Utf8(row.hash),row.id,std::filesystem::path(row.file)};
  Identity identity{Utf8(row.path),Utf8(row.asset)};
  Require((value.kind=="Mesh"||value.kind=="Material"||value.kind=="Texture2D")&&value.id!=0&&!value.cab.empty()&&
          DigestText(value.hash)&&value.file.is_absolute()&&!identity.path.empty()&&!identity.asset.empty(),
          "author template identity/digest/path differs");
  Require(result.emplace(std::move(identity),std::move(value)).second,"author template path+asset duplicated");
 }return result;
}
inline Files ReadFiles(const InputFileView* values,size_t count,const char* label){
 Require((values||!count)&&count<=100000,std::string(label)+" file array extent invalid");Files result;
 for(size_t i=0;i<count;++i){const auto& row=values[i];Require(row.path&&row.data&&row.size,std::string(label)+" file is empty/null");
  auto path=Utf8(row.path);EiemRuntimeAuthorDirectory::Relative(path);
  Require(result.emplace(std::move(path),InputBytes{row.data,row.size}).second,std::string(label)+" path is duplicated");
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
using AuthorMeshes=std::map<std::string,InputBytes>;
using AuthorMaterials=std::map<std::string,InputBytes>;
inline const InputBytes& AuthorFile(const Files& files,const std::string& path){
 auto found=files.find(path);Require(found!=files.end(),"editable author snapshot absent: "+path);return found->second;
}
inline AuthorMeshes ReadAuthorMeshes(const Files& files,const Package& package){
 AuthorMeshes result;std::set<std::string> expected;
 for(const auto& rule:package.rules)for(const auto& part:rule.second.parts)expected.insert(part);
 for(const auto& part:expected){const auto& bytes=AuthorFile(files,part+".mesh");
  Require(bytes.size>=12&&!memcmp(bytes.data,"EFFMESH\0",8)&&Le(bytes.data,bytes.size,8,4)==6,
    "editable author Mesh signature/version differs: "+part);result.emplace(part,bytes);
 }
 return result;
}
inline InputBytes ReadAuthorPhysics(const Files& files,const std::string& resourceId){
 auto bytes=AuthorFile(files,resourceId+".effphys");
 Require(bytes.size>=28&&!memcmp(bytes.data,"EFFPHS3\0",8),
   "editable author Physics snapshot is absent/invalid: "+resourceId);
 auto number=[&](size_t offset){return Le(bytes.data,bytes.size,offset,8);};
 uint32_t version=(uint32_t)Le(bytes.data,bytes.size,8,4);
 auto documentBytes=number(12),skeletonBytes=number(20);
 Require(version==1&&documentBytes>0&&skeletonBytes>0&&documentBytes<=16*1024*1024&&
   skeletonBytes<=16*1024*1024&&documentBytes+skeletonBytes==bytes.size-28&&
   !memcmp(bytes.data+28,"EFFPHYS\0",8)&&
   !memcmp(bytes.data+28+documentBytes,"EFFSKEL\0",8),
   "editable author Physics snapshot structure/extents differ: "+resourceId);
 return bytes;
}
inline AuthorMaterials ReadAuthorMaterials(const Files& files,const Package& package){
 AuthorMaterials result;
 for(const auto& row:package.materials){auto bytes=AuthorFile(files,row.first+".effmat");
  auto text=Text(bytes.data,bytes.size);std::istringstream lines(text);std::string line;std::map<std::string,std::string> properties;
  while(std::getline(lines,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty()||line.front()=='#'||line.front()==';')continue;
   auto equals=line.find('=');Require(equals!=std::string::npos&&equals>0,"editable author Material line is invalid: "+row.first);
   auto key=line.substr(0,equals),value=line.substr(equals+1);Require(properties.emplace(key,value).second,"editable author Material property is duplicated: "+row.first);}
  Require(properties["format"]=="EFFMAT"&&properties["version"]=="1"&&properties["overrides"]=="true"&&
    properties["source"]==row.second.source.path,"editable author Material header/source differs: "+row.first);
  std::map<std::string,std::string> textureProperties;for(const auto& property:properties)if(Begins(property.first,"texture."))
   Require(textureProperties.emplace(property.first.substr(8),property.second).second,"editable author Material texture slot is duplicated: "+row.first);
  Require(textureProperties==row.second.textures,"editable author Material texture bindings differ from mod.ini: "+row.first);
  result.emplace(row.first,bytes);
 }
 return result;
}
inline std::map<std::string,InputBytes> ReadAuthorEffects(const Files& files,const Package& package){
 std::map<std::string,InputBytes> result;
 for(const auto& row:package.effects){Require(Identifier(row.first),"editable author Effect identity is invalid");
  auto bytes=AuthorFile(files,row.first+".effeffect");
  Require(bytes.size>=92&&!memcmp(bytes.data,"EFFFXA3\0",8),
    "editable author Effect snapshot is absent/invalid: "+row.first);
  auto number=[&](size_t offset){return Le(bytes.data,bytes.size,offset,8);};
  uint32_t version=(uint32_t)Le(bytes.data,bytes.size,8,4);
  auto archiveBytes=number(12),authorBytes=number(20);
  Require(version==1&&archiveBytes&&authorBytes&&archiveBytes<=bytes.size-92&&
    authorBytes==bytes.size-92-archiveBytes,
    "editable author Effect snapshot extents differ: "+row.first);
  auto archiveHash=Sha(bytes.data+92,size_t(archiveBytes));auto authorHash=Sha(bytes.data+92+archiveBytes,size_t(authorBytes));
  Require(std::equal(archiveHash.begin(),archiveHash.end(),bytes.data+28)&&
    std::equal(authorHash.begin(),authorHash.end(),bytes.data+60),
    "editable author Effect snapshot checksum differs: "+row.first);
  result.emplace(row.first,bytes);
 }
 return result;
}
inline void Save(const std::filesystem::path& path,const Bytes& bytes){
 std::filesystem::create_directories(path.parent_path());std::ofstream out(path,std::ios::binary|std::ios::trunc);Require(bool(out),"author asset output open failed");
 out.write((const char*)bytes.data(),std::streamsize(bytes.size()));Require(bool(out),"author asset output write failed");
}
inline void SaveText(const std::filesystem::path& path,const std::string& text){
 std::filesystem::create_directories(path.parent_path());std::ofstream out(path,std::ios::binary|std::ios::trunc);Require(bool(out),"author INI output open failed");
 out.write(text.data(),std::streamsize(text.size()));Require(bool(out),"author INI output write failed");
}
inline std::string Ini(const std::vector<Section>& sections){
 std::ostringstream out;for(const auto& section:sections){out<<'['<<section.id<<"]\n";
  for(const auto& row:section.values)out<<row.first<<'='<<row.second<<'\n';out<<'\n';}return out.str();
}
std::string Export(Package package,const std::vector<Section>& ini,const Templates& templates,
                   const Files& authorFiles,const std::filesystem::path& output){
 auto started=std::chrono::steady_clock::now();Sources sources{templates};
 auto authorMeshes=ReadAuthorMeshes(authorFiles,package);auto authorMaterials=ReadAuthorMaterials(authorFiles,package);
 auto authorEffects=ReadAuthorEffects(authorFiles,package);std::set<std::string> expectedAuthorFiles;
 for(const auto& row:authorMeshes)expectedAuthorFiles.insert(row.first+".mesh");
 for(const auto& row:authorMaterials)expectedAuthorFiles.insert(row.first+".effmat");
 for(const auto& row:authorEffects)expectedAuthorFiles.insert(row.first+".effeffect");
 auto target=std::filesystem::weakly_canonical(output);
 Require(!std::filesystem::exists(target)||std::filesystem::is_empty(target),"format 3 output directory must be empty");
 std::filesystem::create_directories(target);
 EiemRuntimeAuthorDirectory::Directory directory;directory.mod=package.id;directory.runtime=EiemAuthorControlMetadata::Capture(package);size_t nativeBytes=0;
 std::map<std::string,std::pair<std::string,std::string>> meshOutputs,materialOutputs,textureOutputs,physicsOutputs;
 std::map<std::string,std::string> partMeshIds;
 for(const auto& rule:package.rules){if(rule.second.skip)continue;Require(!rule.second.targets.empty(),"author rule has no Mesh target");const auto& identity=rule.second.targets.begin()->second;auto source=sources.Get(identity,"Mesh");
  for(const auto& part:rule.second.parts){auto asset=EiemAuthorMeshBundle::Build(package,identity,part,source.second.bytes,source.first.cab,source.first.id);
   EiemRuntimeAuthorDirectory::MeshAsset m;m.part=part;m.file="native/meshes/"+part+".effres";m.path=asset.path;m.marker=asset.meshName;m.material=package.parts.at(part).material;
   const auto& author=authorMeshes.at(part);m.nativeBytes=asset.bytes.size();m.authorBytes=author.size;auto authorHash=Sha(author.data,author.size);m.authorHash=Hex(authorHash.data(),authorHash.size());
   auto hash=Sha(asset.bytes.data(),asset.bytes.size());m.hash=Hex(hash.data(),hash.size());m.vertices=asset.vertices;m.indices=asset.indices;
   const auto& mesh=package.meshes.at(package.parts.at(part).mesh);m.bones=mesh.bones;m.shapes=mesh.shapes;m.binding=std::move(asset.binding);m.fields=std::move(asset.fields);
   auto meshId="Mesh_"+part;Require(Identifier(meshId),"author Mesh id is invalid");partMeshIds.emplace(part,meshId);
   Bytes payload=std::move(asset.bytes);payload.insert(payload.end(),author.data,author.data+author.size);
   auto envelope=EiemAuthorResourceFile::Encode(1,EiemAuthorResourceMetadata::EncodeMesh(m),payload);
   EiemRuntimeAuthorDirectory::Relative(m.file);Save(target/std::filesystem::u8path(m.file),envelope);nativeBytes+=envelope.size();
   meshOutputs[package.meshes.at(package.parts.at(part).mesh).resource]={meshId,m.file};directory.meshes.push_back(std::move(m));
  }
 }
 for(const auto& rule:package.rules)directory.rules.push_back({rule.first,rule.second.targets,rule.second.parts,rule.second.skip});
 std::map<std::string,std::pair<std::string,std::string>> textureFiles;
 for(const auto& texture:package.textures){
  // Texture2D native schema is independent of the material's donor slot.
  auto chosen=std::find_if(templates.begin(),templates.end(),[](const auto& t){return t.second.kind=="Texture2D";});Require(chosen!=templates.end(),"author Texture2D template absent");
  auto source=sources.Get(chosen->first,"Texture2D");auto asset=EiemAuthorTextureBundle::Build(texture.second,*package.resources.at(texture.second.resource),source.second.bytes,source.first.cab,source.first.id,chosen->first.path,package.id+"/"+texture.second.resource);
  auto split=EiemAuthorTextureDds::Separate(asset,texture.second,*package.resources.at(texture.second.resource));
  EiemRuntimeAuthorDirectory::TextureAsset t;t.id=texture.first;t.file="native/textures/"+texture.first+".effres";t.ddsFile="textures/"+texture.first+".dds";t.path=asset.path;
  auto hash=Sha(split.metadata.data(),split.metadata.size());t.hash=Hex(hash.data(),hash.size());t.cab=asset.member;t.pathId=asset.meshId;t.pixelOffset=split.offset;t.pixelBytes=split.pixels;t.width=texture.second.width;t.height=texture.second.height;t.mips=texture.second.mips;t.format=texture.second.format;
  Require(textureFiles.emplace(t.file,std::make_pair(t.hash,t.ddsFile)).second,"author texture resource id collision");
  EiemRuntimeAuthorDirectory::Relative(t.file);EiemRuntimeAuthorDirectory::Relative(t.ddsFile);
  auto envelope=EiemAuthorResourceFile::Encode(3,EiemAuthorResourceMetadata::EncodeTexture(t),split.metadata);
  Save(target/std::filesystem::u8path(t.file),envelope);Save(target/std::filesystem::u8path(t.ddsFile),split.dds);nativeBytes+=envelope.size()+split.dds.size();
  textureOutputs.emplace(texture.first,std::make_pair(t.file,t.ddsFile));
  directory.textures.push_back(std::move(t));
 }
 for(const auto& material:package.materials){
  const auto& author=authorMaterials.at(material.first);EiemRuntimeAuthorDirectory::MaterialEntry entry;
  entry.id=material.first;entry.file="native/materials/"+material.first+".effres";entry.source=material.second.source;
  entry.textures=material.second.textures;entry.authorBytes=author.size;auto authorHash=Sha(author.data,author.size);entry.hash=Hex(authorHash.data(),authorHash.size());
  Bytes authorPayload(author.data,author.data+author.size);
  auto envelope=EiemAuthorResourceFile::Encode(2,EiemAuthorResourceMetadata::EncodeMaterial(entry),authorPayload);
  Save(target/std::filesystem::u8path(entry.file),envelope);nativeBytes+=envelope.size();
  materialOutputs.emplace(material.first,std::make_pair(entry.file,material.second.source.path+"\n"+material.second.source.asset));
  directory.materials.push_back(std::move(entry));
 }
 size_t physics=0;
 for(const auto& row:package.resources)if(row.second->blocks.count("@physics")){
  const auto& resource=*row.second;auto document=EiemPhysicsInput::Decode(resource);
  auto owner=std::find_if(ini.begin(),ini.end(),[&](const auto& section){auto path=section.values.find("path");return Begins(section.id,"Resource")&&path!=section.values.end()&&path->second==row.first;});
  Require(owner!=ini.end(),"format 3 Physics Resource section absent");
  Require(std::find_if(std::next(owner),ini.end(),[&](const auto& section){auto path=section.values.find("path");return Begins(section.id,"Resource")&&path!=section.values.end()&&path->second==row.first;})==ini.end(),"format 3 Physics Resource section is ambiguous");
  auto author=ReadAuthorPhysics(authorFiles,owner->id);expectedAuthorFiles.insert(owner->id+".effphys");
  EiemRuntimeAuthorDirectory::Auxiliary entry;entry.id=owner->id;entry.file="native/physics/"+entry.id+".effres";entry.kind=resource.kind;entry.bytes=resource.bytes.size();entry.hash=ResourceContentHash(resource);
  entry.authorBytes=author.size;auto authorHash=Sha(author.data,author.size);entry.authorHash=Hex(authorHash.data(),authorHash.size());
  for(const auto& group:document.groups)entry.targets.insert(group.targets.begin(),group.targets.end());
  Bytes payload(ResourceData(resource),ResourceData(resource)+ResourceSize(resource));payload.insert(payload.end(),author.data,author.data+author.size);
  auto envelope=EiemAuthorResourceFile::Encode(4,EiemAuthorResourceMetadata::EncodeAuxiliary(entry),payload);
  Save(target/std::filesystem::u8path(entry.file),envelope);nativeBytes+=envelope.size();physicsOutputs.emplace(row.first,std::make_pair(entry.file,owner->id));directory.auxiliary.push_back(std::move(entry));++physics;
 }
 std::map<std::string,std::string> effectResourceIds;std::map<std::string,std::string> effectAuthorResources;std::map<std::string,std::set<std::string>> effectRules;
 for(auto& row:directory.runtime.effects){auto manifest=std::make_shared<EiemEffectNative::Manifest>(*row.second.native);
  for(size_t i=0;i<manifest->bundles.size();++i){auto& input=manifest->bundles[i];
   Require(EiemEffectNative::Archive(input.Data(),input.Size()),"effect input must be encoded offline in the native game envelope");
   input.file="native/effects/"+manifest->key+"/"+std::to_string(i)+".effres";EiemRuntimeAuthorDirectory::Relative(input.file);
   auto digest=Sha(reinterpret_cast<const unsigned char*>(input.file.data()),input.file.size());auto resourceId="ResourceEffect_"+Hex(digest.data(),8);
   auto inserted=effectResourceIds.emplace(input.file,resourceId);
   Require(inserted.second||inserted.first->second==resourceId,"native Effect resource identity collision");
   if(inserted.second){auto payload=Bytes(input.Data(),input.Data()+input.Size());auto envelope=EiemAuthorResourceFile::Encode(5,
      EiemAuthorResourceMetadata::EncodeEffect(input.file,input.file,input.hash,input.Size()),payload);Save(target/std::filesystem::u8path(input.file),envelope);nativeBytes+=envelope.size();}
   if(!row.second.part.empty()){
    for(const auto& rule:package.rules)
     if(std::find(rule.second.parts.begin(),rule.second.parts.end(),row.second.part)!=rule.second.parts.end())effectRules[input.file].insert(rule.first);
   }else if(!row.second.targetPath.empty()&&!row.second.targetAsset.empty()){
    for(const auto& rule:package.rules)for(const auto& ruleTarget:rule.second.targets)
     if(ruleTarget.second.path==row.second.targetPath&&ruleTarget.second.asset==row.second.targetAsset)effectRules[input.file].insert(rule.first);
   }
   input.size=input.Size();input.bytes.reset();input.backing.reset();input.data=nullptr;
  }row.second.native=std::move(manifest);
 }
 std::map<std::string,std::string> effectAuthorFiles;std::vector<Section> effectAuthorSections;size_t effectAuthorBytes=0;
 for(const auto& row:authorEffects){auto digest=Sha(row.second.data,row.second.size);auto hash=Hex(digest.data(),digest.size());
  auto prior=effectAuthorFiles.find(hash);std::string resourceId;
  if(prior!=effectAuthorFiles.end())resourceId=prior->second;
  else{resourceId="ResourceEffectAuthor_"+hash.substr(0,16);auto file="native/effect-author/"+hash+".effres";
   Bytes authorPayload(row.second.data,row.second.data+row.second.size);
   auto envelope=EiemAuthorResourceFile::Encode(7,EiemAuthorResourceMetadata::EncodeEffectAuthor(resourceId,file,row.second.size,hash),authorPayload);
   Save(target/std::filesystem::u8path(file),envelope);nativeBytes+=envelope.size();effectAuthorBytes+=envelope.size();effectAuthorFiles.emplace(hash,resourceId);
   Section section;section.id=resourceId;section.values["type"]="effect-author";section.values["path"]=file;effectAuthorSections.push_back(std::move(section));
  }
  effectAuthorResources.emplace(row.first,resourceId);
 }
 const std::string controlsPath="native/controls.effres";auto controlBytes=EiemAuthorResourceMetadata::EncodeControls(directory.runtime);
 auto controlEnvelope=EiemAuthorResourceFile::Encode(6,controlBytes,{});Save(target/controlsPath,controlEnvelope);nativeBytes+=controlEnvelope.size();
 std::vector<Section> published;published.reserve(ini.size()+directory.meshes.size()+effectResourceIds.size()+1);
 published.insert(published.end(),effectAuthorSections.begin(),effectAuthorSections.end());
 for(auto section:ini){auto& values=section.values;
  if(section.id=="Mod"){values["format"]="3";published.push_back(std::move(section));continue;}
  if(Begins(section.id,"Mesh"))continue;
  if(Begins(section.id,"Part")){auto found=partMeshIds.find(section.id);Require(found!=partMeshIds.end(),"format 3 Part Mesh mapping absent");values["mesh"]=found->second;published.push_back(std::move(section));continue;}
  if(Begins(section.id,"ShapeAnimation")||Begins(section.id,"MaterialAnimation")||Begins(section.id,"EffectAttachment"))continue;
  if(section.id=="ResourceControls")continue;
  if(Begins(section.id,"Material")){auto found=materialOutputs.find(section.id);Require(found!=materialOutputs.end(),"format 3 Material mapping absent");values["path"]=found->second.first;published.push_back(std::move(section));continue;}
  if(Begins(section.id,"Texture")){auto found=textureOutputs.find(section.id);Require(found!=textureOutputs.end(),"format 3 Texture mapping absent");values["path"]=found->second.first;values["dds"]=found->second.second;published.push_back(std::move(section));continue;}
  if(Begins(section.id,"Resource")){auto path=values.find("path");Require(path!=values.end(),"format 2 Resource path absent");const auto sourcePath=path->second;auto found=physicsOutputs.find(sourcePath);if(found!=physicsOutputs.end()){
    values.clear();values["type"]="physics";values["path"]=found->second.first;std::set<std::string> targets;
    auto auxiliary=std::find_if(directory.auxiliary.begin(),directory.auxiliary.end(),[&](const auto& row){return row.file==found->second.first;});
    Require(auxiliary!=directory.auxiliary.end(),"format 3 Physics metadata mapping absent");
    for(const auto& meshTarget:auxiliary->targets){auto rule=package.targetIndex.find(meshTarget);if(rule!=package.targetIndex.end())targets.insert(rule->second);}
    if(!targets.empty()){std::ostringstream list;bool first=true;for(const auto& item:targets){if(!first)list<<',';first=false;list<<item;}values["targets"]=list.str();}
   }else throw std::runtime_error("unsupported legacy Resource section; migration left the source untouched: "+section.id);
   published.push_back(std::move(section));continue;}
  published.push_back(std::move(section));
 }
 for(const auto& row:partMeshIds){Section section;section.id=row.second;section.values.emplace("path","");auto mesh=std::find_if(directory.meshes.begin(),directory.meshes.end(),[&](const auto& value){return value.part==row.first;});
  Require(mesh!=directory.meshes.end(),"format 3 Mesh metadata mapping absent");section.values["path"]=mesh->file;published.push_back(std::move(section));}
 for(const auto& row:effectResourceIds){Section section;section.id=row.second;section.values["type"]="effect";section.values["path"]=row.first;
  const auto& targetRules=effectRules[row.first];if(!targetRules.empty()){std::ostringstream list;bool first=true;for(const auto& targetRule:targetRules){if(!first)list<<',';first=false;list<<targetRule;}section.values["targets"]=list.str();}
  published.push_back(std::move(section));}
 for(const auto& animation:directory.runtime.animations){if(directory.runtime.materialAnimations.count(animation.first))continue;
  Section section;section.id="ShapeAnimation"+animation.first;section.values["path"]=controlsPath;published.push_back(std::move(section));}
 for(const auto& animation:directory.runtime.materialAnimations){
  Section section;section.id="MaterialAnimation"+animation;section.values["path"]=controlsPath;published.push_back(std::move(section));}
 for(const auto& effect:directory.runtime.effects){
  Section section;section.id="EffectAttachment"+effect.first;section.values["path"]=controlsPath;
  auto author=effectAuthorResources.find(effect.first);Require(author!=effectAuthorResources.end(),"format 3 Effect author snapshot mapping absent");
  section.values["author"]=author->second;published.push_back(std::move(section));}
 std::set<std::string> actualAuthorFiles;for(const auto& row:authorFiles)actualAuthorFiles.insert(row.first);
 Require(actualAuthorFiles==expectedAuthorFiles,"editable author snapshot set has missing or unreferenced files");
 Require(std::none_of(published.begin(),published.end(),[](const auto& section){return section.id=="ResourceControls";}),"reserved ResourceControls section already exists");
 published.push_back({"ResourceControls",{{"type","controls"},{"path",controlsPath}}});SaveText(target/"mod.ini",Ini(published));
 auto manifest=EiemRuntimeAuthorManifest::Read(target);Require(manifest.id==package.id,"format 3 Mod publication identity differs");
 auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
 std::ostringstream report;report<<"AUTHOR-FORMAT3 meshes="<<directory.meshes.size()<<" materials="<<directory.materials.size()<<" textures="<<directory.textures.size()<<" nativeBytes="<<nativeBytes<<" effectAuthorBytes="<<effectAuthorBytes<<" manifestBytes="<<std::filesystem::file_size(target/"mod.ini")<<" templateFiles="<<sources.files.size()<<" consumerPrefabs=0 gameIndexRead=0 physicsDocuments="<<physics<<" keyBindings="<<directory.runtime.bindings.size()<<" animations="<<directory.runtime.animations.size()<<" effects="<<directory.runtime.effects.size()<<" milliseconds="<<ms;
 return report.str();
}
}
extern "C" __declspec(dllexport) unsigned __cdecl EFF_AuthorAssetsAbi(){return 2;}
extern "C" __declspec(dllexport) int __cdecl EFF_AuthorAssets_ExportMemoryW(
 const unsigned char* ini,size_t iniSize,
 const AuthorAssets::TemplateView* templates,size_t templateCount,
 const AuthorAssets::InputFileView* resources,size_t resourceCount,
 const AuthorAssets::InputFileView* authorFiles,size_t authorFileCount,
 const wchar_t* output,char* response,size_t responseCapacity){
 auto copy=[&](const std::string& value,int status){
  if(!response||!responseCapacity)return 2;auto count=std::min(value.size(),responseCapacity-1);
  if(count)memcpy(response,value.data(),count);response[count]=0;return count==value.size()?status:2;
 };
 try{
  EiemDirectory::Require(ini&&iniSize&&output,"author writer received an empty INI or null output");
  EiemDirectory::Require(iniSize<=16*1024*1024,"author INI exceeds the input limit");
  EiemDirectory::Bytes iniBytes(ini,ini+iniSize);
  auto resourceViews=AuthorAssets::ReadFiles(resources,resourceCount,"format 2 resource");
  auto authorViews=AuthorAssets::ReadFiles(authorFiles,authorFileCount,"editable author");
  auto parsedTemplates=AuthorAssets::ReadTemplates(templates,templateCount);
  auto package=EiemDirectory::LoadMemory(std::filesystem::absolute(output),iniBytes,resourceViews);
  auto sections=EiemDirectory::ParseIni(iniBytes);
  auto report=AuthorAssets::Export(std::move(package),sections,parsedTemplates,authorViews,
                                   std::filesystem::absolute(output));
  return copy(report,0);
 }catch(const std::exception& error){return copy(std::string("FAIL ")+error.what(),1);}
 catch(...){return copy("FAIL unknown author writer error",1);}
}
