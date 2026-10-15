#pragma once
#include "eiem_compiled_mod.h"
#include "eiem_compiled_input_view_writer.h"
#include "eiem_static_resource_sources.h"
#include "eiem_compiled_export_carryover.h"
#include <optional>

// CPU-only compilation. Export and background source adaptation can share
// this implementation; the current production DLL does not include it.
namespace EiemCompiledModWriter {
using namespace EiemCompiledMod;
inline bool NamespaceSubset(const EiemLoadingGeneration::Names& current,const EiemLoadingGeneration::Names& previous){
 auto covered=[](const auto& next,const auto& old){return std::all_of(next.begin(),next.end(),[&](const auto& value){if(value.first==value.second)return true;auto found=old.find(value.first);return found!=old.end()&&found->first!=found->second;});};
 return covered(current.members,previous.members)&&covered(current.bundles,previous.bundles);
}
inline std::vector<EiemLoadingGeneration::Patch> SelectPristineNamespace(const std::vector<EiemLoadingGeneration::Patch>& previous,const EiemLoadingGeneration::Names& names){
 std::vector<EiemLoadingGeneration::Patch> result;
 for(const auto& patch:previous){std::string after;if(patch.before.rfind("archive:/",0)==0)after=EiemLoadingGeneration::RemapPath(patch.before,names);else{auto found=names.bundles.find(patch.before);after=found==names.bundles.end()?patch.before:found->second;}if(after!=patch.before)result.push_back({patch.offset,patch.before,std::move(after)});}
 return result;
}
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
using InputAnalysis=EiemResourceReloadPlan::InputAnalysis;
inline Artifact DescribeInput(size_t i,const Plan& plan,bool rootAssembly,const EndfieldInput::Bundle& source,const EndfieldInput::Bundle& edited,std::function<void(const char*,double)> cost={},const Artifact* previous=nullptr,const InputAnalysis* analysis=nullptr){
 Artifact artifact;
 size_t priorInput=SIZE_MAX;
 if(previous&&i<plan.originals.size())for(size_t n=0;n<previous->manifest.plan.originals.size();++n){const auto& old=previous->manifest.plan.originals[n];const auto& current=plan.originals[i];if(old.file==current.file&&old.hash==current.hash){priorInput=n;break;}}
  for(const auto& member:edited.members)if(Serialized(member.bytes)){
   auto old=std::find_if(source.members.begin(),source.members.end(),[&](const auto& x){return x.name==member.name;});Require(old!=source.members.end(),"compiled serialized member lacks original");
   const MemberProof* prior=nullptr;if(priorInput!=SIZE_MAX){auto found=previous->members.find(member.name);if(found!=previous->members.end())prior=&found->second;}
   const std::pair<std::shared_ptr<const InputAnalysis::GraphData>,std::shared_ptr<const InputAnalysis::GraphData>>* shared=nullptr;
   if(analysis){auto analyzed=analysis->graphs.find(member.name);if(analyzed!=analysis->graphs.end())shared=&analyzed->second;}
   const std::pair<SerializedLayout,SerializedLayout>* layouts=nullptr;
   if(analysis){auto analyzed=analysis->layouts.find(member.name);if(analyzed!=analysis->layouts.end())layouts=&analyzed->second;}
   MemberProof proof;Measure(cost,"proof-layout",[&]{proof.before=layouts?layouts->first:(shared?shared->first->layout:(prior?prior->before:ParseSerialized(old->bytes)));proof.after=layouts?layouts->second:(shared?shared->second->layout:ParseSerialized(member.bytes));return 0;});
   if(prior&&NamespaceSubset(plan.names,previous->manifest.plan.names))proof.pristineNamespace=SelectPristineNamespace(prior->pristineNamespace,plan.names);
   else{EiemLoadingGeneration::Recipe names;Measure(cost,"proof-namespace",[&]{return EiemLoadingGeneration::PrepareSerialized(old->bytes,plan.names,&names,&proof.before);});proof.pristineNamespace=std::move(names.patches);}
   std::vector<std::vector<bool>> streamNodes;for(const auto& type:proof.after.types){std::vector<bool> nodes(type.nodes.size());for(size_t n=nodes.size();n-->0;){nodes[n]=type.nodes[n].type=="StreamingInfo"||type.nodes[n].type=="StreamedResource";for(auto child:type.nodes[n].children)nodes[n]=nodes[n]||nodes[child];}streamNodes.push_back(std::move(nodes));}
   Measure(cost,"proof-streams",[&]{for(auto id:proof.after.order){const auto& object=proof.after.objects.at(id);auto before=proof.before.objects.find(id);bool changed=before==proof.before.objects.end()||before->second.size!=object.size||!std::equal(member.bytes.begin()+object.offset,member.bytes.begin()+object.offset+object.size,old->bytes.begin()+before->second.offset);
    if(changed){EiemNativeInput::Reader reader(member.bytes,object.offset,false,object.offset+object.size);StreamSites(proof.after.types.at(object.type),0,reader,object.offset,id,proof.streams,streamNodes.at(object.type));Require(reader.p==object.offset+object.size,"compiled stream proof object extent differs");}
   }return 0;});
   Require(artifact.members.emplace(member.name,std::move(proof)).second,"compiled duplicate serialized member");
   if(analysis)continue;
   // Assemblies carry full renderer/bone palettes. Unrelated roots are only
   // inspected; a pristine assembly must not copy a candidate it will replace.
   std::optional<std::vector<Asset>> rootAssets;std::shared_ptr<const EiemRenderAssemblyDescription::GraphData> pristineGraph;
   for(const auto& root:plan.roots)if(root.input==i){
    if(priorInput!=SIZE_MAX&&(!rootAssembly||previous->manifest.rootAssembly)){auto cached=std::find_if(previous->pristine.begin(),previous->pristine.end(),[&](const auto& value){return value.input==priorInput&&value.path==root.path&&value.cab==member.name;});if(cached!=previous->pristine.end()){auto pristine=*cached;pristine.input=i;artifact.pristine.push_back(std::move(pristine));continue;}}
    if(!rootAssets)rootAssets=Measure(cost,"proof-assets",[&]{return SerializedAssets(old->bytes,member.name,artifact.members.at(member.name).before);});auto asset=std::find_if(rootAssets->begin(),rootAssets->end(),[&](const auto& x){return x.classId==1&&x.path==root.path;});if(asset==rootAssets->end())continue;
    Root pristine;if(!rootAssembly)pristine=root;
    pristine.path=root.path;pristine.input=root.input;pristine.cab=member.name;pristine.source=asset->pathId;pristine.markers.clear();
    if(rootAssembly)pristine.assembly=Measure(cost,"proof-root-assembly",[&]{if(!pristineGraph)pristineGraph=std::make_shared<const EiemRenderAssemblyDescription::GraphData>(old->bytes,member.name);EiemRenderAssemblyDescription::Graph graph(pristineGraph,pristine.source);return EiemRenderAssemblyDescription::Build(graph,graph,member.name,Package{},AssetIndex{},{});});artifact.pristine.push_back(std::move(pristine));
   }
  }
 if(analysis){Require(rootAssembly,"shared input analysis requires full root assemblies");std::set<std::string> roots;
  for(const auto& root:plan.roots)if(root.input==i)roots.insert(root.path);
  for(const auto& root:analysis->pristine){Require(root.input==i&&artifact.members.count(root.cab)&&roots.erase(root.path),"shared pristine assembly outside input roots");artifact.pristine.push_back(root);}
  Require(roots.empty(),"shared pristine assembly coverage incomplete");}
 return artifact;
}
inline void MergeProof(Artifact& artifact,Artifact proof){
 for(auto& member:proof.members)Require(artifact.members.emplace(member.first,std::move(member.second)).second,"compiled duplicate serialized member");
 artifact.pristine.insert(artifact.pristine.end(),std::make_move_iterator(proof.pristine.begin()),std::make_move_iterator(proof.pristine.end()));
}
inline std::vector<EiemResourceRecipe::Member> DescribeMembers(const EndfieldInput::Bundle& edited,
 const std::map<std::string,MemberProof>& proofs,const EiemResourceRecipe::Dictionary& original,
 const EiemResourceRecipe::Dictionary& author){
 std::map<std::string,size_t> buffers;for(size_t i=0;i<original.buffers.size();++i)buffers.emplace(original.buffers[i].name,i);
 std::vector<EiemResourceRecipe::Member> result;
 for(const auto& member:edited.members){
  std::vector<EiemResourceRecipe::OriginalSpan> spans;auto source=buffers.find(member.name);
  if(source!=buffers.end()){
   const auto& before=*original.buffers[source->second].bytes;
   if(!before.empty()&&before==member.bytes)spans.push_back({0,source->second,0,before.size()});
   else if(Serialized(member.bytes)){
    const auto& proof=proofs.at(member.name);
    for(auto id:proof.after.order){const auto& object=proof.after.objects.at(id);auto old=proof.before.objects.find(id);
     if(object.size&&old!=proof.before.objects.end()&&old->second.size==object.size&&std::equal(member.bytes.begin()+object.offset,member.bytes.begin()+object.offset+object.size,before.begin()+old->second.offset))
      spans.push_back({object.offset,source->second,old->second.offset,object.size});
    }
    std::sort(spans.begin(),spans.end(),[](const auto& a,const auto& b){return a.output<b.output;});
   }else if(!before.empty()&&before.size()<=member.bytes.size()&&std::equal(before.begin(),before.end(),member.bytes.begin()))spans.push_back({0,source->second,0,before.size()});
  }
  result.push_back(EiemResourceRecipe::Describe(member,original,author,spans));
 }
 return result;
}
inline Artifact Describe(Manifest manifest){
 Artifact artifact;artifact.manifest=std::move(manifest);auto& plan=artifact.manifest.plan;Require(plan.rulesOnly&&plan.spanRecipes&&!artifact.manifest.composed,"author compiler must emit independent addressed recipes");
 EiemPrefabRules::Context replay(std::make_shared<const Plan>(plan),artifact.manifest.package);std::vector<EiemResourceRecipe::Buffer> buffers;
 for(const auto& resource:artifact.manifest.package.resources)buffers.push_back({EiemResourceRecipe::Author,resource.first,&resource.second->bytes});for(const auto& frame:replay.frameBytes)buffers.push_back({EiemResourceRecipe::ExpandedFrame,frame.first,&frame.second});EiemResourceRecipe::Dictionary dictionary(std::move(buffers));
 artifact.views.resize(plan.originals.size());artifact.preparedFrames=replay.frameBytes;
 for(size_t i=0;i<plan.originals.size();++i){auto input=ReadSource({},plan.originals[i]);auto source=EndfieldInput::Decode(input);auto edited=EndfieldInput::Decode(replay.Materialize(i,false));MergeProof(artifact,DescribeInput(i,plan,artifact.manifest.rootAssembly,source,edited));artifact.views[i]=EiemCompiledInputViewWriter::Build(i,artifact,input,source,edited,dictionary);}
 Require(artifact.pristine.size()==plan.roots.size(),"compiled pristine roots missing");return artifact;
}
} // namespace EiemCompiledModWriter
#include "eiem_incremental_material_compile.h"
namespace EiemCompiledModWriter {
inline Artifact Compile(const Package& author,const EiemStaticSources::Selection& selected,const std::filesystem::path& game,
 std::function<void(const char*,double)> cost={},const Artifact* previousArtifact=nullptr,const EiemStaticSources::Discovery* preparedDiscovery=nullptr){
 static std::atomic<uint64_t> serial{0};auto tempRoot=std::filesystem::weakly_canonical(std::filesystem::temp_directory_path());auto work=tempRoot/("eff-compiled-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64())+"-"+std::to_string(serial.fetch_add(1)));Require(std::filesystem::create_directory(work),"author scratch directory already exists");
 struct Cleanup{std::filesystem::path root,work;~Cleanup(){std::error_code error;if(work.parent_path()==root)std::filesystem::remove_all(work,error);}}cleanup{tempRoot,work};
 auto baseline=work/"baseline";Measure(cost,"source-prepare",[&]{EiemStaticSources::Ensure(selected,baseline,work/"sources",author,game,preparedDiscovery);return 0;});
 std::vector<std::string> rootPaths;auto sourceCount=ReadBaseline(baseline,rootPaths,game).size();std::vector<Artifact> proofs(sourceCount);
 std::vector<EiemCompiledInputView::Recipe> views(sourceCount);std::map<std::string,Bytes> preparedFrames;std::once_flag framesReady;
 auto plan=Measure(cost,"resource-build",[&]{return Build(author,baseline,work/"candidate",std::string(structure),{},{},cost,{},true,game,[&](size_t i,Plan& current,const Bytes& input,const EndfieldInput::Bundle& original,const EndfieldInput::Bundle& edited,const EiemResourceRecipe::Dictionary& originalSpans,const EiemResourceRecipe::Dictionary& authorSpans,const InputAnalysis& analysis){
  proofs.at(i)=DescribeInput(i,current,true,original,edited,cost,previousArtifact,&analysis);
  current.memberRecipes.at(i)=Measure(cost,"addressed-recipe",[&]{return DescribeMembers(edited,proofs.at(i).members,originalSpans,authorSpans);});
  // Recipes and native views address the same immutable author buffers.
  // Share their dictionary instead of indexing all author pixels/geometry a
  // second time while the other input workers wait for that duplicate work.
  std::call_once(framesReady,[&]{for(const auto& buffer:authorSpans.buffers)if(buffer.kind==EiemResourceRecipe::ExpandedFrame)Require(preparedFrames.emplace(buffer.name,*buffer.bytes).second,"duplicate offline view frame");});
  views.at(i)=Measure(cost,"compiled-native-view",[&]{return EiemCompiledInputViewWriter::Build(i,current,proofs.at(i).members,input,original,edited,authorSpans);});
 });});Manifest manifest{std::move(plan),author,Hash(EiemNativeInput::Read(baseline/"baseline.tsv"))};
 Artifact artifact;artifact.manifest=std::move(manifest);for(auto& proof:proofs)MergeProof(artifact,std::move(proof));Require(artifact.pristine.size()==artifact.manifest.plan.roots.size(),"compiled pristine roots missing");
 artifact.views=std::move(views);artifact.preparedFrames=std::move(preparedFrames);
 for(auto& root:artifact.manifest.plan.roots){auto pristine=std::find_if(artifact.pristine.begin(),artifact.pristine.end(),[&](const auto& row){return row.path==root.path;});Require(pristine!=artifact.pristine.end(),"compiled root proof absent");root.cab=pristine->cab;root.source=pristine->source;}
 return artifact;
}
inline void Export(const std::filesystem::path& mod,const std::filesystem::path& game,std::function<void(const char*,double)> cost={},const std::filesystem::path& previous={},const Package* preparedAuthor=nullptr,const EiemStaticSources::Selection* preparedSelection=nullptr,const EiemStaticSources::Discovery* preparedDiscovery=nullptr,
 const std::function<Bytes(std::shared_ptr<const Artifact>,const EiemStaticSources::Selection&)>& preparePrefab={},bool currentSelectionComplete=false){
 Require(!currentSelectionComplete||preparedSelection,"complete source selection requires the verified current index adapter");
 auto author=Measure(cost,"author-read",[&]{return preparedAuthor?*preparedAuthor:Load(mod);});auto selected=Measure(cost,"source-selection",[&]{return preparedSelection?*preparedSelection:EiemStaticSources::Merge({author},game);});
 std::optional<Artifact> cached;std::optional<EiemCompiledModStore::Loaded> store;std::optional<EiemStaticSources::Selection> inherited;bool sameDiscovery=false;
 const auto& cacheRoot=previous.empty()?mod:previous;
 if(std::filesystem::is_regular_file(cacheRoot/"compiled.bin")){
  // A changed staging payload cannot validate the previous author's resource
  // hashes. Read the published snapshot before publishing any replacement.
  if(EiemCompiledModStore::IsStore(cacheRoot/"compiled.bin")){store=Measure(cost,"compiled-read",[&]{return EiemCompiledModStore::Load(cacheRoot/"compiled.bin",cacheRoot,game,true);});cached=store->artifact;inherited=store->selection;}
  else cached=Measure(cost,"compiled-read",[&]{return EiemCompiledMod::ReadRuntime(cacheRoot,game,{},&author);});
  Require(cached->manifest.package.id==author.id,"previous compiled discoveries belong to another Mod; export to a new directory");
  if(!inherited)inherited=EiemStaticSources::LoadMod(cacheRoot,game);
  auto targets=[](const Package& package){std::set<std::pair<std::string,std::string>> result;for(const auto& row:package.targetIndex)result.emplace(row.first.path,row.first.asset);return result;};
  sameDiscovery=inherited->candidates==selected.candidates&&targets(cached->manifest.package)==targets(author);
  for(const auto& row:inherited->inputs){auto found=selected.inputs.find(row.first);if(found==selected.inputs.end()||found->second.revision!=row.second.revision)sameDiscovery=false;}
  for(const auto& row:inherited->assets){auto found=selected.assets.find(row.first);if(found==selected.assets.end()||found->second!=row.second)sameDiscovery=false;}
  if(inherited->dependencies!=selected.dependencies)sameDiscovery=false;
  bool gameRefresh=currentSelectionComplete&&EiemCompiledExportCarryover::RevisionsChanged(selected,*inherited);
  selected=Measure(cost,"compiled-source-carryover",[&]{return EiemCompiledExportCarryover::Merge(selected,*inherited,*cached,currentSelectionComplete);});
  EiemCompiledExportCarryover::RebindSources(*cached,selected,gameRefresh);
  // The descriptor keeps the source tool's request identity, not the combined
  // compiler identity, so a subsequent export still reuses the index selection.
  auto descriptor=selected;if(std::filesystem::is_regular_file(mod/"source-inputs.bin"))descriptor.manifest=EiemStaticSources::LoadMod(mod,game).manifest;
  EiemOfflineCompile::AtomicSave(mod/"source-inputs.bin",EiemCompiledModStore::SourceSelection(descriptor,game));
  if(!preparedDiscovery&&sameDiscovery&&cached->manifest.rootAssembly&&cached->views.size()==cached->manifest.plan.originals.size()&&cached->manifest.plan.contract==InputContractDigest(author)&&(!store||cached->manifest.plan.author==PackageDigest(author))){
   for(const auto& source:cached->manifest.plan.originals)ReadSource({},source);
   if(cached){cached->manifest.package=author;cached->manifest.plan.author=PackageDigest(author);
    if(store){if(std::filesystem::weakly_canonical(cacheRoot)!=std::filesystem::weakly_canonical(mod))EiemCompiledExportCarryover::CopySnapshot(cacheRoot/"compiled.bin",mod/"compiled.bin");return;}
    auto bytes=EiemCompiledMod::Encode(*cached,std::filesystem::weakly_canonical(game));if(!std::filesystem::is_regular_file(mod/"compiled.bin")||bytes!=EiemNativeInput::Read(mod/"compiled.bin"))EiemOfflineCompile::AtomicSave(mod/"compiled.bin",bytes);return;}
  }
 }
 if(!store&&sameDiscovery&&cached&&cached->manifest.rootAssembly&&Measure(cost,"incremental-material-wall",[&]{return RecompileMaterialParameters(*cached,author,selected,cost);})){
  auto bytes=Measure(cost,"compiled-encode",[&]{return EiemCompiledMod::Encode(*cached,std::filesystem::weakly_canonical(game),cost);});
  auto decoded=Measure(cost,"compiled-roundtrip",[&]{return EiemCompiledMod::Decode(bytes,mod,game,{},&author);});Require(decoded.manifest.plan.contract==InputContractDigest(author),"incremental material author roundtrip differs");
  Measure(cost,"compiled-publish",[&]{EiemOfflineCompile::AtomicSave(mod/"compiled.bin",bytes);return 0;});return;
 }
 std::optional<EiemStaticSources::Discovery> discovered;
 if(store&&!preparedDiscovery){std::set<Identity> targets;for(const auto& row:author.targetIndex)targets.insert({EiemStaticSources::Logical(row.first.path),row.first.asset});discovered=Measure(cost,"export-consumer-discovery",[&]{return EiemStaticSources::FindConsumers(selected,targets);});preparedDiscovery=&*discovered;}
 auto artifact=Compile(author,selected,game,cost,cached?&*cached:nullptr,preparedDiscovery);
 if(store){
  Bytes prepared;if(preparePrefab)prepared=Measure(cost,"export-shared-pfb",[&]{return preparePrefab(std::make_shared<const Artifact>(artifact),selected);});
  else{EiemModUpdateStore::File file(cacheRoot/"compiled.bin",false);auto snapshot=EiemModUpdateStore::Read(file);Require(!snapshot.blocks.count("runtime/prepared-pfb"),"author export needs a refreshed shared PFB descriptor");}
  // Always create a private file; a partial-export staging file may have been
  // hardlinked to the published snapshot. Never append through that link.
  auto path=mod/"compiled.bin",temporary=path;temporary+=L".export-store-"+std::to_wstring(GetCurrentProcessId());Require(!std::filesystem::exists(temporary),"export store temporary exists");
  struct Cleanup{std::filesystem::path path;~Cleanup(){std::error_code error;std::filesystem::remove(path,error);}}cleanup{temporary};
  EiemCompiledModStore::Save(temporary,0,artifact,game,discovered?discovered->decisions:store->decisions,EiemCompiledExportCarryover::Catalog(selected),prepared.empty()?nullptr:&prepared,&selected);
  {auto decoded=EiemCompiledModStore::Load(temporary,mod,game,true);Require(decoded.artifact.manifest.plan.author==PackageDigest(author),"carried-over export author roundtrip differs");}
  Measure(cost,"compiled-publish",[&]{auto published=std::filesystem::exists(path)?ReplaceFileW(path.c_str(),temporary.c_str(),nullptr,0,nullptr,nullptr):MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_WRITE_THROUGH);auto error=published?ERROR_SUCCESS:GetLastError();Require(published!=0,"export store staging publication failed: "+std::to_string(error));return 0;});return;
 }
 auto bytes=Measure(cost,"compiled-encode",[&]{return EiemCompiledMod::Encode(artifact,std::filesystem::weakly_canonical(game),cost);});auto decoded=Measure(cost,"compiled-roundtrip",[&]{return EiemCompiledMod::Decode(bytes,mod,game,{},&author);});Require(decoded.manifest.plan.author==PackageDigest(author),"compiled author roundtrip differs");
 Measure(cost,"compiled-publish",[&]{EiemOfflineCompile::AtomicSave(mod/"compiled.bin",bytes);return 0;});
}
} // namespace EiemCompiledModWriter
