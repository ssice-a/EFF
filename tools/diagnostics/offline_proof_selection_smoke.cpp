#include <cstdlib>
#include <new>
static bool countProofAllocations=false;
static size_t proofAllocations=0;
void* operator new(size_t size){
 if(countProofAllocations)++proofAllocations;
 if(auto p=std::malloc(size?size:1))return p;
 throw std::bad_alloc();
}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete(void* p,size_t)noexcept{std::free(p);}
#define main StaticFixtureMain
#include "static_mesh_assembly_smoke.cpp"
#undef main
#include "../../src/eiem_compiled_mod_writer.h"

int main(){try{
 Fixture fixture;auto& bundleType=fixture.types[4];auto nameNode=bundleType.nodes.size();
 bundleType.nodes.push_back({"string","m_Name",false,{}});bundleType.nodes[0].children.push_back(nameNode);
 auto bundleName=EmptyValue(bundleType,nameNode);SetString(bundleType,bundleName,"fixture-bundle");fixture.V(300).children.push_back(std::move(bundleName));
 EndfieldInput::Bundle bundle;bundle.members.push_back({fixture.cab,0,fixture.BytesOf()});
 EiemResourceReloadPlan::Plan plan;
 plan.names.bundles["fixture-bundle"]="merged--bundle";
 auto run=[&](){proofAllocations=0;countProofAllocations=true;
  auto proof=EiemCompiledModWriter::DescribeInput(0,plan,false,bundle,bundle);
  countProofAllocations=false;Require(proof.pristine.empty()&&proof.members.size()==1,"unrelated Prefab entered the input proof");
  return proofAllocations;
 };
 auto baseline=run();
 for(size_t i=0;i<128;++i){EiemResourceReloadPlan::Root root;root.input=i+1;root.path="assets/unrelated/character.prefab";
  EiemRenderAssemblyDescription::Source source;source.cab=fixture.cab;source.id=13;
  EiemRenderAssemblyDescription::Target target;target.bones.assign(64,"unrelated/skeleton/hip/cloth/tip");
  source.targets.push_back(std::move(target));root.assembly.sources.push_back(std::move(source));plan.roots.push_back(std::move(root));
 }
 auto unrelated=run();Require(unrelated==baseline,"proof copied assemblies of unrelated Prefab inputs");
 std::cout<<"PASS unrelated Prefab roots add zero allocation work to input proof\n";return 0;
 }catch(const std::exception& error){countProofAllocations=false;std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}
