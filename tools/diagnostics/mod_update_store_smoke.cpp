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
 std::cout<<"PASS one-file incremental blocks, unchanged block reuse, completed positive/negative records, source/dependency invalidation, same-Bundle multi-root identities, torn commit recovery, stale writer rejection, compaction and payload integrity\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
