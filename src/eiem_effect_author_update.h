#pragma once
#include "eiem_compiled_mod_store.h"
#include "eiem_prepared_prefab_compiler.h"

// Effects have their own native bundles. Updating their author/control graph
// changes the compiled author seal, without rebuilding character input views.
namespace EiemEffectAuthorUpdate {
using namespace EiemCompiledMod;
inline std::string CharacterContract(Package package){
 for(auto i=package.resources.begin();i!=package.resources.end();)if(i->second->kind==10)i=package.resources.erase(i);else ++i;
 return InputContractDigest(package);
}
struct Detached {bool views=false;std::set<size_t> proofs;};
inline Detached DetachEffects(Artifact& a){
 // Older dictionaries could reuse coincidentally equal character bytes from
 // an effect bundle. Freeze those spans before the effect bytes are replaced.
 // This preserves the exact native output, including namespace site indices.
 Detached result;
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
inline Bytes RebindPacket(const Bytes& bytes,const Package& author,const std::filesystem::path& previous){
 // DecodeRuntime has verified this envelope. Keep all native views, assemblies,
 // frames and compressed proof blocks byte-for-byte; replace only its manifest.
 EiemOfflinePackage::Reader packet{bytes};auto packetStructure=packet.S();auto packed=packet.Blob();auto plain=Unpack(packed);
 EiemOfflinePackage::Reader index{plain};auto indexStructure=index.S();auto manifestBytes=index.Blob();
 auto manifest=EiemOfflinePackage::Decode(manifestBytes,previous,previous.parent_path(),previous,true);
 manifest.package=author;manifest.plan.author=PackageDigest(author);manifest.plan.contract=InputContractDigest(author);
 Writer nextIndex;nextIndex.S(indexStructure);nextIndex.Blob(EiemOfflinePackage::Encode(manifest));nextIndex.Raw(plain.data()+index.p,plain.size()-index.p);
 Writer result;result.S(packetStructure);result.Blob(Pack(nextIndex.b));result.Raw(bytes.data()+packet.p,bytes.size()-packet.p-32);
 auto digest=Sha(result.b.data(),result.b.size());result.Raw(digest.data(),digest.size());return std::move(result.b);
}
inline void Update(const std::filesystem::path& mod,const std::filesystem::path& previous,const std::filesystem::path& game){
 Require(mod!=previous,"effect update requires private staging");auto author=Load(mod);auto priorPath=previous/"compiled.bin";
 auto store=EiemCompiledModStore::IsStore(priorPath);std::optional<EiemCompiledModStore::Loaded> loaded;
 Artifact artifact;if(store){loaded=EiemCompiledModStore::Load(priorPath,previous,game,true);artifact=loaded->artifact;}else artifact=ReadRuntime(previous,game);
 Require(author.id==artifact.manifest.package.id,"effect update belongs to another Mod");
 Require(CharacterContract(author)==CharacterContract(artifact.manifest.package),"effect update changes character resources; export character and effects together");
 auto detached=DetachEffects(artifact);
 if(!store){Bytes bytes;if(detached.views||!detached.proofs.empty()){
   auto owners=EiemCompiledSourceRecords::MemberInputs(artifact);
   for(size_t i=0;i<artifact.views.size();++i){artifact.retainedInputDirectories[i]=EiemCompiledSourceRecords::Directory(artifact,i,owners);if(!artifact.retainedProofBlocks.count(i))artifact.retainedProofBlocks[i]=artifact.proofBlocks->at(i);}
   artifact.manifest.package=author;artifact.manifest.plan.author=PackageDigest(author);artifact.manifest.plan.contract=InputContractDigest(author);bytes=Encode(artifact,game);
  }else bytes=RebindPacket(EiemNativeInput::Read(priorPath),author,previous);
  auto checked=DecodeRuntime(bytes,mod,game,{},&author);
  Require(checked.manifest.plan.author==PackageDigest(author)&&checked.manifest.plan.contract==InputContractDigest(author),"effect author index roundtrip differs");EiemOfflineCompile::AtomicSave(mod/"compiled.bin",bytes);return;}
 EiemCompiledModStore::Changes records;
 // Retain prepared native source discovery and its resolved Texture templates.
 // Its seal and author references must follow this new effect generation too.
 {EiemModUpdateStore::File file(priorPath,false);auto snapshot=EiemModUpdateStore::Read(file);
  Require(snapshot.generation==loaded->generation,"compiled Mod changed during effect update");
  if(snapshot.blocks.count("runtime/prepared-pfb")){
   Require(bool(loaded->selection),"prepared effect update lacks source selection");
   auto prepared=EiemPreparedPrefabCompiler::Decode(EiemModUpdateStore::Get(file,snapshot,"runtime/prepared-pfb"),std::make_shared<const Artifact>(artifact),*loaded->selection);
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
 // Never append through staging hardlinks to the published compiled.bin.
 auto path=mod/"compiled.bin",temporary=mod/".effect-compiled.bin";Require(!std::filesystem::exists(temporary),"effect index staging already exists");
 struct Cleanup{std::filesystem::path path;~Cleanup(){std::error_code error;std::filesystem::remove(path,error);}}cleanup{temporary};
 std::filesystem::copy_file(priorPath,temporary);EiemModUpdateStore::Commit(temporary,loaded->generation,records);
 auto checked=EiemCompiledModStore::Load(temporary,mod,game,true);Require(checked.artifact.manifest.plan.author==PackageDigest(author),"effect store author roundtrip differs");
 Require(MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0,"effect index staging publication failed");
}
} // namespace EiemEffectAuthorUpdate
