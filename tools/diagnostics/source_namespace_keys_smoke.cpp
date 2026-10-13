#include "../../src/eiem_source_namespace_keys.h"
#include <iostream>
using namespace EiemSourceNamespaceKeys;
using NamespaceSource=EiemSourceNamespaceKeys::Source;
template<class F> void Reject(F action){bool rejected=false;try{action();}catch(const std::exception&){rejected=true;}Require(rejected,"invalid namespace accepted");}
int main(){try{
 auto hash=[](unsigned char v){auto h=Sha(&v,1);return Hex(h.data(),h.size());};
 std::vector<NamespaceSource> sources{{"mesh",hash(1),{"mod-a:author"},{}},{"pfb-a",hash(2),{"mod-a:author"},{0}},{"unrelated",hash(3),{"mod-b:author"},{}}};
 auto original=Build(sources);auto extended=sources;extended.push_back({"new-pfb",hash(4),{"mod-a:author"},{0}});auto added=Build(extended);
 Require(std::equal(original.begin(),original.end(),added.begin()),"adding a Prefab renamed unrelated native sources");
 auto updated=sources;updated[0].content=hash(5);auto update=Build(updated);Require(update[0]!=original[0]&&update[1]!=original[1]&&update[2]==original[2],"dependency update failed to isolate affected names");
 auto author=sources;author[0].contributors.insert("mod-c:new-author");auto authored=Build(author);Require(authored[0]!=original[0]&&authored[1]!=original[1]&&authored[2]==original[2],"shared-source contributor change did not invalidate dependent names");
 auto reordered=std::vector<NamespaceSource>{sources[2],sources[1],sources[0]};reordered[1].dependencies={2};auto order=Build(reordered);Require(order[0]==original[2]&&order[1]==original[1]&&order[2]==original[0],"source vector reordering changed native names");
 auto cycle=sources;cycle[0].dependencies={1};auto cyclic=Build(cycle);Require(cyclic.size()==sources.size()&&cyclic[2]==original[2],"cyclic native dependencies did not terminate");
 auto duplicate=sources;duplicate.push_back(sources[0]);Reject([&]{Build(duplicate);});auto missing=sources;missing[1].dependencies={8};Reject([&]{Build(missing);});
 std::cout<<"PASS source namespaces remain stable for unrelated additions/reordering, invalidate reachable content/contributors, retain graph topology and handle dependency cycles\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}}
