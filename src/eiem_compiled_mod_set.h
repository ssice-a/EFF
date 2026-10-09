#pragma once
#include "eiem_compiled_mod.h"

// Combines already compiled records. No resource builders, scene scans, game
// resource queries or author-side executables are called by this path.
namespace EiemCompiledModSet {
using namespace EiemCompiledMod;
struct Catalog {EiemResourceReloadPlan::Source source;Input input;std::map<std::string,MemberProof> members;std::vector<Root> pristine;};
struct Set {
 Manifest manifest;EiemOfflinePackage::Pointer pointer;
 std::vector<std::shared_ptr<const Artifact>> mods;
 std::vector<std::map<std::string,Bytes>> frames;
 std::vector<Catalog> catalog;
 std::vector<std::vector<std::pair<size_t,size_t>>> contributors;
 mutable std::map<size_t,std::shared_ptr<const Bytes>> isolated;
 std::map<size_t,std::weak_ptr<const Bytes>> isolatedRefs;
 std::map<size_t,std::shared_ptr<const Bytes>> startup;
 std::map<std::filesystem::path,InputFileStamp> files;
 std::map<std::filesystem::path,Bytes> iniBytes;
};
inline std::map<std::filesystem::path,InputFileStamp> AuthorStamps(const std::filesystem::path& root){
 std::map<std::filesystem::path,InputFileStamp> result;for(const auto& file:std::filesystem::recursive_directory_iterator(root))if(file.is_regular_file()){
  auto name=file.path().filename().u8string(),extension=file.path().extension().u8string();if(name=="mod.ini"||name=="compiled.bin"||extension==".mesh"||extension==".mat"||extension==".tex")result.emplace(file.path(),FileStamp(file.path()));}return result;
}
inline std::string DirectoryStamp(const std::filesystem::path& directory){
 Writer w;for(const auto& root:EiemComposedPackage::Roots(directory)){w.S(root.filename().u8string());for(const auto& file:AuthorStamps(root)){w.S(file.first.lexically_relative(root).u8string());w.N(file.second.bytes);w.N(file.second.written);}}return Hash(w.b);
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
inline size_t RelocateSite(const EiemLoadingGeneration::Patch& patch,const Bytes& fromBytes,const SerializedLayout& old,const Bytes& mergedBytes,const SerializedLayout& merged,const std::map<size_t,size_t>& externalSites){
 if(patch.offset<old.tableStart)return patch.offset;
 if(patch.offset>=old.tableEnd&&patch.offset<48+old.metadataSize){auto site=externalSites.find(patch.offset);Require(site!=externalSites.end(),"compiled metadata namespace site lacks external record ownership");return site->second;}
 for(const auto& row:old.objects)if(patch.offset>=row.second.offset&&patch.offset-row.second.offset<row.second.size){auto object=merged.objects.find(row.first);Require(object!=merged.objects.end(),"compiled namespace object absent");const auto& type=old.types.at(row.second.type);
  if(type.classId==142){auto fields=Fields(type,0,fromBytes,row.second.offset,row.second.offset+row.second.size,row.second.offset);auto next=Fields(merged.types.at(object->second.type),0,mergedBytes,object->second.offset,object->second.offset+object->second.size,object->second.offset);
   for(const auto& field:fields)if(type.nodes.at(field.second.node).type=="string"&&patch.offset==field.second.offset+4){Require(field.first=="m_Name"||field.first=="m_AssetBundleName","compiled Bundle namespace field differs");return next.at(field.first).offset+4;}
   throw std::runtime_error("compiled Bundle namespace site lacks string field ownership");
  }return object->second.offset+(patch.offset-row.second.offset);}
 throw std::runtime_error("compiled namespace site lacks exact metadata/object ownership");
}
inline EndfieldInput::Bundle Replay(const Artifact& a,size_t index,const EndfieldInput::Bundle& original,const std::map<std::string,Bytes>& frames){
 std::map<std::string,const Bytes*> members;for(const auto& m:original.members)members.emplace(m.name,&m.bytes);EndfieldInput::Bundle output=original;output.members.clear();
 for(const auto& recipe:a.manifest.plan.memberRecipes.at(index))output.members.push_back({recipe.name,recipe.flags,EiemResourceRecipe::Apply(recipe,members,a.manifest.package,frames)});return output;
}
inline Bytes Materialize(const Set& set,size_t index,bool isolated){
 const auto& catalog=set.catalog.at(index);auto input=ReadSource({},catalog.source);auto original=EndfieldInput::Decode(input);
 struct Edit {size_t mod=0,source=0;EndfieldInput::Bundle bundle;};std::vector<Edit> edits;
 for(const auto& contributor:set.contributors.at(index)){const auto& a=*set.mods.at(contributor.first);auto replay=Replay(a,contributor.second,original,set.frames.at(contributor.first));
  auto check=a.manifest.plan.startupHashes.at(contributor.second)==catalog.source.hash?input:EndfieldInput::Encode(input,replay.members);Require(Hash(check)==a.manifest.plan.startupHashes.at(contributor.second),"compiled member replay differs from offline proof: "+catalog.source.file);edits.push_back({contributor.first,contributor.second,std::move(replay)});}
 // An independent Mod already supplied the complete edits and exact sites.
 // Only shared inputs need record merging; avoid reparsing/rebuilding the rest.
 if(edits.size()==1){auto& edit=edits.front();const auto& a=*set.mods.at(edit.mod);if(!isolated){if(a.manifest.plan.startupHashes.at(edit.source)==catalog.source.hash)return input;return EndfieldInput::Encode(input,edit.bundle.members);}
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
  Require(old!=oldMembers.end(),"compiled serialized member lacks original");const auto& bytes=old->second->bytes;const auto& proof=catalog.members.at(row.first);const auto& layout=proof.before;
  auto mergedLayout=layout;std::map<std::string,size_t> fileIds;for(size_t i=0;i<layout.externals.size();++i)fileIds.emplace(layout.externals[i],i+1);
  for(const auto& variant:row.second){const auto& edit=edits.at(variant.first);const auto& own=set.mods.at(edit.mod)->members.at(row.first);Require(own.before.externals==layout.externals&&ExternalPrefix(layout.externals,own.after.externals),"compiled original external reference order differs");
   for(size_t i=layout.externals.size();i<own.after.externals.size();++i)if(fileIds.emplace(own.after.externals[i],mergedLayout.externals.size()+1).second){Require(mergedLayout.externals.size()<INT_MAX,"compiled FileID field overflow");mergedLayout.externals.push_back(own.after.externals[i]);}}
  std::map<int64_t,std::vector<Bytes>> changes;std::map<int64_t,AddedObject> additions;
  for(const auto& variant:row.second){const auto& edit=edits.at(variant.first);const auto& a=*set.mods.at(edit.mod);const auto& own=a.members.at(row.first);std::vector<size_t> files{0};for(size_t i=0;i<own.after.externals.size();++i)files.push_back(i<layout.externals.size()?i+1:fileIds.at(own.after.externals[i]));
   for(const auto& object:own.after.objects){auto before=layout.objects.find(object.first);auto payload=ObjectBytes(variant.second->bytes,object.second);if(before!=layout.objects.end()&&payload==ObjectBytes(bytes,before->second))continue;
    RebaseFileIds(own.after.types.at(object.second.type),payload,files);
    for(const auto& site:own.streams)if(site.object==object.first){auto shift=streamShift.find({variant.first,site.member});if(shift==streamShift.end()||!shift->second)continue;auto source=oldMembers.find(site.member);size_t extent=source==oldMembers.end()?0:source->second->bytes.size();if(site.value<extent)continue;Require(Le(payload,site.offset,site.width)==site.value,"compiled stream relocation source differs");WriteInteger(payload,site.offset,AddSize(size_t(site.value),shift->second),site.width);}
    if(before!=layout.objects.end()){Require(object.second.type==before->second.type,"compiled object type changed");changes[object.first].push_back(std::move(payload));}
    else{AddedObject next{object.first,object.second.type,std::move(payload)};auto added=additions.find(object.first);if(added==additions.end())additions.emplace(object.first,std::move(next));else Require(added->second.type==next.type&&added->second.bytes==next.bytes,"compiled added object PathID collision");}
   }
  }
  std::map<int64_t,Bytes> mergedChanges;for(const auto& change:changes){const auto& object=layout.objects.at(change.first);try{mergedChanges.emplace(change.first,MergeObject(layout.types.at(object.type),ObjectBytes(bytes,object),change.second));}catch(const std::exception& error){throw std::runtime_error(catalog.source.file+" / "+row.first+" / PathID "+std::to_string(change.first)+": "+error.what());}}
  std::vector<AddedObject> added;for(auto& item:additions)added.push_back(std::move(item.second));auto merged=BuildSerialized(bytes,mergedLayout,mergedChanges,added);
  if(isolated){auto result=ParseSerialized(merged);std::map<size_t,EiemLoadingGeneration::Patch> patches;auto mergedExternals=EiemLoadingGraphAudit::ReadExternals(merged,result);std::map<std::string,size_t> externalPaths;for(const auto& ext:mergedExternals)externalPaths.emplace(ext.member,ext.pathOffset);
   auto add=[&](const std::vector<EiemLoadingGeneration::Patch>& list,const Bytes& fromBytes,const SerializedLayout& from){std::map<size_t,size_t> externalSites;for(const auto& ext:EiemLoadingGraphAudit::ReadExternals(fromBytes,from))externalSites.emplace(ext.pathOffset,ext.fileId<=layout.externals.size()?mergedExternals.at(ext.fileId-1).pathOffset:externalPaths.at(ext.member));
    for(const auto& patch:list){auto site=RelocateSite(patch,fromBytes,from,merged,result,externalSites);auto after=NamespaceName(patch.before,set.manifest.plan.names);Require(after.size()==patch.before.size()&&site<=merged.size()&&after.size()<=merged.size()-site&&std::equal(patch.before.begin(),patch.before.end(),merged.begin()+site),"compiled namespace relocation source differs");auto inserted=patches.emplace(site,EiemLoadingGeneration::Patch{site,patch.before,after});Require(inserted.second||inserted.first->second.after==after,"compiled namespace relocation conflict");}};
   if(row.second.empty())add(proof.pristineNamespace,bytes,proof.before);
   else for(const auto& variant:row.second){const auto& edit=edits.at(variant.first);const auto& a=*set.mods.at(edit.mod);for(const auto& recipe:a.manifest.plan.namespaceRecipes.at(edit.source))if(recipe.member==row.first)add(recipe.patches,variant.second->bytes,a.members.at(row.first).after);}
   std::vector<EiemLoadingGeneration::Patch> list;for(auto& item:patches)list.push_back(std::move(item.second));merged=EiemLoadingGeneration::ApplyProven(merged,Hash(merged),std::move(list));
  }
  output.push_back({row.first,old->second->flags,std::move(merged)});
 }
 // Keep pristine member order; newly introduced streams follow it.
 std::map<std::string,size_t> ordinal;for(size_t i=0;i<original.members.size();++i)ordinal.emplace(original.members[i].name,i);
 std::stable_sort(output.begin(),output.end(),[&](const auto& a,const auto& b){auto ai=ordinal.find(a.name),bi=ordinal.find(b.name);return (ai==ordinal.end()?SIZE_MAX:ai->second)<(bi==ordinal.end()?SIZE_MAX:bi->second);});
 if(!isolated&&output.size()==original.members.size()&&std::equal(output.begin(),output.end(),original.members.begin(),[](const auto& a,const auto& b){return a.name==b.name&&a.flags==b.flags&&a.bytes==b.bytes;}))return input;
 if(isolated)for(auto& m:output)m.name=set.manifest.plan.names.members.at(m.name);return EndfieldInput::Encode(input,output);
}
inline EiemRenderAssemblyDescription::Description MergeAssembly(const Root& pristine,const std::vector<std::pair<const Root*,std::set<std::pair<std::string,int64_t>>>>& variants){
 auto result=pristine.assembly;if(result.sources.empty())return result;for(auto& source:result.sources){const EiemRenderAssemblyDescription::Source* selected=nullptr;for(const auto& variant:variants)if(variant.second.count({source.cab,source.id})){auto next=std::find_if(variant.first->assembly.sources.begin(),variant.first->assembly.sources.end(),[&](const auto& s){return s.cab==source.cab&&s.id==source.id;});Require(next!=variant.first->assembly.sources.end()&&!selected,"compiled Renderer source conflict/missing");selected=&*next;}if(selected)source=*selected;}
 auto expand=[&](std::vector<EiemRenderAssemblyDescription::Address>& list){std::vector<EiemRenderAssemblyDescription::Address> output;for(const auto& address:list){auto source=std::find_if(result.sources.begin(),result.sources.end(),[&](const auto& s){return s.renderer==address;});Require(source!=result.sources.end(),"compiled LOD/helper address unowned");for(const auto& target:source->targets)output.push_back(target.renderer);}list=std::move(output);};
 for(auto& lod:result.lods)for(auto& level:lod.levels)expand(level.renderers);
 for(auto& helper:result.helpers){expand(helper.renderers);for(const auto& variant:variants){auto next=std::find_if(variant.first->assembly.helpers.begin(),variant.first->assembly.helpers.end(),[&](const auto& h){return h.component==helper.component;});if(next!=variant.first->assembly.helpers.end())helper.custom=helper.custom||next->custom;}}
 return result;
}
inline void SelectIsolatedInputs(Set& set){
 auto& plan=set.manifest.plan;Require(plan.startupHashes.size()==set.catalog.size(),"compiled source hash coverage differs");
 std::set<size_t> isolated;for(const auto& root:plan.roots){Require(root.input<set.catalog.size(),"compiled root input differs");isolated.insert(root.input);}
 for(size_t i=0;i<set.catalog.size();++i)if(plan.startupHashes[i]!=set.catalog[i].source.hash)isolated.insert(i);
 // A new Prefab/edited object needs an independent identity. Unchanged
 // dependencies keep the game's CAB identity and ResourceManager ownership;
 // opening renamed copies would duplicate their textures and GPU resources.
 for(size_t i=0;i<set.catalog.size();++i){const auto& catalog=set.catalog[i];bool own=isolated.count(i)!=0;
  for(const auto& member:catalog.members){auto& name=plan.names.members.at(member.first);name=own?"CAB-"+EiemLoadingGeneration::Token(plan.key,member.first,32):member.first;
   if(!own)plan.shared.insert(member.first);}
  if(!catalog.input.bundle.empty())plan.names.bundles.at(catalog.input.bundle)=own?EiemLoadingGeneration::BundleIdentity(plan.key,catalog.input.bundle):catalog.input.bundle;
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
inline std::shared_ptr<Set> Read(const std::filesystem::path& game,const std::shared_ptr<const Set>& previous={},const std::filesystem::path& modsDirectory={}){
 auto directory=modsDirectory.empty()?game/"plugin/mods":modsDirectory;auto roots=EiemComposedPackage::Roots(directory);if(roots.empty()&&!previous)return {};
 if(auto refreshed=RefreshKeySwitches(previous,directory))return refreshed;
 auto set=std::make_shared<Set>();std::string identity=structure;std::vector<Package> authors;
 for(const auto& root:roots){Require(std::filesystem::is_regular_file(root/"compiled.bin"),"Mod lacks compiled.bin; export this Mod with the Blender plug-in: "+root.filename().u8string());auto stamps=AuthorStamps(root);auto a=std::make_shared<Artifact>(EiemCompiledMod::Read(root,game));auto current=Load(root);Require(InputContractDigest(current)==a->manifest.plan.contract,"Mod resource declarations differ from compiled rules; re-export with Blender: "+current.id);auto after=AuthorStamps(root);Require(stamps.size()==after.size(),"Mod files changed while reading compiled rules");for(const auto& row:stamps)Require(after.count(row.first)&&SameStamp(row.second,after.at(row.first)),"Mod changed while reading compiled rules");a->manifest.package=current;
  identity+='\n'+root.filename().u8string()+":"+a->digest+":"+a->manifest.plan.contract;authors.push_back(current);set->mods.push_back(a);std::map<std::string,Bytes> frames;for(const auto& frame:a->manifest.plan.frameRecipes)frames.emplace(frame.resource,EiemResourceRecipe::ExpandFrame(frame,current.resources.at(frame.resource)->bytes));set->frames.push_back(std::move(frames));
  auto ini=EiemNativeInput::Read(root/"mod.ini");Require(Sha(ini.data(),ini.size())==current.iniHash,"Mod INI changed while capturing compiled snapshot");set->iniBytes.emplace(root,std::move(ini));set->files.insert(stamps.begin(),stamps.end());
 }
 auto& manifest=set->manifest;manifest.composed=true;manifest.rootAssembly=true;manifest.packages=authors;manifest.package=EiemComposedPackage::Compose(authors,true,false);auto& plan=manifest.plan;plan.composed=true;plan.rulesOnly=true;plan.key=Hash(Bytes(identity.begin(),identity.end()));plan.author=PackageDigest(manifest.package);plan.contract=InputContractDigest(manifest.package);
 std::map<std::string,size_t> inputs;std::map<std::string,Root> pristine;
 auto add=[&](const Catalog& entry){auto old=inputs.find(entry.source.file);if(old!=inputs.end()){auto& existing=set->catalog.at(old->second);const auto& source=existing.source;Require(source.hash==entry.source.hash&&source.physical==entry.source.physical&&source.offset==entry.source.offset&&source.size==entry.source.size,"compiled shared source range conflict");existing.input.externals.insert(entry.input.externals.begin(),entry.input.externals.end());return old->second;}size_t i=set->catalog.size();inputs.emplace(entry.source.file,i);set->catalog.push_back(entry);set->contributors.emplace_back();return i;};
 if(previous)for(size_t old=0;old<previous->catalog.size();++old)if(!previous->contributors[old].empty()){const auto& catalog=previous->catalog[old];auto i=add(catalog);for(auto root:catalog.pristine){root.input=i;pristine.emplace(root.path,std::move(root));}}
 std::map<std::string,std::vector<std::pair<const Root*,std::set<std::pair<std::string,int64_t>>>>> rootVariants;
 for(size_t m=0;m<set->mods.size();++m){const auto& a=*set->mods[m];const auto& own=a.manifest.plan;const auto& package=a.manifest.package;
  for(size_t i=0;i<own.inputs.size();++i){Catalog catalog;catalog.source=own.originals[i];catalog.input=own.inputs[i];for(const auto& recipe:own.memberRecipes[i]){auto proof=a.members.find(recipe.name);if(proof!=a.members.end())catalog.members.emplace(*proof);}for(const auto& root:a.pristine)if(root.input==i)catalog.pristine.push_back(root);auto next=add(catalog);set->contributors[next].push_back({m,i});
   for(auto root:catalog.pristine){root.input=next;pristine.emplace(root.path,std::move(root));}for(const auto& root:own.roots)if(root.input==i){std::set<std::pair<std::string,int64_t>> affected;for(const auto& rule:own.rendererRules[i])affected.emplace(rule.cab,rule.id);rootVariants[root.path].push_back({&root,std::move(affected)});}
  }
  for(const auto& row:own.parts){auto part=row.second;part.id=EiemComposedPackage::Name(package.id,part.id);part.material=EiemComposedPackage::Name(package.id,part.material);Require(plan.parts.emplace(row.first,std::move(part)).second,"compiled part marker conflict");}
  for(const auto& row:own.materials)Require(plan.materials.emplace(EiemComposedPackage::Name(package.id,row.first),row.second).second,"compiled material identity conflict");plan.shared.insert(own.shared.begin(),own.shared.end());plan.meshParts+=own.meshParts;plan.rendererParts+=own.rendererParts;plan.textures+=own.textures;plan.materialMatches+=own.materialMatches;
 }
 for(const auto& catalog:set->catalog)identity+='\n'+catalog.source.file+":"+catalog.source.hash;plan.key=Hash(Bytes(identity.begin(),identity.end()));
 for(size_t i=0;i<set->catalog.size();++i){auto& catalog=set->catalog[i];plan.originals.push_back(catalog.source);plan.inputs.push_back(catalog.input);plan.inputs.back().hash.clear();plan.inputs.back().path.clear();plan.rendererRules.emplace_back();plan.startupHashes.emplace_back();for(const auto& row:catalog.members)plan.names.members.emplace(row.first,"CAB-"+EiemLoadingGeneration::Token(plan.key,row.first,32));
  if(!catalog.input.bundle.empty())plan.names.bundles.emplace(catalog.input.bundle,EiemLoadingGeneration::BundleIdentity(plan.key,catalog.input.bundle));plan.closure.push_back(i);
 }
 for(const auto& mod:set->mods)for(const auto& member:mod->manifest.plan.names.members)if(!plan.names.members.count(member.first)){auto cab=member.first.substr(0,36);Require(member.first==cab+".resS"&&plan.names.members.count(cab),"compiled stream name lacks owning CAB");plan.names.members.emplace(member.first,plan.names.members.at(cab)+".resS");}
 if(previous)for(const auto& member:previous->manifest.plan.names.members)if(!plan.names.members.count(member.first)){auto cab=member.first.substr(0,36);if(member.first==cab+".resS"&&plan.names.members.count(cab))plan.names.members.emplace(member.first,plan.names.members.at(cab)+".resS");}
 for(auto& row:pristine){auto root=row.second;root.markers.clear();root.assembly=MergeAssembly(root,rootVariants[row.first]);for(const auto& variant:rootVariants[row.first])for(const auto& marker:variant.first->markers)root.markers[marker.first]+=marker.second;plan.roots.push_back(std::move(root));}
 manifest.baseline=plan.key;set->pointer={plan.key,plan.key};
 // INI controls and unchanged re-exports refresh the author snapshot, but
 // retain the already verified native resource inputs and their hashes.
 if(previous&&set->pointer==previous->pointer){
  Require(plan.inputs.size()==previous->manifest.plan.inputs.size(),"same compiled identity changed input count");
  plan.startupHashes=previous->manifest.plan.startupHashes;for(size_t i=0;i<plan.inputs.size();++i)plan.inputs[i].hash=previous->manifest.plan.inputs[i].hash;
  SelectIsolatedInputs(*set);set->startup=previous->startup;set->isolatedRefs=previous->isolatedRefs;for(const auto& row:set->isolatedRefs)if(auto bytes=row.second.lock())set->isolated.emplace(row.first,std::move(bytes));
  Require(Unchanged(*set,directory),"Mod changed during compiled snapshot refresh");return set;
 }
 for(size_t i=0;i<plan.inputs.size();++i){auto startup=Materialize(*set,i,false);plan.inputs[i].hash=plan.startupHashes[i]=Hash(startup);if(plan.startupHashes[i]!=plan.originals[i].hash)set->startup.emplace(i,std::make_shared<const Bytes>(Pack(startup)));}
 SelectIsolatedInputs(*set);
 for(auto i:plan.closure){auto bytes=std::make_shared<const Bytes>(Materialize(*set,i,true));plan.inputs[i].hash=Hash(*bytes);set->isolatedRefs.emplace(i,bytes);set->isolated.emplace(i,std::move(bytes));}
 Require(Unchanged(*set,directory),"Mod changed during compiled rule preparation");return set;
}
inline std::shared_ptr<EiemPrefabRules::Context> Context(const std::shared_ptr<const Set>& set){
 auto result=std::make_shared<EiemPrefabRules::Context>(std::make_shared<const Plan>(set->manifest.plan),set->manifest.package,[set](size_t i,bool isolated){if(isolated){auto ready=set->isolated.find(i);if(ready!=set->isolated.end())return *ready->second;}return Materialize(*set,i,isolated);});
 result->startupInputs=set->startup;result->startupInputsPacked=true;
 auto graph=std::make_shared<EiemBundleDependencies::Names>();std::map<std::string,size_t> members;
 for(size_t i=0;i<set->catalog.size();++i)for(const auto& member:set->catalog[i].members){auto inserted=members.emplace(member.first,i);Require(inserted.second||inserted.first->second==i,"startup dependency CAB has multiple source bundles");}
 for(size_t i=0;i<set->contributors.size();++i)for(const auto& contributor:set->contributors[i]){
  const auto& artifact=*set->mods.at(contributor.first);
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
