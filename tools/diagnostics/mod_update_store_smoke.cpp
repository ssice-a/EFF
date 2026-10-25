#include "../../src/eiem_mod_update_store.h"
#include "../../src/eiem_consumer_update_records.h"
#include <iostream>
using namespace EiemModUpdateStore;
template<class F> void Reject(F action){bool rejected=false;try{action();}catch(const std::exception&){rejected=true;}Require(rejected,"invalid update accepted");}
int main(){try{
 auto folder=std::filesystem::temp_directory_path()/("eff-mod-update-"+std::to_string(GetCurrentProcessId()));Require(!std::filesystem::exists(folder),"fixture folder exists");std::filesystem::create_directory(folder);
 struct Cleanup{std::filesystem::path path;~Cleanup(){std::filesystem::remove_all(path);}}cleanup{folder};auto path=folder/"compiled.bin";
 namespace Records=EiemConsumerUpdateRecords;auto hash=[](unsigned char v){return EiemOfflineCompile::Digest(Bytes{v});};Records::Stamp stamp{hash(1),hash(2),hash(3),"mesh-pptr+root-owner"};
 Records::Decisions decisions;decisions["bundle-a"]={stamp,{{"a.prefab","CAB-a",10,{13,14}},{"b.prefab","CAB-a",20,{23}},{"c.prefab","CAB-b",10,{13}}}};
 decisions["bundle-negative"]={stamp,{}};auto encoded=Records::Encode(decisions);auto decoded=Records::Decode(encoded);
 std::map<std::string,Records::Stamp> candidates{{"bundle-a",stamp},{"bundle-negative",stamp}};Require(Records::Pending(candidates,decoded).empty(),"completed positive/negative rechecked");
 auto changed=stamp;changed.dependencies=hash(4);candidates["bundle-negative"]=changed;candidates["new-bundle"]=stamp;
 Require(Records::Pending(candidates,decoded)==std::set<std::string>{"bundle-negative","new-bundle"},"new source/dependency invalidation missing");
 candidates["bundle-a"].source=hash(5);Require(Records::Pending(candidates,decoded).size()==3,"changed same-Bundle content was skipped");
 auto invalid=decisions;invalid.at("bundle-a").roots.push_back(invalid.at("bundle-a").roots.front());Reject([&]{Records::Encode(invalid);});
 Bytes compiled(1024*1024,42);auto first=Commit(path,0,{{"compiled/input-a",compiled},{"discovery/completed",encoded}});
 auto oldOffset=first.blocks.at("compiled/input-a").offset;auto second=Commit(path,first.generation,{{"compiled/input-b",Bytes{1,2,3}}});
 Require(second.generation==2&&second.blocks.at("compiled/input-a").offset==oldOffset,"append rewrote unrelated compiled block");
 {File file(path,false);auto restarted=Read(file);Require(restarted.generation==2&&Get(file,restarted,"discovery/completed")==encoded&&Get(file,restarted,"compiled/input-a")==compiled,"restart did not retain compiled/negative decisions");}
 auto unchanged=Commit(path,2,{{"compiled/input-a",compiled}});Require(unchanged.generation==2,"unchanged payload produced a generation");
 Reject([&]{Commit(path,1,{{"compiled/input-c",Bytes{4}}});});
 {File file(path,true);file.WriteAt(file.Size(),Bytes{7,8,9});file.Flush();}Require(EiemModUpdateStore::Read(path).generation==2,"uncommitted tail replaced completed index");
 auto third=Commit(path,2,{{"compiled/input-b",Bytes{4,5,6}}});Require(third.generation==3,"next commit failed");
 {File file(path,true);file.WriteAt(16+slotBytes,Bytes(8,0));file.Flush();}auto recovered=EiemModUpdateStore::Read(path);Require(recovered.generation==2,"torn commit did not recover previous generation");
 {File file(path,false);Require(Get(file,recovered,"compiled/input-b")==Bytes({1,2,3}),"recovery exposed new uncommitted payload");}
 auto next=Commit(path,2,{{"compiled/input-b",Bytes{8,8,8}},{"discovery/completed",std::nullopt}});auto oldSize=std::filesystem::file_size(path);
 auto compact=Compact(path,next.generation);Require(compact.generation==next.generation+1&&std::filesystem::file_size(path)<oldSize&&!compact.blocks.count("discovery/completed"),"single-file compaction lost generation/removal");
 {File file(path,false);Require(Get(file,compact,"compiled/input-a")==compiled&&Get(file,compact,"compiled/input-b")==Bytes({8,8,8}),"compaction changed live blocks");}
 Require(std::distance(std::filesystem::directory_iterator(folder),std::filesystem::directory_iterator{})==1,"compaction left permanent extra files");
 std::atomic<unsigned> committed{0},rejected{0};auto writer=[&](const char* key){try{Commit(path,compact.generation,{{key,Bytes{1}}});++committed;}catch(const std::exception&){++rejected;}};
 std::thread left(writer,"writer-a"),right(writer,"writer-b");left.join();right.join();Require(committed==1&&rejected==1,"concurrent writers lost an update or committed a stale index");
 {File file(path,true);file.WriteAt(compact.blocks.at("compiled/input-a").offset,Bytes{0});file.Flush();}Reject([&]{File file(path,false);Get(file,Read(file),"compiled/input-a");});
 auto corrupt=folder/"corrupt.bin";Commit(corrupt,0,{{"a",Bytes{1}}});{File file(corrupt,true);file.WriteAt(16+slotBytes,Bytes(8,0));file.Flush();}Reject([&]{EiemModUpdateStore::Read(corrupt);});
 std::cout<<"PASS one-file incremental blocks, unchanged block reuse, completed positive/negative records, source/dependency invalidation, same-Bundle multi-root identities, torn commit recovery, stale writer rejection, compaction and payload integrity\n";
 auto legacy=folder/"legacy"/"compiled.bin";std::filesystem::create_directory(legacy.parent_path());auto legacyIndex=Commit(legacy,0,{{"large",compiled},{"removed",Bytes{7}},{"small",Bytes{1}}});
 auto firstPath=folder/"first"/"compiled.bin";std::filesystem::create_directory(firstPath.parent_path());auto external=Fork(legacy,firstPath,legacyIndex.generation,{});
 Require(external.external&&std::filesystem::file_size(firstPath)<4096,"external fork retained the large packet payload");
 {File file(firstPath,false);Require(Get(file,external,"large")==compiled,"v1 to v2 migration changed payload");}
 auto secondPath=folder/"second"/"compiled.bin";std::filesystem::create_directory(secondPath.parent_path());auto edited=Fork(firstPath,secondPath,external.generation,{{"small",Bytes{2}},{"removed",std::nullopt}});
 Require(edited.external&&edited.generation==external.generation+1&&!edited.blocks.count("removed")&&std::filesystem::file_size(secondPath)<4096,"external delta generation/removal differs");
 auto oldBlob=firstPath.parent_path()/std::filesystem::u8path(external.blocks.at("large").external),newBlob=secondPath.parent_path()/std::filesystem::u8path(edited.blocks.at("large").external);
 Require(std::filesystem::equivalent(oldBlob,newBlob),"unchanged same-volume block was copied instead of hard-linked");
 {File before(firstPath,false),after(secondPath,false);Require(Get(before,external,"small")==Bytes{1}&&Get(after,edited,"small")==Bytes{2},"external publication modified old generation");}
 auto thirdPath=folder/"third"/"compiled.bin";std::filesystem::create_directory(thirdPath.parent_path());auto same=Fork(secondPath,thirdPath,edited.generation,{{"small",Bytes{2}}});
 Require(same.generation==edited.generation&&std::filesystem::equivalent(newBlob,thirdPath.parent_path()/std::filesystem::u8path(same.blocks.at("large").external)),"unchanged external snapshot rewrote content or advanced generation");
 auto crossRoot=std::filesystem::absolute(std::filesystem::current_path()/(".eiem-cross-volume-"+std::to_string(GetCurrentProcessId()))).lexically_normal();
 if(crossRoot.root_name()!=folder.root_name()){
  Require(std::filesystem::create_directory(crossRoot),"cross-volume fixture exists");Cleanup crossCleanup{crossRoot};auto crossPath=crossRoot/"compiled.bin";auto copied=Fork(firstPath,crossPath,external.generation,{});
  File file(crossPath,false);Require(Get(file,copied,"large")==compiled&&!std::filesystem::equivalent(oldBlob,crossRoot/std::filesystem::u8path(copied.blocks.at("large").external)),"cross-volume external block copy changed content");
  std::cout<<"PASS v2 cross-volume external content copy\n";
 }
 Reject([&]{Fork(secondPath,thirdPath,edited.generation-1,{});});
 auto changedExternal=Commit(secondPath,edited.generation,{{"small",Bytes{3}}});Require(changedExternal.generation==edited.generation+1,"external append did not commit");
 {File file(secondPath,true);file.WriteAt(16+(changedExternal.generation%2)*slotBytes,Bytes(8,0));file.Flush();}
 auto recoveredExternal=EiemModUpdateStore::Read(secondPath);{File file(secondPath,false);Require(recoveredExternal.generation==edited.generation&&Get(file,recoveredExternal,"small")==Bytes{2},"external torn commit did not restore prior content");}
 auto compactExternal=Compact(secondPath,recoveredExternal.generation);{File file(secondPath,false);Require(compactExternal.external&&Get(file,compactExternal,"large")==compiled,"external compaction lost immutable content");}
 auto freshPath=folder/"fresh"/"compiled.bin";std::filesystem::create_directory(freshPath.parent_path());auto fresh=Fork({},freshPath,0,{{"a",Bytes{4}}});Require(fresh.external&&fresh.generation==1,"fresh external snapshot generation differs");
 auto reapPath=folder/"reap"/"compiled.bin";std::filesystem::create_directory(reapPath.parent_path());Bytes retiredBytes(4096,0x31),liveBytes(8192,0x72);
 auto reapFirst=Fork({},reapPath,0,{{"mutable",retiredBytes}});auto retiredBlob=reapPath.parent_path()/std::filesystem::u8path(reapFirst.blocks.at("mutable").external);File retiredReader(retiredBlob,false);
 auto reapNext=Commit(reapPath,reapFirst.generation,{{"mutable",liveBytes}});Require(reapNext.generation==reapFirst.generation+1,"reaper fixture update did not commit");
 Require(ReapOrphanBlobs(reapPath,1)==retiredBytes.size()&&!std::filesystem::exists(retiredBlob),"orphan external block was not reaped");
 Require(retiredReader.ReadAt(0,retiredBytes.size())==retiredBytes,"reaping invalidated a pinned old-generation reader");
 {File current(reapPath,false);auto snapshot=Read(current);auto liveBlob=reapPath.parent_path()/std::filesystem::u8path(snapshot.blocks.at("mutable").external);Require(std::filesystem::exists(liveBlob)&&Get(current,snapshot,"mutable")==liveBytes,"reaping removed or changed the live block");}
 auto autoReapPath=folder/"auto-reap"/"compiled.bin";std::filesystem::create_directory(autoReapPath.parent_path());Bytes autoRetired(1024*1024,0x19),autoLive(1024*1024,0x91);
 auto autoFirst=Fork({},autoReapPath,0,{{"mutable",autoRetired}});auto autoRetiredBlob=autoReapPath.parent_path()/std::filesystem::u8path(autoFirst.blocks.at("mutable").external);File autoReader(autoRetiredBlob,false);
 auto autoNext=Commit(autoReapPath,autoFirst.generation,{{"mutable",autoLive}});Require(autoNext.generation==autoFirst.generation+1&&!std::filesystem::exists(autoRetiredBlob),"external commit did not reap threshold-sized orphan blocks");
 Require(autoReader.ReadAt(0,autoRetired.size())==autoRetired,"automatic orphan reaping invalidated a pinned reader");
 {File pinned(firstPath,false);auto inPlace=Fork(firstPath,firstPath,external.generation,{{"small",Bytes{9}}});File current(firstPath,false);
  Require(inPlace.generation==external.generation+1&&Get(pinned,external,"small")==Bytes{1}&&Get(current,inPlace,"small")==Bytes{9},"in-place external root replacement invalidated the old snapshot");}
 {File blob(freshPath.parent_path()/std::filesystem::u8path(fresh.blocks.at("a").external),true);blob.WriteAt(0,Bytes{5});blob.Flush();}
 Reject([&]{File file(freshPath,false);Get(file,Read(file),"a");});
 std::cout<<"PASS v2 external roots: v1 migration, fresh snapshot, same-volume hardlink reuse, independent generations, removal, unchanged publication, torn commit recovery, compaction, payload integrity\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
