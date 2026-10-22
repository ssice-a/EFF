#include "../../src/eiem_resource_compiler.h"
#include "../../src/eiem_effect_author_update.h"
#include "../../src/eiem_compiled_mod_set.h"
#include <iostream>
using namespace EiemCompiledMod;
namespace fs=std::filesystem;
Artifact ReadArtifact(const fs::path& mod,const fs::path& game){return EiemCompiledModStore::IsStore(mod/"compiled.bin")?EiemCompiledModStore::Load(mod/"compiled.bin",mod,game,true).artifact:ReadRuntime(mod,game);}
EiemCompiledInputView::Buffers Buffers(const Artifact& a,size_t i){
 EiemCompiledInputView::Buffers result;
 for(const auto& row:a.manifest.package.resources)result.emplace(std::make_pair(EiemResourceRecipe::Author,row.first),std::shared_ptr<const Bytes>(row.second,&row.second->bytes));
 for(const auto& row:a.preparedFrames)result.emplace(std::make_pair(EiemResourceRecipe::ExpandedFrame,row.first),std::shared_ptr<const Bytes>(&row.second,[](const Bytes*){}));
 const auto& source=a.manifest.plan.originals.at(i);result.emplace(std::make_pair(EiemResourceRecipe::Original,std::string{}),EiemCompiledInputView::Buffer(std::make_shared<const EiemReadonlyFileRange::Range>(source.physical,source.offset,source.size)));return result;
}
void Check(const fs::path& old,const fs::path& updated,const fs::path& game,bool replacement=false){
 auto before=ReadArtifact(old,game),after=ReadArtifact(updated,game);auto current=Load(updated);
 Require(PackageDigest(current)==after.manifest.plan.author&&InputContractDigest(current)==after.manifest.plan.contract,"cold compiled author differs");
 Require(EiemEffectAuthorUpdate::CharacterContract(before.manifest.package)==EiemEffectAuthorUpdate::CharacterContract(current),"character author changed");
 if(replacement){
  Require(after.manifest.package.effects.size()==before.manifest.package.effects.size()&&!current.effects.empty(),"effect replacement changed declarations");bool changed=false;
  for(const auto& row:current.effects){const auto& next=*row.second.native;const auto& prior=*before.manifest.package.effects.at(row.first).native;
   Require(next.bundles.size()==prior.bundles.size(),"envelope migration changed bundle graph");
   for(size_t i=0;i<next.bundles.size();++i){Require(EiemEffectNative::Archive(*next.bundles[i].bytes),"updated archive still lacks native game envelope");changed|=next.bundles[i].hash!=prior.bundles[i].hash;}
  }Require(changed,"effect replacement fixture did not change archive data");
 }else Require(after.manifest.package.effects.size()>before.manifest.package.effects.size(),"new effects absent from compiled author");
 Require(before.views.size()==after.views.size(),"native input count changed");
 for(size_t i=0;i<after.views.size();++i)Require(after.views[i].startupHash==before.views[i].startupHash&&EiemInputSpanView::Digest(*EiemCompiledInputView::Bind(after.views[i],Buffers(after,i)))==before.views[i].startupHash,"native view payload changed");
 auto ownersBefore=EiemCompiledSourceRecords::MemberInputs(before),ownersAfter=EiemCompiledSourceRecords::MemberInputs(after);
 for(size_t i=0;i<before.views.size();++i)if(EiemCompiledSourceRecords::Proof(before,i,EiemCompiledSourceRecords::Directory(before,i,ownersBefore))!=EiemCompiledSourceRecords::Proof(after,i,EiemCompiledSourceRecords::Directory(after,i,ownersAfter))){
  auto original=EndfieldInput::Decode(ReadSource({},before.manifest.plan.originals.at(i)));auto old=EiemCompiledModSet::Replay(before,i,original,before.preparedFrames),next=EiemCompiledModSet::Replay(after,i,original,after.preparedFrames);
  Require(old.members.size()==next.members.size(),"native recipe member count changed");for(size_t j=0;j<old.members.size();++j)Require(old.members[j].name==next.members[j].name&&old.members[j].bytes==next.members[j].bytes,"native replay output changed");
 }
 if(!EiemCompiledModStore::IsStore(updated/"compiled.bin")){
  auto graph=EiemCompiledModSet::Read(game,{},updated.parent_path());Require(graph&&graph->mods.size()==1&&graph->manifest.packages.front().effects.size()==current.effects.size(),"cold/F10 graph lost effects");
  auto reused=EiemCompiledModSet::Read(game,graph,updated.parent_path());Require(reused->mods.front()==graph->mods.front(),"unchanged character cannot reuse artifact");
 }else{
  auto loaded=EiemCompiledModStore::Load(updated/"compiled.bin",updated,game,true);EiemModUpdateStore::File file(updated/"compiled.bin",false);auto snapshot=EiemModUpdateStore::Read(file);
  auto prepared=EiemPreparedPrefabCompiler::Decode(EiemModUpdateStore::Get(file,snapshot,"runtime/prepared-pfb"),std::make_shared<const Artifact>(loaded.artifact),*loaded.selection);
  Require(prepared.resolved.effects.size()==current.effects.size(),"prepared descriptor lost current effects");
 }
 // Reject changed native resources before writing a replacement index.
 auto scratch=updated.parent_path()/".reject-native";Require(!fs::exists(scratch),"scratch exists");fs::copy(updated,scratch,fs::copy_options::recursive);
 struct Cleanup{fs::path p;~Cleanup(){std::error_code e;fs::remove_all(p,e);}}cleanup{scratch};
 auto ini=Read(scratch/"mod.ini");auto text=std::string(ini.begin(),ini.end());auto at=text.find("[Replace");Require(at!=text.npos,"native rule fixture absent");auto end=text.find('\n',at);text.insert(end+1,"skip=1\n");
 {std::ofstream out(scratch/"mod.ini",std::ios::binary|std::ios::trunc);out<<text;}
 auto index=Read(scratch/"compiled.bin");bool rejected=false;try{EiemEffectAuthorUpdate::Update(scratch,updated,game);}catch(const std::exception&){rejected=true;}
 Require(rejected&&Read(scratch/"compiled.bin")==index,"invalid native update changed compiled file");
 std::cout<<"PASS effect author update: cold reads, "<<after.views.size()<<" identical native payloads and replay proofs, "<<current.effects.size()<<" effects, reuse/prepared descriptor and native-edit rejection\n";
}
void MakeStore(const fs::path& source,const fs::path& destination,const fs::path& game){
 Require(!fs::exists(destination),"store destination exists");fs::create_directories(destination.parent_path());fs::copy(source,destination,fs::copy_options::recursive);
 auto artifact=ReadArtifact(source,game);artifact.manifest.package=Load(destination);auto selection=EiemStaticSources::LoadMod(source,game);
 auto prepared=EiemPreparedPrefabCompiler::Encode(EiemPreparedPrefabCompiler::Prepare(std::make_shared<const Artifact>(artifact),selection));
 fs::remove(destination/"compiled.bin");EiemCompiledModStore::Save(destination/"compiled.bin",0,artifact,game,{},EiemCompiledExportCarryover::Catalog(selection),&prepared,&selection);
 auto loaded=EiemCompiledModStore::Load(destination/"compiled.bin",destination,game,true);
 auto checked=EiemPreparedPrefabCompiler::Decode(prepared,std::make_shared<const Artifact>(loaded.artifact),*loaded.selection);
 std::cout<<"PASS store fixture with prepared native descriptor\n";
}
int wmain(int argc,wchar_t** argv){try{
 Require(argc==5,"--check/--check-replacement/--make-store <original> <updated> <game>");auto option=std::wstring(argv[1]);
 if(option==L"--check")Check(fs::canonical(argv[2]),fs::canonical(argv[3]),fs::canonical(argv[4]));
 else if(option==L"--check-replacement")Check(fs::canonical(argv[2]),fs::canonical(argv[3]),fs::canonical(argv[4]),true);
 else if(option==L"--make-store")MakeStore(fs::canonical(argv[2]),fs::absolute(argv[3]),fs::canonical(argv[4]));else Require(false,"unknown option");return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
