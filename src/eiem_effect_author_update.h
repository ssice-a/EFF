#pragma once
#include "eiem_published_compiled_mod.h"
#include "eiem_prepared_prefab_compiler.h"
#include "eiem_resource_snapshot_publish.h"

// Effects have their own native bundles. Updating their author/control graph
// changes the compiled author seal, without rebuilding character input views.
namespace EiemEffectAuthorUpdate {
using namespace EiemCompiledMod;
inline std::string CharacterContract(Package package){
 for(auto i=package.resources.begin();i!=package.resources.end();)if(i->second->kind==10)i=package.resources.erase(i);else ++i;
 return InputContractDigest(package);
}
struct Detached {bool views=false;std::set<size_t> proofs;};
inline Bytes FrozenEffectSources(const EiemModUpdateStore::Snapshot& snapshot,const EiemModUpdateStore::Changes& changes={}){
 std::map<std::string,std::string> identities;
 auto source=[](const std::string& key){return key.rfind("runtime/source/",0)==0||key.rfind("runtime/proof/",0)==0;};
 for(const auto& row:snapshot.blocks)if(source(row.first))identities[row.first]=row.second.digest;
 for(const auto& row:changes)if(source(row.first)){if(row.second)identities[row.first]=Hash(*row.second);else identities.erase(row.first);}
 Writer proof;for(const auto& row:identities){proof.S(row.first);proof.S(row.second);}
 Writer w;w.S("EFF_EFFECT_SOURCE_FREEZE/proof-identities/1");w.S(Hash(proof.b));return std::move(w.b);
}
inline Detached DetachEffects(Artifact& a){
 // Older dictionaries could reuse coincidentally equal character bytes from
 // an effect bundle. Freeze those spans before the effect bytes are replaced.
 // This preserves the exact native output, including namespace site indices.
 Detached result;
 if(a.explicitResources)return result;
 if(std::none_of(a.manifest.package.resources.begin(),a.manifest.package.resources.end(),[](const auto& row){return row.second->kind==10;}))return result;
 auto detach=[&](auto& spans){bool changed=false;for(auto& span:spans)if(span.kind==EiemResourceRecipe::Author){
  const auto& resource=*a.manifest.package.resources.at(span.source);if(resource.kind!=10)continue;
  Require(span.offset<=resource.bytes.size()&&span.size<=resource.bytes.size()-span.offset,"effect-backed native span extent differs");
  span.literal.assign(resource.bytes.begin()+span.offset,resource.bytes.begin()+span.offset+span.size);span.kind=EiemResourceRecipe::Literal;span.source.clear();span.offset=0;changed=true;
 }return changed;};
 for(size_t i=0;i<a.views.size();++i){result.views=detach(a.views[i].spans)||result.views;HydrateInput(a,i);
  for(auto& member:a.manifest.plan.memberRecipes.at(i))if(detach(member.spans))result.proofs.insert(i);
 }
 if(!result.proofs.empty()){
  auto plain=a;plain.proofBlocks.reset();plain.proofReader={};plain.retainedProofBlocks.clear();
  auto owners=EiemCompiledSourceRecords::MemberInputs(plain);
  for(auto i:result.proofs)a.retainedProofBlocks[i]=EiemCompiledSourceRecords::Proof(plain,i,EiemCompiledSourceRecords::Directory(plain,i,owners));
 }
 return result;
}
inline void Update(const std::filesystem::path& mod,const std::filesystem::path& previous,const std::filesystem::path& game){
 Require(mod!=previous,"effect update requires private staging");auto author=Load(mod);auto priorPath=previous/"compiled.bin";
 if(EiemAddressedResources::TryPublish(mod,previous,game,author,nullptr,{},true))return;
 auto loaded=EiemPublishedCompiledMod::Read(previous,game);auto store=loaded.store;auto artifact=std::move(loaded.artifact);
 Require(author.id==artifact.manifest.package.id,"effect update belongs to another Mod");
 Require(CharacterContract(author)==CharacterContract(artifact.manifest.package),"effect update changes character resources; export character and effects together");
 bool frozen=artifact.explicitResources;std::optional<EiemModUpdateStore::Snapshot> sourceSnapshot;
 if(store){EiemModUpdateStore::File file(priorPath,false);sourceSnapshot=EiemModUpdateStore::Read(file);const auto& snapshot=*sourceSnapshot;
  Require(snapshot.generation==loaded.generation,"compiled Mod changed before effect source validation");
  if(!frozen&&snapshot.blocks.count("runtime/effect-source-freeze")){
   auto bytes=EiemModUpdateStore::Get(file,snapshot,"runtime/effect-source-freeze");EiemOfflinePackage::Reader r{bytes};
   Require(r.S()=="EFF_EFFECT_SOURCE_FREEZE/proof-identities/1"&&DigestText(r.S())&&r.p==bytes.size(),"effect source freeze structure differs");
   frozen=bytes==FrozenEffectSources(snapshot);
  }
 }
 auto detached=frozen?Detached{}:DetachEffects(artifact);
 if(!store){
  // Migrate a legacy packet once. Subsequent effect edits change only the
  // author directory; they never encode every character input/proof again.
  std::optional<EiemStaticSources::Selection> selection;
  if(std::filesystem::is_regular_file(previous/"source-inputs.bin")){
   selection=EiemStaticSources::LoadMod(previous,game);EiemCompiledModStore::VerifySourceSelection(artifact,*selection);
  }
  artifact.manifest.package=author;artifact.manifest.plan.author=PackageDigest(author);artifact.manifest.plan.contract=InputContractDigest(author);
  auto catalog=loaded.catalog;
  if(catalog.domain.empty()){
   catalog.domain="verified-legacy-compiled-sources";
   for(const auto& source:artifact.manifest.plan.originals)catalog.entries.emplace(source.file,EiemResourceCatalogDelta::Entry{source.hash,source.physical.generic_u8string()+":"+std::to_string(source.offset)+":"+std::to_string(source.size),{}});
  }
  Bytes prepared;
  bool supportsPrepared=std::none_of(author.resources.begin(),author.resources.end(),[](const auto& row){return row.second->kind!=10&&(Begins(row.first,"resources/")||row.second->blocks.count("@physics"));});
  if(selection&&supportsPrepared)prepared=EiemPreparedPrefabCompiler::Encode(EiemPreparedPrefabCompiler::Prepare(std::make_shared<const Artifact>(artifact),*selection));
  auto path=mod/"compiled.bin",temporary=mod/".effect-compiled.bin";Require(!std::filesystem::exists(temporary),"effect index staging already exists");
  struct Cleanup{std::filesystem::path path;~Cleanup(){std::error_code error;std::filesystem::remove(path,error);}}cleanup{temporary};
  auto migrated=EiemCompiledModStore::Save(temporary,0,artifact,game,loaded.decisions,catalog,prepared.empty()?nullptr:&prepared,selection?&*selection:nullptr);
  EiemModUpdateStore::Commit(temporary,migrated.generation,{{"runtime/effect-source-freeze",FrozenEffectSources(migrated)}});
  {auto checked=EiemCompiledModStore::Load(temporary,mod,game,true);
   Require(checked.artifact.manifest.plan.author==PackageDigest(author)&&checked.artifact.manifest.plan.contract==InputContractDigest(author),"effect migration author roundtrip differs");}
  auto published=std::filesystem::exists(path)?ReplaceFileW(path.c_str(),temporary.c_str(),nullptr,0,nullptr,nullptr):MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_WRITE_THROUGH);
  auto error=published?ERROR_SUCCESS:GetLastError();Require(published!=0,"effect migration staging publication failed: "+std::to_string(error));return;
 }
 EiemCompiledModStore::Changes records;
 // Retain prepared native source discovery and its resolved Texture templates.
 // Its seal and author references must follow this new effect generation too.
 {EiemModUpdateStore::File file(priorPath,false);auto snapshot=EiemModUpdateStore::Read(file);
  Require(snapshot.generation==loaded.generation,"compiled Mod changed during effect update");
  if(snapshot.blocks.count("runtime/prepared-pfb")){
   Require(bool(loaded.selection),"prepared effect update lacks source selection");
   auto prepared=EiemPreparedPrefabCompiler::Decode(EiemModUpdateStore::Get(file,snapshot,"runtime/prepared-pfb"),std::make_shared<const Artifact>(artifact),*loaded.selection);
   auto textures=prepared.resolved.textures;prepared.resolved=author;prepared.resolved.textures=std::move(textures);
   auto next=artifact;next.manifest.package=author;next.manifest.plan.author=PackageDigest(author);next.manifest.plan.contract=InputContractDigest(author);prepared.baseline=std::make_shared<const Artifact>(std::move(next));
   records["runtime/prepared-pfb"]=EiemPreparedPrefabCompiler::Encode(prepared);
  }
 }
 artifact.manifest.package=author;artifact.manifest.plan.author=PackageDigest(author);artifact.manifest.plan.contract=InputContractDigest(author);
 records["runtime/common"]=EiemCompiledSourceRecords::Common(artifact);records["runtime/author-seal"]=EiemCompiledModStore::AuthorSeal(artifact);
 if(detached.views||!detached.proofs.empty()){
  auto owners=EiemCompiledSourceRecords::MemberInputs(artifact);
  for(size_t i=0;i<artifact.views.size();++i){auto directory=EiemCompiledSourceRecords::Directory(artifact,i,owners);const auto& id=artifact.manifest.plan.originals.at(i).file;
   records["runtime/source/"+id]=EiemCompiledSourceRecords::Source(artifact,i,game,directory);
   if(detached.proofs.count(i))records["runtime/proof/"+id]=artifact.retainedProofBlocks.at(i);
  }
 }
 if(!artifact.explicitResources)records["runtime/effect-source-freeze"]=FrozenEffectSources(*sourceSnapshot,records);
 // Fork immutable content blocks and atomically publish only a small index.
 // Never append through staging hardlinks to the published compiled.bin.
 auto path=mod/"compiled.bin";
 EiemCompiledModStore::PublishSnapshot(priorPath,path,loaded.generation,records);
 auto checked=EiemCompiledModStore::Load(path,mod,game,true);Require(checked.artifact.manifest.plan.author==PackageDigest(author),"effect store author roundtrip differs");
}
} // namespace EiemEffectAuthorUpdate
