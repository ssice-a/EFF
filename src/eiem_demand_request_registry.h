#pragma once
#include "eiem_resource_directory.h"
#include <mutex>

// Pure request identities. A physical binding survives an async thread hop;
// retirement invalidates it. Ambiguous names never become a guessed match.
namespace EiemDemandRequests {
enum class Candidate { Unknown, No, Yes, Target };
struct Record {
  uint64_t token=0,scope=0,bundle=0;
  std::string name;
  Candidate candidate=Candidate::Unknown;
};
inline std::string Name(std::string name) {
  std::replace(name.begin(),name.end(),'\\','/');
  std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return char(std::tolower(c));});
  return name;
}
class Registry {
  mutable std::mutex mutex_;
  uint64_t serial_=0;
  size_t limit_;
  std::map<uint64_t,Record> records_;
  std::map<std::pair<uint64_t,uint64_t>,uint64_t> identities_;
  std::map<std::string,std::set<uint64_t>> names_;
  std::map<uint64_t,std::set<uint64_t>> vfs_;
  std::map<std::pair<std::string,uint64_t>,uint64_t> files_;
  void Erase(uint64_t token) {
    auto record=records_.find(token);if(record==records_.end())return;
    auto name=names_.find(record->second.name);if(name!=names_.end()){name->second.erase(token);if(name->second.empty())names_.erase(name);}
    identities_.erase({record->second.scope,record->second.bundle});records_.erase(record);
    for(auto it=vfs_.begin();it!=vfs_.end();){it->second.erase(token);if(it->second.empty())it=vfs_.erase(it);else ++it;}
    for(auto it=files_.begin();it!=files_.end();)if(it->second==token)it=files_.erase(it);else ++it;
  }
public:
  explicit Registry(size_t limit=8192):limit_(limit){}
  Record ObserveCandidate(uint64_t scope,uint64_t bundle,const std::string& name,Candidate candidate) {
    if(candidate!=Candidate::Yes&&candidate!=Candidate::Target)return {};
    return Observe(scope,bundle,name,candidate);
  }
  Record Observe(uint64_t scope,uint64_t bundle,const std::string& name,Candidate candidate) {
    std::lock_guard<std::mutex> lock(mutex_);auto key=std::make_pair(scope,bundle);auto found=identities_.find(key);
    if(found!=identities_.end()) {auto& old=records_.at(found->second);
      if(old.name!=Name(name))return {}; // A changed directory needs a new scope.
      if(old.candidate!=Candidate::Target&&(candidate==Candidate::Target||candidate==Candidate::Yes||old.candidate==Candidate::Unknown))old.candidate=candidate;
      return old;
    }
    if(records_.size()>=limit_)return {};
    Record record{++serial_,scope,bundle,Name(name),candidate};records_.emplace(record.token,record);identities_[key]=record.token;if(!record.name.empty())names_[record.name].insert(record.token);return record;
  }
  Record Get(uint64_t token) const {std::lock_guard<std::mutex> lock(mutex_);auto it=records_.find(token);return it==records_.end()?Record{}:it->second;}
  bool BindVfs(uint64_t token,uint64_t hash) {
    std::lock_guard<std::mutex> lock(mutex_);if(!records_.count(token))return false;
    if(!vfs_.count(hash)&&vfs_.size()>=limit_)return false;vfs_[hash].insert(token);return true;
  }
  Record Vfs(uint64_t hash) const {
    std::lock_guard<std::mutex> lock(mutex_);auto found=vfs_.find(hash);
    return found!=vfs_.end()&&found->second.size()==1?records_.at(*found->second.begin()):Record{};
  }
  Record Resolve(const std::vector<std::string>& labels) const {
    std::lock_guard<std::mutex> lock(mutex_);uint64_t selected=0;
    for(const auto& label:labels){auto found=names_.find(Name(label));if(found==names_.end())continue;
      for(auto token:found->second){if(selected&&selected!=token)return {};selected=token;}}
    return selected?records_.at(selected):Record{};
  }
  Record BindFile(uint64_t token,const std::string& path,uint64_t offset) {
    std::lock_guard<std::mutex> lock(mutex_);auto found=records_.find(token);if(found==records_.end())return {};
    auto key=std::make_pair(Name(path),offset);auto old=files_.find(key);
    // A current explicit request may legitimately replace a previous file
    // route, but a stale token must never establish that association.
    if(old==files_.end()&&files_.size()>=limit_)return {};
    files_[key]=token;return found->second;
  }
  Record File(const std::string& path,uint64_t offset) const {
    std::lock_guard<std::mutex> lock(mutex_);auto found=files_.find({Name(path),offset});if(found==files_.end())return {};
    auto record=records_.find(found->second);return record==records_.end()?Record{}:record->second;
  }
  void Retire(uint64_t scope,uint64_t bundle) {
    std::lock_guard<std::mutex> lock(mutex_);auto key=identities_.find({scope,bundle});if(key==identities_.end())return;
    Erase(key->second);
  }
  void RetireScope(uint64_t scope) {
    std::lock_guard<std::mutex> lock(mutex_);std::vector<uint64_t> tokens;
    for(const auto& row:records_)if(row.second.scope==scope)tokens.push_back(row.first);
    for(auto token:tokens)Erase(token);
  }
  size_t Size() const {std::lock_guard<std::mutex> lock(mutex_);return records_.size();}
};
} // namespace EiemDemandRequests
