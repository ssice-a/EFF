#include "../../src/eiem_prefab_bone_binding.h"
#include <iostream>
using namespace EiemPrefabBoneBinding;
template<class F>void Rejected(F f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}Require(rejected,"ambiguous/out-of-scope bone binding accepted");}
int main(){try{
 std::vector<Node> nodes{{SIZE_MAX,"Prefab"},{0,"CharacterA"},{1,"Root"},{2,"Bip"},{3,"Left"},{3,"Right"},{1,"Mesh"},{0,"CharacterB"},{7,"Root"},{8,"Bip"},{9,"Left"},{9,"Right"},{7,"Mesh"},{3,"AddedTail"}};
 Tree tree(nodes);std::vector<std::string> paths{"Root/Bip/Right","Root/Bip/Left","Root/Bip/AddedTail"};
 Require(tree.Resolve(6,3,paths)==std::vector<size_t>({5,4,13}),"author slot order/new bone was replaced by original palette order");
 Require(tree.Resolve(12,9,{"Root/Bip/Right","Root/Bip/Left"})==std::vector<size_t>({11,10}),"same-name second skeleton borrowed the first character's bones");
 Rejected([&]{tree.Resolve(12,9,paths);});Rejected([&]{tree.Resolve(6,9,{"Root/Bip/Left"});});
 Rejected([&]{tree.Resolve(6,3,{"Root/Bip/Left","Root/Bip/Left"});});
 auto duplicate=nodes;duplicate.push_back({3,"Left"});Tree ambiguous(duplicate);Rejected([&]{ambiguous.Resolve(6,3,{"Root/Bip/Left"});});
 Require(tree.Resolve(6,3,{}).empty(),"static geometry acquired fake bones");
 std::cout<<"PASS concrete Prefab bone binding: reordered slots, LOD-independent paths, added bone, two skeletons, ambiguity and cross-scope rejection\n";return 0;
 }catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}}
