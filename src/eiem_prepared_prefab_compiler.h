#pragma once
#include "eiem_source_adaptation_compiler.h"

// The PFB-only slice reuses the Mod's already compiled Mesh/Material/Texture
// sources. Shared source descriptors are prepared once per immutable game and
// author snapshot, rather than rebuilding those resources for every new PFB.
// This local path deliberately rejects inputs requiring a core-resource or
// Physics rebuild. Its initial descriptor preparation is separately measured;
// it has not yet been persisted by the offline exporter or wired to Unity.
namespace EiemPreparedPrefabCompiler {
using namespace EiemCompiledMod;
struct Prepared {
 std::shared_ptr<const Artifact> baseline;EiemStaticSources::Selection current;
 Package resolved;
 std::map<std::string,std::string> coreInputs;
 AssetIndex assets;MeshSchemaIndex schemas;MeshBoneHashIndex palettes;MaterialPreloadIndex preloads;
 std::vector<BuiltMeshPart> meshes;std::map<NativeReference,std::string> markers;
 std::set<NativeReference> targets;std::set<std::string> resourceCabs;
};
inline Prepared Prepare(std::shared_ptr<const Artifact> baseline,const EiemStaticSources::Selection& current){
 Require(bool(baseline),"prepared PFB compiler lacks offline author snapshot");Prepared prepared;prepared.baseline=std::move(baseline);prepared.current=current;
 const auto& a=*prepared.baseline;const auto& author=a.manifest.package;prepared.resolved=author;
 for(const auto& resource:author.resources)Require(!Begins(resource.first,"resources/")&&!resource.second->blocks.count("@physics"),"prepared PFB slice requires separate generic/Physics adaptation");
 std::set<std::string> sources;auto add=[&](const std::string& path){if(!path.empty())sources.insert(current.assets.at(EiemStaticSources::Logical(path)));};
 for(const auto& target:author.targetIndex)add(target.first.path);for(const auto& material:author.materials)add(material.second.source.path);for(const auto& texture:author.textures)add(texture.second.source.path);
 sources=EiemStaticSources::Closure(current,std::move(sources));std::vector<Bytes> inputs;std::vector<EndfieldInput::Bundle> bundles;
 for(const auto& name:sources){const auto& entry=current.inputs.at(name);auto id=name+'\n'+entry.revision;auto file=Hash(Bytes(id.begin(),id.end()))+".vfs";
  Require(std::any_of(a.manifest.plan.originals.begin(),a.manifest.plan.originals.end(),[&](const auto& row){return row.file==file&&row.hash==entry.revision;}),"prepared author resource changed; adapt core sources first");
  prepared.coreInputs.emplace(name,entry.revision);
  inputs.push_back(EiemStaticSources::Payload(entry));bundles.push_back(EndfieldInput::Decode(inputs.back()));AddSourceInventory(prepared.assets,prepared.schemas,prepared.palettes,prepared.preloads,bundles.back(),author);
 }
 if(std::any_of(author.textures.begin(),author.textures.end(),[](const auto& row){return row.second.source.path.empty();}))ResolveTextureTemplates(prepared.resolved,inputs,prepared.assets,&bundles);
 for(const auto& target:author.targetIndex){const auto& asset=prepared.assets.at(target.first);Require(asset.classId==43,"prepared target is not a Mesh");prepared.targets.insert({EiemStaticSources::Cab(asset.cab),asset.pathId});prepared.resourceCabs.insert(asset.cab);
  const auto& rule=author.rules.at(target.second);if(rule.skip)continue;
  for(size_t i=0;i<rule.parts.size();++i){const auto& id=rule.parts[i];const auto& mesh=author.meshes.at(author.parts.at(id).mesh);auto pathId=i?AllocateMeshId(asset.cab,author,target.first,id):asset.pathId;
   prepared.meshes.push_back({target.first,target.second,id,asset.cab,asset.pathId,pathId,mesh.vertices,mesh.indices,prepared.palettes.at({asset.cab,asset.pathId})});
   prepared.markers.emplace(NativeReference{asset.cab,pathId},RuntimePartName(author,id,target.first));}}
 for(const auto& material:author.materials)prepared.resourceCabs.insert(prepared.assets.at(material.second.source).cab);
 for(const auto& texture:prepared.resolved.textures)prepared.resourceCabs.insert(prepared.assets.at(texture.second.source).cab);
 return prepared;
}
inline Bytes Encode(const Prepared& prepared){
 Writer w;w.S("EFF_PREPARED_PFB/author+core-inputs+assets+schemas+palettes/le64");const auto& a=*prepared.baseline;
 w.S(a.manifest.plan.author);w.S(a.manifest.plan.contract);WriteAuthor(w,prepared.resolved,true);w.Strings(prepared.coreInputs);
 w.N(prepared.assets.size());for(const auto& row:prepared.assets){const auto& v=row.second;w.Id(row.first);w.S(v.path);w.S(v.asset);w.S(v.cab);w.N(uint64_t(v.pathId));w.N(uint64_t(v.classId));w.Raw(v.type.data(),v.type.size());}
 w.N(prepared.schemas.size());for(const auto& row:prepared.schemas){const auto& type=row.second;w.Raw(row.first.data(),row.first.size());w.N(uint64_t(type.classId));w.N(type.nodes.size());for(const auto& node:type.nodes){w.S(node.type);w.S(node.name);w.B(node.align);w.N(node.children.size());for(auto child:node.children)w.N(child);}}
 w.N(prepared.palettes.size());for(const auto& row:prepared.palettes){w.S(row.first.first);w.N(uint64_t(row.first.second));w.N(row.second.size());for(auto value:row.second)w.N(value);}
 w.N(prepared.preloads.size());for(const auto& row:prepared.preloads){w.S(row.first.first);w.N(uint64_t(row.first.second));w.N(row.second.size());for(const auto& ref:row.second){w.S(ref.first);w.N(uint64_t(ref.second));}}
 w.N(prepared.meshes.size());for(const auto& mesh:prepared.meshes){w.Id(mesh.target);w.S(mesh.rule);w.S(mesh.part);w.S(mesh.cab);w.N(uint64_t(mesh.sourceId));w.N(uint64_t(mesh.id));w.N(mesh.vertices);w.N(mesh.indices);w.N(mesh.sourceBoneHashes.size());for(auto value:mesh.sourceBoneHashes)w.N(value);}
 w.N(prepared.markers.size());for(const auto& row:prepared.markers){w.S(row.first.first);w.N(uint64_t(row.first.second));w.S(row.second);}w.N(prepared.targets.size());for(const auto& ref:prepared.targets){w.S(ref.first);w.N(uint64_t(ref.second));}w.Set(prepared.resourceCabs);
 return Pack(w.b);
}
inline Prepared Decode(const Bytes& packed,std::shared_ptr<const Artifact> baseline,const EiemStaticSources::Selection& current){
 Require(bool(baseline),"prepared PFB descriptor lacks author snapshot");auto bytes=Unpack(packed);EiemOfflinePackage::Reader r{bytes};Require(r.S()=="EFF_PREPARED_PFB/author+core-inputs+assets+schemas+palettes/le64","prepared PFB descriptor structure differs");
 Prepared result;result.baseline=std::move(baseline);result.current=current;const auto& a=*result.baseline;Require(r.S()==a.manifest.plan.author&&r.S()==a.manifest.plan.contract,"prepared PFB descriptor belongs to another author");
 r.verifiedAuthor=&a.manifest.package;result.resolved=ReadAuthor(r,a.manifest.package.root.parent_path(),true,a.manifest.package.root);Require(result.resolved.id==a.manifest.package.id,"prepared resolved author identity differs");
 result.coreInputs=r.StringsMap();Require(!result.coreInputs.empty(),"prepared core resource seal empty");for(const auto& row:result.coreInputs)Require(DigestText(row.second)&&current.inputs.at(row.first).revision==row.second,"prepared core resources changed; update them before compiling a PFB");
 auto n=r.Count();for(size_t i=0;i<n;++i){auto key=r.Id();Asset asset;asset.path=r.S();asset.asset=r.S();asset.cab=r.S();asset.pathId=int64_t(r.N());asset.classId=int(r.N());r.Raw(asset.type.data(),asset.type.size());Require(asset.pathId&&key.path==asset.path&&key.asset==asset.asset&&result.assets.emplace(key,std::move(asset)).second,"prepared asset identity repeated/invalid");}
 n=r.Count();for(size_t i=0;i<n;++i){std::array<unsigned char,16> key{};r.Raw(key.data(),key.size());Type type;type.hash=key;type.classId=int(r.N());Require(type.classId==43,"prepared Mesh schema type differs");auto count=r.Count();for(size_t j=0;j<count;++j){Node node;node.type=r.S();node.name=r.S();node.align=r.B();auto children=r.Count();for(size_t k=0;k<children;++k){auto child=r.Size();Require(child>j&&child<count,"prepared Mesh schema has invalid child/cycle");node.children.push_back(child);}type.nodes.push_back(std::move(node));}Require(!type.nodes.empty(),"prepared Mesh schema empty");PrepareFixedWidths(type);Require(result.schemas.emplace(key,std::move(type)).second,"prepared Mesh schema repeated");}
 auto reference=[&](){auto cab=r.S();auto id=int64_t(r.N());Require(!cab.empty()&&id,"prepared resource reference empty");return NativeReference{std::move(cab),id};};
 n=r.Count();for(size_t i=0;i<n;++i){auto ref=reference();std::vector<uint32_t> hashes;auto count=r.Count();for(size_t j=0;j<count;++j)hashes.push_back(r.U());Require(result.palettes.emplace(ref,std::move(hashes)).second,"prepared palette repeated");}
 n=r.Count();for(size_t i=0;i<n;++i){auto ref=reference();std::set<NativeReference> refs;auto count=r.Count();for(size_t j=0;j<count;++j)Require(refs.insert(reference()).second,"prepared preload repeated");Require(result.preloads.emplace(ref,std::move(refs)).second,"prepared Material preload repeated");}
 n=r.Count();for(size_t i=0;i<n;++i){BuiltMeshPart mesh;mesh.target=r.Id();mesh.rule=r.S();mesh.part=r.S();mesh.cab=r.S();mesh.sourceId=int64_t(r.N());mesh.id=int64_t(r.N());mesh.vertices=r.U();mesh.indices=r.U();auto count=r.Count();for(size_t j=0;j<count;++j)mesh.sourceBoneHashes.push_back(r.U());Require(a.manifest.package.targetIndex.count(mesh.target)&&a.manifest.package.parts.count(mesh.part)&&mesh.sourceId&&mesh.id&&mesh.vertices,"prepared author Mesh part invalid");result.meshes.push_back(std::move(mesh));}
 n=r.Count();for(size_t i=0;i<n;++i){auto ref=reference();auto marker=r.S();Require(a.manifest.plan.parts.count(marker)&&result.markers.emplace(ref,marker).second,"prepared Mesh marker invalid");}n=r.Count();for(size_t i=0;i<n;++i)Require(result.targets.insert(reference()).second,"prepared target reference repeated");result.resourceCabs=r.Set();
 Require(r.p==bytes.size()&&result.targets.size()==a.manifest.package.targetIndex.size(),"prepared PFB descriptor coverage/suffix differs");return result;
}
inline EiemSourceAdaptationJobs::Result Compile(const Prepared& prepared,const EiemSourceAdaptationJobs::Request& request,const std::function<void(const char*,double)>& cost={}){
 auto checkpoint=std::chrono::steady_clock::now();auto stage=[&](const char* name){if(cost){auto now=std::chrono::steady_clock::now();cost(name,std::chrono::duration<double,std::milli>(now-checkpoint).count());checkpoint=now;}};
 const auto& baseline=*prepared.baseline;const auto& author=baseline.manifest.package;const auto& current=prepared.current;
 for(const auto& core:prepared.coreInputs)Require(current.inputs.at(core.first).revision==core.second,"prepared core resource seal is stale");
 Require(request.stamp==EiemSourceAdaptationCompiler::StampFor(author,current,request.source),"prepared PFB request is stale");const auto& entry=current.inputs.at(request.source);
 auto input=EiemStaticSources::Payload(entry);auto original=EndfieldInput::Decode(input);EiemSourceAdaptationJobs::Result result;result.decision.stamp=request.stamp;std::map<NativeReference,Identity> matchedMeshes;
 stage("pfb-read");
 for(const auto& member:original.members){Require(!prepared.resourceCabs.count(member.name),"source owns authored core resources; separate core adaptation required");if(!Serialized(member.bytes))continue;
  auto layout=ParseSerialized(member.bytes);auto consumers=MeshConsumers(member.bytes,layout,member.name,prepared.targets,EiemStaticSources::Cab);if(consumers.empty())continue;
  EiemPrefabConsumers::Ownership ownership(member.bytes,layout,member.name);std::map<int64_t,int64_t> owners;for(const auto& consumer:consumers){owners.emplace(consumer.renderer,ownership.Owner(consumer.renderer));
   auto asset=std::find_if(prepared.assets.begin(),prepared.assets.end(),[&](const auto& row){return EiemStaticSources::Cab(row.second.cab)==EiemStaticSources::Cab(consumer.mesh.first)&&row.second.pathId==consumer.mesh.second;});Require(asset!=prepared.assets.end(),"prepared Renderer Mesh anchor absent");matchedMeshes.emplace(NativeReference{member.name,consumer.renderer},asset->first);}
  std::set<int64_t> covered;
  for(const auto& asset:SerializedAssets(member.bytes,member.name,layout))if(asset.classId==1&&EiemStaticSources::Ends(EiemStaticSources::Logical(asset.path),".prefab")){
   Require(EiemStaticSources::Cab(asset.cab)==EiemStaticSources::Cab(member.name),"prepared PFB external hierarchy requires separate adaptation");auto members=ownership.GameObjects(asset.pathId);
   EiemConsumerUpdateRecords::Root root{EiemStaticSources::Logical(asset.path),member.name,asset.pathId,{}};for(const auto& row:owners)if(members.count(row.second)){root.renderers.insert(row.first);covered.insert(row.first);}if(!root.renderers.empty())result.decision.roots.push_back(std::move(root));}
  Require(covered.size()==owners.size(),"prepared PFB matched Renderer has no exact container root");
 }
 stage("pfb-match");if(result.decision.roots.empty())return result;
 auto artifact=std::make_shared<Artifact>();auto& m=artifact->manifest;auto& p=m.plan;const auto& prior=baseline.manifest.plan;
 m.package=author;m.rootAssembly=true;p.rulesOnly=p.spanRecipes=true;p.author=prior.author;p.contract=prior.contract;p.parts=prior.parts;p.materials=prior.materials;p.materialMatches=prior.materialMatches;p.textures=prior.textures;p.frameRecipes=prior.frameRecipes;p.names=prior.names;p.shared=prior.shared;
 auto identity=request.source+'\n'+entry.revision;auto file=Hash(Bytes(identity.begin(),identity.end()))+".vfs";p.key=Hash(Bytes(identity.begin(),identity.end()));m.baseline=p.key;
 p.originals={{file,entry.revision,entry.physical,entry.offset,entry.size}};p.closure={0};p.inputDependencies.resize(1);p.namespaceRecipes.resize(1);p.memberRecipes.resize(1);p.rendererRules.resize(1);
 for(const auto& row:matchedMeshes)p.rendererRules[0].push_back({row.first.first,row.first.second,row.second});
 stage("pfb-descriptors");auto edited=original;auto renderers=BuildRendererInput(edited,prepared.resolved,prepared.assets,prepared.meshes,prepared.schemas,nullptr,&prepared.preloads,cost);p.rendererParts=renderers.parts.size();stage("pfb-renderers");
 auto appearance=BuildTextureInput(edited,prepared.resolved,prepared.assets);Require(appearance.textures.empty()&&!appearance.changedMaterials,"prepared PFB slice unexpectedly rebuilds author core assets");
 stage("pfb-texture-preloads");
 Input record;record.file=file;for(const auto& member:edited.members)if(Serialized(member.bytes)){
  auto layout=ParseSerialized(member.bytes);p.names.members.emplace(member.name,"CAB-"+EiemLoadingGeneration::Token(p.key,member.name,32));for(const auto& external:EiemLoadingGraphAudit::ReadExternals(member.bytes,layout))record.externals.insert(external.member);
  for(const auto& row:layout.objects)if(layout.types.at(row.second.type).classId==142){record.bundle=Name(layout.types.at(row.second.type),ObjectValue(member.bytes,layout,row.first));p.names.bundles.emplace(record.bundle,EiemLoadingGeneration::BundleIdentity(p.key,record.bundle));}
 }
 Require(!record.bundle.empty(),"prepared PFB lacks native Bundle identity");p.inputs={record};auto startup=EndfieldInput::Encode(input,edited.members);p.startupHashes={Hash(startup)};
 stage("pfb-startup");
 EiemCompiledModWriter::InputAnalysis analysis;
 for(const auto& root:result.decision.roots){auto before=std::find_if(original.members.begin(),original.members.end(),[&](const auto& row){return row.name==root.cab;});auto after=std::find_if(edited.members.begin(),edited.members.end(),[&](const auto& row){return row.name==root.cab;});
  Require(before!=original.members.end()&&after!=edited.members.end(),"prepared PFB root member absent");auto graphs=analysis.graphs.find(root.cab);
  if(graphs==analysis.graphs.end()){auto beforeData=std::make_shared<const EiemRenderAssemblyDescription::GraphData>(before->bytes,root.cab);
   auto afterData=std::make_shared<const EiemRenderAssemblyDescription::GraphData>(after->bytes,root.cab,beforeData.get(),&before->bytes);
   if(cost){cost("assembly-original-object-decodes",double(beforeData->decodedObjects));cost("assembly-modified-object-decodes",double(afterData->decodedObjects));cost("assembly-unchanged-objects-reused",double(afterData->reusedObjects));}
   graphs=analysis.graphs.emplace(root.cab,std::make_pair(std::move(beforeData),std::move(afterData))).first;}
  EiemRenderAssemblyDescription::Graph beforeGraph(graphs->second.first,root.id),afterGraph(graphs->second.second,root.id);
  Root pristine;pristine.path=root.path;pristine.cab=root.cab;pristine.source=root.id;pristine.assembly=EiemRenderAssemblyDescription::Build(beforeGraph,beforeGraph,root.cab,Package{},AssetIndex{},{});analysis.pristine.push_back(std::move(pristine));
  Root compiled;compiled.path=root.path;compiled.cab=root.cab;compiled.source=root.id;compiled.assembly=EiemRenderAssemblyDescription::Build(beforeGraph,afterGraph,root.cab,author,prepared.assets,prepared.markers);
  for(const auto& source:compiled.assembly.sources)for(const auto& target:source.targets)if(!target.marker.empty())++compiled.markers[target.marker];p.roots.push_back(std::move(compiled));}
 stage("pfb-assemblies");
 auto isolated=edited;for(auto& member:isolated.members){if(Serialized(member.bytes)){auto hash=Hash(member.bytes);EiemLoadingGeneration::Recipe recipe;auto graphs=analysis.graphs.find(member.name);
  member.bytes=EiemLoadingGeneration::PrepareSerialized(member.bytes,p.names,&recipe,graphs==analysis.graphs.end()?nullptr:&graphs->second.second->layout).bytes;p.namespaceRecipes[0].push_back({member.name,hash,recipe.patches});}member.name=p.names.members.at(member.name);}
 p.inputs[0].hash=Hash(EndfieldInput::Encode(startup,isolated.members));EiemCompiledModWriter::MergeProof(*artifact,EiemCompiledModWriter::DescribeInput(0,p,true,original,edited,cost,nullptr,&analysis));
 stage("pfb-namespace-proof");
 // This slice never builds core assets. Retain unchanged object bodies by
 // their verified source ranges, encode only changed fields as literals, and
 // avoid all author-byte dictionaries/searches. Both recipes still hash and
 // reconstruct the exact edited member/native view.
 EiemResourceRecipe::Dictionary originalSpans({}),literalEdits({});for(const auto& member:original.members)originalSpans.buffers.push_back({EiemResourceRecipe::Original,member.name,&member.bytes});
 p.memberRecipes[0]=EiemCompiledModWriter::DescribeMembers(edited,artifact->members,originalSpans,literalEdits);stage("pfb-member-recipes");
 artifact->views={EiemCompiledInputViewWriter::Build(0,p,artifact->members,input,original,edited,literalEdits)};
 stage("pfb-native-view");
 Require(artifact->pristine.size()==p.roots.size(),"prepared PFB pristine coverage differs");result.input=0;result.view=std::shared_ptr<const EiemCompiledInputView::Recipe>(artifact,&artifact->views.at(0));result.artifact=std::move(artifact);return result;
}
} // namespace EiemPreparedPrefabCompiler
