#include "../../src/eiem_demand_shared_graph.h"
#include "../../src/eiem_demand_dependency_source.h"
#include "../../src/eiem_demand_dependency_scope.h"
#include <iostream>
using namespace EiemDemandSharedGraph;
int main(){try{
  using EiemDemandDependencyScope::Boundary;
  Require(EiemDemandDependencyScope::OpensRequestScope(Boundary::BundleTree)&&
      EiemDemandDependencyScope::OpensRequestScope(Boundary::AssetTree)&&
      !EiemDemandDependencyScope::OpensRequestScope(Boundary::OperationUpdate),
    "operation polling was incorrectly treated as a new resource request");
  Require(!EiemDemandDependencyScope::MayAugment(false,1)&&!EiemDemandDependencyScope::MayAugment(true,0)&&
      EiemDemandDependencyScope::MayAugment(true,1)&&EiemDemandDependencyScope::MayAugment(true,4),
    "shared dependency augmentation escaped the game load-tree request scope");
  std::map<std::string,int> dependencyCabs;std::map<std::string,uint64_t> indexedDependencies{{"CAB-shader",17}};
  std::vector<uint64_t> dependencyKeys{17,23};std::vector<uint64_t> openedDependencyKeys;
  auto dependency=EiemDemandDependencySource::Resolve("CAB-shader",dependencyKeys,dependencyCabs,indexedDependencies,[&](uint64_t key){
    openedDependencyKeys.push_back(key);if(key==23)dependencyCabs.emplace("CAB-shader",23);});
  Require(dependency!=dependencyCabs.end()&&dependency->second==23&&openedDependencyKeys==std::vector<uint64_t>({17,23}),
    "stale CAB index blocked exact dependency-group fallback");
  std::cout<<"PASS external CAB dependency lookup verifies the current request group before bounded fallback\n";
  auto bytes=std::make_shared<const EiemNativeInput::Bytes>(EiemNativeInput::Bytes{1,2,3});
  auto view=std::make_shared<const EiemInputSpanView::View>(std::vector<EiemInputSpanView::Span>{{SIZE_MAX,0,bytes->size(),bytes}},std::vector<size_t>{},EiemInputSpanView::ReadRange{});
  Graph graph;Input texture{100,1,"eff/texture.ab","CAB-texture",view,{}},material{101,1,"eff/material.ab","CAB-material",view,{100,3}};
  EiemBundleDependencies::Keys native{{1,{2,3}},{4,{2,3}}};graph.Commit({texture,material},{{1,{101}},{4,{101}}},native);
  auto edges=[&](uint64_t key){return graph.Edges(key);};auto inherited=[&](uint64_t key){auto found=native.find(key);return found==native.end()?std::vector<uint64_t>{}:found->second;};
  Require(EiemBundleDependencies::Added(1,{2,3},true,edges,inherited)==std::vector<uint64_t>{101},"shared direct dependency differs");
  Require(EiemBundleDependencies::Added(1,{2,3},false,edges,inherited)==std::vector<uint64_t>({101,100}),"shared transitive query duplicated native dependencies or missed Texture");
  Require(EiemBundleDependencies::Added(101,{},false,edges,inherited)==std::vector<uint64_t>({100,3}),"synthetic Material closure differs");
  Require(graph.Reverse(100)==std::vector<uint64_t>({101,1,4}),"shared native reverse ownership omitted consumers");
  Require(graph.HasExpansion(1,{2,3},false)&&graph.HasExpansion(1,{2,3},true)&&graph.HasExpansion(9,{1},false)&&
      !graph.HasExpansion(2,{3},false)&&!graph.HasExpansion(9,{1},true)&&
      graph.HasRootExpansion(1)&&!graph.HasRootExpansion(9),
    "shared dependency expansion relevance gate differs");
  graph.Commit({texture,material},{{4,{101}}},native);Require(graph.Size()==2&&graph.Edges(4).size()==1,"multiple PFBs duplicated shared inputs");
  auto anotherConsumerTexture=texture;anotherConsumerTexture.categorySource=4;
  auto anotherConsumerMaterial=material;anotherConsumerMaterial.categorySource=4;
  graph.Commit({anotherConsumerTexture,anotherConsumerMaterial},{{4,{101}}},native);
  Input retained;Require(graph.Find(101,retained)&&retained.categorySource==1&&graph.Size()==2,
    "shared Material/Texture identity changed with the consuming PFB category source");
  Input output;Require(graph.FindName("eff/material.ab",output)&&output.key==101&&!graph.FindName("material.ab",output),"synthetic input name was guessed");
  bool rejected=false;try{auto cycle=material;cycle.key=102;cycle.name="eff/cycle.ab";cycle.cab="CAB-cycle";cycle.dependencies={1};graph.Commit({cycle},{{1,{102}}},native);}catch(const std::exception&){rejected=true;}
  Require(rejected&&graph.Size()==2&&!graph.Find(102,output)&&graph.Edges(1)==std::vector<uint64_t>{101},"failed dependency transaction changed the live graph");
  Graph lifetime;Input sharedTexture{701,1,"eff/shared-texture.ab","CAB-lifetime-texture",view,{}},
      sharedMaterial{702,1,"eff/shared-material.ab","CAB-lifetime-material",view,{701}},
      privateTexture{703,1,"eff/private-texture.ab","CAB-lifetime-private",view,{}};
  lifetime.Commit({sharedTexture,sharedMaterial,privateTexture},{{801,{702,703}},{802,{702}}},{});
  auto firstRetire=lifetime.RetireUnreachable({802});
  Require(firstRetire.inputs==1&&firstRetire.viewBytes==view->Size()&&lifetime.Size()==2&&
      lifetime.Find(701,output)&&lifetime.Find(702,output)&&!lifetime.Find(703,output)&&
      lifetime.Edges(801).empty()&&lifetime.Reverse(701)==std::vector<uint64_t>({702,802}),
    "retiring one consumer did not evict only its unshared authored inputs");
  auto lastRetire=lifetime.RetireUnreachable({});
  Require(lastRetire.inputs==2&&lifetime.Size()==0&&!lifetime.FindName("eff/shared-material.ab",output)&&
      lifetime.Edges(802).empty()&&lifetime.Reverse(701).empty(),
    "retiring the last consumer retained shared input views or dependency routes");
  Graph crossBatch;Input crossMaterial{201,1,"eff/cross-batch-material.ab","CAB-cross-batch-material",view,{301}};
  crossBatch.Commit({crossMaterial},{},{{301,{401}}});std::vector<uint64_t> cachedNative;
  Require(crossBatch.Native(301,cachedNative)&&cachedNative==std::vector<uint64_t>{401},"committed native dependencies were not retained");
  bool crossBatchCycle=false;try{crossBatch.Commit({},{{401,{201}}},{});}catch(const std::exception&){crossBatchCycle=true;}
  Require(crossBatchCycle&&crossBatch.Edges(401).empty(),"cycle closed by a later consumer commit was not rejected atomically");
  Graph lateNative;lateNative.Commit({},{{501,{601}}},{});bool lateNativeCycle=false;
  try{lateNative.CacheNative(601,{501});}catch(const std::exception&){lateNativeCycle=true;}
  Require(lateNativeCycle&&!lateNative.Native(601,cachedNative),"late native dependency closed a cycle without rejection");
  lateNative.CacheNative(601,{});Require(lateNative.Native(601,cachedNative)&&cachedNative.empty(),"empty native dependency result was not cached");
  bool changedNative=false;try{lateNative.CacheNative(601,{701});}catch(const std::exception&){changedNative=true;}
  Require(changedNative&&lateNative.Native(601,cachedNative)&&cachedNative.empty(),"native dependency identity changed within a manifest generation");
  DependencyExpansionCache expansions;std::vector<uint64_t> original{2,3},added{101,100},cached;auto revision=lateNative.Revision();
  expansions.Store(revision,1,false,original,added);
  Require(expansions.Read(revision,1,false,original,cached)&&cached==added,"dependency expansion cache missed an identical native input");
  Require(!expansions.Read(revision,1,false,{2},cached),"dependency expansion cache ignored the native input signature");
  expansions.Store(revision+1,1,false,original,{102});
  Require(expansions.Read(revision+1,1,false,original,cached)&&cached==std::vector<uint64_t>{102}&&
      !expansions.Read(revision,1,false,original,cached),"dependency expansion cache did not invalidate by graph revision");
  rejected=false;try{auto collision=texture;collision.cab="CAB-wrong";graph.Commit({collision},{},{});}catch(const std::exception&){rejected=true;}
  Require(rejected&&graph.Find(100,output)&&output.cab=="CAB-texture","shared native key collision was accepted");
  Graph incremental;for(uint64_t i=1;i<=128;++i){Input node{1000+i,1,"eff/incremental/"+std::to_string(i)+".ab","CAB-incremental-"+std::to_string(i),view,{}};
    incremental.Commit({node},{{1,{node.key}}},{});}
  Require(incremental.Size()==128&&incremental.Edges(1).size()==128&&incremental.Reverse(1128)==std::vector<uint64_t>{1},
    "incremental shared graph commit lost cumulative edges or reverse ownership");
  std::cout<<"PASS shared native dependency publication, incremental multi-PFB edges/reverse index, deduplication, native closure, cycle rollback and exact routes\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
