#pragma once
#include "eiem_compiled_mod.h"

// Each native source and its author-only proof is an independent block. The
// common block contains small routing/author metadata, never native views or
// expanded author frames. Dependencies are stored by source identity, so
// appending/reordering sources does not rewrite previous source records.
namespace EiemCompiledSourceRecords {
using namespace EiemCompiledMod;
inline std::map<std::string,size_t> MemberInputs(const Artifact& a){
 std::map<std::string,size_t> result;const auto& p=a.manifest.plan;
 for(size_t i=0;i<p.inputs.size();++i){
  if(a.inputMembers.size()==p.inputs.size()){for(const auto& name:a.inputMembers[i])Require(result.emplace(name,i).second,"source-record member owner repeated");}
  else if(a.retainedInputDirectories.count(i)){for(const auto& name:a.retainedInputDirectories.at(i).members)Require(result.emplace(name,i).second,"retained source-record member owner repeated");}
  else {HydrateInput(a,i);for(const auto& recipe:p.memberRecipes.at(i))Require(result.emplace(recipe.name,i).second,"source-record CAB owner repeated");}
 }return result;
}
inline InputProofDirectory Directory(const Artifact& a,size_t i,const std::map<std::string,size_t>& owners){
 const auto& p=a.manifest.plan;if(a.inputMembers.size()==p.inputs.size()&&a.addedBundles.size()==p.inputs.size())return {a.inputMembers.at(i),a.addedBundles.at(i),p.inputDependencies.at(i)};
 if(a.retainedInputDirectories.count(i))return a.retainedInputDirectories.at(i);
 HydrateInput(a,i);InputProofDirectory result;std::set<size_t> dependencies;
 auto add=[&](const std::string& name){auto found=owners.find(name);if(found!=owners.end()&&found->second!=i)dependencies.insert(found->second);};
 for(const auto& external:p.inputs.at(i).externals)add(external);
 for(const auto& recipe:p.memberRecipes.at(i)){auto found=a.members.find(recipe.name);if(found==a.members.end())continue;result.members.insert(recipe.name);const auto& proof=found->second;
  Require(ExternalPrefix(proof.before.externals,proof.after.externals),"source-record external prefix changed");for(const auto& external:proof.after.externals)add(external);
  for(size_t j=proof.before.externals.size();j<proof.after.externals.size();++j){const auto& external=proof.after.externals[j];auto owner=owners.find(external);if(owner==owners.end()){auto shared=p.sharedBundles.find(external);Require(shared!=p.sharedBundles.end(),"source-record added dependency absent: "+external);result.addedBundles.insert(shared->second);}else if(owner->second!=i)result.addedBundles.insert(p.inputs.at(owner->second).bundle);}
 }result.dependencies.assign(dependencies.begin(),dependencies.end());return result;
}
inline Bytes Proof(const Artifact& a,size_t i,const InputProofDirectory& directory){
 if(a.retainedProofBlocks.count(i))return a.retainedProofBlocks.at(i);
 if(a.proofBlocks)return a.proofBlocks->at(i);
 if(a.proofReader&&(a.inputMembers.size()==a.manifest.plan.inputs.size()||a.retainedInputDirectories.count(i)))return a.proofReader(i);
 HydrateInput(a,i);Writer w;SchemaWriter schemas;w.N(directory.members.size());
 for(const auto& name:directory.members){const auto& p=a.members.at(name);w.S(name);WriteLayout(w,p.before,schemas);WriteLayout(w,p.after,schemas);w.N(p.streams.size());
  for(const auto& site:p.streams){w.N(uint64_t(site.object));w.N(site.offset);w.N(site.width);w.S(site.member);w.N(site.value);}w.N(p.pristineNamespace.size());for(const auto& patch:p.pristineNamespace){w.N(patch.offset);w.S(patch.before);}}
 const auto& p=a.manifest.plan;w.N(p.namespaceRecipes.at(i).size());for(const auto& recipe:p.namespaceRecipes.at(i)){w.S(recipe.member);w.S(recipe.hash);w.N(recipe.patches.size());for(const auto& patch:recipe.patches){w.N(patch.offset);w.S(patch.before);w.S(patch.after);}}
 w.N(p.memberRecipes.at(i).size());for(const auto& member:p.memberRecipes.at(i))WriteCompiledMember(w,member);return Pack(w.b);
}
inline void Frame(Writer& w,const EiemResourceRecipe::Frame& frame){w.S(frame.resource);w.S(frame.hash);w.N(frame.offset);w.N(frame.size);w.N(frame.count);for(auto n:frame.stride)w.N(n);for(auto n:frame.start)w.N(n);}
inline Bytes Common(const Artifact& a){
 const auto& m=a.manifest;const auto& p=m.plan;Require(!m.composed&&m.rootAssembly&&p.rulesOnly&&p.spanRecipes,"source records require independent compiled Mod");
 Writer w;w.S("EFF_COMPILED_SOURCE_COMMON/author+metadata+identity-edges/le64");w.S(m.baseline);w.S(p.key);w.S(p.author);w.S(p.contract);WriteAuthor(w,m.package,true);
 w.Strings(p.names.members);w.Strings(p.names.bundles);w.Set(p.shared);
 w.N(p.closure.size());for(auto i:p.closure)w.S(p.originals.at(i).file);
 w.N(p.parts.size());for(const auto& row:p.parts){const auto& part=row.second;w.S(row.first);w.S(part.id);w.S(part.marker);w.S(part.material);w.N(part.vertices);w.Strings(part.bones);w.Strings(part.shapes);}
 w.N(p.materials.size());for(const auto& row:p.materials){w.S(row.first);w.S(row.second.name);w.N(row.second.slots.size());for(const auto& slot:row.second.slots){w.S(slot.property);w.B(slot.nonnull);w.B(slot.local);w.S(slot.texture.name);w.N(slot.texture.width);w.N(slot.texture.height);w.N(slot.texture.mips);}}
 for(auto n:{p.meshParts,p.rendererParts,p.textures,p.materialMatches,p.opaque,p.rendererBoundsHits,p.rendererBoundsMisses})w.N(n);
 w.N(p.frameRecipes.size());for(const auto& frame:p.frameRecipes)Frame(w,frame);return std::move(w.b);
}
inline Artifact ReadCommon(const Bytes& bytes,const std::filesystem::path& mod,std::vector<std::string>& closure){
 EiemOfflinePackage::Reader r{bytes};Require(r.S()=="EFF_COMPILED_SOURCE_COMMON/author+metadata+identity-edges/le64","source-record common structure differs");Artifact a;auto& m=a.manifest;auto& p=m.plan;
 m.baseline=r.S();p.key=r.S();p.author=r.S();p.contract=r.S();Require(DigestText(m.baseline)&&DigestText(p.key)&&DigestText(p.author)&&DigestText(p.contract),"source-record common digest invalid");
 m.package=ReadAuthor(r,mod.parent_path(),true,mod);m.rootAssembly=true;p.rulesOnly=p.spanRecipes=true;p.directory=mod;
 p.names.members=r.StringsMap();p.names.bundles=r.StringsMap();p.shared=r.Set();closure=r.Strings();
 auto n=r.Count();for(size_t i=0;i<n;++i){auto key=r.S();EiemResourceReloadPlan::Part part;part.id=r.S();part.marker=r.S();part.material=r.S();part.vertices=r.U();part.bones=r.Strings();part.shapes=r.Strings();
  Require(key==part.marker&&m.package.parts.count(part.id)&&m.package.materials.count(part.material),"source-record Part declaration differs");const auto& authored=m.package.parts.at(part.id);const auto& mesh=m.package.meshes.at(authored.mesh);
  Require(part.material==authored.material&&part.vertices==mesh.vertices&&part.bones==mesh.bones&&part.shapes==mesh.shapes&&p.parts.emplace(key,std::move(part)).second,"source-record Part author payload differs");}
 n=r.Count();for(size_t i=0;i<n;++i){auto key=r.S();EiemResourceReloadPlan::Material material;material.name=r.S();auto count=r.Count();std::set<std::string> slots;
  for(size_t j=0;j<count;++j){TextureSlot slot;slot.property=r.S();slot.nonnull=r.B();slot.local=r.B();slot.texture.name=r.S();slot.texture.width=int(r.U());slot.texture.height=int(r.U());slot.texture.mips=int(r.U());
   Require(slots.insert(slot.property).second&&(!slot.local||(slot.nonnull&&slot.texture.width>0&&slot.texture.height>0&&slot.texture.mips>0)),"source-record Texture slot invalid");material.slots.push_back(std::move(slot));}
  Require(m.package.materials.count(key)&&p.materials.emplace(key,std::move(material)).second,"source-record Material declaration invalid");}
 p.meshParts=r.Size();p.rendererParts=r.Size();p.textures=r.Size();p.materialMatches=r.Size();p.opaque=r.Size();p.rendererBoundsHits=r.Size();p.rendererBoundsMisses=r.Size();
 n=r.Count();std::set<std::string> frames;for(size_t i=0;i<n;++i){EiemResourceRecipe::Frame f;f.resource=r.S();f.hash=r.S();f.offset=r.Size();f.size=r.Size();f.count=r.Size();for(auto& v:f.stride)v=r.Size();for(auto& v:f.start)v=r.Size();
  Require(m.package.resources.count(f.resource)&&m.package.resources.at(f.resource)->kind==1&&DigestText(f.hash)&&f.count&&f.stride[0]==16&&f.start[0]==0&&frames.insert(f.resource).second,"source-record author frame invalid");
  const auto& source=m.package.resources.at(f.resource)->bytes;Require(f.offset<=source.size()&&f.size<=source.size()-f.offset,"source-record author frame extent invalid");for(size_t j=0;j<4;++j)Require(f.start[j]<=f.size&&MultiplySize(f.count,f.stride[j])<=f.size-f.start[j],"source-record frame stride invalid");p.frameRecipes.push_back(std::move(f));}
 Require(r.p==bytes.size()&&p.materialMatches==m.package.materials.size()&&p.textures==m.package.textures.size()&&PackageDigest(m.package)==p.author&&InputContractDigest(m.package)==p.contract,"source-record common author/coverage differs");return a;
}
inline Bytes Source(const Artifact& a,size_t i,const std::filesystem::path& game,const InputProofDirectory& directory){
 const auto& p=a.manifest.plan;const auto& original=p.originals.at(i);const auto& in=p.inputs.at(i);
 auto relative=original.physical.lexically_relative(game);Require(!relative.empty()&&!relative.is_absolute()&&relative.u8string().find("..")==std::string::npos,"source record escapes game");
 auto stamp=FileStamp(original.physical);if(a.verifiedSources.size()!=p.inputs.size()||!SameStamp(stamp,a.verifiedSources.at(i))){ReadSource({},original);Require(SameStamp(stamp,FileStamp(original.physical)),"source record changed while compiling");}
 Writer w;w.S("EFF_COMPILED_SOURCE_RECORD/range+view+roots+identity-edges/le64");w.S(original.file);w.S(original.hash);w.S(relative.generic_u8string());w.N(original.offset);w.N(original.size);w.N(stamp.bytes);w.N(stamp.written);
 w.S(in.file);w.S(in.hash);w.S(in.bundle);w.Set(in.externals);w.S(p.startupHashes.at(i));w.Set(directory.members);w.Set(directory.addedBundles);
 w.N(directory.dependencies.size());for(auto index:directory.dependencies)w.S(p.originals.at(index).file);
 w.N(p.rendererRules.at(i).size());for(const auto& rule:p.rendererRules.at(i)){w.S(rule.cab);w.N(uint64_t(rule.id));w.Id(rule.mesh);}
 Artifact local;local.manifest.package=a.manifest.package;local.manifest.plan.parts=p.parts;local.manifest.plan.originals={original};
 for(const auto& root:p.roots)if(root.input==i){auto copy=root;copy.input=0;local.manifest.plan.roots.push_back(copy);}
 for(const auto& root:a.pristine)if(root.input==i){auto copy=root;copy.input=0;local.pristine.push_back(copy);}
 w.N(local.manifest.plan.roots.size());for(const auto& root:local.manifest.plan.roots){w.S(root.path);w.S(root.cab);w.N(uint64_t(root.source));w.N(root.markers.size());for(const auto& marker:root.markers){w.S(marker.first);w.N(marker.second);}}
 auto assemblies=WritePreparedAssemblies(w,local);w.N(local.pristine.size());for(size_t j=0;j<local.pristine.size();++j){const auto& root=local.pristine[j];w.S(root.path);w.S(root.cab);w.N(uint64_t(root.source));w.N(assemblies.pristine.at(j));}
 local.views={a.views.at(i)};WriteNativeViews(w,local);return Pack(w.b);
}
inline void ReadSourceRecord(const Bytes& packed,Artifact& a,const std::filesystem::path& game,const std::map<std::string,size_t>& order,size_t i){
 auto bytes=Unpack(packed);EiemOfflinePackage::Reader r{bytes};Require(r.S()=="EFF_COMPILED_SOURCE_RECORD/range+view+roots+identity-edges/le64","source-record structure differs");auto& p=a.manifest.plan;
 EiemResourceReloadPlan::Source source;source.file=r.S();source.hash=r.S();auto path=std::filesystem::u8path(r.S());source.offset=r.N();source.size=r.N();InputFileStamp stamp{r.N(),r.N()};
 Require(order.at(source.file)==i&&DigestText(source.hash)&&!path.empty()&&!path.is_absolute()&&path.u8string().find("..")==std::string::npos&&source.size&&source.offset<=stamp.bytes&&source.size<=stamp.bytes-source.offset,"source-record range/identity invalid");
 source.physical=std::filesystem::weakly_canonical(game/path);Require(source.physical.lexically_relative(game).u8string().rfind("..",0)!=0,"source-record symlink escapes game");
 Input in;in.file=r.S();in.hash=r.S();in.bundle=r.S();in.externals=r.Set();in.path=a.manifest.package.root/in.file;auto startup=r.S();
 Require(in.file==source.file&&DigestText(in.hash)&&DigestText(startup)&&std::filesystem::u8path(in.file)==std::filesystem::u8path(in.file).filename(),"source-record input identity invalid");
 auto members=r.Set(),added=r.Set();for(const auto& member:members)Require(p.names.members.count(member),"source-record member absent from routing");auto n=r.Count();std::vector<size_t> dependencies;std::set<size_t> unique;
 for(size_t j=0;j<n;++j){auto at=order.at(r.S());Require(at!=i&&unique.insert(at).second,"source-record dependency repeats/self-references");dependencies.push_back(at);}
 n=r.Count();std::vector<RendererRule> rules;std::set<std::pair<std::string,int64_t>> renderers;for(size_t j=0;j<n;++j){RendererRule rule;rule.cab=r.S();rule.id=int64_t(r.N());rule.mesh=r.Id();Require(rule.id&&a.manifest.package.targetIndex.count(rule.mesh)&&renderers.emplace(rule.cab,rule.id).second,"source-record Renderer rule invalid");rules.push_back(std::move(rule));}
 Artifact local;local.manifest.package=a.manifest.package;local.manifest.plan.parts=p.parts;local.manifest.plan.originals={source};n=r.Count();std::set<std::string> rootPaths;
 for(size_t j=0;j<n;++j){Root root;root.path=r.S();root.cab=r.S();root.source=int64_t(r.N());Require(root.path.rfind("assets/",0)==0&&root.path.find("..")==root.path.npos&&rootPaths.insert(root.path).second&&EiemLoadingGeneration::Cab(root.cab)&&root.source,"source-record root identity invalid");
  auto count=r.Count();for(size_t k=0;k<count;++k){auto marker=r.S();auto copies=r.U();Require(copies&&root.markers.emplace(marker,copies).second,"source-record marker invalid");}local.manifest.plan.roots.push_back(std::move(root));}
 auto assemblies=ReadPreparedAssemblies(r,local.manifest.plan);n=r.Count();std::set<std::string> pristine;
 for(size_t j=0;j<n;++j){Root root;root.path=r.S();root.cab=r.S();root.source=int64_t(r.N());root.preparedAssembly=assemblies.at(r.Size());Require(rootPaths.count(root.path)&&pristine.insert(root.path).second&&EiemLoadingGeneration::Cab(root.cab)&&root.source,"source-record pristine root invalid");local.pristine.push_back(std::move(root));}
 ReadNativeViews(r,local);Require(r.p==bytes.size(),"source-record suffix differs: "+source.file);Require(pristine.size()==rootPaths.size(),"source-record pristine root coverage differs: "+source.file);Require(local.preparedFrames.empty(),"source-record embeds shared frames");
 p.originals.push_back(std::move(source));p.inputs.push_back(std::move(in));p.startupHashes.push_back(startup);p.rendererRules.push_back(std::move(rules));p.memberRecipes.emplace_back();p.namespaceRecipes.emplace_back();p.inputDependencies.push_back(std::move(dependencies));a.inputMembers.push_back(std::move(members));a.addedBundles.push_back(std::move(added));a.verifiedSources.push_back(stamp);a.views.push_back(std::move(local.views.at(0)));
 for(auto root:local.manifest.plan.roots){root.input=i;p.roots.push_back(std::move(root));}for(auto root:local.pristine){root.input=i;a.pristine.push_back(std::move(root));}
}
} // namespace EiemCompiledSourceRecords
