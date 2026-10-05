#pragma once
#include "eiem_renderer_input_builder.h"
#include "eiem_render_assembly_description.h"
#include "eiem_loading_generation.h"
#include "eiem_runtime_shape_identity.h"
#include "eiem_loading_input_substitution.h"
#include "eiem_offline_compile_helpers.h"
#include <fstream>
#include <functional>
#include <chrono>
#include <atomic>
#include <exception>
#include <mutex>
#include <thread>

// A disk-only resource generation. Baselines are pristine serialized inputs,
// never the installed substituted inputs. No Unity operation is allowed here.
namespace EiemResourceReloadPlan {
using namespace EiemNativeInput;
constexpr size_t inputLimit=512ull*1024*1024;
inline bool Serialized(const Bytes& b){return b.size()>=48 && !b[8]&&!b[9]&&!b[10]&&b[11]==22;}
inline std::string Hash(const Bytes& b){auto h=Sha(b.data(),b.size());return Hex(h.data(),h.size());}
inline std::string Name(const Type& t,const Value& v){const auto& n=Child(t,v,"m_Name");return Text(n.raw.data(),n.raw.size());}
struct Source {std::string file,hash;};
struct Root {std::string path;size_t input=0;std::map<std::string,unsigned> markers;
 std::string cab;int64_t source=0;EiemRenderAssemblyDescription::Description assembly;};
struct Input {std::string file,hash,bundle;std::filesystem::path path;std::set<std::string> externals;};
struct Texture {std::string name;int width=0,height=0,mips=0;};
struct TextureSlot {std::string property;bool nonnull=false,local=false;Texture texture;};
struct Material {std::string name;std::vector<TextureSlot> slots;};
struct Part {std::string id,marker,material;unsigned vertices=0;std::vector<std::string> bones,shapes;};
struct Plan {
 std::string key,author,contract;std::filesystem::path directory;
 EiemLoadingGeneration::Names names;std::vector<Input> inputs;std::vector<Root> roots;
 std::vector<size_t> closure;std::set<std::string> shared;
 std::map<std::string,Part> parts;std::map<std::string,Material> materials;
 size_t meshParts=0,rendererParts=0,textures=0,materialMatches=0,opaque=0;
 size_t rendererBoundsHits=0,rendererBoundsMisses=0;
 size_t namespaceHits=0,namespaceMisses=0;
 size_t appearanceHits=0,appearanceMisses=0,geometryInputHits=0,geometryInputMisses=0;
};
inline std::vector<Source> ReadBaseline(const std::filesystem::path& directory,std::vector<std::string>& roots) {
 auto bytes=Read(directory/"baseline.tsv",256*1024);std::istringstream lines(Text(bytes.data(),bytes.size()));
 std::string line;Require(bool(std::getline(lines,line)),"reload baseline header absent");if(!line.empty()&&line.back()=='\r')line.pop_back();
 Require(line=="EFF_RESOURCE_RELOAD_BASELINE\t1","reload baseline format differs");
 std::vector<Source> sources;std::set<std::string> files,paths;
 while(std::getline(lines,line)){
  if(!line.empty()&&line.back()=='\r')line.pop_back();auto row=TabFields(line);
  Require(row.size()==3,"reload baseline row differs");
  if(row[0]=="input"){
   std::filesystem::path file=std::filesystem::u8path(row[1]);
   Require(file==file.filename()&&file.extension()==".vfs"&&files.insert(row[1]).second&&DigestText(row[2]),"reload baseline input invalid/duplicate");
   sources.push_back({row[1],row[2]});
  }else{
   Require(row[0]=="root"&&row[1].rfind("assets/",0)==0&&row[1].find("..") ==std::string::npos&&row[2]=="GameObject"&&paths.insert(row[1]).second,"reload root identity invalid/duplicate");
   roots.push_back(row[1]);
  }
 }
 Require(!sources.empty()&&sources.size()<=128&&!roots.empty()&&roots.size()<=8,"reload baseline coverage/limit");
 std::sort(sources.begin(),sources.end(),[](const auto&a,const auto&b){return a.file<b.file;});return sources;
}
inline NativeReference Reference(const Type& t,const Value& v,const SerializedLayout& l,const std::string& cab){
 auto f=int32_t(Integer(Child(t,v,"m_FileID")));auto id=int64_t(Integer(Child(t,v,"m_PathID")));
 Require(f>=0&&size_t(f)<=l.externals.size(),"reload descriptor external index");return {f?l.externals.at(f-1):cab,id};
}
inline Bytes Mark(const Bytes& input,const std::map<NativeReference,std::string>& markers,size_t& count){
 auto bundle=EndfieldInput::Decode(input,inputLimit);bool changed=false;
 for(auto& m:bundle.members)if(Serialized(m.bytes)){
  auto l=ParseSerialized(m.bytes);std::map<int64_t,Bytes> edits;
  for(const auto& o:l.objects){auto it=markers.find({m.name,o.first});if(it==markers.end())continue;
   const auto& t=l.types.at(o.second.type);Require(t.classId==43,"reload marker target not Mesh");
   auto value=ObjectValue(m.bytes,l,o.first);auto shapes=EncodeValue(t,Child(t,value,"m_Shapes")),vertices=EncodeValue(t,Child(t,value,"m_VertexData"));
   SetString(t,Child(t,value,"m_Name"),it->second);
   Require(EncodeValue(t,Child(t,value,"m_Shapes"))==shapes&&EncodeValue(t,Child(t,value,"m_VertexData"))==vertices,"reload marker changed native shape/vertex payload");
   edits.emplace(o.first,EncodeValue(t,value));++count;
  }
  if(!edits.empty()){m.bytes=BuildSerialized(m.bytes,l,edits,{});changed=true;}
 }
 return changed?EndfieldInput::Encode(input,bundle.members,inputLimit):input;
}
inline void Save(const std::filesystem::path& p,const Bytes& b){
 std::ofstream out(p,std::ios::binary);Require(bool(out),"reload input create failed");out.write((const char*)b.data(),b.size());out.flush();Require(bool(out),"reload input write failed");
}
template<class F> inline auto Measure(const std::function<void(const char*,double)>& cost,const char* name,F operation){
 if(!cost)return operation();auto start=std::chrono::steady_clock::now();auto result=operation();
 cost(name,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());return result;
}
// Mesh/Renderer construction is independent of pixels, material fields and
// runtime keys. Persist that stage so a texture/INI edit never rebuilds bones,
// bindposes, vertex streams or Renderer topology.
inline std::string GeometryDigest(const Package& package,const std::vector<Source>& sources){
 auto p=package;for(auto it=p.resources.begin();it!=p.resources.end();)if(it->second->kind!=1)it=p.resources.erase(it);else ++it;
 p.textures.clear();for(auto& m:p.materials)m.second.textures.clear();
 std::string key="EFF_GEOMETRY_STAGE_1\n"+InputContractDigest(p);
 for(const auto& source:sources)key+='\n'+source.file+'\n'+source.hash;
 return Hash(Bytes(key.begin(),key.end()));
}
struct GeometryStage {AssetIndex assets;std::vector<Bytes> inputs;std::vector<std::pair<size_t,size_t>> counts;};
inline GeometryStage ReadGeometry(const std::filesystem::path& directory,const std::string& key,const std::vector<Source>& sources){
 GeometryStage stage;auto data=Read(directory/"stage.tsv",16*1024*1024);std::istringstream lines(Text(data.data(),data.size()));std::string line;
 Require(bool(std::getline(lines,line))&&line=="EFF_GEOMETRY_STAGE\t1\t"+key,"geometry cache identity differs");
 while(std::getline(lines,line)){auto r=TabFields(line);if(r[0]=="asset"){
  Require(r.size()==7&&r[6].size()==32,"geometry asset descriptor");Asset asset{r[1],r[2],r[3],std::stoll(r[4]),std::stoi(r[5]),{}};
  for(size_t i=0;i<16;++i)asset.type[i]=(unsigned char)std::stoul(r[6].substr(i*2,2),nullptr,16);
  Require(stage.assets.emplace(Identity{asset.path,asset.asset},asset).second,"duplicate geometry asset");
 }else{auto i=stage.inputs.size();Require(r.size()==5&&r[0]=="input"&&i<sources.size()&&r[1]==sources[i].file&&DigestText(r[2]),"geometry input descriptor");
  auto bytes=Read(directory/std::filesystem::u8path(r[1]),inputLimit);Require(Hash(bytes)==r[2],"geometry cache payload changed");stage.inputs.push_back(std::move(bytes));stage.counts.emplace_back(std::stoull(r[3]),std::stoull(r[4]));
 }}Require(stage.inputs.size()==sources.size()&&!stage.assets.empty(),"geometry cache incomplete");return stage;
}
inline void WriteGeometry(const std::filesystem::path& directory,const std::string& key,const std::vector<Source>& sources,const GeometryStage& stage){
 std::filesystem::create_directories(directory);std::ostringstream rows;rows<<"EFF_GEOMETRY_STAGE\t1\t"<<key<<'\n';
 for(const auto& r:stage.assets){const auto& a=r.second;rows<<"asset\t"<<a.path<<'\t'<<a.asset<<'\t'<<a.cab<<'\t'<<a.pathId<<'\t'<<a.classId<<'\t'<<Hex(a.type.data(),a.type.size())<<'\n';}
 for(size_t i=0;i<stage.inputs.size();++i){Save(directory/sources[i].file,stage.inputs[i]);rows<<"input\t"<<sources[i].file<<'\t'<<Hash(stage.inputs[i])<<'\t'<<stage.counts[i].first<<'\t'<<stage.counts[i].second<<'\n';}
 auto text=rows.str();Save(directory/"stage.tsv",Bytes(text.begin(),text.end()));
}
inline std::string AppearanceInputDigest(const Bytes& input,const Package& package,const AssetIndex& assets){
 auto bundle=EndfieldInput::Decode(input,inputLimit);std::set<std::string> members;for(const auto& m:bundle.members)members.insert(m.name);
 auto p=package;p.meshes.clear();p.parts.clear();p.rules.clear();p.targetIndex.clear();p.resources.clear();
 // All binding/template declarations remain in the key: preloads in this CAB
 // can reference textures hosted by a different input. Pixel/material payloads
 // only affect the CAB that owns their native template.
 for(const auto& m:p.materials)if(members.count(assets.at(m.second.source).cab))p.resources.emplace(m.second.resource,package.resources.at(m.second.resource));
 for(const auto& t:p.textures)if(members.count(assets.at(t.second.source).cab))p.resources.emplace(t.second.resource,package.resources.at(t.second.resource));
 auto text="EFF_APPEARANCE_INPUT_1\n"+Hash(input)+"\n"+InputContractDigest(p);return Hash(Bytes(text.begin(),text.end()));
}
inline std::string GeometryInputDigest(const Bytes& input,const Package& package,const AssetIndex& assets){
 auto bundle=EndfieldInput::Decode(input,inputLimit);std::set<std::string> members;std::set<Identity> renderTargets;
 std::map<NativeReference,Identity> selected;
 for(const auto& target:package.targetIndex){const auto& asset=assets.at(target.first);selected.emplace(NativeReference{asset.cab,asset.pathId},target.first);}
 for(const auto& member:bundle.members){
  members.insert(member.name);if(!Serialized(member.bytes))continue;auto layout=ParseSerialized(member.bytes);
  for(const auto& row:layout.objects)if(layout.types.at(row.second.type).classId==137){
   const auto& type=layout.types.at(row.second.type);auto value=ObjectValue(member.bytes,layout,row.first);
   auto reference=NullableReference(type,Child(type,value,"m_Mesh"),layout,member.name);auto target=selected.find(reference);
   if(target!=selected.end())renderTargets.insert(target->second);
  }
 }
 auto p=package;p.resources.clear();p.textures.clear();for(auto& m:p.materials)m.second.textures.clear();
 for(const auto& target:package.targetIndex){
  bool local=members.count(assets.at(target.first).cab)!=0;if(!local&&!renderTargets.count(target.first))continue;
  for(const auto& partId:package.rules.at(target.second).parts){
   const auto& mesh=package.meshes.at(package.parts.at(partId).mesh);const auto& resource=package.resources.at(mesh.resource);
   auto found=p.resources.find(mesh.resource);
   if(local){p.resources[mesh.resource]=resource;continue;}
   if(found!=p.resources.end())continue;
   // Index-only changes affect Mesh content/provenance, but not Renderer
   // hierarchy, palette, shape count or bounds. Match the fields actually read
   // by RendererMemberBuilder rather than invalidating every PFB consumer.
   auto dependency=std::make_shared<Resource>();dependency->kind=1;dependency->type=resource->type;
   std::string text="EFF_RENDERER_DEPENDENCY_1\n"+std::to_string(mesh.vertices)+"\n"+Hex(resource->type.data(),16);
   for(const char* field:{"m_VertexData","m_Shapes","m_BindPose","@bones","@shapes"}){
    const auto& block=resource->blocks.at(field);text+='\n'+std::string(field)+":"+Hex(block.digest.data(),block.digest.size());
   }
   dependency->bytes.assign(text.begin(),text.end());p.resources.emplace(mesh.resource,std::move(dependency));
  }
 }
 auto text="EFF_GEOMETRY_INPUT_1\n"+Hash(input)+"\n"+InputContractDigest(p);return Hash(Bytes(text.begin(),text.end()));
}
inline Plan Build(const Package& authoredPackage,const std::filesystem::path& baseline,const std::filesystem::path& destination,
                  const std::string& nonce,std::function<void(size_t,size_t)> progress={},
                  std::function<void(const char*,size_t,size_t)> stage={},
                  std::function<void(const char*,double)> cost={},const std::filesystem::path& geometryCache={}){
 auto package=authoredPackage;
 Require(!std::filesystem::exists(destination),"reload generation directory exists");
 std::vector<std::string> rootPaths;auto sources=ReadBaseline(baseline,rootPaths);
 Plan plan;plan.directory=destination;plan.author=PackageDigest(package);plan.contract=InputContractDigest(package);
 std::string identity=plan.author+"\n"+plan.contract+"\n"+nonce;
 std::vector<Bytes> pristineInputs(sources.size());AssetIndex assets;MeshSchemaIndex schemas;std::vector<BuiltMeshPart> meshParts;
 std::set<std::string> sourceMembers;
 auto geometryKey=GeometryDigest(package,sources);auto geometryDirectory=geometryCache/geometryKey;
 bool geometryHit=!geometryCache.empty()&&std::filesystem::exists(geometryDirectory/"stage.tsv");GeometryStage geometry;
 if(geometryHit){geometry=ReadGeometry(geometryDirectory,geometryKey,sources);assets=geometry.assets;if(stage)stage("geometry-reused",sources.size(),sources.size());}
 if(stage)stage("source",0,sources.size());
 for(size_t i=0;i<sources.size();++i){const auto& s=sources[i];
  identity+="\n"+s.file+"\n"+s.hash;if(geometryHit)continue;
  auto bytes=Read(baseline/"inputs"/s.file,inputLimit);Require(Hash(bytes)==s.hash,"reload pristine source SHA differs: "+s.file);
  AddAssets(assets,bytes);AddMeshSchemas(schemas,bytes);
   pristineInputs[i]=std::move(bytes);if(stage)stage("source",i+1,sources.size());
 }
 if(std::any_of(package.textures.begin(),package.textures.end(),[](const auto& t){return t.second.source.path.empty();})){
  std::vector<Bytes> templateInputs;
  for(const auto& source:sources)templateInputs.push_back(Read(baseline/"inputs"/source.file,inputLimit));
  ResolveTextureTemplates(package,templateInputs,assets);
 }
 for(const auto& target:package.targetIndex)Require(assets.count(target.first),"reload source Mesh absent from pristine graph");
 for(const auto& m:package.materials)Require(assets.count(m.second.source),"reload Material absent from pristine graph");
 for(const auto& t:package.textures)Require(assets.count(t.second.source),"reload Texture absent from pristine graph");
 if(!geometryHit)for(const auto& target:package.targetIndex){
  const auto& asset=assets.at(target.first);const auto& rule=package.rules.at(target.second);if(rule.skip)continue;
  for(size_t i=0;i<rule.parts.size();++i){const auto& id=rule.parts[i];const auto& mesh=package.meshes.at(package.parts.at(id).mesh);
   meshParts.push_back({target.first,target.second,id,asset.cab,asset.pathId,i?AllocateMeshId(asset.cab,package,target.first,id):asset.pathId,mesh.vertices,mesh.indices});
  }
 }
 Bytes identityBytes(identity.begin(),identity.end());plan.key=Hash(identityBytes);
 std::map<NativeReference,std::string> markers;
 for(const auto& rule:package.rules)if(!rule.second.skip)for(const auto& target:rule.second.targets){
  const auto& ref=assets.at(target.second);
  for(size_t i=0;i<rule.second.parts.size();++i){
   const auto& id=rule.second.parts[i];const auto& part=package.parts.at(id);const auto& mesh=package.meshes.at(part.mesh);
   auto marker=RuntimePartName(package,id,target.second);int64_t pathId=i?AllocateMeshId(ref.cab,package,target.second,id):ref.pathId;
   Require(markers.emplace(NativeReference{ref.cab,pathId},marker).second,"reload duplicate source Mesh marker");
   // Multiple LOD source identities are independent, even for the same author part.
   Part expected{id,marker,part.material,mesh.vertices,mesh.bones,mesh.shapes};
   Require(plan.parts.emplace(marker,std::move(expected)).second,"reload marker collision");
  }
 }
  std::vector<Bytes> rebuilt;size_t marked=0;RendererBoundsCache boundsCache;
  if(stage)stage("resource",0,sources.size());
  std::vector<Bytes> rendererInputs(sources.size());
  if(geometryHit){
   for(size_t i=0;i<sources.size();++i){rendererInputs[i]=std::move(geometry.inputs[i]);plan.meshParts+=geometry.counts[i].first;plan.rendererParts+=geometry.counts[i].second;}
  }else{
   auto inputCache=geometryCache.empty()?std::filesystem::path{}:geometryCache.parent_path()/"geometry-inputs";
   if(!inputCache.empty())std::filesystem::create_directories(inputCache);
   std::vector<EiemOfflineCompile::Stage> inputs(sources.size());std::vector<unsigned char> hits(sources.size());std::mutex geometryCostMutex;
   std::function<void(const char*,double)> geometryCost;if(cost)geometryCost=[&](const char* name,double elapsed){std::lock_guard<std::mutex> lock(geometryCostMutex);cost(name,elapsed);};
   EiemOfflineCompile::ParallelInputs(sources.size(),[&](size_t i){
    auto key=GeometryInputDigest(pristineInputs[i],package,assets);auto& output=inputs[i];
    if(EiemOfflineCompile::ReadStage(inputCache,key,output)){hits[i]=1;return;}
    auto mesh=Measure(geometryCost,"mesh-source",[&]{return BuildMeshInput(pristineInputs[i],package);});
    auto renderer=Measure(geometryCost,"renderer",[&]{return BuildRendererInput(mesh.bytes,package,assets,meshParts,schemas,&boundsCache);});
    size_t localMarked=0;auto bytes=Measure(geometryCost,"markers",[&]{return Mark(renderer.bytes,markers,localMarked);});
    output={std::move(bytes),mesh.parts.size(),renderer.parts.size(),localMarked};EiemOfflineCompile::WriteStage(inputCache,key,output);
   });
   for(size_t i=0;i<sources.size();++i){
    if(hits[i])++plan.geometryInputHits;else ++plan.geometryInputMisses;
    marked+=inputs[i].third;plan.meshParts+=inputs[i].first;plan.rendererParts+=inputs[i].second;
    if(!geometryCache.empty()){geometry.inputs.push_back(inputs[i].bytes);geometry.counts.emplace_back(inputs[i].first,inputs[i].second);}
    rendererInputs[i]=std::move(inputs[i].bytes);
   }
  }
  auto appearanceCache=geometryCache.empty()?std::filesystem::path{}:geometryCache.parent_path()/"appearance";
  if(!appearanceCache.empty())std::filesystem::create_directories(appearanceCache);
  std::vector<EiemOfflineCompile::Stage> appearances(sources.size());std::vector<unsigned char> appearanceHits(sources.size());std::mutex appearanceCostMutex;
  std::function<void(const char*,double)> appearanceCost;if(cost)appearanceCost=[&](const char* name,double elapsed){std::lock_guard<std::mutex> lock(appearanceCostMutex);cost(name,elapsed);};
  EiemOfflineCompile::ParallelInputs(sources.size(),[&](size_t i){
   auto key=AppearanceInputDigest(rendererInputs[i],package,assets);auto& output=appearances[i];
   if(EiemOfflineCompile::ReadStage(appearanceCache,key,output)){appearanceHits[i]=1;return;}
   auto texture=Measure(appearanceCost,"texture",[&]{return BuildTextureInput(rendererInputs[i],package,assets);});
   output={std::move(texture.bytes),texture.textures.size(),texture.materialMatches,0};EiemOfflineCompile::WriteStage(appearanceCache,key,output);
  });
  for(size_t i=0;i<sources.size();++i){
   if(appearanceHits[i])++plan.appearanceHits;else ++plan.appearanceMisses;
   plan.textures+=appearances[i].first;plan.materialMatches+=appearances[i].second;
   auto bytes=std::move(appearances[i].bytes);auto bundle=EndfieldInput::Decode(bytes,inputLimit);
   Input input;input.file=sources[i].file;input.path=destination/input.file;
   for(const auto& member:bundle.members){
    Require(sourceMembers.insert(member.name).second,"reload duplicate CAB/resource member");
    if(!Serialized(member.bytes))continue;Require(EiemLoadingGeneration::Cab(member.name),"reload CAB grammar");
    plan.names.members.emplace(member.name,"CAB-"+EiemLoadingGeneration::Token(plan.key,member.name,32));
    auto l=ParseSerialized(member.bytes);
    for(const auto& e:EiemLoadingGraphAudit::ReadExternals(member.bytes,l))input.externals.insert(e.member);
    for(const auto& row:l.objects)if(l.types.at(row.second.type).classId==142){
     const auto& t=l.types.at(row.second.type);auto old=Name(t,ObjectValue(member.bytes,l,row.first));
     Require(old.size()==32&&old.rfind("main/",0)==0&&old.substr(29)==".ab","reload Bundle identity grammar");
     input.bundle=old;
     Require(plan.names.bundles.emplace(old,"main/"+EiemLoadingGeneration::Token(plan.key,"bundle:"+old,24)+".ab").second,"reload duplicate Bundle identity");
    }
   }
   plan.inputs.push_back(std::move(input));rebuilt.push_back(std::move(bytes));if(progress)progress(i+1,sources.size());
   if(stage)stage("resource",i+1,sources.size());
  }
 if(geometryHit)marked=markers.size();
 else if(!geometryCache.empty()){geometry.assets=assets;auto temporary=geometryCache/(geometryKey+"-building-"+std::to_string(GetCurrentProcessId()));WriteGeometry(temporary,geometryKey,sources,geometry);std::filesystem::rename(temporary,geometryDirectory);}
 plan.rendererBoundsHits=boundsCache.hits;plan.rendererBoundsMisses=boundsCache.misses;if(cost)cost("renderer-bounds",boundsCache.computeMs);
 Require(marked==markers.size()&&plan.materialMatches==package.materials.size()&&plan.textures==package.textures.size(),"reload rebuilt author coverage differs: markers="+std::to_string(marked)+"/"+std::to_string(markers.size())+" materials="+std::to_string(plan.materialMatches)+"/"+std::to_string(package.materials.size())+" textures="+std::to_string(plan.textures)+"/"+std::to_string(package.textures.size()));
 for(const auto& m:sourceMembers)if(!plan.names.members.count(m)){
  auto cab=m.substr(0,36);Require(m==cab+".resS"&&plan.names.members.count(cab),"reload unknown stream grammar");plan.names.members.emplace(m,plan.names.members.at(cab)+".resS");
 }
 std::map<std::string,size_t> memberInput;std::map<NativeReference,Texture> textures;
 std::map<Identity,Material> materials;std::vector<std::tuple<Identity,std::vector<std::pair<std::string,NativeReference>>>> textureRefs;
 if(stage)stage("metadata",0,rebuilt.size());
 for(size_t i=0;i<rebuilt.size();++i){
  auto bundle=EndfieldInput::Decode(rebuilt[i],inputLimit);
  for(const auto& m:bundle.members){memberInput.emplace(m.name,i);if(!Serialized(m.bytes))continue;auto l=ParseSerialized(m.bytes);
   for(const auto& a:SerializedAssets(m.bytes,m.name,l)){
    if(a.classId==1&&std::find(rootPaths.begin(),rootPaths.end(),a.path)!=rootPaths.end()){
     Root root;root.path=a.path;root.input=i;root.cab=m.name;root.source=a.pathId;
#ifndef EFF_OFFLINE_PACKAGE_V35
     auto originalBundle=EndfieldInput::Decode(Read(baseline/"inputs"/sources[i].file,inputLimit),inputLimit);
     auto original=std::find_if(originalBundle.members.begin(),originalBundle.members.end(),[&](const auto& x){return x.name==m.name;});Require(original!=originalBundle.members.end(),"assembly pristine member absent");
     root.assembly=EiemRenderAssemblyDescription::Build(original->bytes,m.bytes,m.name,a.pathId,package,assets,markers);
#endif
     plan.roots.push_back(std::move(root));
    }
    if(a.classId==21){
     const auto& t=l.types.at(l.objects.at(a.pathId).type);auto v=ObjectValue(m.bytes,l,a.pathId);Material mat;mat.name=Name(t,v);
     std::vector<std::pair<std::string,NativeReference>> refs;
     for(const auto& p:ArrayValue(t,Child(t,Child(t,v,"m_SavedProperties"),"m_TexEnvs")).children){
      const auto& prop=Child(t,p,"first");refs.push_back({Text(prop.raw.data(),prop.raw.size()),Reference(t,Child(t,Child(t,p,"second"),"m_Texture"),l,m.name)});
     }
     Identity key{a.path,a.asset};materials.emplace(key,std::move(mat));textureRefs.push_back({key,std::move(refs)});
    }
   }
   for(const auto& row:l.objects)if(l.types.at(row.second.type).classId==28){
    const auto& t=l.types.at(row.second.type);auto v=ObjectValue(m.bytes,l,row.first);
    textures.emplace(NativeReference{m.name,row.first},Texture{Name(t,v),int(Integer(Child(t,v,"m_Width"))),int(Integer(Child(t,v,"m_Height"))),int(Integer(Child(t,v,"m_MipCount")))});
   }
  }
  if(stage)stage("metadata",i+1,rebuilt.size());
 }
 Require(plan.roots.size()==rootPaths.size(),"reload prefab roots absent or ambiguous");
 for(auto& root:plan.roots){
  auto bundle=EndfieldInput::Decode(rebuilt.at(root.input),inputLimit);
  for(const auto& member:bundle.members)if(Serialized(member.bytes)){
   auto l=ParseSerialized(member.bytes);
   for(const auto& row:l.objects)if(l.types.at(row.second.type).classId==137){
    const auto& t=l.types.at(row.second.type);auto renderer=ObjectValue(member.bytes,l,row.first);
    auto ref=Reference(t,Child(t,renderer,"m_Mesh"),l,member.name);auto marker=markers.find(ref);
    if(marker!=markers.end())++root.markers[marker->second];
   }
  }
#ifdef EFF_OFFLINE_PACKAGE_V35
  Require(!root.markers.empty(),"reload root has no serialized authored Renderer coverage");
#else
  Require(!root.assembly.sources.empty(),"reload root has no source Renderer assembly");
#endif
 }
 for(const auto& refs:textureRefs){auto& mat=materials.at(std::get<0>(refs));for(const auto& p:std::get<1>(refs)){
  TextureSlot slot;slot.property=p.first;slot.nonnull=p.second.second!=0;auto found=textures.find(p.second);if(found!=textures.end()){slot.local=true;slot.texture=found->second;}mat.slots.push_back(std::move(slot));
 }}
 for(const auto& m:package.materials)plan.materials.emplace(m.first,materials.at(m.second.source));
 std::set<size_t> closure;std::vector<size_t> queue;
 for(const auto& r:plan.roots)queue.push_back(r.input);
 while(!queue.empty()){auto i=queue.back();queue.pop_back();if(!closure.insert(i).second)continue;
  for(const auto& e:plan.inputs[i].externals){auto f=memberInput.find(e);if(f==memberInput.end()){if(e!="unity default resources")plan.shared.insert(e);}else queue.push_back(f->second);}
 }
  plan.closure.assign(closure.begin(),closure.end());Require(!plan.closure.empty(),"reload root closure empty");
  std::filesystem::create_directories(destination);
  std::filesystem::create_directories(destination/"startup");
  if(stage)stage("publish",0,rebuilt.size());
  // Every input is independent after metadata discovery. Run the expensive
  // namespace rewrite, encode, decode verification and disk writes in a small
  // worker pool. The generation remains private until ready.tsv is written, so
  // parallel output cannot expose a partial graph to the loader. Keep the
  // worker count bounded: each job briefly holds decoded TypeTree values and a
  // second encoded copy, which otherwise causes avoidable memory pressure on
  // large 100+ MiB author packages.
  std::vector<std::string> publishedHashes(rebuilt.size());
  std::vector<size_t> publishedOpaque(rebuilt.size());
  std::vector<size_t> namespaceHits(rebuilt.size()),namespaceMisses(rebuilt.size());
  auto namespaceCache=geometryCache.empty()?std::filesystem::path{}:geometryCache.parent_path()/"namespace";
  if(!namespaceCache.empty())std::filesystem::create_directories(namespaceCache);
  std::atomic<size_t> nextPublish{0};
  std::exception_ptr publishError;
  std::mutex publishErrorMutex;
  std::mutex publishCostMutex;
  std::function<void(const char*,double)> workerCost;
  if(cost)workerCost=[&](const char* name,double elapsed){std::lock_guard<std::mutex> lock(publishCostMutex);cost(name,elapsed);};
  auto publishOne=[&](size_t i){
   // Same authored payload in the original CAB namespace for the game's first
   // ordinary resource load; the isolated variant below is used by F10.
   Save(destination/"startup"/sources[i].file,rebuilt[i]);
   auto source=std::move(rebuilt[i]);
   auto bundle=EndfieldInput::Decode(source,inputLimit);
   size_t opaque=0;
   for(auto& m:bundle.members){
    if(Serialized(m.bytes)){
     bool reused=false;auto v=Measure(workerCost,"namespace",[&]{return EiemLoadingGeneration::PrepareSerializedCached(m.bytes,plan.names,namespaceCache,reused);});
     if(reused)++namespaceHits[i];else ++namespaceMisses[i];
     m.bytes=std::move(v.bytes);opaque+=v.opaqueObjectsPreserved;
    }
    m.name=plan.names.members.at(m.name);
   }
   auto bytes=Measure(workerCost,"encode",[&]{return EndfieldInput::Encode(source,bundle.members,inputLimit);});
   auto check=Measure(workerCost,"verify-decode",[&]{return EndfieldInput::Decode(bytes,inputLimit);});
   Require(check.members.size()==bundle.members.size(),"reload encoded graph coverage differs");
   for(size_t j=0;j<bundle.members.size();++j)Require(check.members[j].name==bundle.members[j].name&&check.members[j].flags==bundle.members[j].flags&&check.members[j].bytes==bundle.members[j].bytes,"reload encoded graph payload differs");
   publishedHashes[i]=Hash(bytes);publishedOpaque[i]=opaque;Save(plan.inputs[i].path,bytes);
  };
  auto hardware=std::thread::hardware_concurrency();
  auto workers=std::min<size_t>(rebuilt.size(),std::max<size_t>(1,std::min<unsigned>(8,hardware?hardware:1)));
  std::vector<std::thread> pool;pool.reserve(workers);
  for(size_t worker=0;worker<workers;++worker)pool.emplace_back([&]{
   for(;;){
    auto i=nextPublish.fetch_add(1);if(i>=rebuilt.size())return;
    try{publishOne(i);}catch(...){std::lock_guard<std::mutex> lock(publishErrorMutex);if(!publishError)publishError=std::current_exception();}
   }
  });
  for(auto& worker:pool)worker.join();
  if(publishError)std::rethrow_exception(publishError);
  for(size_t i=0;i<rebuilt.size();++i){plan.inputs[i].hash=std::move(publishedHashes[i]);plan.opaque+=publishedOpaque[i];plan.namespaceHits+=namespaceHits[i];plan.namespaceMisses+=namespaceMisses[i];}
  if(stage)stage("publish",rebuilt.size(),rebuilt.size());
 // Ready is the last write. An interrupted worker cannot publish a partial graph.
 std::ostringstream report;report<<"EFF_RESOURCE_RELOAD_GENERATION\t1\t"<<plan.key<<"\t"<<plan.author<<"\t"<<plan.contract<<'\n';
 for(const auto& in:plan.inputs)report<<"input\t"<<in.file<<'\t'<<in.hash<<'\n';
 auto text=report.str();Save(destination/"ready.tsv",Bytes(text.begin(),text.end()));return plan;
}
} // namespace EiemResourceReloadPlan
