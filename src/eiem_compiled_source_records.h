#pragma once
#include "eiem_compiled_mod.h"
#include <unordered_map>

// Each native source and its author-only proof is an independent block. The
// common block contains small routing/author metadata, never native views or
// expanded author frames. Dependencies are stored by source identity, so
// appending/reordering sources does not rewrite previous source records.
namespace EiemCompiledSourceRecords {
using namespace EiemCompiledMod;
inline bool SameSchema(const Type& left,const Type& right){
 if(left.classId!=right.classId||left.hash!=right.hash||left.nodes.size()!=right.nodes.size())return false;
 for(size_t i=0;i<left.nodes.size();++i){const auto& a=left.nodes[i];const auto& b=right.nodes[i];if(a.type!=b.type||a.name!=b.name||a.align!=b.align||a.children!=b.children)return false;}return true;
}
using SchemaKey=std::pair<int,std::array<unsigned char,16>>;
inline SchemaKey KeyOf(const Type& type){return {type.classId,type.hash};}
struct SharedSchemaIndex {
 const std::vector<Type>* types=nullptr;std::map<SchemaKey,std::vector<size_t>> candidates;std::unordered_map<const Type*,size_t> validated;
 size_t Find(const Type& type)const{
  auto known=validated.find(&type);if(known!=validated.end())return known->second;
  auto found=candidates.find(KeyOf(type));Require(found!=candidates.end(),"source Type absent from shared schema dictionary");
  for(auto id:found->second)if(SameSchema(types->at(id),type))return id;throw std::runtime_error("source Type differs from shared schema dictionary");
 }
};
inline SharedSchemaIndex PrepareSchemaTypes(const std::vector<Type>& types){
 SharedSchemaIndex result;result.types=&types;
 for(size_t i=0;i<types.size();++i){auto& candidates=result.candidates[KeyOf(types[i])];for(auto existing:candidates)Require(!SameSchema(types[existing],types[i]),"shared schema dictionary contains duplicate types");candidates.push_back(i);}return result;
}
inline SharedSchemaIndex PrepareSchemas(const Artifact& a){
 auto result=PrepareSchemaTypes(a.sharedSchemas);if(a.sharedSchemas.empty())return result;
 for(const auto& member:a.members)for(const auto* layout:{&member.second.before,&member.second.after})for(const auto& type:layout->types)result.validated.emplace(&type,result.Find(type));return result;
}
inline void WriteSharedLayout(Writer& w,const SerializedLayout& layout,const SharedSchemaIndex& schemas){
 for(auto n:{layout.metadataSize,layout.dataOffset,layout.countOffset,layout.tableStart,layout.tableEnd})w.N(n);
 w.N(layout.types.size());for(const auto& type:layout.types)w.N(schemas.Find(type));
 w.N(layout.order.size());for(auto id:layout.order){const auto& object=layout.objects.at(id);w.N(uint64_t(id));w.N(object.offset);w.N(object.size);w.N(object.type);}w.Strings(layout.externals);
}
inline void WriteDeltaLayout(Writer& w,const SerializedLayout& before,const SerializedLayout& after,const SharedSchemaIndex& schemas){
 WriteLayoutDeltaValue(w,before.metadataSize,after.metadataSize);WriteLayoutDeltaValue(w,before.dataOffset,after.dataOffset);WriteLayoutDeltaValue(w,before.countOffset,after.countOffset);WriteLayoutDeltaValue(w,before.tableStart,after.tableStart);WriteLayoutDeltaValue(w,before.tableEnd,after.tableEnd);
 size_t prefix=0;while(prefix<before.types.size()&&prefix<after.types.size()&&schemas.Find(before.types[prefix])==schemas.Find(after.types[prefix]))++prefix;
 w.N(prefix);w.N(after.types.size()-prefix);for(size_t i=prefix;i<after.types.size();++i)w.N(schemas.Find(after.types[i]));
 std::unordered_map<int64_t,size_t> ordinals;ordinals.reserve(before.order.size());for(size_t i=0;i<before.order.size();++i)Require(ordinals.emplace(before.order[i],i+1).second,"compiled delta original order repeats an object");
 w.N(after.order.size());for(auto id:after.order){const auto& object=after.objects.at(id);auto old=before.objects.find(id);
  if(old!=before.objects.end()){w.N(ordinals.at(id));WriteLayoutDeltaValue(w,old->second.offset,object.offset);WriteLayoutDeltaValue(w,old->second.size,object.size);WriteLayoutDeltaValue(w,old->second.type,object.type);}
  else {w.N(0);w.N(uint64_t(id));w.N(object.offset);w.N(object.size);w.N(object.type);}
 }
 Require(ExternalPrefix(before.externals,after.externals),"compiled delta external prefix differs");w.N(after.externals.size()-before.externals.size());for(size_t i=before.externals.size();i<after.externals.size();++i)w.S(after.externals[i]);
}
inline Bytes EncodeSchemas(const std::vector<Type>& types){
 PrepareSchemaTypes(types);Writer w;w.S("EFF_COMPILED_SCHEMA_DICTIONARY/le64");w.N(types.size());for(const auto& type:types){auto bytes=EncodeSchema(type);w.Blob(bytes);}return Pack(w.b);
}
inline std::vector<Type> DecodeSchemas(const Bytes& bytes){
 auto plain=Unpack(bytes);EiemOfflinePackage::Reader r{plain};Require(r.S()=="EFF_COMPILED_SCHEMA_DICTIONARY/le64","shared schema dictionary structure differs");auto count=r.Count();std::vector<Type> result;std::set<Bytes> unique;
 for(size_t i=0;i<count;++i){auto encoded=r.Blob();EiemOfflinePackage::Reader schema{encoded};auto type=DecodeSchema(schema);Require(schema.p==encoded.size()&&unique.insert(encoded).second,"shared schema dictionary type repeated/suffix invalid");result.push_back(std::move(type));}
 Require(r.p==plain.size(),"shared schema dictionary suffix differs");return result;
}
inline void FinalizeSchemas(Artifact& a){
 // IDs of an existing dictionary never move; old immutable source proofs can
 // keep referring to the same prefix when a new source introduces a Type.
 for(size_t i=0;i<a.manifest.plan.inputs.size();++i)HydrateInput(a,i);
 auto previous=a.sharedSchemas.size();auto schemas=PrepareSchemaTypes(a.sharedSchemas);
 for(const auto& member:a.members)for(const auto* layout:{&member.second.before,&member.second.after})for(const auto& type:layout->types){auto& candidates=schemas.candidates[KeyOf(type)];bool known=false;
  for(auto id:candidates)if(SameSchema(a.sharedSchemas[id],type)){known=true;break;}if(!known){candidates.push_back(a.sharedSchemas.size());a.sharedSchemas.push_back(type);}
 }
 if(!previous&&!a.sharedSchemas.empty()){
  // Independent v1 proofs embed their own schema declarations. They cannot be
  // reused after readers start with a populated shared dictionary.
  a.proofBlocks.reset();a.proofReader={};a.retainedProofBlocks.clear();a.retainedInputDirectories.clear();
 }
}
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
inline Bytes Proof(const Artifact& a,size_t i,const InputProofDirectory& directory,const SharedSchemaIndex* shared=nullptr){
 if(a.retainedProofBlocks.count(i))return a.retainedProofBlocks.at(i);
 if(a.proofBlocks)return a.proofBlocks->at(i);
 if(a.proofReader&&(a.inputMembers.size()==a.manifest.plan.inputs.size()||a.retainedInputDirectories.count(i)))return a.proofReader(i);
 HydrateInput(a,i);std::optional<SharedSchemaIndex> prepared;if(!a.sharedSchemas.empty()&&!shared){prepared=PrepareSchemas(a);shared=&*prepared;}
 Writer w;SchemaWriter local;if(!a.sharedSchemas.empty())w.S(sharedProofStructure);w.N(directory.members.size());
 for(const auto& name:directory.members){const auto& p=a.members.at(name);w.S(name);if(!a.sharedSchemas.empty()){WriteSharedLayout(w,p.before,*shared);WriteDeltaLayout(w,p.before,p.after,*shared);}else {WriteLayout(w,p.before,local);WriteLayout(w,p.after,local);}w.N(p.streams.size());
  for(const auto& site:p.streams){w.N(uint64_t(site.object));w.N(site.offset);w.N(site.width);w.S(site.member);w.N(site.value);}w.N(p.pristineNamespace.size());for(const auto& patch:p.pristineNamespace){w.N(patch.offset);w.S(patch.before);}}
 const auto& p=a.manifest.plan;w.N(p.namespaceRecipes.at(i).size());for(const auto& recipe:p.namespaceRecipes.at(i)){w.S(recipe.member);w.S(recipe.hash);w.N(recipe.patches.size());for(const auto& patch:recipe.patches){w.N(patch.offset);w.S(patch.before);w.S(patch.after);}}
 w.N(p.memberRecipes.at(i).size());for(const auto& member:p.memberRecipes.at(i))WriteCompiledMember(w,member);return Pack(w.b);
}
inline void Frame(Writer& w,const EiemResourceRecipe::Frame& frame){w.S(frame.resource);w.S(frame.hash);w.N(frame.offset);w.N(frame.size);w.N(frame.count);for(auto n:frame.stride)w.N(n);for(auto n:frame.start)w.N(n);}
// Early /1 and /2 snapshots stored this record without compression. Keep that
// reader while new snapshots retain the old packet's compact metadata size.
inline Bytes CommonBytes(const Bytes& bytes){EiemOfflinePackage::Reader r{bytes};return r.S()==compressionStructure?Unpack(bytes):bytes;}
inline Bytes Common(const Artifact& a){
 const auto& m=a.manifest;const auto& p=m.plan;Require(!m.composed&&m.rootAssembly&&p.rulesOnly&&p.spanRecipes,"source records require independent compiled Mod");
 Writer w;w.S("EFF_COMPILED_SOURCE_COMMON/author+metadata+identity-edges+key-label/le64");w.S(m.baseline);w.S(p.key);w.S(p.author);w.S(p.contract);WriteAuthor(w,m.package,true);
 w.Strings(p.names.members);w.Strings(p.names.bundles);w.Set(p.shared);
 w.N(p.closure.size());for(auto i:p.closure)w.S(p.originals.at(i).file);
 w.N(p.parts.size());for(const auto& row:p.parts){const auto& part=row.second;w.S(row.first);w.S(part.id);w.S(part.marker);w.S(part.material);w.N(part.vertices);w.Strings(part.bones);w.Strings(part.shapes);}
 w.N(p.materials.size());for(const auto& row:p.materials){w.S(row.first);w.S(row.second.name);w.N(row.second.slots.size());for(const auto& slot:row.second.slots){w.S(slot.property);w.B(slot.nonnull);w.B(slot.local);w.S(slot.texture.name);w.N(slot.texture.width);w.N(slot.texture.height);w.N(slot.texture.mips);}}
 for(auto n:{p.meshParts,p.rendererParts,p.textures,p.materialMatches,p.opaque,p.rendererBoundsHits,p.rendererBoundsMisses})w.N(n);
 w.N(p.frameRecipes.size());for(const auto& frame:p.frameRecipes)Frame(w,frame);return Pack(w.b);
}
inline Artifact ReadCommon(const Bytes& encoded,const std::filesystem::path& mod,std::vector<std::string>& closure,const Package* verifiedAuthor=nullptr,bool mapResourceBytes=false){
 auto bytes=CommonBytes(encoded);
 EiemOfflinePackage::Reader r{bytes};r.mapResourceBytes=mapResourceBytes;auto structure=r.S();const bool bindingLabels=structure=="EFF_COMPILED_SOURCE_COMMON/author+metadata+identity-edges+key-label/le64";Require(bindingLabels||structure=="EFF_COMPILED_SOURCE_COMMON/author+metadata+identity-edges/le64","source-record common structure differs");Artifact a;auto& m=a.manifest;auto& p=m.plan;
 m.baseline=r.S();p.key=r.S();p.author=r.S();p.contract=r.S();Require(DigestText(m.baseline)&&DigestText(p.key)&&DigestText(p.author)&&DigestText(p.contract),"source-record common digest invalid");
 r.verifiedAuthor=verifiedAuthor;m.package=ReadAuthor(r,mod.parent_path(),true,mod,bindingLabels);m.rootAssembly=true;p.rulesOnly=p.spanRecipes=true;p.directory=mod;
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
  auto sourceSize=ResourceSize(*m.package.resources.at(f.resource));Require(f.offset<=sourceSize&&f.size<=sourceSize-f.offset,"source-record author frame extent invalid");for(size_t j=0;j<4;++j)Require(f.start[j]<=f.size&&MultiplySize(f.count,f.stride[j])<=f.size-f.start[j],"source-record frame stride invalid");p.frameRecipes.push_back(std::move(f));}
 Require(r.p==bytes.size()&&p.materialMatches==m.package.materials.size()&&p.textures==m.package.textures.size()&&PackageDigest(m.package)==p.author&&InputContractDigest(m.package)==p.contract,"source-record common author/coverage differs");return a;
}
inline Bytes Source(const Artifact& a,size_t i,const std::filesystem::path& game,const InputProofDirectory& directory){
 const auto& p=a.manifest.plan;const auto& original=p.originals.at(i);const auto& in=p.inputs.at(i);
 auto relative=original.physical.lexically_relative(game);Require(!relative.empty()&&!relative.is_absolute()&&relative.u8string().find("..")==std::string::npos,"source record escapes game");
 auto stamp=FileStamp(original.physical);if(a.verifiedSources.size()!=p.inputs.size()||!SameStamp(stamp,a.verifiedSources.at(i))){ReadSource({},original);Require(SameStamp(stamp,FileStamp(original.physical)),"source record changed while compiling");}
 Writer w;w.S("EFF_COMPILED_SOURCE_RECORD/range+view+roots+identity-edges+semantic-renderer-v1/le64");w.S(original.file);w.S(original.hash);w.S(relative.generic_u8string());w.N(original.offset);w.N(original.size);w.N(stamp.bytes);w.N(stamp.written);
 w.S(in.file);w.S(in.hash);w.S(in.bundle);w.Set(in.externals);w.S(p.startupHashes.at(i));w.Set(directory.members);w.Set(directory.addedBundles);
 w.N(directory.dependencies.size());for(auto index:directory.dependencies)w.S(p.originals.at(index).file);
 w.N(p.rendererRules.at(i).size());for(const auto& rule:p.rendererRules.at(i)){w.S(rule.cab);w.N(uint64_t(rule.id));w.Id(rule.mesh);w.S(rule.meshBundle);}
 w.B(EiemResourceReloadPlan::SemanticCompleteAt(p,i));const auto& semanticOperations=EiemResourceReloadPlan::SemanticOperationsAt(p,i);w.N(semanticOperations.size());for(const auto& operation:semanticOperations){
  w.N(operation.kind);w.Id(operation.mesh);w.S(operation.meshBundle);w.S(operation.cab);w.N(uint64_t(operation.id));w.Blob(operation.beforeSchema);w.Blob(operation.editedSchema);w.Blob(operation.before);w.Blob(operation.edited);w.Strings(operation.editedExternals);
 }
 Artifact local;local.manifest.plan.originals={original};
 for(const auto& root:p.roots)if(root.input==i){auto copy=root;copy.input=0;local.manifest.plan.roots.push_back(copy);}
 for(const auto& root:a.pristine)if(root.input==i){auto copy=root;copy.input=0;local.pristine.push_back(copy);}
 w.N(local.manifest.plan.roots.size());for(const auto& root:local.manifest.plan.roots){w.S(root.path);w.S(root.cab);w.N(uint64_t(root.source));w.N(root.markers.size());for(const auto& marker:root.markers){w.S(marker.first);w.N(marker.second);}}
 auto assemblies=WritePreparedAssemblies(w,local);w.N(local.pristine.size());for(size_t j=0;j<local.pristine.size();++j){const auto& root=local.pristine[j];w.S(root.path);w.S(root.cab);w.N(uint64_t(root.source));w.N(assemblies.pristine.at(j));}
 local.views={a.views.at(i)};WriteNativeViews(w,local);return Pack(w.b);
}
inline void ReadSourceRecord(const Bytes& packed,Artifact& a,const std::filesystem::path& game,const std::map<std::string,size_t>& order,size_t i){
 auto bytes=Unpack(packed);EiemOfflinePackage::Reader r{bytes};auto structure=r.S();const bool semantic=structure=="EFF_COMPILED_SOURCE_RECORD/range+view+roots+identity-edges+semantic-renderer-v1/le64";
 Require(semantic||structure=="EFF_COMPILED_SOURCE_RECORD/range+view+roots+identity-edges/le64","source-record structure differs");auto& p=a.manifest.plan;
 EiemResourceReloadPlan::Source source;source.file=r.S();source.hash=r.S();auto path=std::filesystem::u8path(r.S());source.offset=r.N();source.size=r.N();InputFileStamp stamp{r.N(),r.N()};
 Require(order.at(source.file)==i&&DigestText(source.hash)&&!path.empty()&&!path.is_absolute()&&path.u8string().find("..")==std::string::npos&&source.size&&source.offset<=stamp.bytes&&source.size<=stamp.bytes-source.offset,"source-record range/identity invalid");
 source.physical=std::filesystem::weakly_canonical(game/path);Require(source.physical.lexically_relative(game).u8string().rfind("..",0)!=0,"source-record symlink escapes game");source.baselineHash=source.hash;source.runtimeStampBytes=stamp.bytes;source.runtimeStampWritten=stamp.written;
 Input in;in.file=r.S();in.hash=r.S();in.bundle=r.S();in.externals=r.Set();in.path=a.manifest.package.root/in.file;auto startup=r.S();
 Require(in.file==source.file&&DigestText(in.hash)&&DigestText(startup)&&std::filesystem::u8path(in.file)==std::filesystem::u8path(in.file).filename(),"source-record input identity invalid");
 auto members=r.Set(),added=r.Set();for(const auto& member:members)Require(p.names.members.count(member),"source-record member absent from routing");auto n=r.Count();std::vector<size_t> dependencies;std::set<size_t> unique;
 for(size_t j=0;j<n;++j){auto at=order.at(r.S());Require(at!=i&&unique.insert(at).second,"source-record dependency repeats/self-references");dependencies.push_back(at);}
 n=r.Count();std::vector<RendererRule> rules;std::set<std::pair<std::string,int64_t>> renderers;for(size_t j=0;j<n;++j){RendererRule rule;rule.cab=r.S();rule.id=int64_t(r.N());rule.mesh=r.Id();if(semantic)rule.meshBundle=r.S();Require(rule.id&&a.manifest.package.targetIndex.count(rule.mesh)&&renderers.emplace(rule.cab,rule.id).second,"source-record Renderer rule invalid");rules.push_back(std::move(rule));}
 bool semanticComplete=false;std::vector<SemanticOperation> semanticOperations;if(semantic){semanticComplete=r.B();n=r.Count();std::set<std::tuple<unsigned,Identity,std::string,int64_t>> unique;
  for(size_t j=0;j<n;++j){SemanticOperation operation;operation.kind=unsigned(r.N());operation.mesh=r.Id();operation.meshBundle=r.S();operation.cab=r.S();operation.id=int64_t(r.N());operation.beforeSchema=r.Blob();operation.editedSchema=r.Blob();operation.before=r.Blob();operation.edited=r.Blob();operation.editedExternals=r.Strings();
   Require((operation.kind==SemanticOperation::MeshAsset||operation.kind==SemanticOperation::Renderer)&&a.manifest.package.targetIndex.count(operation.mesh)&&!operation.meshBundle.empty()&&operation.id&&!operation.before.empty()&&!operation.edited.empty()&&unique.emplace(operation.kind,operation.mesh,operation.cab,operation.id).second,"semantic operation identity invalid");
   EiemOfflinePackage::Reader beforeSchema{operation.beforeSchema},editedSchema{operation.editedSchema};auto beforeType=DecodeSchema(beforeSchema),editedType=DecodeSchema(editedSchema);Require(beforeSchema.p==operation.beforeSchema.size()&&editedSchema.p==operation.editedSchema.size()&&beforeType.classId==editedType.classId,"semantic operation TypeTree invalid");
   Require(DecodeValue(beforeType,0,operation.before).node==0&&DecodeValue(editedType,0,operation.edited).node==0,"semantic operation object invalid");semanticOperations.push_back(std::move(operation));
  }
 }
 if(semanticComplete)for(const auto& rule:rules)Require(!rule.meshBundle.empty(),"complete semantic source record lacks a logical Mesh route");
 Artifact local;local.manifest.plan.originals={source};n=r.Count();std::set<std::string> rootPaths;
 for(size_t j=0;j<n;++j){Root root;root.path=r.S();root.cab=r.S();root.source=int64_t(r.N());Require(root.path.rfind("assets/",0)==0&&root.path.find("..")==root.path.npos&&rootPaths.insert(root.path).second&&EiemLoadingGeneration::Cab(root.cab)&&root.source,"source-record root identity invalid");
  auto count=r.Count();for(size_t k=0;k<count;++k){auto marker=r.S();auto copies=r.U();Require(copies&&root.markers.emplace(marker,copies).second,"source-record marker invalid");}local.manifest.plan.roots.push_back(std::move(root));}
 auto assemblies=ReadPreparedAssemblies(r,local.manifest.plan,&p);n=r.Count();std::set<std::string> pristine;
 for(size_t j=0;j<n;++j){Root root;root.path=r.S();root.cab=r.S();root.source=int64_t(r.N());root.preparedAssembly=assemblies.at(r.Size());Require(rootPaths.count(root.path)&&pristine.insert(root.path).second&&EiemLoadingGeneration::Cab(root.cab)&&root.source,"source-record pristine root invalid");local.pristine.push_back(std::move(root));}
 ReadNativeViews(r,local,&a.manifest.package);Require(r.p==bytes.size(),"source-record suffix differs: "+source.file);Require(pristine.size()==rootPaths.size(),"source-record pristine root coverage differs: "+source.file);Require(local.preparedFrames.empty(),"source-record embeds shared frames");
 p.originals.push_back(std::move(source));p.inputs.push_back(std::move(in));p.startupHashes.push_back(startup);p.rendererRules.push_back(std::move(rules));p.semanticOperations.push_back(std::move(semanticOperations));p.semanticComplete.push_back(semanticComplete);p.memberRecipes.emplace_back();p.namespaceRecipes.emplace_back();p.inputDependencies.push_back(std::move(dependencies));a.inputMembers.push_back(std::move(members));a.addedBundles.push_back(std::move(added));a.verifiedSources.push_back(stamp);a.views.push_back(std::move(local.views.at(0)));
 for(auto root:local.manifest.plan.roots){root.input=i;p.roots.push_back(std::move(root));}for(auto root:local.pristine){root.input=i;a.pristine.push_back(std::move(root));}
}
} // namespace EiemCompiledSourceRecords
