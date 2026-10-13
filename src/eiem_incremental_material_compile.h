#pragma once

// Parameter edits do not change the renderer graph or Texture references.
// Reuse its offline proofs and rebuild only the owning serialized inputs.
// This is an author-only adapter; no new game DLL code or format is needed.
namespace EiemCompiledModWriter {
inline std::vector<std::string> MaterialParameterChanges(const Package& author,const Package& old){
 if(author.materials.size()!=old.materials.size()||author.resources.size()!=old.resources.size())return {};
 auto comparable=author;std::set<std::string> allowed;std::vector<std::string> changed;
 for(const auto& row:author.materials){auto prior=old.materials.find(row.first);if(prior==old.materials.end())return {};
  const auto& m=row.second;const auto& p=prior->second;
  if(m.resource!=p.resource||m.source.path!=p.source.path||m.source.asset!=p.source.asset||m.textures!=p.textures)return {};
  allowed.insert(m.resource);
  if(ResourceContentHash(*author.resources.at(m.resource))!=ResourceContentHash(*old.resources.at(m.resource)))changed.push_back(row.first);
 }
 if(changed.empty())return {};
 for(const auto& row:author.resources){auto prior=old.resources.find(row.first);if(prior==old.resources.end())return {};
  if(ResourceContentHash(*row.second)==ResourceContentHash(*prior->second))continue;
  if(!allowed.count(row.first)||row.second->kind!=2||row.second->type!=prior->second->type)return {};
  const std::set<std::string> fields={"m_SavedProperties","m_CustomRenderQueue","m_ValidKeywords","m_InvalidKeywords","m_ShaderKeywords","stringTagMap"};
  for(const auto* resource:{row.second.get(),prior->second.get()})for(const auto& block:resource->blocks)if(!fields.count(block.first))return {};
  comparable.resources[row.first]=prior->second;
 }
 if(InputContractDigest(comparable)!=InputContractDigest(old))return {};
 return changed;
}
inline void FreezeMaterialDonors(std::vector<EiemResourceRecipe::Span>& spans,const std::set<std::string>& changed,const Package& old){
 for(auto& span:spans)if(span.kind==EiemResourceRecipe::Author&&changed.count(span.source)){
  const auto& bytes=old.resources.at(span.source)->bytes;
  Require(span.offset<=bytes.size()&&span.size<=bytes.size()-span.offset,"incremental material donor range differs");
  span.literal.assign(bytes.begin()+span.offset,bytes.begin()+span.offset+span.size);span.kind=EiemResourceRecipe::Literal;span.source.clear();span.offset=0;
 }
}
inline bool RecompileMaterialParameters(Artifact& artifact,const Package& author,const EiemStaticSources::Selection& selection,const std::function<void(const char*,double)>& cost){
 if(artifact.proofReader){for(size_t i=0;i<artifact.manifest.plan.inputs.size();++i)HydrateInput(artifact,i);artifact.proofReader={};}
 if(!artifact.retainedProofBlocks.empty()&&!artifact.proofBlocks)for(size_t i=0;i<artifact.manifest.plan.inputs.size();++i)HydrateInput(artifact,i);
 artifact.retainedProofBlocks.clear();
 artifact.retainedInputDirectories.clear();
 const auto old=artifact.manifest.package;auto changed=MaterialParameterChanges(author,old);if(changed.empty())return false;
 auto& plan=artifact.manifest.plan;
 if(!plan.rulesOnly||!plan.spanRecipes||artifact.views.size()!=plan.originals.size()||artifact.verifiedSources.size()!=plan.originals.size())return false;
 std::map<size_t,std::vector<std::string>> jobs;std::set<std::string> changedResources;
 for(const auto& id:changed){const auto& material=author.materials.at(id);auto bundle=selection.assets.find(EiemStaticSources::Logical(material.source.path));if(bundle==selection.assets.end())return false;
  const auto& entry=selection.inputs.at(bundle->second);size_t index=SIZE_MAX;
  for(size_t i=0;i<plan.originals.size();++i){const auto& source=plan.originals[i];if(source.physical==entry.physical&&source.offset==entry.offset&&source.size==entry.size&&source.hash==entry.revision){index=i;break;}}
  if(index==SIZE_MAX)return false; jobs[index].push_back(id);changedResources.insert(material.resource);
 }
 // A material field can also have been chosen as an identical byte donor for
 // an unrelated object. Freeze those spans before replacing its author bytes.
 for(size_t i=0;i<plan.inputs.size();++i){HydrateInput(artifact,i);for(auto& member:plan.memberRecipes[i])FreezeMaterialDonors(member.spans,changedResources,old);FreezeMaterialDonors(artifact.views[i].spans,changedResources,old);}
 for(size_t i=0;i<plan.originals.size();++i)if(!SameStamp(FileStamp(plan.originals[i].physical),artifact.verifiedSources[i]))return false;
 // The previous input recipes already describe their author dependencies.
 // Parameter edits cannot introduce geometry/texture dependencies: index only
 // this affected working set, rather than every Mod texture and Mesh again.
 std::set<std::string> usedAuthors=changedResources,usedFrames;
 auto collect=[&](const auto& spans){for(const auto& span:spans){if(span.kind==EiemResourceRecipe::Author)usedAuthors.insert(span.source);if(span.kind==EiemResourceRecipe::ExpandedFrame)usedFrames.insert(span.source);}};
 for(const auto& job:jobs){collect(artifact.views[job.first].spans);for(const auto& member:plan.memberRecipes[job.first])collect(member.spans);}
 std::vector<EiemResourceRecipe::Buffer> authorBuffers;
 for(const auto& name:usedAuthors)authorBuffers.push_back({EiemResourceRecipe::Author,name,&author.resources.at(name)->bytes});
 for(const auto& name:usedFrames)authorBuffers.push_back({EiemResourceRecipe::ExpandedFrame,name,&artifact.preparedFrames.at(name)});
 EiemResourceRecipe::Dictionary authorSpans(std::move(authorBuffers));
 for(const auto& job:jobs){const auto i=job.first;auto input=ReadSource({},plan.originals[i]);auto original=EndfieldInput::Decode(input);
  std::map<std::string,const Bytes*> originalMembers;std::vector<EiemResourceRecipe::Buffer> buffers;
  for(const auto& member:original.members){originalMembers.emplace(member.name,&member.bytes);buffers.push_back({EiemResourceRecipe::Original,member.name,&member.bytes});}
  EiemResourceRecipe::Dictionary originalSpans(std::move(buffers));auto edited=original;edited.members.clear();
  for(const auto& recipe:plan.memberRecipes[i])edited.members.push_back({recipe.name,recipe.flags,EiemResourceRecipe::Apply(recipe,originalMembers,old,artifact.preparedFrames)});
  std::set<std::string> found;
  for(auto& member:edited.members){if(!Serialized(member.bytes))continue;
   auto source=originalMembers.find(member.name);Require(source!=originalMembers.end(),"incremental material original member absent");
   auto pristineLayout=ParseSerialized(*source->second);auto layout=ParseSerialized(member.bytes);auto assets=SerializedAssets(*source->second,member.name,pristineLayout);std::map<int64_t,Bytes> replacements;
   for(const auto& id:job.second){const auto& material=author.materials.at(id);
    auto asset=std::find_if(assets.begin(),assets.end(),[&](const auto& a){return a.classId==21&&a.path==material.source.path&&a.asset==material.source.asset;});if(asset==assets.end())continue;
    auto cloneId=AllocateMaterialId(member.name,author,id);auto clone=layout.objects.find(cloneId);Require(clone!=layout.objects.end(),"incremental material clone absent");
    const auto& type=layout.types.at(clone->second.type);const auto& resource=*author.resources.at(material.resource);Require(type.hash==resource.type,"incremental material schema differs");
    auto current=ObjectValue(member.bytes,layout,cloneId);auto value=ObjectValue(*source->second,pristineLayout,asset->pathId);
    for(const auto& block:resource.blocks){auto& field=Child(type,value,block.first);Bytes bytes(resource.bytes.begin()+block.second.offset,resource.bytes.begin()+block.second.offset+block.second.size);field=DecodeValue(type,field.node,bytes);Require(EncodeValue(type,field)==bytes,"incremental material field roundtrip differs");}
    auto& environments=ArrayValue(type,Child(type,Child(type,value,"m_SavedProperties"),"m_TexEnvs"));
    const auto& oldEnvironments=ArrayValue(type,Child(type,Child(type,current,"m_SavedProperties"),"m_TexEnvs"));
    for(auto& pair:environments.children){const auto& key=Child(type,pair,"first");auto property=Text(key.raw.data(),key.raw.size());if(!material.textures.count(property))continue;
     auto prior=std::find_if(oldEnvironments.children.begin(),oldEnvironments.children.end(),[&](const auto& p){const auto& k=Child(type,p,"first");return Text(k.raw.data(),k.raw.size())==property;});Require(prior!=oldEnvironments.children.end(),"incremental material authored texture slot absent");
     Child(type,Child(type,pair,"second"),"m_Texture")=Child(type,Child(type,*prior,"second"),"m_Texture");
    }
    Child(type,value,"m_Name")=Child(type,current,"m_Name");auto bytes=EncodeValue(type,value);Require(EncodeValue(type,DecodeValue(type,0,bytes))==bytes,"incremental material object roundtrip differs");replacements.emplace(cloneId,std::move(bytes));Require(found.insert(id).second,"incremental material source ambiguous");
   }
   if(!replacements.empty())member.bytes=BuildSerialized(member.bytes,layout,replacements,{});
  }
  Require(found.size()==job.second.size(),"incremental material source coverage differs");
  plan.namespaceRecipes[i].clear();for(const auto& member:edited.members)if(Serialized(member.bytes)){EiemLoadingGeneration::Recipe names;EiemLoadingGeneration::PrepareSerialized(member.bytes,plan.names,&names);plan.namespaceRecipes[i].push_back({member.name,Hash(member.bytes),std::move(names.patches)});}
  auto proof=DescribeInput(i,plan,true,original,edited,cost,&artifact);
  plan.memberRecipes[i]=DescribeMembers(edited,proof.members,originalSpans,authorSpans);
  for(size_t n=0;n<edited.members.size();++n)Require(EiemResourceRecipe::Apply(plan.memberRecipes[i][n],originalMembers,author,artifact.preparedFrames)==edited.members[n].bytes,"incremental material member replay differs");
  auto startup=EndfieldInput::Encode(input,edited.members);plan.startupHashes[i]=Hash(startup);
  artifact.views[i]=EiemCompiledInputViewWriter::Build(i,plan,proof.members,input,original,edited,authorSpans);
  auto isolated=edited;for(auto& member:isolated.members){if(Serialized(member.bytes))member.bytes=EiemLoadingGeneration::PrepareSerialized(member.bytes,plan.names).bytes;member.name=plan.names.members.at(member.name);}plan.inputs[i].hash=Hash(EndfieldInput::Encode(startup,isolated.members));
  for(auto& member:proof.members)artifact.members[member.first]=std::move(member.second);
 }
 artifact.manifest.package=author;plan.author=PackageDigest(author);plan.contract=InputContractDigest(author);
 if(cost){cost("incremental-material-inputs",double(jobs.size()));cost("reused-native-inputs",double(plan.inputs.size()-jobs.size()));}
 return true;
}
} // namespace EiemCompiledModWriter
