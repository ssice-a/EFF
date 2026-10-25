#pragma once

// Geometry edits with the same renderer/bone/shape contract reuse the already
// compiled appearance and prefab graph. Structural edits rebuild only affected
// Prefab sources when their author and source contracts remain supported.
namespace EiemCompiledModWriter {
inline std::set<std::string> MeshResourceChanges(const Package& author,const Package& old){
 if(author.resources.size()!=old.resources.size()||author.meshes.size()!=old.meshes.size())return {};
 for(const auto& row:author.meshes){auto found=old.meshes.find(row.first);if(found==old.meshes.end())return {};
  const auto& mesh=row.second;const auto& prior=found->second;
  if(mesh.resource!=prior.resource||mesh.vertices!=prior.vertices||mesh.indices!=prior.indices||mesh.bones!=prior.bones||mesh.shapes!=prior.shapes)return {};
 }
 auto comparable=author;std::set<std::string> changed,meshes;
 for(const auto& mesh:author.meshes)meshes.insert(mesh.second.resource);
 for(const auto& row:author.resources){auto prior=old.resources.find(row.first);if(prior==old.resources.end())return {};
  // Physics can introduce transforms and change renderer bone resolution.
  if(row.second->blocks.count("@physics"))return {};
  if(ResourceContentHash(*row.second)==ResourceContentHash(*prior->second))continue;
  if(!meshes.count(row.first)||row.second->kind!=1||row.second->type!=prior->second->type)return {};
  comparable.resources[row.first]=prior->second;changed.insert(row.first);
 }
 if(changed.empty()||InputContractDigest(comparable)!=InputContractDigest(old))return {};
 return changed;
}
struct MeshStructureDelta {std::set<Identity> targets;std::set<std::string> resources;};
inline std::optional<MeshStructureDelta> MeshStructureChanges(const Package& next,const Package& old){
 auto meshResources=[](const Package& package){std::set<std::string> names;for(const auto& row:package.meshes)names.insert(row.second.resource);return names;};
 const auto oldMeshes=meshResources(old),newMeshes=meshResources(next);
 auto stable=[](Package package,const std::set<std::string>& resources){
  for(const auto& name:resources)package.resources.erase(name);
  package.meshes.clear();package.parts.clear();for(auto& rule:package.rules)rule.second.parts.clear();return InputContractDigest(package);
 };
 bool sameTargets=next.targetIndex.size()==old.targetIndex.size();
 if(sameTargets){auto a=next.targetIndex.begin(),b=old.targetIndex.begin();for(;a!=next.targetIndex.end();++a,++b)
   if(a->first.path!=b->first.path||a->first.asset!=b->first.asset||a->second!=b->second){sameTargets=false;break;}}
 if(stable(next,newMeshes)!=stable(old,oldMeshes)||!sameTargets)return {};
 MeshStructureDelta result;
 auto sameMesh=[](const Mesh& a,const Mesh& b){return a.resource==b.resource&&a.vertices==b.vertices&&a.indices==b.indices&&a.bones==b.bones&&a.shapes==b.shapes;};
 for(const auto& name:oldMeshes){auto before=old.resources.find(name),after=next.resources.find(name);
  if(after==next.resources.end()||ResourceContentHash(*before->second)!=ResourceContentHash(*after->second))result.resources.insert(name);
 }
 for(const auto& name:newMeshes)if(!oldMeshes.count(name))result.resources.insert(name);
 for(const auto& row:old.meshes){auto after=next.meshes.find(row.first);if(after==next.meshes.end()||!sameMesh(row.second,after->second)){
   result.resources.insert(row.second.resource);if(after!=next.meshes.end())result.resources.insert(after->second.resource);
  }}
 for(const auto& row:next.meshes)if(!old.meshes.count(row.first))result.resources.insert(row.second.resource);
 for(const auto& name:result.resources){auto before=old.resources.find(name),after=next.resources.find(name);
  if((before!=old.resources.end()&&before->second->kind!=1)||(after!=next.resources.end()&&after->second->kind!=1))return {};
 }
 auto samePart=[](const EiemDirectory::Part& a,const EiemDirectory::Part& b){return a.mesh==b.mesh&&a.material==b.material;};
 for(const auto& row:old.targetIndex){auto oldRule=old.rules.find(row.second),newRule=next.rules.find(row.second);if(oldRule==old.rules.end()||newRule==next.rules.end())return {};
  bool changed=oldRule->second.skip!=newRule->second.skip||oldRule->second.parts!=newRule->second.parts;
  std::set<std::string> parts(oldRule->second.parts.begin(),oldRule->second.parts.end());parts.insert(newRule->second.parts.begin(),newRule->second.parts.end());
  for(const auto& id:parts){auto before=old.parts.find(id),after=next.parts.find(id);if(before==old.parts.end()||after==next.parts.end()){changed=true;continue;}
   if(!samePart(before->second,after->second)){changed=true;continue;}
   auto beforeMesh=old.meshes.find(before->second.mesh),afterMesh=next.meshes.find(after->second.mesh);
   if(beforeMesh==old.meshes.end()||afterMesh==next.meshes.end()||!sameMesh(beforeMesh->second,afterMesh->second)){changed=true;continue;}
   auto oldResource=old.resources.find(beforeMesh->second.resource),newResource=next.resources.find(afterMesh->second.resource);
   if(oldResource==old.resources.end()||newResource==next.resources.end()||ResourceContentHash(*oldResource->second)!=ResourceContentHash(*newResource->second))changed=true;
  }
  if(changed)result.targets.insert(row.first);
 }
 if(result.targets.empty())return result;
 for(const auto& resource:result.resources){bool used=false;
  for(const auto& target:result.targets){const auto& before=old.rules.at(old.targetIndex.at(target));const auto& after=next.rules.at(next.targetIndex.at(target));
   for(const auto& id:before.parts){auto part=old.parts.find(id);if(part!=old.parts.end()){auto mesh=old.meshes.find(part->second.mesh);used=used||(mesh!=old.meshes.end()&&mesh->second.resource==resource);}}
   for(const auto& id:after.parts){auto part=next.parts.find(id);if(part!=next.parts.end()){auto mesh=next.meshes.find(part->second.mesh);used=used||(mesh!=next.meshes.end()&&mesh->second.resource==resource);}}
  }
  if(!used)return {};
 }
 return result;
}
inline bool MeshStructureSources(const Artifact& previous,const EiemStaticSources::Selection& selection,const std::set<Identity>& targets,
 std::set<std::string>& files,EiemStaticSources::Discovery& discovery){
 const auto& plan=previous.manifest.plan;if(plan.rendererRules.size()!=plan.originals.size()||plan.inputDependencies.size()!=plan.originals.size())return false;
 std::vector<unsigned char> roots(plan.originals.size());for(const auto& root:plan.roots){if(root.input>=roots.size())return false;roots[root.input]=1;}
 std::set<size_t> affected;
 for(size_t i=0;i<plan.rendererRules.size();++i)for(const auto& renderer:plan.rendererRules[i])if(targets.count(renderer.mesh)){affected.insert(i);break;}
 if(affected.empty())return false;
 bool changed=true;while(changed){changed=false;for(size_t i=0;i<plan.inputDependencies.size();++i)if(!affected.count(i))for(auto dependency:plan.inputDependencies[i]){
   if(dependency>=roots.size())return false;if(affected.count(dependency)){if(!roots[i])return false;affected.insert(i);changed=true;break;}
  }}
 for(auto i:affected){if(!roots[i])return false;const auto& source=plan.originals[i];std::string logical;
  for(const auto& row:selection.inputs)if(EiemStaticSources::SourceFile(row.first,row.second.revision)==source.file){logical=row.first;break;}
  if(logical.empty()||!selection.candidates.count(logical))return false;files.insert(source.file);discovery.sources.insert(logical);
  for(const auto& root:plan.roots)if(root.input==i)discovery.roots.insert(root.path);discovery.renderers+=plan.rendererRules[i].size();
 }
 return !discovery.roots.empty()&&!files.empty();
}
inline bool PrepareMeshStructureBaseline(Artifact& artifact,const Package& author){
 for(const auto& mesh:author.meshes)if(!author.resources.count(mesh.second.resource))return false;
 std::map<std::string,EiemResourceReloadPlan::Part> parts;size_t meshParts=0;
 for(const auto& rule:author.rules)if(!rule.second.skip)for(const auto& target:rule.second.targets)for(const auto& id:rule.second.parts){
  const auto& part=author.parts.at(id);const auto& mesh=author.meshes.at(part.mesh);auto marker=EiemDirectory::RuntimePartName(author,id,target.second);
  EiemResourceReloadPlan::Part expected{id,marker,part.material,mesh.vertices,mesh.bones,mesh.shapes};
  Require(parts.emplace(marker,std::move(expected)).second,"incremental Mesh marker collision");++meshParts;
 }
 artifact.manifest.package=author;auto& plan=artifact.manifest.plan;plan.author=PackageDigest(author);plan.contract=InputContractDigest(author);
 plan.parts=std::move(parts);plan.meshParts=meshParts;
 return true;
}
inline bool PrepareMeshStructureFrames(Artifact& artifact,const Package& author,const std::set<std::string>& changedResources,
 const EiemNativeInput::MeshSchemaIndex& schemas){
 for(const auto& mesh:author.meshes)if(!mesh.second.shapes.empty()){
  auto resource=author.resources.find(mesh.second.resource);if(resource==author.resources.end()||!schemas.count(resource->second->type))return false;
 }
 auto frames=EiemResourceRecipe::DescribeFrames(author,schemas);std::map<std::string,Bytes> prepared;
 for(const auto& frame:frames){auto old=artifact.preparedFrames.find(frame.resource);
  if(old!=artifact.preparedFrames.end()&&!changedResources.count(frame.resource)&&
     (frame.hash.empty()||EiemResourceRecipe::Digest(old->second)==frame.hash))prepared.emplace(frame.resource,old->second);
  else prepared.emplace(frame.resource,EiemResourceRecipe::ExpandFrame(frame,author.resources.at(frame.resource)->bytes));
 }
 for(const auto& row:artifact.preparedFrames)if(row.first.rfind("@bounds:",0)==0||row.first.rfind("@literal:",0)==0)prepared.emplace(row);
 artifact.manifest.plan.frameRecipes=std::move(frames);artifact.preparedFrames=std::move(prepared);
 return true;
}
inline void FreezeMeshDonors(std::vector<EiemResourceRecipe::Span>& spans,const std::set<std::string>& changed,
 const Package& old,const std::map<std::string,Bytes>& frames){
 for(auto& span:spans)if((span.kind==EiemResourceRecipe::Author||span.kind==EiemResourceRecipe::ExpandedFrame)&&changed.count(span.source)){
  const auto& bytes=span.kind==EiemResourceRecipe::Author?old.resources.at(span.source)->bytes:frames.at(span.source);
  Require(span.offset<=bytes.size()&&span.size<=bytes.size()-span.offset,"incremental Mesh donor range differs");
  span.literal.assign(bytes.begin()+span.offset,bytes.begin()+span.offset+span.size);span.kind=EiemResourceRecipe::Literal;span.source.clear();span.offset=0;
 }
}
inline bool RecompileMeshResources(Artifact& artifact,const Package& author,const EiemStaticSources::Selection& selection,
 const std::function<void(const char*,double)>& cost){
 const auto old=artifact.manifest.package;const auto changed=MeshResourceChanges(author,old);if(changed.empty())return false;
 auto& plan=artifact.manifest.plan;
 if(!plan.rulesOnly||!plan.spanRecipes||artifact.views.size()!=plan.originals.size()||artifact.verifiedSources.size()!=plan.originals.size())return false;
 for(size_t i=0;i<plan.originals.size();++i)if(!SameStamp(FileStamp(plan.originals[i].physical),artifact.verifiedSources[i]))return false;
 std::set<Identity> changedTargets;std::set<size_t> jobs,inventories,meshJobs;
 auto sourceIndex=[&](const Identity& identity){auto bundle=selection.assets.find(EiemStaticSources::Logical(identity.path));Require(bundle!=selection.assets.end(),"incremental Mesh source selection absent");
  const auto& entry=selection.inputs.at(bundle->second);
  for(size_t i=0;i<plan.originals.size();++i){const auto& source=plan.originals[i];if(source.physical==entry.physical&&source.offset==entry.offset&&source.size==entry.size&&source.hash==entry.revision)return i;}
  return SIZE_MAX;
 };
 for(const auto& target:author.targetIndex){inventories.insert(sourceIndex(target.first));
  for(const auto& part:author.rules.at(target.second).parts)if(changed.count(author.meshes.at(author.parts.at(part).mesh).resource))changedTargets.insert(target.first);
 }
 for(const auto& material:author.materials)inventories.insert(sourceIndex(material.second.source));
 if(inventories.count(SIZE_MAX))return false;
 for(const auto& target:changedTargets)meshJobs.insert(sourceIndex(target));
 jobs=meshJobs;
 for(size_t i=0;i<plan.rendererRules.size();++i)for(const auto& renderer:plan.rendererRules[i])if(changedTargets.count(renderer.mesh))jobs.insert(i);
 if(jobs.empty()||jobs.count(SIZE_MAX))return false;
 if(artifact.proofReader){for(size_t i=0;i<plan.inputs.size();++i)HydrateInput(artifact,i);artifact.proofReader={};}
 if(!artifact.retainedProofBlocks.empty()&&!artifact.proofBlocks)for(size_t i=0;i<plan.inputs.size();++i)HydrateInput(artifact,i);
 artifact.retainedProofBlocks.clear();artifact.retainedInputDirectories.clear();
 std::map<size_t,Bytes> inputs;std::map<size_t,EndfieldInput::Bundle> originals;
 auto load=[&](size_t i)->const EndfieldInput::Bundle&{if(!originals.count(i)){inputs.emplace(i,ReadSource({},plan.originals.at(i)));originals.emplace(i,EndfieldInput::Decode(inputs.at(i)));}return originals.at(i);};
 AssetIndex assets;MeshSchemaIndex schemas;MeshBoneHashIndex palettes;MaterialPreloadIndex preloads;
 for(auto i:inventories)AddSourceInventory(assets,schemas,palettes,preloads,load(i),author);
 std::vector<BuiltMeshPart> meshParts;
 for(const auto& target:author.targetIndex){const auto& asset=assets.at(target.first);const auto& rule=author.rules.at(target.second);if(rule.skip)continue;
  for(size_t n=0;n<rule.parts.size();++n){const auto& id=rule.parts[n];const auto& mesh=author.meshes.at(author.parts.at(id).mesh);
   meshParts.push_back({target.first,target.second,id,asset.cab,asset.pathId,n?AllocateMeshId(asset.cab,author,target.first,id):asset.pathId,mesh.vertices,mesh.indices,palettes.at({asset.cab,asset.pathId})});
  }
 }
 // Identical byte donors can belong to unrelated objects. Preserve their old
 // bytes before replacing author buffers, including expanded shape frames.
 for(size_t i=0;i<plan.inputs.size();++i){HydrateInput(artifact,i);for(auto& member:plan.memberRecipes[i])FreezeMeshDonors(member.spans,changed,old,artifact.preparedFrames);FreezeMeshDonors(artifact.views[i].spans,changed,old,artifact.preparedFrames);}
 auto frames=EiemResourceRecipe::DescribeFrames(author,schemas);auto preparedFrames=artifact.preparedFrames;
 for(const auto& frame:frames)if(changed.count(frame.resource))preparedFrames[frame.resource]=EiemResourceRecipe::ExpandFrame(frame,author.resources.at(frame.resource)->bytes);
 std::set<std::string> usedAuthors=changed,usedFrames;
 auto collect=[&](const auto& spans){for(const auto& span:spans){if(span.kind==EiemResourceRecipe::Author)usedAuthors.insert(span.source);if(span.kind==EiemResourceRecipe::ExpandedFrame)usedFrames.insert(span.source);}};
 for(auto i:jobs){collect(artifact.views[i].spans);for(const auto& member:plan.memberRecipes[i])collect(member.spans);}
 for(const auto& resource:changed)if(preparedFrames.count(resource))usedFrames.insert(resource);
 std::vector<EiemResourceRecipe::Buffer> authorBuffers;
 for(const auto& name:usedAuthors)authorBuffers.push_back({EiemResourceRecipe::Author,name,&author.resources.at(name)->bytes});
 for(const auto& name:usedFrames)authorBuffers.push_back({EiemResourceRecipe::ExpandedFrame,name,&preparedFrames.at(name)});
 EiemResourceRecipe::Dictionary authorSpans(std::move(authorBuffers));RendererBoundsCache bounds;
 std::vector<size_t> work(jobs.begin(),jobs.end());std::vector<EndfieldInput::Bundle> geometries(work.size());
 for(auto i:work)load(i);
 EiemOfflineCompile::ParallelInputs(work.size(),[&](size_t n){auto i=work[n];auto& geometry=geometries[n];geometry=originals.at(i);
  if(meshJobs.count(i))Measure(cost,"incremental-mesh-build",[&]{return BuildMeshInput(geometry,author);});
  if(!plan.rendererRules.at(i).empty())Measure(cost,"incremental-mesh-renderer",[&]{return BuildRendererInput(geometry,author,assets,meshParts,schemas,&bounds);});
 });
 size_t rebuiltInputs=0;
 for(size_t n=0;n<work.size();++n){auto i=work[n];const auto& original=originals.at(i);std::map<std::string,const Bytes*> originalMembers;std::vector<EiemResourceRecipe::Buffer> buffers;
  for(const auto& member:original.members){originalMembers.emplace(member.name,&member.bytes);buffers.push_back({EiemResourceRecipe::Original,member.name,&member.bytes});}
  EiemResourceRecipe::Dictionary originalSpans(std::move(buffers));auto edited=original;edited.members.clear();
  for(const auto& recipe:plan.memberRecipes[i])edited.members.push_back({recipe.name,recipe.flags,EiemResourceRecipe::Apply(recipe,originalMembers,old,artifact.preparedFrames)});
  const auto& geometry=geometries[n];bool modified=false;
  for(auto& member:edited.members){if(!Serialized(member.bytes))continue;
   auto generated=std::find_if(geometry.members.begin(),geometry.members.end(),[&](const auto& m){return m.name==member.name;});Require(generated!=geometry.members.end(),"incremental Mesh generated member absent");
   auto layout=ParseSerialized(member.bytes),fresh=ParseSerialized(generated->bytes);std::map<int64_t,Bytes> replacements;
   for(auto id:fresh.order){const auto& object=fresh.objects.at(id);const auto& type=fresh.types.at(object.type);if(type.classId!=43&&type.classId!=137)continue;
    auto current=layout.objects.find(id);Require(current!=layout.objects.end()&&layout.types.at(current->second.type).hash==type.hash,"incremental Mesh object contract differs");
    Bytes bytes;
    if(type.classId==43)bytes.assign(generated->bytes.begin()+object.offset,generated->bytes.begin()+object.offset+object.size);
    else{auto before=ObjectValue(member.bytes,layout,id),after=ObjectValue(generated->bytes,fresh,id);
     Child(type,before,"m_AABB")=Child(type,after,"m_AABB");bytes=EncodeValue(type,before);
    }
    if(bytes.size()==current->second.size&&std::equal(bytes.begin(),bytes.end(),member.bytes.begin()+current->second.offset))continue;
    Require(EncodeValue(type,DecodeValue(type,0,bytes))==bytes,"incremental Mesh object roundtrip differs");replacements.emplace(id,std::move(bytes));
   }
   if(!replacements.empty()){modified=true;member.bytes=BuildSerialized(member.bytes,layout,replacements,{});}
  }
  if(!modified)continue;
  ++rebuiltInputs;
  plan.namespaceRecipes[i].clear();for(const auto& member:edited.members)if(Serialized(member.bytes)){EiemLoadingGeneration::Recipe names;EiemLoadingGeneration::PrepareSerialized(member.bytes,plan.names,&names);plan.namespaceRecipes[i].push_back({member.name,Hash(member.bytes),std::move(names.patches)});}
  auto proof=DescribeInput(i,plan,true,original,edited,cost,&artifact);
  plan.memberRecipes[i]=DescribeMembers(edited,proof.members,originalSpans,authorSpans);
  for(size_t n=0;n<edited.members.size();++n)Require(EiemResourceRecipe::Apply(plan.memberRecipes[i][n],originalMembers,author,preparedFrames)==edited.members[n].bytes,"incremental Mesh member replay differs");
  auto startup=EndfieldInput::Encode(inputs.at(i),edited.members);plan.startupHashes[i]=Hash(startup);
  artifact.views[i]=EiemCompiledInputViewWriter::Build(i,plan,proof.members,inputs.at(i),original,edited,authorSpans);
  auto isolated=edited;for(auto& member:isolated.members){if(Serialized(member.bytes))member.bytes=EiemLoadingGeneration::PrepareSerialized(member.bytes,plan.names).bytes;member.name=plan.names.members.at(member.name);}plan.inputs[i].hash=Hash(EndfieldInput::Encode(startup,isolated.members));
  for(auto& member:proof.members)artifact.members[member.first]=std::move(member.second);
 }
 artifact.preparedFrames=std::move(preparedFrames);plan.frameRecipes=std::move(frames);artifact.manifest.package=author;plan.author=PackageDigest(author);plan.contract=InputContractDigest(author);
 if(cost){cost("incremental-mesh-inputs",double(rebuiltInputs));cost("reused-native-inputs",double(plan.inputs.size()-rebuiltInputs));}
 return true;
}
} // namespace EiemCompiledModWriter
