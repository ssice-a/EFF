#pragma once
#include "eiem_renderer_input_builder.h"
#include "eiem_physics_input_builder.h"
#include "eiem_render_assembly_description.h"
#include "eiem_loading_generation.h"
#include "eiem_runtime_shape_identity.h"
#include "eiem_loading_input_substitution.h"
#include "eiem_offline_compile_helpers.h"
#include "eiem_input_retention.h"
#include <fstream>
#include <functional>
#include <chrono>
#include <atomic>
#include <exception>
#include <mutex>
#include <thread>

// A disk-only resource generation. Baselines are pristine serialized inputs,
// never the installed substituted inputs. No Unity operation is allowed here.
#include "eiem_resource_recipe.h"
namespace EiemResourceReloadPlan {
using namespace EiemNativeInput;
inline bool Serialized(const Bytes& b){return EiemNativeInput::SerializedHeader(b);}
inline std::string Hash(const Bytes& b){auto h=Sha(b.data(),b.size());return Hex(h.data(),h.size());}
inline std::string Name(const Type& t,const Value& v){const auto& n=Child(t,v,"m_Name");return Text(n.raw.data(),n.raw.size());}
struct Source {std::string file,hash;std::filesystem::path physical;uint64_t offset=0,size=0;};
inline Bytes ReadSource(const std::filesystem::path& directory,const Source& s){
 if(s.physical.empty())return Read(directory/"inputs"/s.file);
 auto before=FileStamp(s.physical);Require(s.offset<=INT64_MAX&&s.offset<=before.bytes&&s.size<=before.bytes-s.offset&&s.size<=SIZE_MAX,"rule source range differs");
 std::ifstream in(s.physical,std::ios::binary);Require(bool(in),"rule source open failed");in.seekg(std::streamoff(s.offset));Bytes bytes(size_t(s.size));
 for(size_t p=0;p<bytes.size();){auto n=std::min<size_t>(1024*1024,bytes.size()-p);in.read((char*)bytes.data()+p,n);Require(size_t(in.gcount())==n,"rule source truncated");p+=n;}
 Require(SameStamp(before,FileStamp(s.physical))&&Hash(bytes)==s.hash,"rule source content changed");return bytes;
}
struct RendererRule {std::string cab;int64_t id=0;Identity mesh;};
inline void VerifySourceSnapshot(const std::filesystem::path& directory,const Source& source,const InputFileStamp& stamp){
 const auto path=source.physical.empty()?directory/"inputs"/source.file:source.physical;
 if(!SameStamp(stamp,FileStamp(path))){auto bytes=ReadSource(directory,source);Require(Hash(bytes)==source.hash,"reload pristine source changed during compilation");}
}
struct MemberNamespaceRecipe {std::string member,hash;std::vector<EiemLoadingGeneration::Patch> patches;};
struct Root {std::string path;size_t input=0;std::map<std::string,unsigned> markers;
 std::string cab;int64_t source=0;EiemRenderAssemblyDescription::Description assembly;
 std::shared_ptr<const EiemRenderAssemblyDescription::Description> preparedAssembly;
 const EiemRenderAssemblyDescription::Description& Assembly()const{return preparedAssembly?*preparedAssembly:assembly;}
};
// Transient analysis from this exact immutable input, never serialized. Full
// export retains only layouts/pristine assemblies between metadata and proof
// stages; large GraphData values are released after each input's metadata.
struct InputAnalysis {
 using GraphData=EiemRenderAssemblyDescription::GraphData;
 std::map<std::string,std::pair<std::shared_ptr<const GraphData>,std::shared_ptr<const GraphData>>> graphs;
 std::map<std::string,std::pair<SerializedLayout,SerializedLayout>> layouts;
 std::vector<Root> pristine;
};
struct Input {std::string file,hash,bundle;std::filesystem::path path;std::set<std::string> externals;};
struct Texture {std::string name;int width=0,height=0,mips=0;};
struct TextureSlot {std::string property;bool nonnull=false,local=false;Texture texture;};
struct Material {std::string name;std::vector<TextureSlot> slots;};
struct Part {std::string id,marker,material;unsigned vertices=0;std::vector<std::string> bones,shapes;};
struct Plan {
 std::string key,author,contract;std::filesystem::path directory;
 EiemLoadingGeneration::Names names;std::vector<Input> inputs;std::vector<Root> roots;
 std::vector<size_t> closure;std::set<std::string> shared;
 // Offline input edges; runtime unions indices when independently compiled Mods share a file.
 std::vector<std::vector<size_t>> inputDependencies;
 // Transient author-build selection. Compacted before an artifact is encoded.
 std::vector<size_t> retainedInputs;
 // Build-only routing for new references to native dependencies omitted from
 // compiled views. Encoders persist their Bundle edges, without copying them.
 std::map<std::string,std::string> sharedBundles;
 // Native views already prove their source/author bytes offline. Their
 // relocated identity is separate from the byte SHA of a materialized file.
 std::map<size_t,std::string> viewIdentities;
 std::map<std::string,Part> parts;std::map<std::string,Material> materials;
 size_t meshParts=0,rendererParts=0,textures=0,materialMatches=0,opaque=0;
 size_t rendererBoundsHits=0,rendererBoundsMisses=0;
 size_t namespaceHits=0,namespaceMisses=0;
 size_t appearanceHits=0,appearanceMisses=0,geometryInputHits=0,geometryInputMisses=0;
 bool composed=false;
 bool rulesOnly=false,resourceAddressed=false;std::vector<Source> originals;std::vector<std::vector<RendererRule>> rendererRules;std::vector<std::string> startupHashes;
 std::vector<std::vector<MemberNamespaceRecipe>> namespaceRecipes;
 bool spanRecipes=false;std::vector<EiemResourceRecipe::Frame> frameRecipes;std::vector<std::vector<EiemResourceRecipe::Member>> memberRecipes;
};
inline std::vector<Source> ReadBaseline(const std::filesystem::path& directory,std::vector<std::string>& roots,const std::filesystem::path& game={}) {
 auto bytes=Read(directory/"baseline.tsv");std::istringstream lines(Text(bytes.data(),bytes.size()));
 std::string line;Require(bool(std::getline(lines,line)),"reload baseline header absent");if(!line.empty()&&line.back()=='\r')line.pop_back();
 Require(TableHeader(line,"EFF_RESOURCE_RELOAD_BASELINE"),"reload baseline format differs");
 std::vector<Source> sources;std::set<std::string> files,paths;
 while(std::getline(lines,line)){
  if(!line.empty()&&line.back()=='\r')line.pop_back();auto row=TabFields(line);
  Require(row.size()==3||row.size()==6,"reload baseline row differs");
  if(row[0]=="input"){
   std::filesystem::path file=std::filesystem::u8path(row[1]);
   Require(file==file.filename()&&file.extension()==".vfs"&&files.insert(row[1]).second&&DigestText(row[2]),"reload baseline input invalid/duplicate");
   Source source{row[1],row[2]};if(row.size()==6){auto path=std::filesystem::u8path(row[3]);Require(!path.is_absolute()&&path.u8string().find("..") ==std::string::npos,"rule original path escapes game");source.physical=(game.empty()?directory.parent_path().parent_path():game)/path;source.offset=std::stoull(row[4]);source.size=std::stoull(row[5]);}sources.push_back(std::move(source));
  }else{
   Require(row[0]=="root"&&row[1].rfind("assets/",0)==0&&row[1].find("..") ==std::string::npos&&row[2]=="GameObject"&&paths.insert(row[1]).second,"reload root identity invalid/duplicate");
   roots.push_back(row[1]);
  }
 }
 Require(!sources.empty()&&!roots.empty(),"reload baseline coverage/limit");
 std::sort(sources.begin(),sources.end(),[](const auto&a,const auto&b){return a.file<b.file;});return sources;
}
inline NativeReference Reference(const Type& t,const Value& v,const SerializedLayout& l,const std::string& cab){
 auto f=int32_t(Integer(Child(t,v,"m_FileID")));auto id=int64_t(Integer(Child(t,v,"m_PathID")));
 Require(f>=0&&size_t(f)<=l.externals.size(),"reload descriptor external index");return {f?l.externals.at(f-1):cab,id};
}
inline bool Mark(EndfieldInput::Bundle& bundle,const std::map<NativeReference,std::string>& markers,size_t& count){
 bool changed=false;
 for(auto& m:bundle.members)if(Serialized(m.bytes)){
  auto l=ParseSerialized(m.bytes);std::map<int64_t,Bytes> edits;std::map<int64_t,AuthorByteSpans> objectOrigins;
  for(const auto& o:l.objects){auto it=markers.find({m.name,o.first});if(it==markers.end())continue;
   const auto& t=l.types.at(o.second.type);Require(t.classId==43,"reload marker target not Mesh");
   auto value=ObjectValue(m.bytes,l,o.first,&m.origins);auto shapes=EncodeValue(t,Child(t,value,"m_Shapes")),vertices=EncodeValue(t,Child(t,value,"m_VertexData"));
   SetString(t,Child(t,value,"m_Name"),it->second);
   Require(EncodeValue(t,Child(t,value,"m_Shapes"))==shapes&&EncodeValue(t,Child(t,value,"m_VertexData"))==vertices,"reload marker changed native shape/vertex payload");
   edits.emplace(o.first,EncodeValue(t,value,&objectOrigins[o.first]));++count;
  }
  if(!edits.empty()){m.bytes=BuildSerialized(m.bytes,l,edits,{},{},{},{},&m.origins,&objectOrigins,&m.origins);changed=true;}
 }
 return changed;
}
inline Bytes Mark(const Bytes& input,const std::map<NativeReference,std::string>& markers,size_t& count){
 auto bundle=EndfieldInput::Decode(input);return Mark(bundle,markers,count)?EndfieldInput::Encode(input,bundle.members):input;
}
inline void Save(const std::filesystem::path& p,const Bytes& b){
 std::ofstream out(p,std::ios::binary);Require(bool(out),"reload input create failed");out.write((const char*)b.data(),b.size());out.flush();Require(bool(out),"reload input write failed");
}
inline bool SaveFromCache(const std::filesystem::path& output,const std::filesystem::path& cached,const Bytes& expected){
 // Stage writers replace files atomically instead of modifying published
 // bytes. A same-volume hard link therefore survives cache replacement or
 // deletion, and avoids another full write of the original-namespace input.
 // Verify the linked file too: the stage may have changed after ReadStage.
 Require(!std::filesystem::exists(output),"reload output already exists");
 if(!cached.empty()&&CreateHardLinkW(output.c_str(),cached.c_str(),nullptr)){
  Require(Read(output)==expected,"reload linked startup cache payload changed");return true;
 }
 Save(output,expected);return false;
}
template<class F> inline auto Measure(const std::function<void(const char*,double)>& cost,const char* name,F operation){
 if(!cost)return operation();auto start=std::chrono::steady_clock::now();auto result=operation();
 cost(name,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());return result;
}
// Mesh/Renderer construction is independent of pixels, material fields and
// runtime keys. Persist that stage so a texture/INI edit never rebuilds bones,
// bindposes, vertex streams or Renderer topology.
inline std::string GeometryDigest(const Package& package,const std::vector<Source>& sources){
 auto p=package;for(auto it=p.resources.begin();it!=p.resources.end();)if(it->second->kind!=1&&!it->second->blocks.count("@physics"))it=p.resources.erase(it);else ++it;
 p.textures.clear();for(auto& m:p.materials)m.second.textures.clear();
 std::string key="EFF_GEOMETRY_STAGE/mesh-filter+renderer+material-clones+native-material-preloads\n"+InputContractDigest(p);
 for(const auto& source:sources)key+='\n'+source.file+'\n'+source.hash;
 return Hash(Bytes(key.begin(),key.end()));
}
struct GeometryStage {AssetIndex assets;std::vector<Bytes> inputs;std::vector<std::pair<size_t,size_t>> counts;};
inline GeometryStage ReadGeometry(const std::filesystem::path& directory,const std::string& key,const std::vector<Source>& sources){
 GeometryStage stage;auto data=Read(directory/"stage.tsv");std::istringstream lines(Text(data.data(),data.size()));std::string line;
 Require(bool(std::getline(lines,line))&&TableIdentity(Trim(line),"EFF_GEOMETRY_STAGE",key),"geometry cache identity differs");
 while(std::getline(lines,line)){auto r=TabFields(line);if(r[0]=="asset"){
  Require(r.size()==7&&r[6].size()==32,"geometry asset descriptor");Asset asset{r[1],r[2],r[3],std::stoll(r[4]),std::stoi(r[5]),{}};
  for(size_t i=0;i<16;++i)asset.type[i]=(unsigned char)std::stoul(r[6].substr(i*2,2),nullptr,16);
  Require(stage.assets.emplace(Identity{asset.path,asset.asset},asset).second,"duplicate geometry asset");
 }else{auto i=stage.inputs.size();Require(r.size()==5&&r[0]=="input"&&i<sources.size()&&r[1]==sources[i].file&&DigestText(r[2]),"geometry input descriptor");
  auto bytes=Read(directory/std::filesystem::u8path(r[1]));Require(Hash(bytes)==r[2],"geometry cache payload changed");stage.inputs.push_back(std::move(bytes));stage.counts.emplace_back(std::stoull(r[3]),std::stoull(r[4]));
 }}Require(stage.inputs.size()==sources.size()&&!stage.assets.empty(),"geometry cache incomplete");return stage;
}
inline void WriteGeometry(const std::filesystem::path& directory,const std::string& key,const std::vector<Source>& sources,const GeometryStage& stage){
 std::filesystem::create_directories(directory);std::ostringstream rows;rows<<"EFF_GEOMETRY_STAGE\t1\t"<<key<<'\n';
 for(const auto& r:stage.assets){const auto& a=r.second;rows<<"asset\t"<<a.path<<'\t'<<a.asset<<'\t'<<a.cab<<'\t'<<a.pathId<<'\t'<<a.classId<<'\t'<<Hex(a.type.data(),a.type.size())<<'\n';}
 for(size_t i=0;i<stage.inputs.size();++i){Save(directory/sources[i].file,stage.inputs[i]);rows<<"input\t"<<sources[i].file<<'\t'<<Hash(stage.inputs[i])<<'\t'<<stage.counts[i].first<<'\t'<<stage.counts[i].second<<'\n';}
 auto text=rows.str();Save(directory/"stage.tsv",Bytes(text.begin(),text.end()));
}
inline std::string AppearanceInputDigest(const Bytes& input,const Package& package,const AssetIndex& assets){
 auto bundle=EndfieldInput::Decode(input);std::set<std::string> members;for(const auto& m:bundle.members)members.insert(m.name);
 auto p=package;p.meshes.clear();p.parts.clear();p.rules.clear();p.targetIndex.clear();p.resources.clear();
 // All binding/template declarations remain in the key: preloads in this CAB
 // can reference textures hosted by a different input. Pixel/material payloads
 // only affect the CAB that owns their native template.
 for(const auto& m:p.materials)if(members.count(assets.at(m.second.source).cab))p.resources.emplace(m.second.resource,package.resources.at(m.second.resource));
 for(const auto& t:p.textures)if(members.count(assets.at(t.second.source).cab))p.resources.emplace(t.second.resource,package.resources.at(t.second.resource));
 auto text="EFF_APPEARANCE_INPUT_1/material-clones\n"+Hash(input)+"\n"+InputContractDigest(p);return Hash(Bytes(text.begin(),text.end()));
}
inline std::string GeometryInputDigest(const Bytes& input,const Package& package,const AssetIndex& assets,const MaterialPreloadIndex* materialPreloads=nullptr){
 auto bundle=EndfieldInput::Decode(input);std::set<std::string> members;std::set<Identity> renderTargets;
 std::map<NativeReference,Identity> selected;
 for(const auto& target:package.targetIndex){const auto& asset=assets.at(target.first);selected.emplace(NativeReference{asset.cab,asset.pathId},target.first);}
 std::set<NativeReference> references;for(const auto& row:selected)references.insert(row.first);
 for(const auto& member:bundle.members){
  members.insert(member.name);if(!Serialized(member.bytes))continue;auto layout=ParseSerialized(member.bytes);
  for(const auto& consumer:MeshConsumers(member.bytes,layout,member.name,references))renderTargets.insert(selected.at(consumer.mesh));
 }
 auto p=package;p.resources.clear();p.textures.clear();for(auto& m:p.materials)m.second.textures.clear();
 for(const auto& row:package.resources)if(row.second->blocks.count("@physics"))p.resources.emplace(row);
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
 auto text="EFF_GEOMETRY_INPUT/mesh-filter+renderer+material-clones+native-material-preloads\n"+Hash(input)+"\n"+InputContractDigest(p);
 if(materialPreloads&&!renderTargets.empty())for(const auto& material:p.materials){const auto& asset=assets.at(material.second.source);const auto& refs=materialPreloads->at({asset.cab,asset.pathId});
  text+='\n'+material.first;for(const auto& ref:refs)text+='\n'+ref.first+":"+std::to_string(ref.second);
 }
 return Hash(Bytes(text.begin(),text.end()));
}
inline Plan Build(const Package& authoredPackage,const std::filesystem::path& baseline,const std::filesystem::path& destination,
                  const std::string& nonce,std::function<void(size_t,size_t)> progress={},
                  std::function<void(const char*,size_t,size_t)> stage={},
                  std::function<void(const char*,double)> cost={},const std::filesystem::path& geometryCache={},bool rulesOnly=false,const std::filesystem::path& game={},
                  std::function<void(size_t,Plan&,const Bytes&,const EndfieldInput::Bundle&,const EndfieldInput::Bundle&,
                                     const EiemResourceRecipe::Dictionary&,const EiemResourceRecipe::Dictionary&,const InputAnalysis&)> describeInput={}){
 auto package=authoredPackage;
 Require(!std::filesystem::exists(destination),"reload generation directory exists");
 std::vector<std::string> rootPaths;auto sources=ReadBaseline(baseline,rootPaths,game);
 Plan plan;plan.directory=destination;plan.author=PackageDigest(package);plan.contract=InputContractDigest(package);plan.composed=package.composed;plan.rulesOnly=rulesOnly;plan.originals=rulesOnly?sources:std::vector<Source>{};
 std::string identity=plan.author+"\n"+plan.contract+"\n"+nonce;
 std::vector<Bytes> pristineInputs(sources.size());AssetIndex assets;MeshSchemaIndex schemas;std::vector<BuiltMeshPart> meshParts;
 std::vector<InputFileStamp> sourceStamps(sources.size());
 auto sourcePath=[&](size_t i){return sources[i].physical.empty()?baseline/"inputs"/sources[i].file:sources[i].physical;};
 // Per-export ownership: later proofs read these exact SHA-verified inputs.
 // Keep the immutable encoded baseline only. Decoded Bundles are large (their
 // member byte arrays and TypeTree views are copied); retain them per stage
 // instead of keeping a second full decoded copy for the whole export.
 MeshBoneHashIndex sourcePalettes;
 MaterialPreloadIndex materialPreloads;
 std::set<std::string> sourceMembers;
 auto geometryKey=geometryCache.empty()?std::string{}:GeometryDigest(package,sources);auto geometryDirectory=geometryCache/geometryKey;
 bool geometryHit=!geometryCache.empty()&&std::filesystem::exists(geometryDirectory/"stage.tsv");GeometryStage geometry;
 if(geometryHit){geometry=ReadGeometry(geometryDirectory,geometryKey,sources);assets=geometry.assets;if(stage)stage("geometry-reused",sources.size(),sources.size());}
 if(stage)stage("source",0,sources.size());
 auto phaseStart=std::chrono::steady_clock::now();
 for(const auto& s:sources)identity+="\n"+s.file+"\n"+s.hash;
 if(!geometryHit){
  struct SourceInventory {AssetIndex assets;MeshSchemaIndex schemas;MeshBoneHashIndex palettes;MaterialPreloadIndex preloads;};
  std::vector<SourceInventory> inventories(sources.size());
  EiemOfflineCompile::ParallelInputs(sources.size(),[&](size_t i){const auto& s=sources[i];auto& inventory=inventories[i];
   sourceStamps[i]=FileStamp(sourcePath(i));
   auto bytes=ReadSource(baseline,s);Require(Hash(bytes)==s.hash,"reload pristine source SHA differs: "+s.file);
   Require(SameStamp(sourceStamps[i],FileStamp(sourcePath(i))),"reload pristine source changed while reading");
   auto pristine=EndfieldInput::Decode(bytes);
   AddSourceInventory(inventory.assets,inventory.schemas,inventory.palettes,inventory.preloads,pristine,package);
   pristineInputs[i]=std::move(bytes);
  });
  for(size_t i=0;i<inventories.size();++i){auto& inventory=inventories[i];
   for(auto& asset:inventory.assets)AddAsset(assets,std::move(asset.second));
   schemas.merge(inventory.schemas);
   for(auto& palette:inventory.palettes)Require(sourcePalettes.emplace(palette.first,std::move(palette.second)).second,"duplicate source Mesh bone palette");
   for(const auto& preload:inventory.preloads)materialPreloads[preload.first].insert(preload.second.begin(),preload.second.end());
   if(stage)stage("source",i+1,sources.size());
  }
 }
 if(cost)cost("source-inventory-wall",std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-phaseStart).count());
 if(std::any_of(package.textures.begin(),package.textures.end(),[](const auto& t){return t.second.source.path.empty();})){
  // Geometry only edits Mesh/Renderer topology and their preload lists;
  // Material slot PPtrs and original Texture objects remain pristine. Both
  // paths already hold SHA-verified bytes, so template discovery needs no
  // second read of every baseline input on each texture export.
   Measure(cost,"texture-templates",[&]{ResolveTextureTemplates(package,geometryHit?geometry.inputs:pristineInputs,assets);return 0;});
 }
 for(const auto& target:package.targetIndex)Require(assets.count(target.first),"reload source Mesh absent from pristine graph");
 for(const auto& m:package.materials)Require(assets.count(m.second.source),"reload Material absent from pristine graph");
 for(const auto& t:package.textures)Require(assets.count(t.second.source),"reload Texture absent from pristine graph");
 if(!geometryHit)for(const auto& target:package.targetIndex){
  const auto& asset=assets.at(target.first);const auto& rule=package.rules.at(target.second);if(rule.skip)continue;
  for(size_t i=0;i<rule.parts.size();++i){const auto& id=rule.parts[i];const auto& mesh=package.meshes.at(package.parts.at(id).mesh);
   meshParts.push_back({target.first,target.second,id,asset.cab,asset.pathId,i?AllocateMeshId(asset.cab,package,target.first,id):asset.pathId,mesh.vertices,mesh.indices,sourcePalettes.at({asset.cab,asset.pathId})});
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
  // Independent Mod exports need no encoded intermediate stages. Keep one
  // editable bundle through geometry, appearance, metadata and proof building.
  const bool inMemoryBuild=rulesOnly&&geometryCache.empty();
  std::vector<EndfieldInput::Bundle> editedBundles(inMemoryBuild?sources.size():0);
  std::vector<unsigned char> inputChanged(inMemoryBuild?sources.size():0);
  if(stage)stage("resource",0,sources.size());
  std::vector<Bytes> rendererInputs(sources.size());
  phaseStart=std::chrono::steady_clock::now();
  if(geometryHit){
   for(size_t i=0;i<sources.size();++i){rendererInputs[i]=std::move(geometry.inputs[i]);plan.meshParts+=geometry.counts[i].first;plan.rendererParts+=geometry.counts[i].second;}
  }else{
   auto physicsDocuments=Measure(cost,"physics-author",[&]{return EiemPhysicsInput::Documents(package);});
   std::set<std::string> meshCabs;for(const auto& target:package.targetIndex)meshCabs.insert(assets.at(target.first).cab);
   auto inputCache=geometryCache.empty()?std::filesystem::path{}:geometryCache.parent_path()/"geometry-inputs";
   if(!inputCache.empty())std::filesystem::create_directories(inputCache);
   std::vector<EiemOfflineCompile::Stage> inputs(sources.size());std::vector<unsigned char> hits(sources.size());std::mutex geometryCostMutex;
   std::function<void(const char*,double)> geometryCost;if(cost)geometryCost=[&](const char* name,double elapsed){std::lock_guard<std::mutex> lock(geometryCostMutex);cost(name,elapsed);};
   EiemOfflineCompile::ParallelInputs(sources.size(),[&](size_t i){
    auto key=inputCache.empty()?std::string{}:GeometryInputDigest(pristineInputs[i],package,assets,&materialPreloads);auto& output=inputs[i];
    if(EiemOfflineCompile::ReadStage(inputCache,key,output)){hits[i]=1;return;}
     if(inMemoryBuild){
      auto& bundle=editedBundles[i];
      bundle=EndfieldInput::Decode(pristineInputs[i]);
     bool ownsMesh=std::any_of(bundle.members.begin(),bundle.members.end(),[&](const auto& member){return meshCabs.count(member.name)!=0;});
     auto mesh=Measure(geometryCost,"mesh-source",[&]{return ownsMesh?BuildMeshInput(bundle,package):MeshInputBuild{};});
     bool changed=mesh.changedObjects!=0;
     changed=Measure(geometryCost,"physics",[&]{return EiemPhysicsInput::Build(bundle,package,assets,&physicsDocuments);})||changed;
     auto renderer=Measure(geometryCost,"renderer",[&]{return BuildRendererInput(bundle,package,assets,meshParts,schemas,&boundsCache,&materialPreloads,geometryCost);});
     changed=renderer.changedObjects!=0||changed;
     size_t localMarked=0;changed=Measure(geometryCost,"markers",[&]{return Mark(bundle,markers,localMarked);})||changed;
     inputChanged[i]=changed;output={{},mesh.parts.size(),renderer.parts.size(),localMarked};return;
    }
    auto mesh=Measure(geometryCost,"mesh-source",[&]{
      auto pristine=EndfieldInput::Decode(pristineInputs[i]);
      bool ownsMesh=std::any_of(pristine.members.begin(),pristine.members.end(),[&](const auto& member){return meshCabs.count(member.name)!=0;});
     if(ownsMesh)return BuildMeshInput(pristineInputs[i],package);MeshInputBuild unchanged;unchanged.bytes=pristineInputs[i];return unchanged;
    });
    auto physics=Measure(geometryCost,"physics",[&]{return EiemPhysicsInput::Build(mesh.bytes,package,assets,&physicsDocuments);});
    auto renderer=Measure(geometryCost,"renderer",[&]{return BuildRendererInput(physics,package,assets,meshParts,schemas,&boundsCache,&materialPreloads,geometryCost);});
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
  if(cost)cost("geometry-wall",std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-phaseStart).count());
  auto appearanceCache=geometryCache.empty()?std::filesystem::path{}:geometryCache.parent_path()/"appearance";
  if(!appearanceCache.empty())std::filesystem::create_directories(appearanceCache);
  std::vector<EiemOfflineCompile::Stage> appearances(sources.size());std::vector<unsigned char> appearanceHits(sources.size());std::mutex appearanceCostMutex;
  std::vector<std::filesystem::path> appearanceFiles(sources.size());
  struct Metadata {
   std::vector<Root> roots;std::map<std::string,size_t> members;
   InputAnalysis analysis;
   std::map<NativeReference,Texture> textures;std::map<std::string,Material> materials;
   std::vector<std::tuple<std::string,std::vector<std::pair<std::string,NativeReference>>>> refs;
  };
  struct Inventory {Input input;EiemLoadingGeneration::Names names;std::vector<std::string> members;Metadata metadata;};
  std::vector<Inventory> inventories(sources.size());
  std::function<void(const char*,double)> appearanceCost;if(cost)appearanceCost=[&](const char* name,double elapsed){std::lock_guard<std::mutex> lock(appearanceCostMutex);cost(name,elapsed);};
  std::map<std::string,size_t> memberInput;std::map<NativeReference,Texture> textures;
  std::map<std::string,Material> materials;std::vector<std::tuple<std::string,std::vector<std::pair<std::string,NativeReference>>>> textureRefs;
  std::map<Identity,std::vector<std::string>> materialCopies;
  std::vector<std::shared_ptr<const std::vector<std::string>>> bonePalettes;
  std::unordered_map<size_t,std::vector<size_t>> bonePaletteBuckets;
  auto bonePaletteHash=[](const std::vector<std::string>& bones){
   size_t value=1469598103934665603ull;
   for(const auto& bone:bones){for(unsigned char c:bone){value^=c;value*=1099511628211ull;}value^=0xff;value*=1099511628211ull;}
   return value;
  };
  for(const auto& material:package.materials)materialCopies[material.second.source].push_back(material.first);
  phaseStart=std::chrono::steady_clock::now();
  // Resource descriptors retain the root graph analysis until proof publish;
  // keep this stage bounded so several full PFB GraphData trees can run in
  // parallel without recreating the old unbounded working-set spike.
  EiemOfflineCompile::ParallelInputs(sources.size(),[&](size_t i){
   auto key=appearanceCache.empty()?std::string{}:Measure(appearanceCost,"appearance-key",[&]{return AppearanceInputDigest(rendererInputs[i],package,assets);});auto& output=appearances[i];
   if(!appearanceCache.empty())appearanceFiles[i]=appearanceCache/(key+".vfs");
   if(Measure(appearanceCost,"appearance-cache",[&]{return EiemOfflineCompile::ReadStage(appearanceCache,key,output);}))appearanceHits[i]=1;
   else{
    auto texture=Measure(appearanceCost,"texture",[&]{return inMemoryBuild?BuildTextureInput(editedBundles[i],package,assets):BuildTextureInput(rendererInputs[i],package,assets);});
    if(inMemoryBuild){
     inputChanged[i]=inputChanged[i]||!texture.textures.empty()||texture.changedMaterials||texture.changedPreloads;
     texture.bytes=Measure(appearanceCost,"startup-encode",[&]{return inputChanged[i]?EndfieldInput::Encode(pristineInputs[i],editedBundles[i].members):pristineInputs[i];});
    }
    output={std::move(texture.bytes),texture.textures.size(),texture.materialMatches,0};EiemOfflineCompile::WriteStage(appearanceCache,key,output);
   }
   Measure(appearanceCost,"resource-descriptors",[&]{
   auto& inventory=inventories[i];auto& input=inventory.input;
   input.file=sources[i].file;input.path=destination/input.file;
   auto decoded=inMemoryBuild?EndfieldInput::Bundle{}:EndfieldInput::Decode(output.bytes);
   const auto& bundle=inMemoryBuild?editedBundles[i]:decoded;
   EndfieldInput::Bundle originalBundle;bool originalLoaded=false;
   const auto named=NamedSerializedAssets(bundle);
   for(const auto& member:bundle.members){
    inventory.members.push_back(member.name);inventory.metadata.members.emplace(member.name,i);
    if(!Serialized(member.bytes))continue;Require(EiemLoadingGeneration::Cab(member.name),"reload CAB grammar");
    inventory.names.members.emplace(member.name,"CAB-"+EiemLoadingGeneration::Token(plan.key,member.name,32));
    auto layout=ParseSerialized(member.bytes);
    for(const auto& e:EiemLoadingGraphAudit::ReadExternals(member.bytes,layout))input.externals.insert(e.member);
    for(const auto& row:layout.objects)if(layout.types.at(row.second.type).classId==142){
     const auto& t=layout.types.at(row.second.type);auto old=Name(t,ObjectValue(member.bytes,layout,row.first));
     input.bundle=old;
     Require(inventory.names.bundles.emplace(old,EiemLoadingGeneration::BundleIdentity(plan.key,old)).second,"reload duplicate Bundle identity");
    }
    auto nativeAssets=SerializedReferencedAssets(member.bytes,member.name,layout,&named);
    std::shared_ptr<const EiemRenderAssemblyDescription::GraphData> beforeGraph,afterGraph;
    for(const auto& asset:nativeAssets)if(asset.classId==1&&std::find(rootPaths.begin(),rootPaths.end(),asset.path)!=rootPaths.end()){
     Root root;root.path=asset.path;root.input=i;root.cab=member.name;root.source=asset.pathId;
     if(geometryHit&&!originalLoaded){originalBundle=EndfieldInput::Decode(ReadSource(baseline,sources[i]));originalLoaded=true;}
     EndfieldInput::Bundle decodedOriginal;
     if(!geometryHit)decodedOriginal=EndfieldInput::Decode(pristineInputs[i]);
     const auto& originalSource=geometryHit?originalBundle:decodedOriginal;
     auto original=std::find_if(originalSource.members.begin(),originalSource.members.end(),[&](const auto& x){return x.name==member.name;});Require(original!=originalSource.members.end(),"assembly pristine member absent");
     if(!beforeGraph)beforeGraph=std::make_shared<const EiemRenderAssemblyDescription::GraphData>(original->bytes,member.name);
     if(!afterGraph)afterGraph=std::make_shared<const EiemRenderAssemblyDescription::GraphData>(member.bytes,member.name,std::move(layout),beforeGraph.get(),&original->bytes);
     EiemRenderAssemblyDescription::Graph before(beforeGraph,asset.pathId),after(afterGraph,asset.pathId);
     root.assembly=EiemRenderAssemblyDescription::Build(before,after,member.name,package,assets,markers);
     // The pristine assembly is rebuilt on demand during proof publication.
     // Keeping a second full Description here doubles every input's renderer
     // and bone-path metadata while the candidate plan is still being built.
     inventory.metadata.roots.push_back(std::move(root));
    }
    const auto& parsed=afterGraph?afterGraph->layout:layout;
    std::set<std::string> materialMetadataSections;
    for(const auto& asset:nativeAssets)if(asset.classId==21){
     auto copies=materialCopies.find(Identity{asset.path,asset.asset});if(copies==materialCopies.end())continue;
     for(const auto& section:copies->second){
      // A CAB may contain several native Material objects that reference the
      // same logical source asset. Register that author section once per CAB;
      // its texture references are identical and duplicate rows are invalid.
      if(!materialMetadataSections.insert(section).second)continue;
      auto nativeId=AllocateMaterialId(member.name,package,section);
      const auto& type=parsed.types.at(parsed.objects.at(nativeId).type);auto value=ObjectValue(member.bytes,parsed,nativeId);Material material;material.name=Name(type,value);
      std::vector<std::pair<std::string,NativeReference>> refs;
      for(const auto& property:ArrayValue(type,Child(type,Child(type,value,"m_SavedProperties"),"m_TexEnvs")).children){
       const auto& name=Child(type,property,"first");refs.push_back({Text(name.raw.data(),name.raw.size()),Reference(type,Child(type,Child(type,property,"second"),"m_Texture"),parsed,member.name)});
      }
      auto inserted=inventory.metadata.materials.emplace(section,std::move(material));
      if(!inserted.second)
       throw std::runtime_error("duplicate authored Material metadata: section="+section+
        " member="+member.name+" nativeId="+std::to_string(nativeId));
      inventory.metadata.refs.push_back({section,std::move(refs)});
     }
    }
    for(const auto& row:parsed.objects)if(parsed.types.at(row.second.type).classId==28){
     const auto& type=parsed.types.at(row.second.type);auto value=ObjectValue(member.bytes,parsed,row.first);
     inventory.metadata.textures.emplace(NativeReference{member.name,row.first},Texture{Name(type,value),int(Integer(Child(type,value,"m_Width"))),int(Integer(Child(type,value,"m_Height"))),int(Integer(Child(type,value,"m_MipCount")))});
    }
   }
    return 0;
   });
  },4);
  if(cost)cost("appearance-wall",std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-phaseStart).count());
  for(size_t i=0;i<sources.size();++i){
   if(appearanceHits[i])++plan.appearanceHits;else ++plan.appearanceMisses;
   plan.textures+=appearances[i].first;plan.materialMatches+=appearances[i].second;
   auto& inventory=inventories[i];
   for(const auto& member:inventory.members)Require(sourceMembers.insert(member).second,"reload duplicate CAB/resource member");
   plan.names.members.merge(inventory.names.members);
   for(auto& bundle:inventory.names.bundles)Require(plan.names.bundles.emplace(bundle).second,"reload duplicate Bundle identity");
   auto& metadata=inventory.metadata;
   for(auto& root:metadata.roots){
    for(auto& source:root.assembly.sources)for(auto& target:source.targets)if(!target.bones.empty()){
     auto hash=bonePaletteHash(target.bones);auto& bucket=bonePaletteBuckets[hash];size_t palette=SIZE_MAX;
     for(auto candidate:bucket)if(*bonePalettes[candidate]==target.bones){palette=candidate;break;}
     if(palette==SIZE_MAX){palette=bonePalettes.size();bonePalettes.push_back(std::make_shared<const std::vector<std::string>>(target.bones));bucket.push_back(palette);}
     target.preparedBones=bonePalettes[palette];
     std::vector<std::string>().swap(target.bones);
    }
    plan.roots.push_back(std::move(root));
   }
   memberInput.merge(metadata.members);textures.merge(metadata.textures);materials.merge(metadata.materials);
   for(auto& ref:metadata.refs)textureRefs.push_back(std::move(ref));
   plan.inputs.push_back(std::move(inventory.input));rebuilt.push_back(std::move(appearances[i].bytes));if(progress)progress(i+1,sources.size());
   if(stage)stage("resource",i+1,sources.size());
  }
  if(stage){stage("metadata",0,sources.size());stage("metadata",sources.size(),sources.size());}
 if(geometryHit)marked=markers.size();
 else if(!geometryCache.empty()){geometry.assets=assets;auto temporary=geometryCache/(geometryKey+"-building-"+std::to_string(GetCurrentProcessId()));WriteGeometry(temporary,geometryKey,sources,geometry);std::filesystem::rename(temporary,geometryDirectory);}
 plan.rendererBoundsHits=boundsCache.hits;plan.rendererBoundsMisses=boundsCache.misses;if(cost)cost("renderer-bounds",boundsCache.computeMs);
 Require(marked==markers.size()&&plan.materialMatches==package.materials.size()&&plan.textures==package.textures.size(),"reload rebuilt author coverage differs: markers="+std::to_string(marked)+"/"+std::to_string(markers.size())+" materials="+std::to_string(plan.materialMatches)+"/"+std::to_string(package.materials.size())+" textures="+std::to_string(plan.textures)+"/"+std::to_string(package.textures.size()));
 for(const auto& m:sourceMembers)if(!plan.names.members.count(m)){
  auto cab=m.substr(0,36);Require(m==cab+".resS"&&plan.names.members.count(cab),"reload unknown stream grammar");plan.names.members.emplace(m,plan.names.members.at(cab)+".resS");
 }
 Require(plan.roots.size()==rootPaths.size(),"reload prefab roots absent or ambiguous");
 for(auto& root:plan.roots){
  // Assembly validation has already covered every native/injected renderer
  // exactly once. Count its proven targets instead of decoding the same root
  // and reconstructing a third hierarchy graph solely to count markers.
  for(const auto& source:root.assembly.sources)for(const auto& target:source.targets)
   if(!target.marker.empty())++root.markers[target.marker];
   Require(package.composed||!root.assembly.sources.empty(),"reload root has no source Renderer assembly");
 }
 for(const auto& refs:textureRefs){auto& mat=materials.at(std::get<0>(refs));for(const auto& p:std::get<1>(refs)){
  TextureSlot slot;slot.property=p.first;slot.nonnull=p.second.second!=0;auto found=textures.find(p.second);if(found!=textures.end()){slot.local=true;slot.texture=found->second;}mat.slots.push_back(std::move(slot));
 }}
 for(const auto& m:package.materials)plan.materials.emplace(m.first,materials.at(m.first));
 std::vector<unsigned char> retained(sources.size(),1);
 if(rulesOnly&&describeInput){
  std::vector<std::vector<size_t>> dependencies(sources.size());std::vector<unsigned char> changed(sources.size());std::vector<size_t> roots;
  for(size_t i=0;i<sources.size();++i){changed[i]=Hash(rebuilt[i])!=sources[i].hash;for(const auto& external:plan.inputs[i].externals){auto owner=memberInput.find(external);if(owner!=memberInput.end()&&owner->second!=i)dependencies[i].push_back(owner->second);}}
  for(const auto& root:plan.roots)roots.push_back(root.input);
  retained=EiemInputRetention::Select(dependencies,changed,roots);
  for(size_t i=0;i<retained.size();++i)if(retained[i])plan.retainedInputs.push_back(i);
  for(auto it=plan.names.members.begin();it!=plan.names.members.end();)if(!retained[memberInput.at(it->first)]){auto owner=memberInput.at(it->first);Require(!plan.inputs[owner].bundle.empty(),"shared native dependency lacks Bundle identity");plan.sharedBundles.emplace(it->first,plan.inputs[owner].bundle);plan.shared.insert(it->first);it=plan.names.members.erase(it);}else ++it;
  for(size_t i=0;i<retained.size();++i)if(!retained[i]&&!plan.inputs[i].bundle.empty())plan.names.bundles.erase(plan.inputs[i].bundle);
  if(cost){cost("inputs-retained",double(plan.retainedInputs.size()));cost("inputs-shared-original",double(sources.size()-plan.retainedInputs.size()));}
 }
 std::set<size_t> closure;std::vector<size_t> queue;
 for(const auto& r:plan.roots)queue.push_back(r.input);
 while(!queue.empty()){auto i=queue.back();queue.pop_back();if(!closure.insert(i).second)continue;
  for(const auto& e:plan.inputs[i].externals){auto f=memberInput.find(e);if(f==memberInput.end()||!retained[f->second]){if(e!="unity default resources")plan.shared.insert(e);}else queue.push_back(f->second);}
 }
  plan.closure.assign(closure.begin(),closure.end());Require(!plan.closure.empty(),"reload root closure empty");
  std::filesystem::create_directories(destination);
  if(!rulesOnly)std::filesystem::create_directories(destination/"startup");
  if(rulesOnly){plan.rendererRules.resize(sources.size());plan.startupHashes.resize(sources.size());plan.namespaceRecipes.resize(sources.size());std::map<NativeReference,Identity> targets;
   for(const auto& target:package.targetIndex){const auto& asset=assets.at(target.first);targets.emplace(NativeReference{asset.cab,asset.pathId},target.first);}
   std::set<NativeReference> references;for(const auto& row:targets)references.insert(row.first);
   EiemOfflineCompile::ParallelInputs(sources.size(),[&](size_t i){auto local=EndfieldInput::Decode(geometryHit?ReadSource(baseline,sources[i]):pristineInputs[i]);for(const auto& member:local.members)if(Serialized(member.bytes)){auto l=ParseSerialized(member.bytes);
    for(const auto& consumer:MeshConsumers(member.bytes,l,member.name,references))plan.rendererRules[i].push_back({member.name,consumer.renderer,targets.at(consumer.mesh)});
   }});
  }
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
  std::map<std::string,Bytes> frameBytes;std::unique_ptr<EiemResourceRecipe::Dictionary> authorSpans;
  if(rulesOnly){plan.spanRecipes=true;plan.frameRecipes=EiemResourceRecipe::DescribeFrames(package,schemas);plan.memberRecipes.resize(sources.size());std::vector<EiemResourceRecipe::Buffer> buffers;
   for(const auto& resource:package.resources)if(resource.second->kind!=10)buffers.push_back({EiemResourceRecipe::Author,resource.first,&resource.second->bytes});
   for(const auto& frame:plan.frameRecipes){auto inserted=frameBytes.emplace(frame.resource,EiemResourceRecipe::ExpandFrame(frame,package.resources.at(frame.resource)->bytes));Require(inserted.second,"duplicate recipe frame source");buffers.push_back({EiemResourceRecipe::ExpandedFrame,frame.resource,&inserted.first->second});}
   authorSpans=std::make_unique<EiemResourceRecipe::Dictionary>(std::move(buffers));
  }
  auto publishOne=[&](size_t i){
   if(!retained[i]){plan.startupHashes[i]=publishedHashes[i]=sources[i].hash;inventories[i].metadata.analysis={};rebuilt[i].clear();if(inMemoryBuild)editedBundles[i]={};return;}
   // Same authored payload in the original CAB namespace for the game's first
   // ordinary resource load; the isolated variant below is used by F10.
   if(rulesOnly)plan.startupHashes[i]=Hash(rebuilt[i]);else Measure(workerCost,"startup-write",[&]{return SaveFromCache(destination/"startup"/sources[i].file,appearanceFiles[i],rebuilt[i]);});
   auto source=std::move(rebuilt[i]);
   auto bundle=inMemoryBuild?std::move(editedBundles[i]):Measure(workerCost,"publish-source-decode",[&]{return EndfieldInput::Decode(source);});
   if(rulesOnly)Measure(workerCost,"span-recipe",[&]{auto raw=geometryHit?ReadSource(baseline,sources[i]):pristineInputs[i];auto local=EndfieldInput::Decode(raw);const auto& original=local;std::vector<EiemResourceRecipe::Buffer> buffers;std::map<std::string,const Bytes*> members;
    for(const auto& member:original.members){buffers.push_back({EiemResourceRecipe::Original,member.name,&member.bytes});Require(members.emplace(member.name,&member.bytes).second,"duplicate recipe original member");}EiemResourceRecipe::Dictionary spans(std::move(buffers));
    if(describeInput)Measure(workerCost,"compiled-proof",[&]{describeInput(i,plan,raw,original,bundle,spans,*authorSpans,inventories[i].metadata.analysis);return 0;});
    else for(const auto& member:bundle.members)plan.memberRecipes[i].push_back(EiemResourceRecipe::Describe(member,spans,*authorSpans));
    Require(plan.memberRecipes[i].size()==bundle.members.size(),"offline member recipe coverage differs");
    std::map<std::string,const Bytes*> generated;for(const auto& member:bundle.members)for(const auto& origin:member.origins)if(origin.resource.rfind("@bounds:",0)==0)generated.emplace(origin.resource,origin.bytes);
    for(size_t n=0;n<bundle.members.size();++n)Require(EiemResourceRecipe::Apply(plan.memberRecipes[i][n],members,package,frameBytes,true,&generated)==bundle.members[n].bytes,"offline member recipe output differs");return 0;
   });
   size_t opaque=0;
   for(auto& m:bundle.members){
    if(Serialized(m.bytes)){
     bool reused=false;EiemLoadingGeneration::Recipe proof;
     auto shared=inventories[i].metadata.analysis.layouts.find(m.name);const auto* layout=shared==inventories[i].metadata.analysis.layouts.end()?nullptr:&shared->second.second;
     auto v=Measure(workerCost,"namespace",[&]{return rulesOnly?EiemLoadingGeneration::PrepareSerialized(m.bytes,plan.names,&proof,layout):EiemLoadingGeneration::PrepareSerializedCached(m.bytes,plan.names,namespaceCache,reused);});
     if(rulesOnly)plan.namespaceRecipes[i].push_back({m.name,Hash(m.bytes),std::move(proof.patches)});
     if(reused)++namespaceHits[i];else ++namespaceMisses[i];
     m.bytes=std::move(v.bytes);opaque+=v.opaqueObjectsPreserved;
    }
    m.name=plan.names.members.at(m.name);
   }
   auto bytes=Measure(workerCost,"encode",[&]{return EndfieldInput::Encode(source,bundle.members);});
   auto check=Measure(workerCost,"verify-decode",[&]{return EndfieldInput::Decode(bytes);});
   Require(check.members.size()==bundle.members.size(),"reload encoded graph coverage differs");
   for(size_t j=0;j<bundle.members.size();++j)Require(check.members[j].name==bundle.members[j].name&&check.members[j].flags==bundle.members[j].flags&&check.members[j].bytes==bundle.members[j].bytes,"reload encoded graph payload differs");
   publishedHashes[i]=Measure(workerCost,"publish-hash",[&]{return Hash(bytes);});publishedOpaque[i]=opaque;
   inventories[i].metadata.analysis={};
   if(!rulesOnly)Measure(workerCost,"isolated-write",[&]{Save(plan.inputs[i].path,bytes);return 0;});
  };
  auto hardware=std::thread::hardware_concurrency();
  auto workers=std::min<size_t>(rebuilt.size(),std::max<size_t>(1,std::min<unsigned>(4,hardware?hardware:1)));
  std::vector<std::thread> pool;pool.reserve(workers);
  for(size_t worker=0;worker<workers;++worker)pool.emplace_back([&]{
   for(;;){
    auto i=nextPublish.fetch_add(1);if(i>=rebuilt.size())return;
    try{publishOne(i);}catch(...){std::lock_guard<std::mutex> lock(publishErrorMutex);if(!publishError)publishError=std::current_exception();}
   }
  });
  for(auto& worker:pool)worker.join();
  if(publishError)std::rethrow_exception(publishError);
  // Shared proofs refer to the SHA-verified snapshot read at the start. Only
  // reread changed files; updates outside the selected range remain valid.
  if(!geometryHit)for(size_t i=0;i<sources.size();++i)VerifySourceSnapshot(baseline,sources[i],sourceStamps[i]);
  for(size_t i=0;i<rebuilt.size();++i){plan.inputs[i].hash=std::move(publishedHashes[i]);plan.opaque+=publishedOpaque[i];plan.namespaceHits+=namespaceHits[i];plan.namespaceMisses+=namespaceMisses[i];}
  if(stage)stage("publish",rebuilt.size(),rebuilt.size());
 // Ready is the last write. An interrupted worker cannot publish a partial graph.
 std::ostringstream report;report<<"EFF_RESOURCE_RELOAD_GENERATION\t1\t"<<plan.key<<"\t"<<plan.author<<"\t"<<plan.contract<<'\n';
 for(const auto& in:plan.inputs)report<<"input\t"<<in.file<<'\t'<<in.hash<<'\n';
 auto text=report.str();Save(destination/"ready.tsv",Bytes(text.begin(),text.end()));return plan;
}
} // namespace EiemResourceReloadPlan
