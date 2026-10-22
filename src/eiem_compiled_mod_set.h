#pragma once
#include "eiem_compiled_mod.h"
#include "eiem_compiled_object_merge.h"
#include "eiem_native_input_offsets.h"
#include "eiem_source_namespace_keys.h"
#include "eiem_logical_source_routes.h"
#include "eiem_input_retention.h"

// Combines already compiled records. No resource builders, scene scans, game
// resource queries or author-side executables are called by this path.
namespace EiemCompiledModSet {
using namespace EiemCompiledMod;
inline bool StandaloneEffectRoot(const std::filesystem::path& root){auto sections=EiemDirectory::ParseIni(EiemDirectory::Read(root/"mod.ini"));for(const auto& s:sections)if(s.id=="Mod")return s.values.count("type")&&s.values.at("type")=="effect";return false;}
struct Catalog {EiemResourceReloadPlan::Source source;Input input;std::map<std::string,std::shared_ptr<const MemberProof>> members;std::vector<std::shared_ptr<const Root>> pristine;
 std::shared_ptr<const Artifact> proofArtifact;size_t proofInput=0;std::set<std::string> dependencyFiles;
};
inline std::shared_ptr<const MemberProof> PristineMember(const MemberProof& proof){
 auto result=std::make_shared<MemberProof>();result->before=proof.before;result->pristineNamespace=proof.pristineNamespace;return result;
}
using PreparedNamespaces=std::vector<EiemResourceReloadPlan::MemberNamespaceRecipe>;
using Frames=std::map<std::string,Bytes>;
struct Set {
 Manifest manifest;EiemOfflinePackage::Pointer pointer;
 std::vector<std::shared_ptr<const Artifact>> mods;
 std::vector<std::shared_ptr<const Frames>> frames;
 std::vector<PreparedNamespaces> namespaces;
 std::vector<EiemNativeInputOffsets::Layout> layouts;
 std::vector<Catalog> catalog;
 std::vector<std::string> sourceNamespaceKeys;
 std::vector<std::vector<std::pair<size_t,size_t>>> contributors;
 mutable std::map<size_t,std::shared_ptr<const Bytes>> isolated;
 std::map<size_t,std::weak_ptr<const Bytes>> isolatedRefs;
 std::map<size_t,std::shared_ptr<const Bytes>> startup;
 std::map<size_t,std::shared_ptr<const EiemInputSpanView::View>> views,startupViews;
 std::map<size_t,EiemCompiledInputView::Buffer> sourceBuffers;
 std::map<size_t,InputFileStamp> sourceStamps;
 std::map<std::filesystem::path,InputFileStamp> files;
 std::map<std::filesystem::path,Bytes> iniBytes;
 EiemLogicalSourceRoutes::Routes logicalSources;
};
inline std::vector<std::pair<size_t,size_t>> Writers(const Set& set,size_t index){
 std::vector<std::pair<size_t,size_t>> result;for(const auto& contributor:set.contributors.at(index)){
  const auto& a=*set.mods.at(contributor.first);if(a.manifest.plan.startupHashes.at(contributor.second)!=set.catalog.at(index).source.hash)result.push_back(contributor);
 }return result;
}
inline void EnsureCatalogProof(Catalog& catalog,bool retire=false){
 bool missing=false;for(const auto& member:catalog.members)missing=missing||!member.second;
 if(!missing){if(retire&&catalog.proofArtifact){for(auto& member:catalog.members)member.second=PristineMember(*member.second);catalog.proofArtifact.reset();}return;}
 Require(bool(catalog.proofArtifact),"compiled pristine proof backing absent");auto artifact=catalog.proofArtifact;HydrateInput(*artifact,catalog.proofInput);
 for(auto& member:catalog.members){const auto& proof=artifact->members.at(member.first);member.second=retire?PristineMember(proof):std::shared_ptr<const MemberProof>(artifact,&proof);}
 if(retire)catalog.proofArtifact.reset();
}
inline std::map<std::filesystem::path,InputFileStamp> AuthorStamps(const std::filesystem::path& root){
 std::map<std::filesystem::path,InputFileStamp> result;for(const auto& file:EiemModDirectory::Files(root)){
 auto name=file.filename().u8string(),extension=file.extension().u8string();auto relative=file.lexically_relative(root).generic_u8string();if(name=="mod.ini"||name=="compiled.bin"||name=="source-inputs.bin"||extension==".mesh"||extension==".mat"||extension==".tex"||Begins(relative,"resources/"))result.emplace(file,FileStamp(file));}return result;
}
inline std::string DirectoryStamp(const std::filesystem::path& directory){
 Writer w;for(const auto& root:EiemComposedPackage::Roots(directory)){w.S(root.lexically_relative(std::filesystem::canonical(directory)).generic_u8string());for(const auto& file:AuthorStamps(root)){w.S(file.first.lexically_relative(root).u8string());w.N(file.second.bytes);w.N(file.second.written);}}return Hash(w.b);
}
inline bool Unchanged(const Set& set,const std::filesystem::path& directory){
 if(!EiemComposedPackage::Matches(directory,set.manifest.packages))return false;
 for(const auto& row:set.files)if(!std::filesystem::is_regular_file(row.first)||!SameStamp(FileStamp(row.first),row.second))return false;return true;
}
inline Bytes ObjectBytes(const Bytes& bytes,const Object& object){Require(object.offset<=bytes.size()&&object.size<=bytes.size()-object.offset,"compiled object payload extent differs");return Bytes(bytes.begin()+object.offset,bytes.begin()+object.offset+object.size);}
inline std::string NamespaceName(const std::string& before,const EiemLoadingGeneration::Names& names){
 auto found=names.members.find(before);if(found!=names.members.end())return found->second;auto bundle=names.bundles.find(before);if(bundle!=names.bundles.end())return bundle->second;return EiemLoadingGeneration::RemapPath(before,names);
}
inline void RebaseFileIds(const Type& type,size_t node,EiemNativeInput::Reader& reader,Bytes& bytes,
                          const std::vector<bool>& pointers,const std::vector<size_t>& files,unsigned depth=0){
 Require(depth<type.nodes.size(),"compiled PPtr schema cycle");const auto& schema=type.nodes.at(node);
 if(!pointers.at(node)){SkipValue(type,node,reader,0,depth);return;}
 if(schema.type.rfind("PPtr<",0)==0){
  auto fields=Fields(type,node,bytes,reader.p,reader.limit,0);const auto& file=fields.at("m_FileID");const auto& path=fields.at("m_PathID");
  Require(file.size==4&&path.size==8,"compiled PPtr field width differs");auto id=Le(bytes,file.offset,4);Require(id<files.size(),"compiled PPtr FileID absent from external table");
  WriteInteger(bytes,file.offset,files[size_t(id)],4);SkipValue(type,node,reader,0,depth);return;
 }
 if(schema.type=="Array"){
  Require(schema.children.size()==2,"compiled PPtr array schema differs");auto count=reader.Count(reader.limit-reader.p);
  for(size_t i=0;i<count;++i)RebaseFileIds(type,schema.children[1],reader,bytes,pointers,files,depth+1);
 }else for(auto child:schema.children)RebaseFileIds(type,child,reader,bytes,pointers,files,depth+1);
 if(schema.align)reader.Align();
}
inline void RebaseFileIds(const Type& type,Bytes& bytes,const std::vector<size_t>& files){
 bool changed=false;for(size_t i=0;i<files.size();++i)changed=changed||files[i]!=i;if(!changed)return;
 std::vector<bool> pointers(type.nodes.size(),false);for(size_t i=type.nodes.size();i-->0;){pointers[i]=type.nodes[i].type.rfind("PPtr<",0)==0;for(auto child:type.nodes[i].children)pointers[i]=pointers[i]||pointers.at(child);}
 if(!pointers.at(0))return;EiemNativeInput::Reader reader(bytes);RebaseFileIds(type,0,reader,bytes,pointers,files);Require(reader.p==reader.limit,"compiled PPtr relocation payload suffix");
}
inline size_t RelocateSite(const EiemLoadingGeneration::Patch& patch,const Bytes& fromBytes,const SerializedLayout& old,const Bytes& mergedBytes,const SerializedLayout& merged,const std::map<size_t,size_t>& externalSites,const std::map<size_t,const Object*>& objectSites){
 if(patch.offset<old.tableStart)return patch.offset;
 if(patch.offset>=old.tableEnd&&patch.offset<48+old.metadataSize){auto site=externalSites.find(patch.offset);Require(site!=externalSites.end(),"compiled metadata namespace site lacks external record ownership");return site->second;}
 auto owner=objectSites.upper_bound(patch.offset);if(owner!=objectSites.begin()){--owner;const auto& object=*owner->second;if(patch.offset-object.offset<object.size){auto target=merged.objects.find(object.id);Require(target!=merged.objects.end(),"compiled namespace object absent");const auto& type=old.types.at(object.type);
  if(type.classId==142){auto fields=Fields(type,0,fromBytes,object.offset,object.offset+object.size,object.offset);auto next=Fields(merged.types.at(target->second.type),0,mergedBytes,target->second.offset,target->second.offset+target->second.size,target->second.offset);
   for(const auto& field:fields)if(type.nodes.at(field.second.node).type=="string"&&patch.offset==field.second.offset+4){Require(field.first=="m_Name"||field.first=="m_AssetBundleName","compiled Bundle namespace field differs");return next.at(field.first).offset+4;}
   throw std::runtime_error("compiled Bundle namespace site lacks string field ownership");
  }return target->second.offset+(patch.offset-object.offset);
 }}
 throw std::runtime_error("compiled namespace site lacks exact metadata/object ownership");
}
inline EndfieldInput::Bundle Replay(const Artifact& a,size_t index,const EndfieldInput::Bundle& original,const std::map<std::string,Bytes>& frames){
 HydrateInput(a,index);
 std::map<std::string,const Bytes*> members;for(const auto& m:original.members)members.emplace(m.name,&m.bytes);EndfieldInput::Bundle output=original;output.members.clear();
 for(const auto& recipe:a.manifest.plan.memberRecipes.at(index)){
  auto bytes=EiemResourceRecipe::Apply(recipe,members,a.manifest.package,frames);auto proof=a.members.find(recipe.name);
  if(proof!=a.members.end()&&(a.genericResources||GenericObjectChanges(proof->second.before,proof->second.after)))VerifyObjectReferences(bytes,proof->second.before,ParseSerialized(bytes),*members.at(recipe.name));
  output.members.push_back({recipe.name,recipe.flags,std::move(bytes)});
 }return output;
}
inline Bytes Materialize(const Set& set,size_t index,bool isolated,PreparedNamespaces* prepared=nullptr,EiemNativeInputOffsets::Layout* nativeLayout=nullptr,const std::function<void(const char*,double)>& cost={}){
 auto report=[&](const char* stage,std::chrono::steady_clock::time_point begin){if(cost)cost(stage,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count());};
 const auto& ready=isolated?set.views:set.startupViews;auto view=ready.find(index);if(view!=ready.end()){Bytes bytes(view->second->Size());Require(view->second->ReadAt(0,bytes.data(),bytes.size())==bytes.size(),"prepared native view read incomplete");return bytes;}
 if(prepared)prepared->clear();
 const auto& catalog=set.catalog.at(index);auto begin=std::chrono::steady_clock::now();auto input=ReadSource({},catalog.source);report("shared-source-read",begin);begin=std::chrono::steady_clock::now();auto original=EndfieldInput::Decode(input);report("shared-source-decode",begin);
 const auto writers=Writers(set,index);
 if(writers.empty()){
  if(nativeLayout)*nativeLayout=EiemNativeInputOffsets::Describe(original.members);
  for(auto& member:original.members)if(Serialized(member.bytes)){
   const auto& proof=*catalog.members.at(member.name);auto hash=Hash(member.bytes);
   if(prepared)prepared->push_back({member.name,hash,proof.pristineNamespace});
   if(isolated){auto patches=proof.pristineNamespace;for(auto& patch:patches)patch.after=NamespaceName(patch.before,set.manifest.plan.names);member.bytes=EiemLoadingGeneration::ApplyProven(member.bytes,hash,std::move(patches));}
  }
  if(!isolated)return input;for(auto& member:original.members)member.name=set.manifest.plan.names.members.at(member.name);return EndfieldInput::Encode(input,original.members);
 }
 struct Edit {size_t mod=0,source=0;EndfieldInput::Bundle bundle;};std::vector<Edit> edits;Bytes singleStartup;
 for(const auto& contributor:writers){const auto& a=*set.mods.at(contributor.first);const auto& frames=set.frames.at(contributor.first);const Frames empty;begin=std::chrono::steady_clock::now();auto replay=Replay(a,contributor.second,original,frames?*frames:empty);report("shared-contributor-replay",begin);begin=std::chrono::steady_clock::now();
  Bytes encoded;const auto& check=a.manifest.plan.startupHashes.at(contributor.second)==catalog.source.hash?input:(encoded=EndfieldInput::Encode(input,replay.members));Require(Hash(check)==a.manifest.plan.startupHashes.at(contributor.second),"compiled member replay differs from offline proof: "+catalog.source.file);if(writers.size()==1)singleStartup=std::move(encoded);edits.push_back({contributor.first,contributor.second,std::move(replay)});report("shared-contributor-proof",begin);}
 // An independent Mod already supplied the complete edits and exact sites.
 // Only shared inputs need record merging; avoid reparsing/rebuilding the rest.
 if(edits.size()==1){auto& edit=edits.front();const auto& a=*set.mods.at(edit.mod);if(prepared)*prepared=a.manifest.plan.namespaceRecipes.at(edit.source);if(nativeLayout)*nativeLayout=EiemNativeInputOffsets::Describe(edit.bundle.members);if(!isolated){if(a.manifest.plan.startupHashes.at(edit.source)==catalog.source.hash)return input;return singleStartup;}
  for(auto& member:edit.bundle.members){if(Serialized(member.bytes)){const auto& recipes=a.manifest.plan.namespaceRecipes.at(edit.source);auto recipe=std::find_if(recipes.begin(),recipes.end(),[&](const auto& row){return row.member==member.name;});Require(recipe!=recipes.end(),"compiled namespace member absent");auto patches=recipe->patches;for(auto& patch:patches)patch.after=NamespaceName(patch.before,set.manifest.plan.names);member.bytes=EiemLoadingGeneration::ApplyProven(member.bytes,recipe->hash,std::move(patches));}member.name=set.manifest.plan.names.members.at(member.name);}return EndfieldInput::Encode(input,edit.bundle.members);
 }
 std::map<std::string,const EndfieldInput::Member*> oldMembers;for(const auto& m:original.members)oldMembers.emplace(m.name,&m);
 std::map<std::string,std::vector<std::pair<size_t,const EndfieldInput::Member*>>> byMember;
 for(size_t i=0;i<edits.size();++i)for(const auto& m:edits[i].bundle.members)byMember[m.name].push_back({i,&m});
 for(const auto& m:original.members)byMember.try_emplace(m.name);
 // Stream payloads are append-only. Rebase each Mod's offline offset before
 // combining its object records; original payload bytes remain untouched.
 std::map<std::pair<size_t,std::string>,size_t> streamShift;std::map<std::string,Bytes> streams;
 for(const auto& row:byMember){auto old=oldMembers.find(row.first);const Bytes empty;const auto& base=old==oldMembers.end()?empty:old->second->bytes;if(Serialized(base))continue;
  bool serializedEdit=std::any_of(row.second.begin(),row.second.end(),[](const auto& v){return Serialized(v.second->bytes);});if(serializedEdit)continue;
  Bytes combined=base;for(const auto& variant:row.second){const auto& bytes=variant.second->bytes;Require(bytes.size()>=base.size()&&std::equal(base.begin(),base.end(),bytes.begin()),"compiled stream changes its original prefix");auto shift=AlignSize(combined.size()-base.size(),16);combined.resize(AddSize(base.size(),shift));streamShift[{variant.first,row.first}]=shift;combined.insert(combined.end(),bytes.begin()+base.size(),bytes.end());}streams.emplace(row.first,std::move(combined));
 }
 std::vector<EndfieldInput::Member> output;
 for(const auto& row:byMember){auto old=oldMembers.find(row.first);auto stream=streams.find(row.first);if(stream!=streams.end()){uint32_t flags=old!=oldMembers.end()?old->second->flags:row.second.front().second->flags;output.push_back({row.first,flags,stream->second});continue;}
  Require(old!=oldMembers.end(),"compiled serialized member lacks original");const auto& bytes=old->second->bytes;const auto& proof=*catalog.members.at(row.first);const auto& layout=proof.before;
  bool extendedTypes=std::any_of(row.second.begin(),row.second.end(),[&](const auto& variant){return set.mods.at(edits.at(variant.first).mod)->members.at(row.first).after.types.size()!=layout.types.size();});
  auto mergedLayout=extendedTypes?ParseSerialized(bytes):layout;std::unique_ptr<NativeRegistrations> registrations;
  if(extendedTypes)registrations=std::make_unique<NativeRegistrations>(mergedLayout,bytes);
  std::map<std::string,size_t> fileIds;for(size_t i=0;i<layout.externals.size();++i)fileIds.emplace(layout.externals[i],i+1);
  for(const auto& variant:row.second){const auto& edit=edits.at(variant.first);const auto& own=set.mods.at(edit.mod)->members.at(row.first);Require(own.before.externals==layout.externals&&ExternalPrefix(layout.externals,own.after.externals),"compiled original external reference order differs");
   for(size_t i=layout.externals.size();i<own.after.externals.size();++i)if(fileIds.emplace(own.after.externals[i],mergedLayout.externals.size()+1).second){Require(mergedLayout.externals.size()<INT_MAX,"compiled FileID field overflow");mergedLayout.externals.push_back(own.after.externals[i]);}}
  auto mergePhase=std::chrono::steady_clock::now();std::map<int64_t,std::vector<Bytes>> changes;std::map<int64_t,AddedObject> additions;std::set<int64_t> removed;
  for(const auto& variant:row.second){const auto& edit=edits.at(variant.first);const auto& a=*set.mods.at(edit.mod);const auto& own=a.members.at(row.first);std::vector<size_t> files{0};for(size_t i=0;i<own.after.externals.size();++i)files.push_back(i<layout.externals.size()?i+1:fileIds.at(own.after.externals[i]));
   std::vector<size_t> types;if(registrations)types=registrations->Merge(variant.second->bytes,ParseSerialized(variant.second->bytes),files);
   else for(size_t i=0;i<own.after.types.size();++i)types.push_back(i);
   for(const auto& object:layout.objects)if(!own.after.objects.count(object.first))removed.insert(object.first);
   for(const auto& object:own.after.objects){auto before=layout.objects.find(object.first);auto payload=ObjectBytes(variant.second->bytes,object.second);if(before!=layout.objects.end()&&payload==ObjectBytes(bytes,before->second))continue;
    RebaseFileIds(own.after.types.at(object.second.type),payload,files);
    for(const auto& site:own.streams)if(site.object==object.first){auto shift=streamShift.find({variant.first,site.member});if(shift==streamShift.end()||!shift->second)continue;auto source=oldMembers.find(site.member);size_t extent=source==oldMembers.end()?0:source->second->bytes.size();if(site.value<extent)continue;Require(Le(payload,site.offset,site.width)==site.value,"compiled stream relocation source differs");WriteInteger(payload,site.offset,AddSize(size_t(site.value),shift->second),site.width);}
    if(before!=layout.objects.end()){Require(types.at(object.second.type)==before->second.type,"compiled object type changed");changes[object.first].push_back(std::move(payload));}
    else{AddedObject next{object.first,types.at(object.second.type),std::move(payload)};auto added=additions.find(object.first);if(added==additions.end())additions.emplace(object.first,std::move(next));else Require(added->second.type==next.type&&added->second.bytes==next.bytes,"compiled added object PathID collision");}
   }
  }
  report("shared-object-relocations",mergePhase);mergePhase=std::chrono::steady_clock::now();for(auto id:removed)Require(!changes.count(id),"compiled deletion conflicts with another Mod's object edit: "+std::to_string(id));
  std::map<int64_t,Bytes> mergedChanges;for(const auto& change:changes){const auto& object=layout.objects.at(change.first);try{mergedChanges.emplace(change.first,MergeObject(layout.types.at(object.type),ObjectBytes(bytes,object),change.second));}catch(const std::exception& error){throw std::runtime_error(catalog.source.file+" / "+row.first+" / PathID "+std::to_string(change.first)+": "+error.what());}}
  report("shared-object-merge",mergePhase);mergePhase=std::chrono::steady_clock::now();const std::vector<Bytes> emptyRecords;
  std::vector<AddedObject> added;for(auto& item:additions)added.push_back(std::move(item.second));auto merged=BuildSerialized(bytes,mergedLayout,mergedChanges,added,removed,registrations?registrations->types:emptyRecords,registrations?registrations->scripts:emptyRecords);
  bool generic=std::any_of(row.second.begin(),row.second.end(),[&](const auto& variant){return set.mods.at(edits.at(variant.first).mod)->genericResources;});
  if(!removed.empty()||registrations||generic)VerifyObjectReferences(merged,layout,ParseSerialized(merged),bytes);
  report("shared-member-write",mergePhase);mergePhase=std::chrono::steady_clock::now();if(isolated||prepared){auto result=ParseSerialized(merged);std::map<size_t,EiemLoadingGeneration::Patch> patches;auto mergedExternals=EiemLoadingGraphAudit::ReadExternals(merged,result);std::map<std::string,size_t> externalPaths;for(const auto& ext:mergedExternals)externalPaths.emplace(ext.member,ext.pathOffset);
   auto add=[&](const std::vector<EiemLoadingGeneration::Patch>& list,const Bytes& fromBytes,const SerializedLayout& from){std::map<size_t,const Object*> objectSites;for(const auto& object:from.objects)Require(objectSites.emplace(object.second.offset,&object.second).second,"compiled namespace object range duplicated");std::map<size_t,size_t> externalSites;for(const auto& ext:EiemLoadingGraphAudit::ReadExternals(fromBytes,from))externalSites.emplace(ext.pathOffset,ext.fileId<=layout.externals.size()?mergedExternals.at(ext.fileId-1).pathOffset:externalPaths.at(ext.member));
    for(const auto& patch:list){bool deleted=false;for(auto id:removed){auto old=from.objects.find(id);if(old!=from.objects.end()&&patch.offset>=old->second.offset&&patch.offset-old->second.offset<old->second.size){deleted=true;break;}}if(deleted)continue;auto site=RelocateSite(patch,fromBytes,from,merged,result,externalSites,objectSites);auto after=NamespaceName(patch.before,set.manifest.plan.names);Require(after.size()==patch.before.size()&&site<=merged.size()&&after.size()<=merged.size()-site&&std::equal(patch.before.begin(),patch.before.end(),merged.begin()+site),"compiled namespace relocation source differs");auto inserted=patches.emplace(site,EiemLoadingGeneration::Patch{site,patch.before,after});Require(inserted.second||inserted.first->second.after==after,"compiled namespace relocation conflict");}};
   if(row.second.empty())add(proof.pristineNamespace,bytes,proof.before);
   else for(const auto& variant:row.second){const auto& edit=edits.at(variant.first);const auto& a=*set.mods.at(edit.mod);for(const auto& recipe:a.manifest.plan.namespaceRecipes.at(edit.source))if(recipe.member==row.first)add(recipe.patches,variant.second->bytes,a.members.at(row.first).after);}
   std::vector<EiemLoadingGeneration::Patch> list;for(auto& item:patches)list.push_back(std::move(item.second));auto hash=Hash(merged);if(prepared)prepared->push_back({row.first,hash,list});if(isolated)merged=EiemLoadingGeneration::ApplyProven(merged,hash,std::move(list));
  }
  report("shared-namespace-relocations",mergePhase);output.push_back({row.first,old->second->flags,std::move(merged)});
 }
 // Keep pristine member order; newly introduced streams follow it.
 std::map<std::string,size_t> ordinal;for(size_t i=0;i<original.members.size();++i)ordinal.emplace(original.members[i].name,i);
 std::stable_sort(output.begin(),output.end(),[&](const auto& a,const auto& b){auto ai=ordinal.find(a.name),bi=ordinal.find(b.name);return (ai==ordinal.end()?SIZE_MAX:ai->second)<(bi==ordinal.end()?SIZE_MAX:bi->second);});
 if(nativeLayout)*nativeLayout=EiemNativeInputOffsets::Describe(output);
 if(!isolated&&output.size()==original.members.size()&&std::equal(output.begin(),output.end(),original.members.begin(),[](const auto& a,const auto& b){return a.name==b.name&&a.flags==b.flags&&a.bytes==b.bytes;}))return input;
 if(isolated)for(auto& m:output)m.name=set.manifest.plan.names.members.at(m.name);return EndfieldInput::Encode(input,output);
}
inline Bytes IsolatePreparedInput(const Set& set,size_t index,const Bytes& startup,const PreparedNamespaces& prepared){
 Require(Hash(startup)==set.manifest.plan.startupHashes.at(index),"prepared startup input digest differs");
 if(index<set.layouts.size()&&set.manifest.plan.startupHashes.at(index)!=set.catalog.at(index).source.hash)
  return EiemNativeInputOffsets::Relocate(startup,set.layouts[index],prepared,{},set.manifest.plan.names);
 auto bundle=EndfieldInput::Decode(startup);
 for(auto& member:bundle.members){
  if(Serialized(member.bytes)){auto recipe=std::find_if(prepared.begin(),prepared.end(),[&](const auto& value){return value.member==member.name;});Require(recipe!=prepared.end(),"prepared namespace member absent");
   auto patches=recipe->patches;for(auto& patch:patches)patch.after=NamespaceName(patch.before,set.manifest.plan.names);
   member.bytes=EiemLoadingGeneration::ApplyProven(member.bytes,recipe->hash,std::move(patches));
  }
  member.name=set.manifest.plan.names.members.at(member.name);
 }
 return EndfieldInput::Encode(startup,bundle.members);
}
inline Bytes StartupInput(const Set& set,size_t index){
 auto ready=set.startup.find(index);Require(ready!=set.startup.end(),"prepared startup input absent");
 Require(index<set.layouts.size()&&index<set.namespaces.size(),"prepared startup offset coverage differs");
 return EiemNativeInputOffsets::Relocate(*ready->second,set.layouts[index],set.namespaces[index],set.manifest.plan.names,{});
}
inline EiemRenderAssemblyDescription::Description MergeAssembly(const Root& pristine,const std::vector<std::pair<const Root*,std::set<std::pair<std::string,int64_t>>>>& variants){
 if(variants.empty())return pristine.Assembly();
 if(variants.size()==1)return variants.front().first->Assembly();
 EiemRenderAssemblyDescription::Description result;if(pristine.Assembly().sources.empty())return pristine.Assembly();
 std::map<std::pair<std::string,int64_t>,const EiemRenderAssemblyDescription::Source*> selected;
 for(const auto& variant:variants){std::set<std::pair<std::string,int64_t>> found;for(const auto& source:variant.first->Assembly().sources){auto id=std::make_pair(source.cab,source.id);if(variant.second.count(id)){Require(selected.emplace(id,&source).second,"compiled Renderer source conflict");found.insert(id);}}Require(found.size()==variant.second.size(),"compiled Renderer source missing");}
 result.sources.reserve(pristine.Assembly().sources.size());for(const auto& source:pristine.Assembly().sources){auto found=selected.find({source.cab,source.id});result.sources.push_back(found==selected.end()?source:*found->second);}
 result.lods=pristine.Assembly().lods;result.helpers=pristine.Assembly().helpers;
 std::map<EiemRenderAssemblyDescription::Address,const EiemRenderAssemblyDescription::Source*> addresses;for(const auto& source:result.sources)Require(addresses.emplace(source.renderer,&source).second,"compiled Renderer address duplicated");
 auto expand=[&](std::vector<EiemRenderAssemblyDescription::Address>& list){std::vector<EiemRenderAssemblyDescription::Address> output;for(const auto& address:list){auto source=addresses.find(address);Require(source!=addresses.end(),"compiled LOD/helper address unowned");for(const auto& target:source->second->targets)output.push_back(target.renderer);}list=std::move(output);};
 for(auto& lod:result.lods)for(auto& level:lod.levels)expand(level.renderers);
 for(auto& helper:result.helpers){expand(helper.renderers);for(const auto& variant:variants){auto next=std::find_if(variant.first->Assembly().helpers.begin(),variant.first->Assembly().helpers.end(),[&](const auto& h){return h.component==helper.component;});if(next!=variant.first->Assembly().helpers.end())helper.custom=helper.custom||next->custom;}}
 return result;
}
inline void SelectIsolatedInputs(Set& set){
 auto& plan=set.manifest.plan;Require(plan.startupHashes.size()==set.catalog.size(),"compiled source hash coverage differs");
 Require(set.sourceNamespaceKeys.size()==set.catalog.size(),"compiled source namespace coverage differs");
 std::vector<size_t> roots;for(const auto& root:plan.roots)roots.push_back(root.input);
 std::vector<unsigned char> changed(set.catalog.size());for(size_t i=0;i<changed.size();++i)changed[i]=plan.startupHashes[i]!=set.catalog[i].source.hash;
 auto retained=EiemInputRetention::Select(plan.inputDependencies,changed,roots);std::set<size_t> isolated;
 for(size_t i=0;i<retained.size();++i)if(retained[i])isolated.insert(i);
 // A new Prefab/edited object needs an independent identity. Unchanged
 // dependencies keep the game's CAB identity and ResourceManager ownership;
 // opening renamed copies would duplicate their textures and GPU resources.
 for(size_t i=0;i<set.catalog.size();++i){const auto& catalog=set.catalog[i];bool own=isolated.count(i)!=0;
  for(const auto& member:catalog.members){auto& name=plan.names.members.at(member.first);name=own?"CAB-"+EiemLoadingGeneration::Token(set.sourceNamespaceKeys.at(i),member.first,32):member.first;
   if(!own)plan.shared.insert(member.first);}
  if(!catalog.input.bundle.empty())plan.names.bundles.at(catalog.input.bundle)=own?EiemLoadingGeneration::BundleIdentity(set.sourceNamespaceKeys.at(i),catalog.input.bundle):catalog.input.bundle;
 }
 for(auto& member:plan.names.members){auto cab=member.first.substr(0,36);if(member.first==cab+".resS")member.second=plan.names.members.at(cab)+".resS";}
 plan.closure.assign(isolated.begin(),isolated.end());
}
inline std::shared_ptr<Set> RefreshKeySwitches(const std::shared_ptr<const Set>& previous,const std::filesystem::path& directory){
 if(!previous||!EiemComposedPackage::Matches(directory,previous->manifest.packages))return {};
 std::vector<Package> authors=previous->manifest.packages;std::map<std::filesystem::path,InputFileStamp> files;std::map<std::filesystem::path,Bytes> iniBytes;
 for(auto& author:authors){auto stamps=AuthorStamps(author.root);for(const auto& row:stamps){auto old=previous->files.find(row.first);if(old==previous->files.end()||(row.first.filename()!="mod.ini"&&!SameStamp(row.second,old->second)))return {};files.emplace(row);}
  auto oldIni=previous->iniBytes.find(author.root);if(oldIni==previous->iniBytes.end())return {};auto ini=EiemNativeInput::Read(author.root/"mod.ini");auto before=ParseIni(oldIni->second),after=ParseIni(ini);if(before.size()!=after.size())return {};
  for(size_t i=0;i<before.size();++i){if(before[i].id!=after[i].id)return {};if(after[i].id=="Mod"){auto enabled=after[i].values.find("key_switch_enabled");author.keySwitchEnabled=enabled==after[i].values.end()?true:KeySwitchEnabled(enabled->second);before[i].values.erase("key_switch_enabled");after[i].values.erase("key_switch_enabled");}if(before[i].values!=after[i].values)return {};}
  author.iniHash=Sha(ini.data(),ini.size());iniBytes.emplace(author.root,std::move(ini));
 }
 if(files.size()!=previous->files.size())return {};
 auto set=std::make_shared<Set>(*previous);set->manifest.packages=std::move(authors);set->manifest.package=EiemComposedPackage::Compose(set->manifest.packages,true,false);set->manifest.plan.author=PackageDigest(set->manifest.package);set->files=std::move(files);set->iniBytes=std::move(iniBytes);
 set->isolated.clear();for(const auto& row:set->isolatedRefs)if(auto bytes=row.second.lock())set->isolated.emplace(row.first,std::move(bytes));
 Require(Unchanged(*set,directory),"Mod changed during key switch snapshot refresh");return set;
}
inline std::shared_ptr<Set> Read(const std::filesystem::path& game,const std::shared_ptr<const Set>& previous={},const std::filesystem::path& modsDirectory={},const std::function<void(const char*,double)>& cost={}){
 auto started=std::chrono::steady_clock::now();auto report=[&](const char* stage,std::chrono::steady_clock::time_point begin){if(cost)cost(stage,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count());};
 auto directory=modsDirectory.empty()?game/"plugin/mods":modsDirectory;auto roots=EiemComposedPackage::Roots(directory);roots.erase(std::remove_if(roots.begin(),roots.end(),StandaloneEffectRoot),roots.end());if(roots.empty()&&!previous)return {};
 if(auto refreshed=RefreshKeySwitches(previous,directory))return refreshed;
 auto set=std::make_shared<Set>();if(previous)set->logicalSources=previous->logicalSources;std::string identity=structure;std::vector<Package> authors;
 // Independent Mod indexes and payloads can be verified concurrently. Source
 // identity/composition publication remains ordered and transactional.
 std::vector<std::shared_ptr<Artifact>> decoded(roots.size());
 std::vector<std::map<std::filesystem::path,InputFileStamp>> decodedStamps(roots.size());
 EiemOfflineCompile::ParallelInputs(roots.size(),[&](size_t index){const auto& root=roots[index];decodedStamps[index]=AuthorStamps(root);bool reuse=false;if(previous)for(const auto& mod:previous->mods)if(mod->manifest.package.root==root){const auto& stamps=decodedStamps[index];size_t prior=0;for(const auto& row:previous->files){if(EiemModDirectory::Owns(root,row.first))++prior;}reuse=prior==stamps.size();for(const auto& row:stamps){auto old=previous->files.find(row.first);reuse=reuse&&old!=previous->files.end()&&SameStamp(row.second,old->second);}break;}
  if(!reuse){Require(std::filesystem::is_regular_file(root/"compiled.bin"),"Mod lacks compiled.bin; export this Mod with Blender: "+root.filename().u8string());auto begin=std::chrono::steady_clock::now();decoded[index]=std::make_shared<Artifact>(EiemCompiledMod::ReadRuntime(root,game,cost));report("artifact",begin);}
 },4);
 size_t rootIndex=0;for(const auto& root:roots){Require(std::filesystem::is_regular_file(root/"compiled.bin"),"Mod lacks compiled.bin; export this Mod with the Blender plug-in: "+root.filename().u8string());auto stamps=AuthorStamps(root);Require(stamps.size()==decodedStamps[rootIndex].size(),"Mod changed during parallel index read");for(const auto& row:stamps){auto old=decodedStamps[rootIndex].find(row.first);Require(old!=decodedStamps[rootIndex].end()&&SameStamp(old->second,row.second),"Mod changed during parallel index read");}
  size_t reuse=SIZE_MAX;
  if(previous)for(size_t m=0;m<previous->mods.size();++m)if(previous->mods[m]->manifest.package.root==root){
   size_t oldCount=0;for(const auto& row:previous->files){if(EiemModDirectory::Owns(root,row.first))++oldCount;}
   bool same=oldCount==stamps.size();for(const auto& row:stamps){auto old=previous->files.find(row.first);same=same&&old!=previous->files.end()&&SameStamp(old->second,row.second);}if(same)reuse=m;break;
  }
  std::shared_ptr<const Artifact> a;std::shared_ptr<const Frames> frames;Package current;
  if(reuse!=SIZE_MAX){a=previous->mods[reuse];current=previous->manifest.packages.at(reuse);frames=previous->frames[reuse];}
  else{auto begin=std::chrono::steady_clock::now();auto loaded=decoded.at(rootIndex);if(!loaded)loaded=std::make_shared<Artifact>(EiemCompiledMod::ReadRuntime(root,game,cost));begin=std::chrono::steady_clock::now();current=Load(root,&loaded->manifest.package);Require(InputContractDigest(current)==loaded->manifest.plan.contract,"Mod resource declarations differ from compiled rules; re-export with Blender: "+current.id);loaded->manifest.package=current;a=std::move(loaded);report("author-load",begin);
   begin=std::chrono::steady_clock::now();frames=std::shared_ptr<const Frames>(a,&a->preparedFrames);report("frame-expand",begin);
  }
  auto after=AuthorStamps(root);Require(stamps.size()==after.size(),"Mod files changed while reading compiled rules");for(const auto& row:stamps)Require(after.count(row.first)&&SameStamp(row.second,after.at(row.first)),"Mod changed while reading compiled rules");
  identity+='\n'+root.filename().u8string()+":"+a->digest+":"+a->manifest.plan.contract;authors.push_back(current);set->mods.push_back(a);set->frames.push_back(std::move(frames));
  auto ini=EiemNativeInput::Read(root/"mod.ini");Require(Sha(ini.data(),ini.size())==current.iniHash,"Mod INI changed while capturing compiled snapshot");set->iniBytes.emplace(root,std::move(ini));set->files.insert(stamps.begin(),stamps.end());++rootIndex;
  if(std::filesystem::is_regular_file(root/"source-inputs.bin"))EiemLogicalSourceRoutes::Include(set->logicalSources,EiemStaticSources::LoadMod(root,game));
 }
 auto compositionStarted=std::chrono::steady_clock::now();auto& manifest=set->manifest;manifest.composed=true;manifest.rootAssembly=true;manifest.packages=authors;manifest.package=EiemComposedPackage::Compose(authors,true,false);auto& plan=manifest.plan;plan.composed=true;plan.rulesOnly=true;plan.key=Hash(Bytes(identity.begin(),identity.end()));plan.author=PackageDigest(manifest.package);plan.contract=InputContractDigest(manifest.package);report("author-composition",compositionStarted);
 auto catalogStarted=std::chrono::steady_clock::now();
 std::map<std::string,size_t> inputs;std::map<std::string,std::pair<std::shared_ptr<const Root>,size_t>> pristine;
 auto add=[&](const Catalog& entry){auto old=inputs.find(entry.source.file);if(old!=inputs.end()){auto& existing=set->catalog.at(old->second);const auto& source=existing.source;Require(source.hash==entry.source.hash&&source.size==entry.source.size,"compiled shared source content conflict");existing.input.externals.insert(entry.input.externals.begin(),entry.input.externals.end());existing.dependencyFiles.insert(entry.dependencyFiles.begin(),entry.dependencyFiles.end());for(const auto& root:entry.pristine){auto found=std::find_if(existing.pristine.begin(),existing.pristine.end(),[&](const auto& value){return value->path==root->path;});if(found==existing.pristine.end())existing.pristine.push_back(root);else Require((*found)->cab==root->cab&&(*found)->source==root->source,"compiled pristine root identity conflict");}return old->second;}size_t i=set->catalog.size();inputs.emplace(entry.source.file,i);set->catalog.push_back(entry);set->contributors.emplace_back();return i;};
 if(previous)for(size_t old=0;old<previous->catalog.size();++old)if(!previous->contributors[old].empty()){const auto& catalog=previous->catalog[old];auto i=add(catalog);for(const auto& root:catalog.pristine)pristine.emplace(root->path,std::make_pair(root,i));}
 std::map<std::string,std::vector<std::pair<const Root*,std::set<std::pair<std::string,int64_t>>>>> rootVariants;
 for(size_t m=0;m<set->mods.size();++m){const auto& a=*set->mods[m];const auto& own=a.manifest.plan;const auto& package=a.manifest.package;
  for(size_t i=0;i<own.inputs.size();++i){Catalog catalog;catalog.source=own.originals[i];catalog.input=own.inputs[i];catalog.proofArtifact=set->mods[m];catalog.proofInput=i;auto existing=inputs.find(catalog.source.file);
   if(own.inputDependencies.size()==own.inputs.size())for(auto dependency:own.inputDependencies[i])catalog.dependencyFiles.insert(own.originals.at(dependency).file);
   if(existing!=inputs.end())catalog.members=set->catalog.at(existing->second).members;
   else {auto copyStarted=std::chrono::steady_clock::now();if(!a.inputMembers.empty()){for(const auto& name:a.inputMembers.at(i))catalog.members.emplace(name,nullptr);}
    else for(const auto& recipe:own.memberRecipes[i]){auto proof=a.members.find(recipe.name);if(proof!=a.members.end())catalog.members.emplace(proof->first,std::shared_ptr<const MemberProof>(set->mods[m],&proof->second));}report("pristine-proof-copy",copyStarted);}
   if(existing!=inputs.end())catalog.pristine=set->catalog.at(existing->second).pristine;
   for(const auto& root:a.pristine)if(root.input==i){auto found=std::find_if(catalog.pristine.begin(),catalog.pristine.end(),[&](const auto& value){return value->path==root.path;});if(found==catalog.pristine.end())catalog.pristine.push_back(std::make_shared<const Root>(root));else Require((*found)->cab==root.cab&&(*found)->source==root.source,"compiled pristine root identity conflict");}auto next=add(catalog);set->contributors[next].push_back({m,i});
   for(const auto& root:catalog.pristine)pristine.emplace(root->path,std::make_pair(root,next));for(const auto& root:own.roots)if(root.input==i){std::set<std::pair<std::string,int64_t>> affected;for(const auto& rule:own.rendererRules[i])affected.emplace(rule.cab,rule.id);rootVariants[root.path].push_back({&root,std::move(affected)});}
  }
  for(const auto& row:own.parts){auto part=row.second;part.id=EiemComposedPackage::Name(package.id,part.id);part.material=EiemComposedPackage::Name(package.id,part.material);Require(plan.parts.emplace(row.first,std::move(part)).second,"compiled part marker conflict");}
  for(const auto& row:own.materials)Require(plan.materials.emplace(EiemComposedPackage::Name(package.id,row.first),row.second).second,"compiled material identity conflict");plan.shared.insert(own.shared.begin(),own.shared.end());plan.meshParts+=own.meshParts;plan.rendererParts+=own.rendererParts;plan.textures+=own.textures;plan.materialMatches+=own.materialMatches;
 }
 if(std::filesystem::is_directory(game/"Endfield_Data/StreamingAssets/VFS")){
  std::vector<EiemResourceReloadPlan::Source> sources;for(const auto& entry:set->catalog)sources.push_back(entry.source);
  EiemLogicalSourceRoutes::Rebind(sources,set->logicalSources,game);for(size_t i=0;i<sources.size();++i)set->catalog[i].source=std::move(sources[i]);
 }
 report("catalog-membership",catalogStarted);auto namesStarted=std::chrono::steady_clock::now();
 std::map<std::string,size_t> memberInputs;for(size_t i=0;i<set->catalog.size();++i)for(const auto& member:set->catalog[i].members)Require(memberInputs.emplace(member.first,i).second,"compiled CAB has multiple input owners");
 for(size_t i=0;i<set->catalog.size();++i){const auto& catalog=set->catalog[i];std::set<size_t> dependencies;
  for(const auto& file:catalog.dependencyFiles){auto found=inputs.find(file);if(found!=inputs.end()&&found->second!=i)dependencies.insert(found->second);}
  for(const auto& external:catalog.input.externals){auto found=memberInputs.find(external);if(found!=memberInputs.end()&&found->second!=i)dependencies.insert(found->second);}
  plan.inputDependencies.emplace_back(dependencies.begin(),dependencies.end());
 }
 for(const auto& catalog:set->catalog)identity+='\n'+catalog.source.file+":"+catalog.source.hash;plan.key=Hash(Bytes(identity.begin(),identity.end()));
 std::vector<EiemSourceNamespaceKeys::Source> namespaceSources;namespaceSources.reserve(set->catalog.size());
 for(size_t i=0;i<set->catalog.size();++i){const auto& catalog=set->catalog[i];EiemSourceNamespaceKeys::Source source{catalog.source.file,catalog.source.hash,{},plan.inputDependencies.at(i)};
  for(const auto& contributor:set->contributors.at(i)){const auto& artifact=*set->mods.at(contributor.first);source.contributors.insert(artifact.manifest.package.id+":"+artifact.manifest.plan.contract);}namespaceSources.push_back(std::move(source));}
 auto namespaceKeys=EiemSourceNamespaceKeys::Build(namespaceSources);set->sourceNamespaceKeys=namespaceKeys;
 for(size_t i=0;i<set->catalog.size();++i){auto& catalog=set->catalog[i];plan.originals.push_back(catalog.source);plan.inputs.push_back(catalog.input);plan.inputs.back().hash.clear();plan.inputs.back().path.clear();plan.rendererRules.emplace_back();plan.startupHashes.emplace_back();for(const auto& row:catalog.members)plan.names.members.emplace(row.first,"CAB-"+EiemLoadingGeneration::Token(namespaceKeys[i],row.first,32));
  if(!catalog.input.bundle.empty())plan.names.bundles.emplace(catalog.input.bundle,EiemLoadingGeneration::BundleIdentity(namespaceKeys[i],catalog.input.bundle));plan.closure.push_back(i);
 }
 for(const auto& mod:set->mods)for(const auto& member:mod->manifest.plan.names.members)if(!plan.names.members.count(member.first)){auto cab=member.first.substr(0,36);Require(member.first==cab+".resS"&&plan.names.members.count(cab),"compiled stream name lacks owning CAB");plan.names.members.emplace(member.first,plan.names.members.at(cab)+".resS");}
 if(previous)for(const auto& member:previous->manifest.plan.names.members)if(!plan.names.members.count(member.first)){auto cab=member.first.substr(0,36);if(member.first==cab+".resS"&&plan.names.members.count(cab))plan.names.members.emplace(member.first,plan.names.members.at(cab)+".resS");}
 report("native-names",namesStarted);auto assemblyStarted=std::chrono::steady_clock::now();
 for(const auto& row:pristine){const auto& before=*row.second.first;Root root;root.path=before.path;root.input=row.second.second;root.cab=before.cab;root.source=before.source;const auto& variants=rootVariants[row.first];
  if(variants.empty()&&before.preparedAssembly)root.preparedAssembly=before.preparedAssembly;
  else if(variants.size()==1&&variants.front().first->preparedAssembly)root.preparedAssembly=variants.front().first->preparedAssembly;
  else root.preparedAssembly=std::make_shared<const EiemRenderAssemblyDescription::Description>(MergeAssembly(before,variants));
  for(const auto& variant:variants)for(const auto& marker:variant.first->markers)root.markers[marker.first]+=marker.second;plan.roots.push_back(std::move(root));}report("root-assembly",assemblyStarted);
 manifest.baseline=plan.key;set->pointer={plan.key,plan.key};
 report("catalog-and-roots",catalogStarted);
 // INI controls and unchanged re-exports refresh the author snapshot, but
 // retain the already verified native resource inputs and their hashes.
 if(previous&&set->pointer==previous->pointer){
  Require(plan.inputs.size()==previous->manifest.plan.inputs.size(),"same compiled identity changed input count");
  plan.startupHashes=previous->manifest.plan.startupHashes;for(size_t i=0;i<plan.inputs.size();++i)plan.inputs[i].hash=previous->manifest.plan.inputs[i].hash;
  plan.viewIdentities=previous->manifest.plan.viewIdentities;
  SelectIsolatedInputs(*set);set->startup=previous->startup;set->namespaces=previous->namespaces;set->layouts=previous->layouts;set->isolatedRefs=previous->isolatedRefs;for(const auto& row:set->isolatedRefs)if(auto bytes=row.second.lock())set->isolated.emplace(row.first,std::move(bytes));
  set->views=previous->views;set->startupViews=previous->startupViews;set->sourceBuffers=previous->sourceBuffers;set->sourceStamps=previous->sourceStamps;
  Require(Unchanged(*set,directory),"Mod changed during compiled snapshot refresh");return set;
 }
 // Compose each input once. Capture the verified namespace sites while its
 // merged layout is already available; the isolated candidate only renames
 // this prepared input instead of replaying and merging all author records.
 report("artifact-and-plan",started);
 set->namespaces.resize(plan.inputs.size());set->layouts.resize(plan.inputs.size());auto& namespaces=set->namespaces;
 std::map<std::string,size_t> priorInputs;if(previous)for(size_t i=0;i<previous->catalog.size();++i)priorInputs.emplace(previous->catalog[i].source.file,i);
 auto reusable=[&](size_t i)->size_t{
  auto found=priorInputs.find(set->catalog[i].source.file);if(found==priorInputs.end())return SIZE_MAX;auto old=found->second;
  if(previous->namespaces.size()!=previous->catalog.size()||previous->layouts.size()!=previous->catalog.size()||set->catalog[i].source.hash!=previous->catalog[old].source.hash||set->contributors[i].size()!=previous->contributors[old].size())return SIZE_MAX;
  for(size_t n=0;n<set->contributors[i].size();++n){auto a=set->contributors[i][n],b=previous->contributors[old][n];if(a.second!=b.second||set->mods[a.first]->digest!=previous->mods[b.first]->digest)return SIZE_MAX;}return old;
 };
 std::vector<std::shared_ptr<const Bytes>> rawStartup(plan.inputs.size());
 std::vector<std::shared_ptr<const EiemInputSpanView::View>> rawViews(plan.inputs.size());
 std::vector<EiemCompiledInputView::Buffers> viewBuffers(plan.inputs.size());
 std::vector<const EiemCompiledInputView::Recipe*> viewRecipes(plan.inputs.size(),nullptr);
 std::vector<InputFileStamp> viewSourceStamps(plan.inputs.size());
 std::vector<size_t> reusedInputs(plan.inputs.size(),SIZE_MAX);
 // Decode proofs only for multiple actual writers, or a removed Mod's
 // pristine candidate. Common read-only dependencies never enter record merge.
 for(size_t i=0;i<set->catalog.size();++i){auto writers=Writers(*set,i);if(writers.size()>1){for(const auto& contributor:writers)HydrateInput(*set->mods.at(contributor.first),contributor.second);EnsureCatalogProof(set->catalog[i]);}
  else if(writers.empty()&&set->contributors[i].empty())EnsureCatalogProof(set->catalog[i],true);
 }
 EiemOfflineCompile::ParallelInputs(plan.inputs.size(),[&](size_t i){
  auto writers=Writers(*set,i);auto contribution=writers.size()==1?writers.front():(writers.empty()&&!set->contributors[i].empty()?set->contributors[i].front():std::make_pair(SIZE_MAX,SIZE_MAX));
  if(contribution.first!=SIZE_MAX&&writers.size()<=1){const auto& mod=set->mods.at(contribution.first);if(mod->views.size()==mod->manifest.plan.originals.size()&&!mod->views.empty()){
   auto begin=std::chrono::steady_clock::now();auto& buffers=viewBuffers[i];auto found=priorInputs.find(set->catalog[i].source.file);EiemCompiledInputView::Buffer original;viewSourceStamps[i]=FileStamp(set->catalog[i].source.physical);
   if(found!=priorInputs.end()){auto old=found->second;auto cached=previous->sourceBuffers.find(old);auto stamp=previous->sourceStamps.find(old);const auto& prior=previous->catalog[old].source;const auto& current=set->catalog[i].source;if(cached!=previous->sourceBuffers.end()&&stamp!=previous->sourceStamps.end()&&current.hash==prior.hash&&current.physical==prior.physical&&current.offset==prior.offset&&SameStamp(stamp->second,viewSourceStamps[i]))original=cached->second;}
   if(!original){const auto& source=set->catalog[i].source;auto mapped=std::make_shared<const EiemReadonlyFileRange::Range>(source.physical,source.offset,source.size);
    const auto& authored=mod->manifest.plan.originals.at(contribution.second);
    if(set->logicalSources.count(source.file)||source.physical!=authored.physical||source.offset!=authored.offset||!SameStamp(mapped->Stamp(),mod->verifiedSources.at(contribution.second))){auto hash=Sha(mapped->Data(),mapped->Size());Require(Hex(hash.data(),hash.size())==source.hash,"game source content changed; repair the Mod offline: "+(set->logicalSources.count(source.file)?set->logicalSources.at(source.file):source.file));}
    viewSourceStamps[i]=mapped->Stamp();original=EiemCompiledInputView::Buffer(std::move(mapped));}buffers.emplace(std::make_pair(EiemResourceRecipe::Original,std::string{}),original);
   for(const auto& resource:mod->manifest.package.resources)buffers.emplace(std::make_pair(EiemResourceRecipe::Author,resource.first),std::shared_ptr<const Bytes>(resource.second,&resource.second->bytes));
   for(const auto& frame:mod->preparedFrames)buffers.emplace(std::make_pair(EiemResourceRecipe::ExpandedFrame,frame.first),std::shared_ptr<const Bytes>(mod,&frame.second));
   viewRecipes[i]=&mod->views.at(contribution.second);rawViews[i]=EiemCompiledInputView::Bind(*viewRecipes[i],buffers);report("view-bind-startup",begin);begin=std::chrono::steady_clock::now();
   plan.inputs[i].hash=plan.startupHashes[i]=mod->manifest.plan.startupHashes.at(contribution.second)==set->catalog[i].source.hash?set->catalog[i].source.hash:viewRecipes[i]->startupHash;report("view-startup-hash",begin);return;
  }}
  auto old=reusable(i);if(old!=SIZE_MAX&&!previous->views.count(old)){plan.inputs[i].hash=plan.startupHashes[i]=previous->manifest.plan.startupHashes[old];namespaces[i]=previous->namespaces[old];set->layouts[i]=previous->layouts[old];reusedInputs[i]=old;return;}
  auto begin=std::chrono::steady_clock::now();auto startup=Materialize(*set,i,false,&namespaces[i],&set->layouts[i],cost);report("replay",begin);begin=std::chrono::steady_clock::now();plan.inputs[i].hash=plan.startupHashes[i]=Hash(startup);report("startup-hash",begin);
  rawStartup[i]=std::make_shared<const Bytes>(std::move(startup));},4);
 SelectIsolatedInputs(*set);
 std::vector<std::shared_ptr<const Bytes>> isolatedInputs(plan.closure.size());
 std::vector<std::shared_ptr<const EiemInputSpanView::View>> isolatedViews(plan.closure.size());
 EiemOfflineCompile::ParallelInputs(plan.closure.size(),[&](size_t n){auto i=plan.closure[n];auto begin=std::chrono::steady_clock::now();
  if(viewRecipes[i]){isolatedViews[n]=EiemCompiledInputView::Bind(*viewRecipes[i],viewBuffers[i],plan.names);report("view-bind-isolated",begin);begin=std::chrono::steady_clock::now();plan.inputs[i].hash.clear();report("view-isolated-hash",begin);return;}
  auto old=reusedInputs[i];std::shared_ptr<const Bytes> prior;
  if(old!=SIZE_MAX){auto ready=previous->isolatedRefs.find(old);if(ready!=previous->isolatedRefs.end())prior=ready->second.lock();}
  if(prior){isolatedInputs[n]=std::make_shared<const Bytes>(EiemNativeInputOffsets::Relocate(*prior,set->layouts[i],namespaces[i],previous->manifest.plan.names,plan.names));report("rekey",begin);}
  else{auto startup=rawStartup[i];if(!startup){startup=std::make_shared<const Bytes>(Materialize(*set,i,false,&namespaces[i],&set->layouts[i]));report("replay",begin);begin=std::chrono::steady_clock::now();}
   isolatedInputs[n]=std::make_shared<const Bytes>(IsolatePreparedInput(*set,i,*startup,namespaces[i]));report("isolate",begin);}
  begin=std::chrono::steady_clock::now();plan.inputs[i].hash=Hash(*isolatedInputs[n]);report("isolated-hash",begin);rawStartup[i].reset();},4);
 for(size_t i=0;i<viewRecipes.size();++i)if(viewRecipes[i]){set->sourceBuffers.emplace(i,viewBuffers[i].at({EiemResourceRecipe::Original,{}}));set->sourceStamps.emplace(i,viewSourceStamps[i]);if(plan.startupHashes[i]!=plan.originals[i].hash)set->startupViews.emplace(i,std::move(rawViews[i]));}
 for(size_t n=0;n<isolatedInputs.size();++n){auto i=plan.closure[n];if(isolatedViews[n]){plan.viewIdentities.emplace(i,EiemCompiledInputView::Identity(*viewRecipes[i],plan.names));set->views.emplace(i,std::move(isolatedViews[n]));continue;}if(plan.startupHashes[i]!=plan.originals[i].hash)set->startup.emplace(i,isolatedInputs[n]);set->isolatedRefs.emplace(i,isolatedInputs[n]);set->isolated.emplace(i,std::move(isolatedInputs[n]));}
 Require(Unchanged(*set,directory),"Mod changed during compiled rule preparation");return set;
}
inline std::shared_ptr<EiemPrefabRules::Context> Context(const std::shared_ptr<const Set>& set){
 auto result=std::make_shared<EiemPrefabRules::Context>(std::make_shared<const Plan>(set->manifest.plan),set->manifest.package,[set](size_t i,bool isolated){if(isolated){auto ready=set->isolatedRefs.find(i);if(ready!=set->isolatedRefs.end())if(auto bytes=ready->second.lock())return *bytes;}else if(set->startup.count(i))return StartupInput(*set,i);return Materialize(*set,i,isolated);});
 result->startupInputs=set->startup;result->startupViews=set->startupViews;result->startupMaterializer=[set](size_t i){return StartupInput(*set,i);};
 auto graph=std::make_shared<EiemBundleDependencies::Names>();std::map<std::string,size_t> members;
 for(size_t i=0;i<set->catalog.size();++i)for(const auto& member:set->catalog[i].members){auto inserted=members.emplace(member.first,i);Require(inserted.second||inserted.first->second==i,"startup dependency CAB has multiple source bundles");}
 for(size_t i=0;i<set->contributors.size();++i)for(const auto& contributor:set->contributors[i]){
  const auto& artifact=*set->mods.at(contributor.first);
  if(!artifact.addedBundles.empty()){const auto& from=set->catalog.at(i).input.bundle;const auto& added=artifact.addedBundles.at(contributor.second);if(!added.empty()){Require(!from.empty(),"compiled dependency source Bundle absent");(*graph)[from].insert(added.begin(),added.end());}continue;}
  for(const auto& recipe:artifact.manifest.plan.namespaceRecipes.at(contributor.second)){
   const auto& proof=artifact.members.at(recipe.member);Require(ExternalPrefix(proof.before.externals,proof.after.externals),"startup dependency source external order differs");
   for(size_t n=proof.before.externals.size();n<proof.after.externals.size();++n){auto found=members.find(proof.after.externals[n]);Require(found!=members.end(),"compiled new dependency lacks a source bundle: "+proof.after.externals[n]);
    if(found->second==i)continue;const auto& from=set->catalog.at(i).input.bundle;const auto& to=set->catalog.at(found->second).input.bundle;
    Require(!from.empty()&&!to.empty(),"compiled new dependency lacks an original Bundle name");(*graph)[from].insert(to);
   }
  }
 }
 result->addedDependencies=std::move(graph);return result;
}
} // namespace EiemCompiledModSet
