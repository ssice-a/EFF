#pragma once
#include "eiem_bundle_dependency_graph.h"
#include "eiem_input_span_view.h"
#include <mutex>

// Declarations for additional native inputs, not a second object loader. The
// game's dependency lists, BundleProxy and unload machinery own live objects.
namespace EiemDemandSharedGraph {
using EiemNativeInput::Require;
struct Input {
  uint64_t key=0,categorySource=0;
  std::string name,cab;
  std::shared_ptr<const EiemInputSpanView::View> view;
  std::vector<uint64_t> dependencies;
};
class Graph {
  mutable std::mutex mutex_;
  uint64_t revision_=0;
  std::map<uint64_t,Input> inputs_;
  std::map<std::string,uint64_t> names_;
  EiemBundleDependencies::Keys edges_;
  EiemBundleDependencies::Keys native_;
  EiemBundleDependencies::Keys reverse_;
  void ValidateAcyclic(const EiemBundleDependencies::Keys& stagedEdges,
      const EiemBundleDependencies::Keys& stagedNative)const {
    std::map<uint64_t,unsigned char> colors;
    std::function<void(uint64_t)> visit=[&](uint64_t key){auto& color=colors[key];
      Require(color!=1,"shared native dependency cycle");if(color==2)return;color=1;
      auto changed=stagedEdges.find(key);auto current=edges_.find(key);const auto* children=changed!=stagedEdges.end()?&changed->second:current==edges_.end()?nullptr:&current->second;
      if(children)for(auto child:*children)visit(child);
      auto staged=stagedNative.find(key);auto existing=native_.find(key);const auto* native=staged!=stagedNative.end()?&staged->second:existing==native_.end()?nullptr:&existing->second;
      if(native)for(auto child:*native)visit(child);color=2;};
    for(const auto& row:stagedEdges)visit(row.first);
    for(const auto& row:stagedNative)visit(row.first);
  }
public:
  struct Eviction {size_t inputs=0,viewBytes=0,dependencyNodes=0;};
  void Commit(const std::vector<Input>& inputs,const EiemBundleDependencies::Keys& edges,
      const EiemBundleDependencies::Keys& native) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::map<uint64_t,Input> stagedInputs;std::map<std::string,uint64_t> stagedNames;
    std::map<uint64_t,std::vector<uint64_t>> stagedEdges;
    // A resource may first be requested by either a world or a UI consumer.
    // Keep the first native scheduling category; the consuming PFB is not
    // part of the immutable resource identity.
    auto same=[](const Input& left,const Input& right){return left.name==right.name&&left.cab==right.cab&&
      left.view==right.view&&left.dependencies==right.dependencies;};
    for(const auto& input:inputs){Require(input.key&&input.categorySource&&!input.name.empty()&&!input.cab.empty()&&input.view,
        "shared native input declaration incomplete");
      auto prior=inputs_.find(input.key);if(prior!=inputs_.end())Require(same(prior->second,input),"shared native input key collision");
      else{auto inserted=stagedInputs.emplace(input.key,input);Require(inserted.second||same(inserted.first->second,input),"shared native input batch key collision");
        auto named=names_.find(input.name);Require(named==names_.end()||named->second==input.key,"shared native input name collision");
        auto stagedName=stagedNames.emplace(input.name,input.key);Require(stagedName.second||stagedName.first->second==input.key,"shared native input batch name collision");}
      stagedEdges[input.key]=input.dependencies;
    }
    for(const auto& row:edges){auto found=stagedEdges.find(row.first);if(found==stagedEdges.end()){
        auto current=edges_.find(row.first);found=stagedEdges.emplace(row.first,current==edges_.end()?std::vector<uint64_t>{}:current->second).first;}
      auto& output=found->second;
      for(auto key:row.second)if(std::find(output.begin(),output.end(),key)==output.end())output.push_back(key);}

    EiemBundleDependencies::Keys stagedNative;
    for(const auto& row:native){auto prior=native_.find(row.first);
      if(prior!=native_.end())Require(prior->second==row.second,"native dependency set changed within one manifest generation");
      else stagedNative.emplace(row.first,row.second);}
    // Keep native edges between commits so a later consumer cannot close a
    // cycle through a source Bundle declared by an earlier Material.
    ValidateAcyclic(stagedEdges,stagedNative);

    std::set<uint64_t> touchedReverse;std::map<uint64_t,std::set<uint64_t>> reverseAdds;
    for(const auto& row:stagedEdges){auto old=edges_.find(row.first);if(old!=edges_.end())touchedReverse.insert(old->second.begin(),old->second.end());
      touchedReverse.insert(row.second.begin(),row.second.end());for(auto child:row.second)reverseAdds[child].insert(row.first);}
    std::map<uint64_t,std::vector<uint64_t>> stagedReverse;
    for(auto child:touchedReverse){std::vector<uint64_t> parents;auto old=reverse_.find(child);if(old!=reverse_.end())parents=old->second;
      parents.erase(std::remove_if(parents.begin(),parents.end(),[&](uint64_t parent){return stagedEdges.count(parent)!=0;}),parents.end());
      auto added=reverseAdds.find(child);if(added!=reverseAdds.end())for(auto parent:added->second)
        if(std::find(parents.begin(),parents.end(),parent)==parents.end())parents.push_back(parent);
      stagedReverse.emplace(child,std::move(parents));}

    std::map<uint64_t,std::vector<uint64_t>> newEdges,newReverse;
    for(auto& row:stagedEdges)if(!edges_.count(row.first))newEdges.emplace(row.first,std::move(row.second));
    for(auto& row:stagedReverse)if(!reverse_.count(row.first))newReverse.emplace(row.first,std::move(row.second));
    // All allocations and validations are complete. Existing vectors swap,
    // while preallocated map nodes merge into the published generation.
    inputs_.merge(stagedInputs);names_.merge(stagedNames);
    for(auto& row:stagedEdges){auto current=edges_.find(row.first);if(current!=edges_.end())current->second.swap(row.second);}
    edges_.merge(newEdges);
    for(auto& row:stagedReverse){auto current=reverse_.find(row.first);if(current!=reverse_.end())current->second.swap(row.second);}
    reverse_.merge(newReverse);
    native_.merge(stagedNative);
    ++revision_;
  }
  bool Find(uint64_t key,Input& output)const {
    std::lock_guard<std::mutex> lock(mutex_);auto found=inputs_.find(key);if(found==inputs_.end())return false;output=found->second;return true;
  }
  bool FindName(const std::string& name,Input& output)const {
    std::lock_guard<std::mutex> lock(mutex_);auto found=names_.find(name);if(found==names_.end())return false;output=inputs_.at(found->second);return true;
  }
  std::vector<uint64_t> Edges(uint64_t key)const {
    std::lock_guard<std::mutex> lock(mutex_);auto found=edges_.find(key);return found==edges_.end()?std::vector<uint64_t>{}:found->second;
  }
  bool HasExpansion(uint64_t root,const std::vector<uint64_t>& original,bool direct)const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto has=[&](uint64_t key){auto found=edges_.find(key);return found!=edges_.end()&&!found->second.empty();};
    if(has(root))return true;
    if(!direct)for(auto key:original)if(has(key))return true;
    return false;
  }
  bool HasRootExpansion(uint64_t root)const {
    std::lock_guard<std::mutex> lock(mutex_);auto found=edges_.find(root);
    return found!=edges_.end()&&!found->second.empty();
  }
  uint64_t Revision()const{std::lock_guard<std::mutex> lock(mutex_);return revision_;}
  bool Native(uint64_t key,std::vector<uint64_t>& output)const {
    std::lock_guard<std::mutex> lock(mutex_);auto found=native_.find(key);if(found==native_.end())return false;output=found->second;return true;
  }
  void CacheNative(uint64_t key,const std::vector<uint64_t>& dependencies) {
    std::lock_guard<std::mutex> lock(mutex_);auto prior=native_.find(key);
    if(prior!=native_.end()){Require(prior->second==dependencies,"native dependency set changed within one manifest generation");return;}
    EiemBundleDependencies::Keys staged{{key,dependencies}};ValidateAcyclic({},staged);native_.merge(staged);++revision_;
  }
  std::vector<uint64_t> Reverse(uint64_t key)const {
    std::lock_guard<std::mutex> lock(mutex_);std::vector<uint64_t> work{key},result;std::set<uint64_t> seen{key};
    for(size_t i=0;i<work.size();++i){auto found=reverse_.find(work[i]);if(found==reverse_.end())continue;
      for(auto parent:found->second)if(seen.insert(parent).second){result.push_back(parent);work.push_back(parent);}}
    return result;
  }
  Eviction RetireUnreachable(const std::vector<uint64_t>& roots) {
    std::lock_guard<std::mutex> lock(mutex_);Eviction result;std::set<uint64_t> reachable;std::vector<uint64_t> work=roots;
    for(size_t i=0;i<work.size();++i){auto key=work[i];if(!reachable.insert(key).second)continue;
      auto edges=edges_.find(key);if(edges!=edges_.end())work.insert(work.end(),edges->second.begin(),edges->second.end());
      auto native=native_.find(key);if(native!=native_.end())work.insert(work.end(),native->second.begin(),native->second.end());}
    std::map<uint64_t,std::vector<uint64_t>> stagedReverse;
    for(const auto& row:edges_)if(reachable.count(row.first))for(auto child:row.second)stagedReverse[child].push_back(row.first);
    for(auto it=inputs_.begin();it!=inputs_.end();)if(!reachable.count(it->first)){
      if(it->second.view)result.viewBytes+=it->second.view->Size();
      names_.erase(it->second.name);it=inputs_.erase(it);++result.inputs;
    }else ++it;
    for(auto it=edges_.begin();it!=edges_.end();)if(!reachable.count(it->first)){it=edges_.erase(it);++result.dependencyNodes;}else ++it;
    for(auto it=native_.begin();it!=native_.end();)if(!reachable.count(it->first)){it=native_.erase(it);++result.dependencyNodes;}else ++it;
    if(result.inputs||result.dependencyNodes){
      reverse_.swap(stagedReverse);
      ++revision_;
    }
    return result;
  }
  size_t Size()const{std::lock_guard<std::mutex> lock(mutex_);return inputs_.size();}
};

class DependencyExpansionCache {
  struct Entry {std::vector<uint64_t> original,added;};
  mutable std::mutex mutex_;
  uint64_t revision_=0;
  std::map<std::pair<uint64_t,bool>,Entry> entries_;
public:
  bool Read(uint64_t revision,uint64_t key,bool direct,const std::vector<uint64_t>& original,std::vector<uint64_t>& added){
    std::lock_guard<std::mutex> lock(mutex_);
    if(revision_!=revision){entries_.clear();revision_=revision;}
    auto found=entries_.find({key,direct});
    if(found==entries_.end()||found->second.original!=original)return false;
    added=found->second.added;return true;
  }
  void Store(uint64_t revision,uint64_t key,bool direct,const std::vector<uint64_t>& original,const std::vector<uint64_t>& added){
    std::lock_guard<std::mutex> lock(mutex_);
    if(revision_!=revision){entries_.clear();revision_=revision;}
    constexpr size_t maxEntries=512;
    if(entries_.size()>=maxEntries&&!entries_.count({key,direct}))entries_.clear();
    entries_.insert_or_assign({key,direct},Entry{original,added});
  }
};
} // namespace EiemDemandSharedGraph
