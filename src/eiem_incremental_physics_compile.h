#pragma once
#include <atomic>

// Physics parameter edits preserve Mesh/Renderer authoring and only rewrite
// the generated Physics objects in Prefabs that consume the target Meshes.
namespace EiemCompiledModWriter {

inline bool SamePhysicsTemplateSchema(const EiemPhysicsInput::Template& a,
                                     const EiemPhysicsInput::Template& b) {
 if(a.cab!=b.cab||a.layout.externals!=b.layout.externals||
    a.layout.objects.size()!=b.layout.objects.size())return false;
 for(const auto& object:a.layout.objects){
  auto found=b.layout.objects.find(object.first);if(found==b.layout.objects.end())return false;
  const auto& at=a.layout.types.at(object.second.type);const auto& bt=b.layout.types.at(found->second.type);
  if(at.classId!=bt.classId||at.hash!=bt.hash)return false;
 }
 return true;
}

inline bool SamePhysicsTopology(const EiemPhysicsInput::Document& a,
                                const EiemPhysicsInput::Document& b) {
 if(a.id!=b.id||a.nodes.size()!=b.nodes.size()||a.templates.size()!=b.templates.size()||
    a.colliders.size()!=b.colliders.size()||a.groups.size()!=b.groups.size())return false;
 for(const auto& row:a.nodes){
  auto found=b.nodes.find(row.first);if(found==b.nodes.end())return false;const auto& x=row.second;const auto& y=found->second;
  if(x.path!=y.path||x.parent!=y.parent||x.source!=y.source||x.hasParent!=y.hasParent||
     x.position!=y.position||x.rotation!=y.rotation||x.scale!=y.scale)return false;
 }
 for(const auto& row:a.templates){auto found=b.templates.find(row.first);if(found==b.templates.end()||!SamePhysicsTemplateSchema(row.second,found->second))return false;}
 for(const auto& row:a.colliders){
  auto found=b.colliders.find(row.first);if(found==b.colliders.end())return false;const auto& x=row.second;const auto& y=found->second;
  if(x.id!=y.id||x.name!=y.name||x.bone!=y.bone||x.cab!=y.cab||x.object!=y.object)return false;
 }
 for(size_t i=0;i<a.groups.size();++i){
  const auto& x=a.groups[i];const auto& y=b.groups[i];
  if(x.id!=y.id||x.name!=y.name||x.cab!=y.cab||x.object!=y.object||x.nodes!=y.nodes||x.colliders!=y.colliders||x.targets.size()!=y.targets.size())return false;
  auto left=x.targets.begin(),right=y.targets.begin();for(;left!=x.targets.end();++left,++right)if(left->path!=right->path||left->asset!=right->asset)return false;
 }
 return true;
}

inline std::set<std::string> PhysicsParameterChanges(const Package& author,const Package& old) {
 if(author.resources.size()!=old.resources.size())return {};
 auto comparable=author;std::set<std::string> changed;
 for(const auto& row:author.resources){
  auto prior=old.resources.find(row.first);if(prior==old.resources.end())return {};
  if(ResourceContentHash(*row.second)==ResourceContentHash(*prior->second))continue;
  if(row.second->kind!=prior->second->kind||row.second->type!=prior->second->type||
     !row.second->blocks.count("@physics")||!prior->second->blocks.count("@physics")||
     row.second->blocks.size()!=prior->second->blocks.size())return {};
  for(const auto& block:row.second->blocks){
   auto oldBlock=prior->second->blocks.find(block.first);if(oldBlock==prior->second->blocks.end())return {};
   if(block.first!="@physics"&&(block.second.size!=oldBlock->second.size||block.second.digest!=oldBlock->second.digest))return {};
  }
  comparable.resources[row.first]=prior->second;changed.insert(row.first);
 }
 if(changed.empty()||PackageDigest(comparable)!=PackageDigest(old)||InputContractDigest(comparable)!=InputContractDigest(old))return {};
 auto before=EiemPhysicsInput::Documents(old),after=EiemPhysicsInput::Documents(author);
 if(before.size()!=after.size())return {};
 for(size_t i=0;i<before.size();++i)if(!SamePhysicsTopology(before[i],after[i]))return {};
 return changed;
}

inline bool FreezePhysicsDonors(std::vector<EiemResourceRecipe::Span>& spans,
                                const std::set<std::string>& changed,const Package& old) {
 bool frozen=false;
 for(auto& span:spans)if(span.kind==EiemResourceRecipe::Author&&changed.count(span.source)){
  const auto& bytes=old.resources.at(span.source)->bytes;
  Require(span.offset<=bytes.size()&&span.size<=bytes.size()-span.offset,"incremental Physics donor range differs");
  span.literal.assign(bytes.begin()+span.offset,bytes.begin()+span.offset+span.size);
  span.kind=EiemResourceRecipe::Literal;span.source.clear();span.offset=0;frozen=true;
 }
 return frozen;
}

inline void RemapPhysicsPointers(const Type& sourceType,Value& value,size_t node,
                                 const SerializedLayout& sourceLayout,const std::string& sourceCab,
                                 const Type& targetType,SerializedLayout& targetLayout,const std::string& targetCab) {
 const auto& schema=sourceType.nodes.at(node);
 if(schema.type.rfind("PPtr<",0)==0){
  auto reference=NullableReference(sourceType,value,sourceLayout,sourceCab);
  SetReference(targetType,value,reference,targetLayout,targetCab);return;
 }
 for(auto& child:value.children)RemapPhysicsPointers(sourceType,child,child.node,sourceLayout,sourceCab,targetType,targetLayout,targetCab);
}

inline bool PatchPhysicsMember(Bytes& current,const Bytes& pristine,const Bytes& physics,const std::string& cab) {
 auto originalLayout=ParseSerialized(pristine),physicsLayout=ParseSerialized(physics),currentLayout=ParseSerialized(current);
 std::map<int64_t,Bytes> replacements;std::set<int64_t> added;
 for(const auto& object:physicsLayout.objects){
  auto old=originalLayout.objects.find(object.first);
  const auto& physicsType=physicsLayout.types.at(object.second.type);
  if(old==originalLayout.objects.end()){
   auto existing=currentLayout.objects.find(object.first);if(existing==currentLayout.objects.end())return false;
   const auto& currentType=currentLayout.types.at(existing->second.type);
   if(currentType.classId!=physicsType.classId||currentType.hash!=physicsType.hash)return false;
   auto value=ObjectValue(physics,physicsLayout,object.first);
   RemapPhysicsPointers(physicsType,value,0,physicsLayout,cab,currentType,currentLayout,cab);
   auto bytes=EncodeValue(currentType,value);
   if(bytes.size()!=existing->second.size||!std::equal(bytes.begin(),bytes.end(),current.begin()+existing->second.offset))
    replacements.emplace(object.first,std::move(bytes));
   added.insert(object.first);continue;
  }
  const auto& originalType=originalLayout.types.at(old->second.type);
  bool same=old->second.size==object.second.size&&std::equal(
      pristine.begin()+old->second.offset,pristine.begin()+old->second.offset+old->second.size,
      physics.begin()+object.second.offset);
  if(same)continue;
  auto existing=currentLayout.objects.find(object.first);if(existing==currentLayout.objects.end())return false;
  const auto& currentType=currentLayout.types.at(existing->second.type);
  if(currentType.classId!=physicsType.classId||currentType.hash!=physicsType.hash||originalType.hash!=physicsType.hash)return false;
  if(physicsType.classId==4){
   auto expected=ObjectValue(physics,physicsLayout,object.first),actual=ObjectValue(current,currentLayout,object.first);
   for(const char* field:{"m_Father","m_LocalPosition","m_LocalRotation","m_LocalScale"})
    if(EncodeValue(physicsType,Child(physicsType,expected,field))!=EncodeValue(currentType,Child(currentType,actual,field)))return false;
  }else if(physicsType.classId!=142)return false;
 }
 if(added.empty())return false;

 std::map<int64_t,std::set<int64_t>> requiredChildren;
 for(auto id:added){
  const auto& object=physicsLayout.objects.at(id);const auto& type=physicsLayout.types.at(object.type);if(type.classId!=4)continue;
  auto value=ObjectValue(physics,physicsLayout,id);auto parent=NullableReference(type,Child(type,value,"m_Father"),physicsLayout,cab);
  if(parent.first==cab&&parent.second)requiredChildren[parent.second].insert(id);
 }
 for(const auto& row:requiredChildren){
  auto parent=currentLayout.objects.find(row.first);if(parent==currentLayout.objects.end()||currentLayout.types.at(parent->second.type).classId!=4)return false;
  const auto& type=currentLayout.types.at(parent->second.type);auto value=ObjectValue(current,currentLayout,row.first);std::set<int64_t> children;
  for(const auto& child:ArrayValue(type,Child(type,value,"m_Children")).children){auto reference=NullableReference(type,child,currentLayout,cab);if(reference.first==cab)children.insert(reference.second);}
  for(auto child:row.second)if(!children.count(child))return false;
 }
 std::set<int64_t> preloaded;
 for(auto id:currentLayout.order){
  const auto& object=currentLayout.objects.at(id);const auto& type=currentLayout.types.at(object.type);if(type.classId!=142)continue;
  auto value=ObjectValue(current,currentLayout,id);
  for(const auto& item:ArrayValue(type,Child(type,value,"m_PreloadTable")).children){auto ref=NullableReference(type,item,currentLayout,cab);if(ref.first==cab)preloaded.insert(ref.second);}
 }
 for(auto id:added)if(!preloaded.count(id))return false;
 if(replacements.empty())return true;
 current=BuildSerialized(current,currentLayout,replacements,{});return true;
}

inline bool RecompilePhysicsParameters(Artifact& artifact,const Package& author,
                                      const EiemStaticSources::Selection& selection,
                                      const std::function<void(const char*,double)>& cost) {
 auto phase=std::chrono::steady_clock::now();auto reportPhase=[&](const char* name){if(cost)cost(name,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-phase).count());phase=std::chrono::steady_clock::now();};
 const auto old=artifact.manifest.package;auto changed=PhysicsParameterChanges(author,old);if(changed.empty())return false;
 Artifact next=artifact;auto& plan=next.manifest.plan;
 if(!plan.rulesOnly||!plan.spanRecipes||!artifact.manifest.rootAssembly||artifact.views.size()!=plan.originals.size()||
    artifact.verifiedSources.size()!=plan.originals.size())return false;
 for(size_t i=0;i<plan.originals.size();++i){
  auto stamp=FileStamp(plan.originals[i].physical);
  if(!SameStamp(stamp,next.verifiedSources[i])){
   ReadSource({},plan.originals[i]);stamp=FileStamp(plan.originals[i].physical);
   next.verifiedSources[i]=stamp;
  }
 }
 reportPhase("incremental-physics-source-stamps");

 auto documents=EiemPhysicsInput::Documents(author);std::set<Identity> targets;
 for(const auto& document:documents)for(const auto& group:document.groups)for(const auto& target:group.targets){
  if(!author.targetIndex.count(target))return false;targets.insert(target);
 }
 if(targets.empty())return false;
 std::set<size_t> jobs;
 for(size_t i=0;i<plan.rendererRules.size();++i)for(const auto& rule:plan.rendererRules[i])if(targets.count(rule.mesh))jobs.insert(i);
 if(cost)cost("incremental-physics-jobs",double(jobs.size()));
 if(jobs.empty())return false;
 for(const auto& target:targets){
  bool covered=std::any_of(plan.rendererRules.begin(),plan.rendererRules.end(),[&](const auto& rules){
   return std::any_of(rules.begin(),rules.end(),[&](const auto& rule){return rule.mesh.path==target.path&&rule.mesh.asset==target.asset;});
  });
  if(!covered)return false;
 }
 reportPhase("incremental-physics-document-targets");
 AssetIndex assets;MeshSchemaIndex schemas;MeshBoneHashIndex palettes;MaterialPreloadIndex preloads;
 std::set<std::string> targetSources;
 for(const auto& target:targets){auto route=selection.assets.find(EiemStaticSources::Logical(target.path));if(route==selection.assets.end()){if(cost)cost("incremental-physics-fallback-target-route",1);return false;}targetSources.insert(route->second);}
 for(const auto& source:targetSources){auto entry=selection.inputs.find(source);if(entry==selection.inputs.end())return false;
  auto bundle=EndfieldInput::Decode(EiemStaticSources::Payload(entry->second));
  AddSourceInventory(assets,schemas,palettes,preloads,bundle,author);
 }
 for(const auto& target:targets)if(!assets.count(target)){if(cost)cost("incremental-physics-fallback-target-asset",1);return false;}
 if(cost)cost("incremental-physics-target-assets",double(assets.size()));
 reportPhase("incremental-physics-source-assets");

 auto& nextPlan=next.manifest.plan;const size_t inputCount=nextPlan.inputs.size();
 if(next.inputMembers.size()!=inputCount||next.addedBundles.size()!=inputCount||nextPlan.inputDependencies.size()!=inputCount)return false;
 // Preserve compressed proofs for inputs whose authored output is unchanged.
 for(size_t i=0;i<inputCount;++i){
  if(!next.retainedInputDirectories.count(i))next.retainedInputDirectories.emplace(i,InputProofDirectory{
   next.inputMembers.at(i),next.addedBundles.at(i),nextPlan.inputDependencies.at(i)});
  if(!next.retainedProofBlocks.count(i)){
   if(next.proofBlocks&&next.proofBlocks->size()==inputCount)next.retainedProofBlocks.emplace(i,next.proofBlocks->at(i));
   else if(next.proofReader)next.retainedProofBlocks.emplace(i,next.proofReader(i));
  }
 }
 for(size_t i=0;i<inputCount;++i)HydrateInput(next,i);
 next.proofReader={};next.proofBlocks.reset();
 reportPhase("incremental-physics-proof-hydrate");
 std::set<size_t> rewriteProofs=jobs;
 for(size_t i=0;i<inputCount;++i)if(!jobs.count(i)){
  bool frozen=false;
  for(auto& member:nextPlan.memberRecipes.at(i))frozen|=FreezePhysicsDonors(member.spans,changed,old);
  frozen|=FreezePhysicsDonors(next.views.at(i).spans,changed,old);
  if(frozen)rewriteProofs.insert(i);
 }

 std::vector<EiemResourceRecipe::Buffer> authorBuffers;
 for(const auto& row:author.resources)if(row.second->kind!=10)authorBuffers.push_back({EiemResourceRecipe::Author,row.first,&row.second->bytes});
 for(const auto& frame:next.preparedFrames)authorBuffers.push_back({EiemResourceRecipe::ExpandedFrame,frame.first,&frame.second});
 EiemResourceRecipe::Dictionary authorSpans(std::move(authorBuffers));
 struct InputResult {std::map<std::string,MemberProof> members;size_t targetPairs=0;};
 std::vector<size_t> work(jobs.begin(),jobs.end());std::vector<InputResult> results(work.size());
 std::vector<std::map<std::string,double>> workerCosts(work.size());std::atomic<bool> unsupported{false};
 EiemOfflineCompile::ParallelInputs(work.size(),[&](size_t job){
  auto i=work[job];auto& timings=workerCosts[job];auto measure=[&](const char* name,double ms){timings[name]+=ms;};
  auto stage=std::chrono::steady_clock::now();auto report=[&](const char* name){measure(name,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-stage).count());stage=std::chrono::steady_clock::now();};
  std::set<Identity> inputTargets;
  for(const auto& rule:nextPlan.rendererRules.at(i))if(targets.count(rule.mesh))inputTargets.insert(rule.mesh);
  if(inputTargets.empty()){unsupported.store(true);return;}results[job].targetPairs=inputTargets.size();
  auto input=ReadSource({},nextPlan.originals.at(i));auto pristine=EndfieldInput::Decode(input);report("incremental-physics-prefab-read");
  auto physics=pristine;EiemPhysicsInput::Build(physics,author,assets,&documents,&inputTargets);report("incremental-physics-build");
  std::map<std::string,const Bytes*> originalMembers;std::vector<EiemResourceRecipe::Buffer> originalBuffers;
  for(const auto& member:pristine.members){originalMembers.emplace(member.name,&member.bytes);originalBuffers.push_back({EiemResourceRecipe::Original,member.name,&member.bytes});}
  EiemResourceRecipe::Dictionary originalSpans(std::move(originalBuffers));
  auto edited=pristine;edited.members.clear();
  for(const auto& recipe:nextPlan.memberRecipes.at(i))edited.members.push_back({recipe.name,recipe.flags,
      EiemResourceRecipe::Apply(recipe,originalMembers,old,next.preparedFrames)});
  report("incremental-physics-apply-recipes");bool modified=false;
  for(auto& member:edited.members){
   auto fresh=std::find_if(physics.members.begin(),physics.members.end(),[&](const auto& row){return row.name==member.name;});
   if(fresh==physics.members.end()||!Serialized(fresh->bytes)||!Serialized(member.bytes))continue;
   auto original=std::find_if(pristine.members.begin(),pristine.members.end(),[&](const auto& row){return row.name==member.name;});
   if(original==pristine.members.end()){unsupported.store(true);return;}
   auto before=member.bytes;if(!PatchPhysicsMember(member.bytes,original->bytes,fresh->bytes,member.name)){
    measure("incremental-physics-fallback-object-merge",1);unsupported.store(true);return;
   }
   modified|=before!=member.bytes;
  }
  report("incremental-physics-patch-members");if(!modified){measure("incremental-physics-fallback-no-object-delta",1);unsupported.store(true);return;}
  nextPlan.namespaceRecipes[i].clear();
  for(const auto& member:edited.members)if(Serialized(member.bytes)){
   EiemLoadingGeneration::Recipe names;EiemLoadingGeneration::PrepareSerialized(member.bytes,nextPlan.names,&names);
   nextPlan.namespaceRecipes[i].push_back({member.name,Hash(member.bytes),std::move(names.patches)});
  }
  report("incremental-physics-namespace");auto proof=DescribeInput(i,nextPlan,true,pristine,edited,measure,&next);
  report("incremental-physics-describe-input");nextPlan.memberRecipes[i]=DescribeMembers(edited,proof.members,originalSpans,authorSpans);
  report("incremental-physics-describe-members");
  for(size_t j=0;j<edited.members.size();++j)Require(EiemResourceRecipe::Apply(nextPlan.memberRecipes[i][j],originalMembers,author,next.preparedFrames)==edited.members[j].bytes,
      "incremental Physics member replay differs");
  auto startup=EndfieldInput::Encode(input,edited.members);nextPlan.startupHashes[i]=Hash(startup);report("incremental-physics-encode-startup");
  next.views[i]=EiemCompiledInputViewWriter::Build(i,nextPlan,proof.members,input,pristine,edited,authorSpans);report("incremental-physics-view");
  auto isolated=edited;
  for(auto& member:isolated.members){if(Serialized(member.bytes))member.bytes=EiemLoadingGeneration::PrepareSerialized(member.bytes,nextPlan.names).bytes;member.name=nextPlan.names.members.at(member.name);}
  nextPlan.inputs[i].hash=Hash(EndfieldInput::Encode(startup,isolated.members));report("incremental-physics-isolate");
  results[job].members=std::move(proof.members);
 },4);
 if(unsupported.load()){if(cost)cost("incremental-physics-fallback-worker",1);return false;}
 size_t rebuilt=work.size(),targetInputPairs=0;
 for(size_t job=0;job<work.size();++job){
  targetInputPairs+=results[job].targetPairs;
  for(auto& row:workerCosts[job])if(cost)cost(row.first.c_str(),row.second);
  for(auto& row:results[job].members)next.members[row.first]=std::move(row.second);
 }
 for(auto i:rewriteProofs){next.retainedProofBlocks.erase(i);next.retainedInputDirectories.erase(i);}
 if(cost)cost("incremental-physics-prefabs-rebuilt",double(rebuilt));
 if(cost)cost("incremental-physics-target-input-pairs",double(targetInputPairs));
 next.manifest.package=author;nextPlan.author=PackageDigest(author);nextPlan.contract=InputContractDigest(author);
 if(cost){cost("incremental-physics-inputs",double(rebuilt));cost("reused-native-inputs",double(inputCount-rebuilt));}
 artifact=std::move(next);return true;
}

} // namespace EiemCompiledModWriter
