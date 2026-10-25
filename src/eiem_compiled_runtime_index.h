#pragma once
#include "eiem_compiled_assembly_index.h"

// The compact runtime index is sufficient for
// native views; TypeTrees/member rebuilds are decoded only for actual writers
// sharing an input, or for a retired Mod's pristine candidate.
namespace EiemCompiledMod {
inline constexpr const char* sharedProofStructure="EFF_SHARED_SOURCE_PROOF/delta-layout/le64";
inline void WriteLayoutDeltaValue(Writer& w,size_t before,size_t after){
 auto distance=before>after?before-after:after-before;Require(distance<=UINT64_MAX/2,"compiled layout delta overflow");w.N(before>after?uint64_t(distance)*2-1:uint64_t(distance)*2);
}
inline size_t ReadLayoutDeltaValue(EiemOfflinePackage::Reader& r,size_t before){
 auto encoded=r.N();auto distance=encoded/2;if(encoded&1){Require(distance<SIZE_MAX&&distance+1<=before,"compiled layout negative delta underflow");return before-size_t(distance+1);}Require(distance<=SIZE_MAX,"compiled layout delta extent invalid");return AddSize(before,size_t(distance));
}
inline SerializedLayout ReadDeltaLayout(EiemOfflinePackage::Reader& r,const SerializedLayout& before,const SchemaReader& schemas){
 SerializedLayout after;after.metadataSize=ReadLayoutDeltaValue(r,before.metadataSize);after.dataOffset=ReadLayoutDeltaValue(r,before.dataOffset);after.countOffset=ReadLayoutDeltaValue(r,before.countOffset);after.tableStart=ReadLayoutDeltaValue(r,before.tableStart);after.tableEnd=ReadLayoutDeltaValue(r,before.tableEnd);
 Require(after.countOffset>=48&&after.tableStart>=after.countOffset+4&&after.tableEnd<=AddSize(48,after.metadataSize)&&after.dataOffset>=AddSize(48,after.metadataSize),"compiled delta object metadata extent differs");
 auto prefix=r.Size();Require(prefix<=before.types.size(),"compiled delta schema prefix invalid");after.types.assign(before.types.begin(),before.types.begin()+prefix);auto n=r.Count();for(size_t i=0;i<n;++i)after.types.push_back(schemas.types.at(r.Size()));
 auto count=r.Count();for(size_t i=0;i<count;++i){Object object;auto original=r.N();
  if(original){Require(original<=before.order.size(),"compiled delta original object ordinal absent");object.id=before.order[size_t(original-1)];const auto& old=before.objects.at(object.id);object.offset=ReadLayoutDeltaValue(r,old.offset);object.size=ReadLayoutDeltaValue(r,old.size);object.type=ReadLayoutDeltaValue(r,old.type);}
  else {object.id=int64_t(r.N());object.offset=r.Size();object.size=r.Size();object.type=r.Size();Require(!before.objects.count(object.id),"compiled delta new object reuses an original ID");}
  Require(object.id&&object.type<after.types.size()&&object.offset>=after.dataOffset&&after.objects.emplace(object.id,object).second,"compiled delta object identity/type differs");after.order.push_back(object.id);
 }
 Require(MultiplySize(count,24)==after.tableEnd-after.tableStart,"compiled delta object table extent differs");after.externals=before.externals;n=r.Count();for(size_t i=0;i<n;++i)after.externals.push_back(r.S());return after;
}
inline void WriteCompiledMember(Writer& w,const EiemResourceRecipe::Member& member){
 w.S(member.name);w.S(member.hash);w.N(member.flags);w.N(member.size);w.N(member.spans.size());
 for(const auto& span:member.spans){w.N(span.kind);w.S(span.source);w.N(span.offset);w.N(span.size);if(span.kind==EiemResourceRecipe::Literal)w.Blob(span.literal);}
}
inline EiemResourceRecipe::Member ReadCompiledMember(EiemOfflinePackage::Reader& r,const Artifact& a,size_t input){
 EiemResourceRecipe::Member member;member.name=r.S();member.hash=r.S();member.flags=r.U();member.size=r.Size();
 Require(a.manifest.plan.names.members.count(member.name)&&DigestText(member.hash),"compiled deferred member identity differs");
 auto n=r.Count();size_t extent=0;for(size_t i=0;i<n;++i){EiemResourceRecipe::Span span;auto kind=r.U();Require(kind<=EiemResourceRecipe::Literal,"compiled deferred span kind differs");span.kind=EiemResourceRecipe::Kind(kind);span.source=r.S();span.offset=r.Size();span.size=r.Size();
  Require(span.size&&extent<=member.size&&span.size<=member.size-extent,"compiled deferred span coverage differs");extent+=span.size;
  if(span.kind==EiemResourceRecipe::Literal){span.literal=r.Blob();Require(span.source.empty()&&!span.offset&&span.literal.size()==span.size,"compiled deferred literal extent differs");}
  else if(span.kind==EiemResourceRecipe::Original)Require(a.manifest.plan.names.members.count(span.source),"compiled deferred original member absent");
  else {const Bytes* source=nullptr;if(span.kind==EiemResourceRecipe::ExpandedFrame){auto f=a.preparedFrames.find(span.source);Require(f!=a.preparedFrames.end(),"compiled deferred frame absent");source=&f->second;}
   else {auto f=a.manifest.package.resources.find(span.source);Require(f!=a.manifest.package.resources.end(),"compiled deferred author absent");source=&f->second->bytes;}
   Require(span.offset<=source->size()&&span.size<=source->size()-span.offset,"compiled deferred author extent differs");}
  member.spans.push_back(std::move(span));
 }Require(extent==member.size&&input<a.manifest.plan.inputs.size(),"compiled deferred member incomplete");return member;
}
inline void HydrateInput(const Artifact& a,size_t input){
 auto retained=a.retainedProofBlocks.find(input);if(!a.proofBlocks&&!a.proofReader&&retained==a.retainedProofBlocks.end())return;
 std::lock_guard<std::mutex> lock(*a.proofMutex);if(a.hydrated.count(input))return;
 const auto& expectedMembers=a.inputMembers.size()==a.manifest.plan.inputs.size()?a.inputMembers.at(input):a.retainedInputDirectories.at(input).members;
 // A source merged into a lazily loaded artifact can have its own retained
 // block while the callback still addresses the older store's directory.
 // Prefer that explicit block; its index may not exist in the old reader.
 auto plain=Unpack(retained!=a.retainedProofBlocks.end()?retained->second:(a.proofBlocks?a.proofBlocks->at(input):a.proofReader(input)));EiemOfflinePackage::Reader r{plain};SchemaReader schemas;schemas.types=a.sharedSchemas;
 auto markerBytes=std::strlen(sharedProofStructure);bool delta=!a.sharedSchemas.empty()&&plain.size()>=8+markerBytes&&Le(plain,0,8)==markerBytes&&std::memcmp(plain.data()+8,sharedProofStructure,markerBytes)==0;if(delta)r.S();
 std::map<std::string,MemberProof> proofs;auto n=r.Count();for(size_t i=0;i<n;++i){auto name=r.S();MemberProof p;p.before=ReadLayout(r,schemas);p.after=delta?ReadDeltaLayout(r,p.before,schemas):ReadLayout(r,schemas);
  Require(ExternalPrefix(p.before.externals,p.after.externals),"compiled deferred external order differs");auto count=r.Count();for(size_t j=0;j<count;++j){StreamSite site;site.object=int64_t(r.N());site.offset=r.Size();site.width=r.Size();site.member=r.S();site.value=r.N();auto object=p.after.objects.find(site.object);
   Require(object!=p.after.objects.end()&&site.width&&site.width<=8&&site.offset<=object->second.size&&site.width<=object->second.size-site.offset,"compiled deferred stream extent differs");p.streams.push_back(std::move(site));}
  count=r.Count();for(size_t j=0;j<count;++j){EiemLoadingGeneration::Patch patch;patch.offset=r.Size();patch.before=r.S();Require(!patch.before.empty(),"compiled deferred pristine name empty");p.pristineNamespace.push_back(std::move(patch));}
  Require(expectedMembers.count(name)&&proofs.emplace(name,std::move(p)).second,"compiled deferred member proof duplicate/absent");
 }
 std::vector<MemberNamespaceRecipe> names;n=r.Count();for(size_t i=0;i<n;++i){MemberNamespaceRecipe recipe;recipe.member=r.S();recipe.hash=r.S();Require(proofs.count(recipe.member)&&DigestText(recipe.hash),"compiled deferred namespace identity differs");auto count=r.Count();for(size_t j=0;j<count;++j){EiemLoadingGeneration::Patch patch;patch.offset=r.Size();patch.before=r.S();patch.after=r.S();Require(!patch.before.empty()&&patch.before.size()==patch.after.size(),"compiled deferred name width differs");recipe.patches.push_back(std::move(patch));}names.push_back(std::move(recipe));}
 std::vector<EiemResourceRecipe::Member> members;n=r.Count();std::set<std::string> seen;for(size_t i=0;i<n;++i){auto member=ReadCompiledMember(r,a,input);Require(seen.insert(member.name).second,"compiled deferred recipe duplicated");members.push_back(std::move(member));}
 Require(r.p==plain.size()&&proofs.size()==expectedMembers.size()&&!members.empty(),"compiled deferred proof coverage differs");
 for(auto& proof:proofs)Require(a.members.emplace(proof.first,std::move(proof.second)).second,"compiled proof member spans multiple inputs");
 a.manifest.plan.namespaceRecipes.at(input)=std::move(names);a.manifest.plan.memberRecipes.at(input)=std::move(members);a.hydrated.insert(input);
}
inline void WriteNativeViews(Writer& w,const Artifact& a){
 w.N(a.views.size());for(size_t i=0;i<a.views.size();++i){const auto& view=a.views[i];auto hash=view.templateHash.empty()?view.startupHash:view.templateHash;
  Require(DigestText(hash),"compiled view offline digest absent");
  w.S(view.sourceHash);w.S(hash);w.N(view.size);w.N(view.spans.size());for(const auto& span:view.spans){w.N(span.kind);w.S(span.source);w.N(span.offset);w.N(span.size);if(span.kind==EiemResourceRecipe::Literal)w.Blob(span.literal);}w.N(view.names.size());for(const auto& site:view.names){w.N(site.span);w.N(site.offset);w.S(site.before);w.B(site.directory);}}
 w.N(a.preparedFrames.size());for(const auto& frame:a.preparedFrames){w.S(frame.first);w.Blob(frame.second);}
}
inline void ReadNativeViews(EiemOfflinePackage::Reader& r,Artifact& a,const Package* declarations=nullptr){
 const auto& plan=a.manifest.plan;const auto& author=declarations?*declarations:a.manifest.package;auto n=r.Count();Require(n==plan.originals.size(),"compiled index native view source coverage differs");
 for(size_t i=0;i<n;++i){EiemCompiledInputView::Recipe view;view.sourceHash=r.S();view.startupHash=r.S();view.size=r.Size();Require(view.sourceHash==plan.originals[i].hash&&DigestText(view.startupHash)&&view.size&&view.size<=INT64_MAX,"compiled index view identity/size differs");auto count=r.Count();size_t extent=0;
  for(size_t j=0;j<count;++j){EiemResourceRecipe::Span span;auto kind=r.U();Require(kind<=EiemResourceRecipe::Literal,"compiled index view source kind differs");span.kind=EiemResourceRecipe::Kind(kind);span.source=r.S();span.offset=r.Size();span.size=r.Size();Require(span.size,"compiled index view span empty");extent=AddSize(extent,span.size);
   if(span.kind==EiemResourceRecipe::Literal){span.literal=r.Blob();Require(span.source.empty()&&!span.offset&&span.literal.size()==span.size,"compiled index view literal extent differs");}
   else if(span.kind==EiemResourceRecipe::Original)Require(span.source.empty()&&span.offset<=plan.originals[i].size&&span.size<=plan.originals[i].size-span.offset,"compiled index original extent differs");
   else Require(author.resources.count(span.source)||(span.kind==EiemResourceRecipe::ExpandedFrame&&(span.source.rfind("@bounds:",0)==0||span.source.rfind("@literal:",0)==0)),"compiled index author source absent");view.spans.push_back(std::move(span));
  }Require(extent==view.size,"compiled index view coverage differs");count=r.Count();for(size_t j=0;j<count;++j){EiemCompiledInputView::Site site;site.span=r.Size();site.offset=r.Size();site.before=r.S();site.directory=r.B();
   Require(!site.before.empty()&&site.span<view.spans.size()&&view.spans[site.span].kind==EiemResourceRecipe::Literal&&site.offset<=view.spans[site.span].size&&site.before.size()<=view.spans[site.span].size-site.offset,"compiled index namespace extent differs");view.names.push_back(std::move(site));}a.views.push_back(std::move(view));
 }
 n=r.Count();for(size_t i=0;i<n;++i){auto resource=r.S();auto bytes=r.Blob();auto frame=std::find_if(plan.frameRecipes.begin(),plan.frameRecipes.end(),[&](const auto& row){return row.resource==resource;});Require(frame!=plan.frameRecipes.end()&&Hash(bytes)==frame->hash&&a.preparedFrames.emplace(resource,std::move(bytes)).second,"compiled index frame proof differs");}
 Require(a.preparedFrames.size()==plan.frameRecipes.size(),"compiled index frame coverage differs");
}
inline Bytes Encode(const Artifact& a,const std::filesystem::path& game,const std::function<void(const char*,double)>& cost={}){
 auto checkpoint=std::chrono::steady_clock::now();auto report=[&](const char* name){auto now=std::chrono::steady_clock::now();if(cost)cost(name,std::chrono::duration<double,std::milli>(now-checkpoint).count());checkpoint=now;};
 Require(!a.views.empty(),"compiled native views absent; re-export this Mod");
 if(!a.sharedSchemas.empty())for(size_t i=0;i<a.manifest.plan.inputs.size();++i)HydrateInput(a,i);
 auto portable=a.manifest;auto& plan=portable.plan;Require(a.views.size()==plan.inputs.size(),"compiled index view source coverage differs");
 for(auto& source:plan.originals){source.physical=source.physical.lexically_relative(game);Require(!source.physical.empty()&&!source.physical.is_absolute()&&source.physical.u8string().find("..")==std::string::npos,"compiled source escapes game");}
 plan.spanRecipes=false;plan.memberRecipes.clear();plan.namespaceRecipes.clear();plan.frameRecipes.clear();
 for(auto& root:plan.roots){root.assembly={};root.preparedAssembly.reset();}
 Writer w;w.S(EiemPackageProtocol::compiledRuntimeIndex);w.Blob(EiemOfflinePackage::Encode(portable));std::vector<Bytes> proofBlocks;
 auto assemblyIds=WritePreparedAssemblies(w,a);
 report("compiled-encode-assemblies");
 w.N(a.manifest.plan.originals.size());for(size_t i=0;i<a.manifest.plan.originals.size();++i){const auto& source=a.manifest.plan.originals[i];auto stamp=FileStamp(source.physical);if(a.verifiedSources.size()!=a.manifest.plan.originals.size()||!SameStamp(stamp,a.verifiedSources.at(i))){ReadSource({},source);Require(SameStamp(stamp,FileStamp(source.physical)),"compiled source changed while recording offline stamp");}w.N(stamp.bytes);w.N(stamp.written);}
 const auto& own=a.manifest.plan;w.N(own.frameRecipes.size());for(const auto& frame:own.frameRecipes){w.S(frame.resource);w.S(frame.hash);w.N(frame.offset);w.N(frame.size);w.N(frame.count);for(auto n:frame.stride)w.N(n);for(auto n:frame.start)w.N(n);}
 std::map<std::string,size_t> memberInputs;for(size_t i=0;i<own.inputs.size();++i){auto retained=a.retainedInputDirectories.find(i);
  if(retained!=a.retainedInputDirectories.end()&&a.sharedSchemas.empty()){Require(a.retainedProofBlocks.count(i)||a.proofReader,"retained input directory lacks compressed proof");for(const auto& name:retained->second.members)Require(memberInputs.emplace(name,i).second,"retained member has multiple inputs");}
  else {HydrateInput(a,i);for(const auto& member:own.memberRecipes.at(i))Require(memberInputs.emplace(member.name,i).second,"compiled member has multiple inputs");}}
 report("compiled-encode-source-directory");
 // Each source has its own schema/compression dictionary. Encode independently
 // in a bounded pool, then concatenate in source order for identical bytes.
 // Hydration above finishes before workers read the immutable member map.
 std::vector<Bytes> directories(own.inputs.size());proofBlocks.resize(own.inputs.size());
 EiemOfflineCompile::ParallelInputs(own.inputs.size(),[&](size_t i){Writer directoryBytes;auto directory=a.retainedInputDirectories.find(i);
  if(directory!=a.retainedInputDirectories.end()&&a.sharedSchemas.empty()){const auto& record=directory->second;directoryBytes.Set(record.members);std::set<size_t> unique;
   directoryBytes.N(record.dependencies.size());for(auto dependency:record.dependencies){Require(dependency<own.inputs.size()&&dependency!=i&&unique.insert(dependency).second,"retained input dependency invalid");directoryBytes.N(dependency);}directoryBytes.Set(record.addedBundles);proofBlocks[i]=a.retainedProofBlocks.count(i)?a.retainedProofBlocks.at(i):a.proofReader(i);directories[i]=std::move(directoryBytes.b);return;}
  Writer proof;SchemaWriter schemas;std::set<std::string> members;for(const auto& recipe:own.memberRecipes.at(i))if(a.members.count(recipe.name))members.insert(recipe.name);directoryBytes.Set(members);proof.N(members.size());
  bool provenEdges=a.addedBundles.size()==own.inputs.size();std::set<std::string> added=provenEdges?a.addedBundles.at(i):std::set<std::string>{};std::set<size_t> dependencies;auto add=[&](const std::string& member){auto found=memberInputs.find(member);if(found!=memberInputs.end()&&found->second!=i)dependencies.insert(found->second);};for(const auto& external:own.inputs[i].externals)add(external);
  bool retained=a.retainedProofBlocks.count(i)!=0&&a.sharedSchemas.empty();
  for(const auto& name:members){const auto& p=a.members.at(name);if(!retained){proof.S(name);WriteLayout(proof,p.before,schemas);WriteLayout(proof,p.after,schemas);proof.N(p.streams.size());for(const auto& site:p.streams){proof.N(uint64_t(site.object));proof.N(site.offset);proof.N(site.width);proof.S(site.member);proof.N(site.value);}proof.N(p.pristineNamespace.size());for(const auto& patch:p.pristineNamespace){proof.N(patch.offset);proof.S(patch.before);}}
   Require(ExternalPrefix(p.before.externals,p.after.externals),"compiled index external prefix differs");for(const auto& external:p.after.externals)add(external);
   if(!provenEdges)for(size_t n=p.before.externals.size();n<p.after.externals.size();++n){const auto& external=p.after.externals[n];auto found=memberInputs.find(external);if(found==memberInputs.end()){auto shared=own.sharedBundles.find(external);Require(shared!=own.sharedBundles.end(),"compiled index new dependency input absent: "+external);added.insert(shared->second);}else if(found->second!=i){const auto& bundle=own.inputs[found->second].bundle;Require(!bundle.empty(),"compiled index new dependency Bundle absent");added.insert(bundle);}}
  }
  directoryBytes.N(dependencies.size());for(auto dependency:dependencies)directoryBytes.N(dependency);directoryBytes.Set(added);
  if(retained)proofBlocks[i]=a.retainedProofBlocks.at(i);
  else{const auto& names=own.namespaceRecipes.at(i);proof.N(names.size());for(const auto& recipe:names){proof.S(recipe.member);proof.S(recipe.hash);proof.N(recipe.patches.size());for(const auto& patch:recipe.patches){proof.N(patch.offset);proof.S(patch.before);proof.S(patch.after);}}
   proof.N(own.memberRecipes[i].size());for(const auto& member:own.memberRecipes[i])WriteCompiledMember(proof,member);proofBlocks[i]=Pack(proof.b);}
  directories[i]=std::move(directoryBytes.b);
 },4);
 w.N(own.inputs.size());for(const auto& directory:directories)w.Raw(directory.data(),directory.size());report("compiled-encode-proofs-wall");
 w.N(a.pristine.size());for(size_t i=0;i<a.pristine.size();++i){const auto& root=a.pristine[i];w.S(root.path);w.N(root.input);w.S(root.cab);w.N(uint64_t(root.source));w.N(assemblyIds.pristine.at(i));}
 WriteNativeViews(w,a);
 report("compiled-encode-native-index");
 // Independently compressed proof sections stay compressed on the runtime
 // path. Do not decompress a whole envelope just to skip author-only records.
 auto packedIndex=Pack(w.b);report("compiled-encode-index-pack");Writer packet;packet.S(EiemPackageProtocol::compiledRuntimeIndex);
 size_t packetBytes=AddSize(AddSize(packet.b.size(),packedIndex.size()),48);for(const auto& proof:proofBlocks)packetBytes=AddSize(packetBytes,AddSize(8,proof.size()));packet.b.reserve(packetBytes);
 packet.Blob(packedIndex);packet.N(proofBlocks.size());for(const auto& proof:proofBlocks)packet.Blob(proof);
 auto hash=Sha(packet.b.data(),packet.b.size());packet.Raw(hash.data(),hash.size());return packet.b;
}
inline Artifact DecodeIndexed(const Bytes& bytes,const Bytes& plain,const std::filesystem::path& mod,const std::filesystem::path& game,bool runtime,const std::function<void(const char*,double)>& cost,const std::shared_ptr<const std::vector<Bytes>>& externalProofs,const Package* verifiedAuthor=nullptr){
 auto checkpoint=std::chrono::steady_clock::now();auto report=[&](const char* stage){if(cost){auto now=std::chrono::steady_clock::now();cost(stage,std::chrono::duration<double,std::milli>(now-checkpoint).count());checkpoint=now;}};
 EiemOfflinePackage::Reader r{plain};Require(r.S()==EiemPackageProtocol::compiledRuntimeIndex,"compiled index structure differs");Artifact a;auto manifest=r.Blob();a.manifest=EiemOfflinePackage::Decode(manifest,mod,mod.parent_path(),mod,true,verifiedAuthor);report("artifact-manifest");auto& plan=a.manifest.plan;
 Require(!a.manifest.composed&&plan.rulesOnly&&!plan.spanRecipes,"compiled index requires independent native views");plan.spanRecipes=true;plan.memberRecipes.resize(plan.inputs.size());plan.namespaceRecipes.resize(plan.inputs.size());
 auto base=std::filesystem::weakly_canonical(std::filesystem::absolute(game));for(auto& source:plan.originals){auto relative=source.physical;Require(!relative.empty()&&!relative.is_absolute()&&relative.u8string().find("..")==std::string::npos,"compiled source escapes game");source.physical=std::filesystem::weakly_canonical(base/relative);Require(source.physical.lexically_relative(base).u8string().rfind("..",0)!=0,"compiled source link escapes game");}report("artifact-source-paths");
 auto assemblies=ReadPreparedAssemblies(r,plan);report("artifact-assembly-index");
 auto stampCount=r.Count();Require(stampCount==plan.originals.size(),"compiled source stamp coverage differs");for(size_t i=0;i<stampCount;++i){InputFileStamp stamp{r.N(),r.N()};const auto& source=plan.originals[i];Require(source.offset<=stamp.bytes&&source.size<=stamp.bytes-source.offset,"compiled source stamp range differs");a.verifiedSources.push_back(stamp);}
 auto n=r.Count();std::set<std::string> frameSources;for(size_t i=0;i<n;++i){EiemResourceRecipe::Frame frame;frame.resource=r.S();frame.hash=r.S();frame.offset=r.Size();frame.size=r.Size();frame.count=r.Size();for(auto& value:frame.stride)value=r.Size();for(auto& value:frame.start)value=r.Size();
  Require(a.manifest.package.resources.count(frame.resource)&&a.manifest.package.resources.at(frame.resource)->kind==1&&DigestText(frame.hash)&&frame.count&&frame.stride[0]==16&&frame.start[0]==0&&frameSources.insert(frame.resource).second,"compiled index frame identity differs");
  const auto& source=a.manifest.package.resources.at(frame.resource)->bytes;Require(frame.offset<=source.size()&&frame.size<=source.size()-frame.offset,"compiled index frame resource extent differs");
  for(size_t stream=0;stream<4;++stream)Require(frame.start[stream]<=frame.size&&MultiplySize(frame.count,frame.stride[stream])<=frame.size-frame.start[stream],"compiled index frame stream extent differs");plan.frameRecipes.push_back(std::move(frame));}
 n=r.Count();Require(n==plan.inputs.size()&&externalProofs&&externalProofs->size()==n,"compiled index input coverage differs");for(size_t i=0;i<n;++i){a.inputMembers.push_back(r.Set());auto count=r.Count();std::vector<size_t> dependencies;std::set<size_t> seen;for(size_t j=0;j<count;++j){auto input=r.Size();Require(input<n&&input!=i&&seen.insert(input).second,"compiled index input dependency differs");dependencies.push_back(input);}plan.inputDependencies.push_back(std::move(dependencies));a.addedBundles.push_back(r.Set());}a.proofBlocks=externalProofs;
 n=r.Count();for(size_t i=0;i<n;++i){Root root;root.path=r.S();root.input=r.Size();root.cab=r.S();root.source=int64_t(r.N());root.preparedAssembly=assemblies.at(r.Size());Require(root.input<plan.inputs.size(),"compiled index pristine root input differs");a.pristine.push_back(std::move(root));}report("artifact-pristine-roots");
 ReadNativeViews(r,a);report("artifact-native-views");Require(r.p==plain.size()&&a.pristine.size()==plan.roots.size(),"compiled index suffix/pristine coverage differs");
 a.genericResources=std::any_of(a.manifest.package.resources.begin(),a.manifest.package.resources.end(),[](const auto& resource){return Begins(resource.first,"resources/");});a.digest=Hash(bytes);report("artifact-digest");
 if(!runtime){for(size_t i=0;i<plan.inputs.size();++i)HydrateInput(a,i);for(auto& root:plan.roots)ExpandPreparedAssembly(root);for(auto& root:a.pristine)ExpandPreparedAssembly(root);report("artifact-layouts");}return a;
}
inline Artifact DecodeCompiled(const Bytes& bytes,const std::filesystem::path& mod,const std::filesystem::path& game,bool runtime,const std::function<void(const char*,double)>& cost,const Package* verifiedAuthor=nullptr){
 auto start=std::chrono::steady_clock::now();const auto length=std::strlen(EiemPackageProtocol::compiledRuntimeIndex);
 Require(bytes.size()>=8+length+32&&Le(bytes,0,8)==length&&std::memcmp(bytes.data()+8,EiemPackageProtocol::compiledRuntimeIndex,length)==0,"compiled runtime index absent; re-export this Mod with the current Blender plug-in");
 auto hash=Sha(bytes.data(),bytes.size()-32);Require(std::equal(hash.begin(),hash.end(),bytes.end()-32),"compiled index packet digest differs");EiemOfflinePackage::Reader r{bytes};r.S();auto index=r.Blob();auto proofs=std::make_shared<std::vector<Bytes>>();auto n=r.Count();for(size_t i=0;i<n;++i)proofs->push_back(r.Blob());Require(r.p==bytes.size()-32,"compiled index packet suffix differs");
 auto plain=Unpack(index);if(cost)cost("artifact-unpack",std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());return DecodeIndexed(bytes,plain,mod,game,runtime,cost,proofs,verifiedAuthor);
}
inline Artifact Decode(const Bytes& bytes,const std::filesystem::path& mod,const std::filesystem::path& game,const std::function<void(const char*,double)>& cost={},const Package* verifiedAuthor=nullptr){return DecodeCompiled(bytes,mod,game,false,cost,verifiedAuthor);}
inline Artifact DecodeRuntime(const Bytes& bytes,const std::filesystem::path& mod,const std::filesystem::path& game,const std::function<void(const char*,double)>& cost={},const Package* verifiedAuthor=nullptr){return DecodeCompiled(bytes,mod,game,true,cost,verifiedAuthor);}
} // namespace EiemCompiledMod
