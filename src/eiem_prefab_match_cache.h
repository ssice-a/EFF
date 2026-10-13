#pragma once
#include "eiem_runtime_author_directory.h"

// Event-driven discovery cache. A negative result belongs to one rule set;
// discovered Mesh references belong to source+dependency content and can be
// matched again after adding/removing Mods without rescanning that hierarchy.
namespace EiemPrefabMatchCache {
using namespace EiemDirectory;
struct SourceIdentity {std::string path,content,dependencies;};
inline bool operator==(const SourceIdentity& a,const SourceIdentity& b){return a.path==b.path&&a.content==b.content&&a.dependencies==b.dependencies;}
struct Consumer {std::string address;unsigned component=0,kind=0;Identity mesh;};
struct Observation {SourceIdentity source;std::vector<Consumer> consumers;};
struct Match {size_t consumer=0;std::string rule;};
struct Result {std::string rules;std::vector<Match> matches;};
using Rules=std::map<Identity,std::string>;
inline Result MatchRules(const Observation& observation,const Rules& rules,const std::string& digest){
 Require(!digest.empty(),"Prefab rule identity absent");Result result;result.rules=digest;
 for(size_t i=0;i<observation.consumers.size();++i){const auto& consumer=observation.consumers[i];auto rule=rules.find(consumer.mesh);if(rule!=rules.end())result.matches.push_back({i,rule->second});}return result;
}
struct Entry {Observation observation;Result result;};
class Cache {
 std::map<std::string,Entry> entries;
 public:
 template<class Discover>const Entry& Get(const SourceIdentity& source,const Rules& rules,const std::string& digest,Discover discover){
  Require(!source.path.empty()&&!source.content.empty()&&!source.dependencies.empty(),"Prefab cache source/dependency identity absent");
  auto found=entries.find(source.path);
  if(found==entries.end()||!(found->second.observation.source==source)){
   Observation observation{source,discover()};Entry next{std::move(observation),{}};next.result=MatchRules(next.observation,rules,digest);
   found=entries.insert_or_assign(source.path,std::move(next)).first;
  }else if(found->second.result.rules!=digest)found->second.result=MatchRules(found->second.observation,rules,digest);
  return found->second;
 }
 void Restore(Observation observation){auto key=observation.source.path;entries[std::move(key)]={std::move(observation),{}};}
 size_t Size()const{return entries.size();}
};
inline Bytes Encode(const Observation& observation){
 using EiemRuntimeAuthorDirectory::Writer;Writer w;w.S("EFF_PREFAB_OBSERVATION/content+dependencies+mesh-addresses/le64+sha256");
 Require(!observation.source.path.empty()&&!observation.source.content.empty()&&!observation.source.dependencies.empty(),"Prefab observation identity absent");
 w.S(observation.source.path);w.S(observation.source.content);w.S(observation.source.dependencies);w.N(observation.consumers.size());
 std::set<std::pair<std::string,unsigned>> addresses;
 for(const auto& consumer:observation.consumers){Require((consumer.kind==137||consumer.kind==23)&&!consumer.mesh.path.empty()&&!consumer.mesh.asset.empty()&&addresses.emplace(consumer.address,consumer.component).second,"Prefab observation consumer identity/address differs");
  w.S(consumer.address);w.N(consumer.component);w.N(consumer.kind);w.S(consumer.mesh.path);w.S(consumer.mesh.asset);}
 auto hash=Sha(w.b.data(),w.b.size());w.b.insert(w.b.end(),hash.begin(),hash.end());return w.b;
}
inline Observation Decode(const Bytes& bytes){
 Require(bytes.size()>=32,"Prefab observation truncated");auto hash=Sha(bytes.data(),bytes.size()-32);Require(std::equal(hash.begin(),hash.end(),bytes.end()-32),"Prefab observation checksum differs");
 Bytes payload(bytes.begin(),bytes.end()-32);EiemRuntimeAuthorDirectory::Reader r{payload};
 Require(r.S()=="EFF_PREFAB_OBSERVATION/content+dependencies+mesh-addresses/le64+sha256","Prefab observation structure differs");
 Observation result;result.source={r.S(),r.S(),r.S()};auto count=r.Count();result.consumers.reserve(count);
 for(size_t i=0;i<count;++i){Consumer consumer;consumer.address=r.S();consumer.component=r.U();consumer.kind=r.U();consumer.mesh={r.S(),r.S()};result.consumers.push_back(std::move(consumer));}
 Require(r.p==payload.size(),"Prefab observation trailing data");Require(Encode(result)==bytes,"Prefab observation noncanonical fields");return result;
}
}
