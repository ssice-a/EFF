#pragma once
#include "eiem_compiled_mod.h"
#include "eiem_static_resource_sources.h"
#include <optional>

// Author-side only. This header is not included by the game DLL.
namespace EiemCompiledModWriter {
using namespace EiemCompiledMod;
inline void StreamSites(const Type& type,size_t node,EiemNativeInput::Reader& reader,size_t base,int64_t id,std::vector<StreamSite>& out,const std::vector<bool>& containsStream){
 const auto& schema=type.nodes.at(node);
 if(!containsStream.at(node)){SkipValue(type,node,reader,base);return;}
 if(schema.type=="StreamingInfo"||schema.type=="StreamedResource"){
  auto start=reader.p;auto value=ReadValue(type,node,reader,base);auto path=EiemLoadingGraphAudit::Find(type,value,"path");if(!path)path=EiemLoadingGraphAudit::Find(type,value,"source");
  auto offset=EiemLoadingGraphAudit::Find(type,value,"offset"),size=EiemLoadingGraphAudit::Find(type,value,"size");if(!path||!offset||!size||!Integer(*size))return;
  Bytes prefix(start-base);for(const auto& child:value.children){if(child.node==offset->node){out.push_back({id,prefix.size(),offset->raw.size(),EiemLoadingGraphAudit::Basename(Text(path->raw.data(),path->raw.size())),Integer(*offset)});break;}WriteValue(type,child,prefix);}return;
 }
 auto primitive=Primitive(schema.type);if(primitive||schema.type=="string"||schema.type=="TypelessData"){SkipValue(type,node,reader,base);return;}
 if(schema.type=="Array"){auto count=reader.Count();auto element=schema.children.at(1);for(size_t i=0;i<count;++i)StreamSites(type,element,reader,base,id,out,containsStream);}
 else for(auto child:schema.children)StreamSites(type,child,reader,base,id,out,containsStream);
 if(schema.align)reader.Skip((size_t(0)-(reader.p-base))&3);
}
inline Artifact DescribeInput(size_t i,const Plan& plan,bool rootAssembly,const EndfieldInput::Bundle& source,const EndfieldInput::Bundle& edited,std::function<void(const char*,double)> cost={}){
 Artifact artifact;
  for(const auto& member:edited.members)if(Serialized(member.bytes)){
   auto old=std::find_if(source.members.begin(),source.members.end(),[&](const auto& x){return x.name==member.name;});Require(old!=source.members.end(),"compiled serialized member lacks original");MemberProof proof;Measure(cost,"proof-layout",[&]{proof.before=ParseSerialized(old->bytes);proof.after=ParseSerialized(member.bytes);return 0;});
   EiemLoadingGeneration::Recipe names;Measure(cost,"proof-namespace",[&]{return EiemLoadingGeneration::PrepareSerialized(old->bytes,plan.names,&names);});proof.pristineNamespace=std::move(names.patches);
   std::vector<std::vector<bool>> streamNodes;for(const auto& type:proof.after.types){std::vector<bool> nodes(type.nodes.size());for(size_t n=nodes.size();n-->0;){nodes[n]=type.nodes[n].type=="StreamingInfo"||type.nodes[n].type=="StreamedResource";for(auto child:type.nodes[n].children)nodes[n]=nodes[n]||nodes[child];}streamNodes.push_back(std::move(nodes));}
   Measure(cost,"proof-streams",[&]{for(auto id:proof.after.order){const auto& object=proof.after.objects.at(id);auto before=proof.before.objects.find(id);bool changed=before==proof.before.objects.end()||before->second.size!=object.size||!std::equal(member.bytes.begin()+object.offset,member.bytes.begin()+object.offset+object.size,old->bytes.begin()+before->second.offset);
    if(changed){EiemNativeInput::Reader reader(member.bytes,object.offset,false,object.offset+object.size);StreamSites(proof.after.types.at(object.type),0,reader,object.offset,id,proof.streams,streamNodes.at(object.type));Require(reader.p==object.offset+object.size,"compiled stream proof object extent differs");}
   }return 0;});
   Require(artifact.members.emplace(member.name,std::move(proof)).second,"compiled duplicate serialized member");
   for(auto root:plan.roots)if(root.input==i){
    auto assets=Measure(cost,"proof-assets",[&]{return SerializedAssets(old->bytes,member.name,artifact.members.at(member.name).before);});auto asset=std::find_if(assets.begin(),assets.end(),[&](const auto& x){return x.classId==1&&x.path==root.path;});if(asset==assets.end())continue;
    root.cab=member.name;root.source=asset->pathId;Root pristine=root;pristine.markers.clear();
    if(rootAssembly)pristine.assembly=Measure(cost,"proof-root-assembly",[&]{return EiemRenderAssemblyDescription::Build(old->bytes,old->bytes,member.name,root.source,Package{},AssetIndex{},{});});artifact.pristine.push_back(std::move(pristine));
   }
  }
 return artifact;
}
inline void MergeProof(Artifact& artifact,Artifact proof){
 for(auto& member:proof.members)Require(artifact.members.emplace(member.first,std::move(member.second)).second,"compiled duplicate serialized member");
 artifact.pristine.insert(artifact.pristine.end(),std::make_move_iterator(proof.pristine.begin()),std::make_move_iterator(proof.pristine.end()));
}
inline Artifact Describe(Manifest manifest){
 Artifact artifact;artifact.manifest=std::move(manifest);auto& plan=artifact.manifest.plan;Require(plan.rulesOnly&&plan.spanRecipes&&!artifact.manifest.composed,"author compiler must emit independent addressed recipes");
 EiemPrefabRules::Context replay(std::make_shared<const Plan>(plan),artifact.manifest.package);
 for(size_t i=0;i<plan.originals.size();++i){auto source=EndfieldInput::Decode(ReadSource({},plan.originals[i]));auto edited=EndfieldInput::Decode(replay.Materialize(i,false));MergeProof(artifact,DescribeInput(i,plan,artifact.manifest.rootAssembly,source,edited));}
 Require(artifact.pristine.size()==plan.roots.size(),"compiled pristine roots missing");return artifact;
}
inline void Export(const std::filesystem::path& mod,const std::filesystem::path& game,std::function<void(const char*,double)> cost={}){
 auto author=Measure(cost,"author-read",[&]{return Load(mod);});auto selected=Measure(cost,"source-selection",[&]{return EiemStaticSources::Merge({author},game);});
 if(std::filesystem::is_regular_file(mod/"compiled.bin")){
  std::optional<Artifact> cached;try{cached=Measure(cost,"compiled-read",[&]{return EiemCompiledMod::Read(mod,game);});}catch(const std::exception&){}
  if(cached&&cached->manifest.rootAssembly&&cached->manifest.plan.contract==InputContractDigest(author)){
   try{for(const auto& source:cached->manifest.plan.originals)ReadSource({},source);}catch(const std::exception&){cached.reset();}
   if(cached){cached->manifest.package=author;cached->manifest.plan.author=PackageDigest(author);
    auto bytes=EiemCompiledMod::Encode(*cached,std::filesystem::weakly_canonical(game));if(bytes!=EiemNativeInput::Read(mod/"compiled.bin"))EiemOfflineCompile::AtomicSave(mod/"compiled.bin",bytes);return;}
  }
 }
 auto tempRoot=std::filesystem::weakly_canonical(std::filesystem::temp_directory_path());auto work=tempRoot/("eff-compiled-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));Require(std::filesystem::create_directory(work),"author scratch directory already exists");
 struct Cleanup{std::filesystem::path root,work;~Cleanup(){std::error_code error;if(work.parent_path()==root)std::filesystem::remove_all(work,error);}}cleanup{tempRoot,work};
 auto baseline=work/"baseline";Measure(cost,"source-prepare",[&]{EiemStaticSources::Ensure(selected,baseline,work/"sources",author,game);return 0;});
 std::vector<std::string> rootPaths;auto sourceCount=ReadBaseline(baseline,rootPaths,game).size();std::vector<Artifact> proofs(sourceCount);
 auto plan=Measure(cost,"resource-build",[&]{return Build(author,baseline,work/"candidate",std::string(structure),{},{},cost,{},true,game,[&](size_t i,const Plan& current,const EndfieldInput::Bundle& original,const EndfieldInput::Bundle& edited){proofs.at(i)=DescribeInput(i,current,true,original,edited,cost);});});Manifest manifest{std::move(plan),author,Hash(EiemNativeInput::Read(baseline/"baseline.tsv"))};
 Artifact artifact;artifact.manifest=std::move(manifest);for(auto& proof:proofs)MergeProof(artifact,std::move(proof));Require(artifact.pristine.size()==artifact.manifest.plan.roots.size(),"compiled pristine roots missing");
 for(auto& root:artifact.manifest.plan.roots){auto pristine=std::find_if(artifact.pristine.begin(),artifact.pristine.end(),[&](const auto& row){return row.path==root.path;});Require(pristine!=artifact.pristine.end(),"compiled root proof absent");root.cab=pristine->cab;root.source=pristine->source;}
 auto bytes=Measure(cost,"compiled-encode",[&]{return EiemCompiledMod::Encode(artifact,std::filesystem::weakly_canonical(game));});auto decoded=Measure(cost,"compiled-roundtrip",[&]{return EiemCompiledMod::Decode(bytes,mod,game);});Require(decoded.manifest.plan.author==PackageDigest(author),"compiled author roundtrip differs");
 Measure(cost,"compiled-publish",[&]{EiemOfflineCompile::AtomicSave(mod/"compiled.bin",bytes);return 0;});
}
} // namespace EiemCompiledModWriter
