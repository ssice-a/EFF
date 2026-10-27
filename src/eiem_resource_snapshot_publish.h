#pragma once
#include "eiem_addressed_resource_update.h"
#include "eiem_compiled_mod_store.h"

// Frequent author exports do not read source templates or object proofs. The
// committed store already authenticates those immutable blocks. Only the author
// declaration, typed bindings and changed derived buffers need to be read.
namespace EiemAddressedResources {
inline Bytes RebindCommon(const Bytes& encoded,const Artifact& before,const Artifact& after){
 auto common=EiemCompiledSourceRecords::CommonBytes(encoded);
 EiemOfflinePackage::Reader r{common};auto marker=r.S(),baseline=r.S(),key=r.S();r.S();r.S();r.verifiedAuthor=&before.manifest.package;
 const bool bindingLabels=marker=="EFF_COMPILED_SOURCE_COMMON/author+metadata+identity-edges+key-label/le64";
 Require(bindingLabels||marker=="EFF_COMPILED_SOURCE_COMMON/author+metadata+identity-edges/le64","resource snapshot author structure differs");
 ReadAuthor(r,before.manifest.package.root.parent_path(),true,before.manifest.package.root,bindingLabels);
 Writer oldFrames;oldFrames.N(before.manifest.plan.frameRecipes.size());for(const auto& frame:before.manifest.plan.frameRecipes)EiemCompiledSourceRecords::Frame(oldFrames,frame);
 Require(oldFrames.b.size()<=common.size()-r.p&&std::equal(oldFrames.b.begin(),oldFrames.b.end(),common.end()-oldFrames.b.size()),"resource snapshot frame suffix differs");
 Writer w;w.S("EFF_COMPILED_SOURCE_COMMON/author+metadata+identity-edges+key-label/le64");w.S(baseline);w.S(key);w.S(after.manifest.plan.author);w.S(after.manifest.plan.contract);WriteAuthor(w,after.manifest.package,true);
 w.Raw(common.data()+r.p,common.size()-r.p-oldFrames.b.size());w.N(after.manifest.plan.frameRecipes.size());for(const auto& frame:after.manifest.plan.frameRecipes)EiemCompiledSourceRecords::Frame(w,frame);return Pack(w.b);
}
inline bool SameSelection(const EiemStaticSources::Selection& current,const EiemStaticSources::Selection& prior){
 for(const auto& candidate:current.candidates)if(!prior.candidates.count(candidate))return false;
 for(const auto& row:current.inputs){auto old=prior.inputs.find(row.first);if(old==prior.inputs.end()||old->second.revision!=row.second.revision||old->second.physical!=row.second.physical||old->second.offset!=row.second.offset||old->second.size!=row.second.size)return false;}
 for(const auto& row:current.assets){auto old=prior.assets.find(row.first);if(old==prior.assets.end()||old->second!=row.second)return false;}
 for(const auto& row:current.dependencies){auto old=prior.dependencies.find(row.first);if(old==prior.dependencies.end()||old->second!=row.second)return false;}
 return true;
}
inline bool RequiresSemanticRebuild(const Package& author,const std::set<std::string>& changedResources){
 for(const auto& name:changedResources){auto resource=author.resources.find(name);if(resource!=author.resources.end()&&resource->second->kind==1)return true;}
 return false;
}
inline bool TryPublish(const std::filesystem::path& mod,const std::filesystem::path& previous,const std::filesystem::path& game,const Package& author,
 const EiemStaticSources::Selection* current=nullptr,const std::function<void(const char*,double)>& cost={},bool effectsOnly=false,bool currentSelectionComplete=false){
 auto path=previous/"compiled.bin";if(!std::filesystem::is_regular_file(path)||!EiemCompiledModStore::IsStore(path))return false;
 EiemModUpdateStore::File file(path,false);auto snapshot=EiemModUpdateStore::Read(file);
 if(!snapshot.blocks.count("runtime/resource-bindings")||!snapshot.blocks.count("discovery/source-selection"))return false;
 auto selected=EiemCompiledModStore::ReadSourceSelection(EiemModUpdateStore::Get(file,snapshot,"discovery/source-selection"),game);
 if(current&&!SameSelection(*current,selected))return false;
 if(current&&currentSelectionComplete&&!SameSelection(selected,*current))return false;
 auto common=EiemModUpdateStore::Get(file,snapshot,"runtime/common");std::vector<std::string> closure;
 auto before=EiemCompiledSourceRecords::ReadCommon(common,previous,closure,&author);Require(before.manifest.package.id==author.id,"resource update belongs to another Mod");
 if(effectsOnly){auto core=[](Package package){for(auto i=package.resources.begin();i!=package.resources.end();)if(i->second->kind==10)i=package.resources.erase(i);else ++i;return InputContractDigest(package);};if(core(author)!=core(before.manifest.package))return false;}
 before.explicitResources=before.manifest.plan.resourceAddressed=true;before.resourceBindingBytes=EiemModUpdateStore::Get(file,snapshot,"runtime/resource-bindings");
 if(!CurrentBindings(before.resourceBindingBytes))return false;
 // A binding decode authenticates resource ranges before any update. Static
 // pooled metadata and source schemas are not needed by the author fast path.
 std::set<std::string> changed;for(const auto& row:author.resources){auto old=before.manifest.package.resources.find(row.first);if(old==before.manifest.package.resources.end()||ResourceContentHash(*row.second)!=ResourceContentHash(*old->second))changed.insert(row.first);}
 // Mesh object bytes are also the baseline for the cross-version semantic
 // replay record. Rebuild them together so a later game update cannot replay
 // stale vertex/index data from the previous author snapshot.
 if(RequiresSemanticRebuild(author,changed))return false;
 for(const auto& frame:before.manifest.plan.frameRecipes)if(changed.count(frame.resource))before.preparedFrames.emplace(frame.resource,Unpack(EiemModUpdateStore::Get(file,snapshot,"runtime/frame/"+frame.resource)));
 for(const auto& row:snapshot.blocks)if(row.first.rfind("runtime/generated/",0)==0){auto name=row.first.substr(strlen("runtime/generated/"));size_t root;AffineMatrix matrix;std::string resource;
  if(DecodeBoundsOrigin(name,root,matrix,resource)&&changed.count(resource))before.preparedFrames.emplace(name,Unpack(EiemModUpdateStore::Get(file,snapshot,row.first)));}
 auto after=before;auto update=EiemResourceReloadPlan::Measure(cost,"resource-rebind-wall",[&]{return Rebind(after,author,cost);});if(!update.compatible)return false;
 auto nextCommon=RebindCommon(common,before,after);EiemModUpdateStore::Changes records{{"runtime/common",nextCommon},{"runtime/author-seal",EiemCompiledModStore::AuthorSeal(after)}};
 for(const auto& name:update.generated)records[(name.rfind("@bounds:",0)==0?"runtime/generated/":"runtime/frame/")+name]=Pack(after.preparedFrames.at(name));
 if(snapshot.blocks.count("runtime/prepared-pfb"))records["runtime/prepared-pfb"]=RebindPrepared(EiemModUpdateStore::Get(file,snapshot,"runtime/prepared-pfb"),before,after);
 if(current){selected.manifest=current->manifest;records["discovery/source-selection"]=EiemCompiledModStore::SourceSelection(selected,game);}
 // Verify the changed author seal and declaration before publishing. Unchanged
 // source/proof content keeps its validated digest and exact byte ownership.
 std::vector<std::string> checkedClosure;auto checked=EiemCompiledSourceRecords::ReadCommon(nextCommon,mod,checkedClosure,&author);
 Require(checkedClosure==closure&&checked.manifest.plan.author==PackageDigest(author),"resource snapshot author/closure roundtrip differs");
 EiemResourceReloadPlan::Measure(cost,"compiled-publish",[&]{EiemCompiledModStore::PublishSnapshot(path,mod/"compiled.bin",snapshot.generation,records);return 0;});
 if(current&&std::filesystem::is_regular_file(mod/"source-inputs.bin"))EiemOfflineCompile::AtomicSave(mod/"source-inputs.bin",EiemCompiledModStore::SourceSelection(selected,game));
 if(cost)cost("resource-snapshot-fast-path",1);return true;
}
} // namespace EiemAddressedResources
