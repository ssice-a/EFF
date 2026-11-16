#define main StaticAssemblyFixtureMain
#include "static_mesh_assembly_smoke.cpp"
#undef main
#include "../../src/eiem_author_mesh_bundle.h"
#include "../../src/eiem_demand_mesh_provenance.h"
#include "../../src/eiem_demand_target_lookup.h"

namespace P=EiemDemandMeshProvenance;
namespace R=EiemNativeRandomAccess;
static std::shared_ptr<const P::Prefab> ReadProof(Fixture& fixture,const std::string& path,const std::set<Identity>& targets){
 auto input=EndfieldInput::Encode(EiemAuthorMeshBundle::EnvelopeHeader(),{{fixture.cab,4,fixture.BytesOf()}});
 R::Reader reader(input.size(),[&](size_t at,unsigned char* out,size_t size){memcpy(out,input.data()+at,size);});
 P::Meshes meshes;
 for(const auto& target:targets)for(const auto& asset:EiemDemandTargetLookup::Find(reader,target))
  meshes[CanonicalSerializedAssetReference({asset.cab,asset.pathId})].insert(target);
 return P::Read(reader,path,meshes);
}
static void IdentityChecks(){
 Fixture fixture;fixture.Add(101,5);SetString(fixture.T(101),Child(fixture.T(101),fixture.V(101),"m_Name"),"Static");
 fixture.Pointer(22,"m_Mesh",101);
 auto& container=ArrayValue(fixture.T(300),Child(fixture.T(300),fixture.V(300),"m_Container"));
 auto row=ArrayElement(fixture.T(300),container);SetString(fixture.T(300),Child(fixture.T(300),row,"first"),"assets/other.mesh");
 SetReference(fixture.T(300),Child(fixture.T(300),Child(fixture.T(300),row,"second"),"asset"),{fixture.cab,101},fixture.refs,fixture.cab);
 container.children.push_back(row);container.count=container.children.size();
 Identity sword{"assets/static.mesh","Static"},other{"assets/other.mesh","Static"};
 auto a=ReadProof(fixture,"assets/a.prefab",{sword});auto b=ReadProof(fixture,"assets/b.prefab",{sword});
 Require(a&&a->uses.size()==1&&a->uses.front().identities.size()==1&&a->uses.front().identities.count(sword),"exact native Mesh logical proof missing");
 Require(b&&b->uses.empty(),"same-name Mesh from a different logical resource acquired the attachment proof");
 fixture.Pointer(22,"m_Mesh",100);b=ReadProof(fixture,"assets/b.prefab",{sword});
 Require(b&&b->uses.size()==1&&b->uses.front().identities.size()==1&&b->uses.front().identities.count(sword),"shared native Mesh in a second PFB lost its logical identity");
 Require(!ReadProof(fixture,"assets/missing.prefab",{sword}),"missing requested PFB fell back to an unrelated native root");
 // A dependency FBX may expose the Mesh only through that exact container's
 // preload interval. A second FBX with the same Mesh name is kept separate.
 fixture.Pointer(22,"m_Mesh",101);fixture.SetList(300,"m_PreloadTable",{100,101});
 for(auto entry:std::vector<std::pair<std::string,int>>{{"assets/a.fbx",0},{"assets/b.fbx",1}}){
  auto item=ArrayElement(fixture.T(300),container);SetString(fixture.T(300),Child(fixture.T(300),item,"first"),entry.first);
  auto& info=Child(fixture.T(300),item,"second");SetReference(fixture.T(300),Child(fixture.T(300),info,"asset"),{fixture.cab,entry.second?20:10},fixture.refs,fixture.cab);
  SetInteger(fixture.T(300),Child(fixture.T(300),info,"preloadIndex"),entry.second);SetInteger(fixture.T(300),Child(fixture.T(300),info,"preloadSize"),1);
  container.children.push_back(item);container.count=container.children.size();
 }
 auto fbx=ReadProof(fixture,"assets/a.prefab",{{"assets/a.fbx","Static"}});
 Require(fbx&&fbx->uses.size()==1&&fbx->uses.front().identities.count({"assets/a.fbx","Static"}),"FBX preload-owned native Mesh was not proved");
 Require(ReadProof(fixture,"assets/b.prefab",{{"assets/a.fbx","Static"}})->uses.empty(),"same-name second FBX contaminated the first FBX proof");
 std::cout<<"PASS native logical Mesh proof: static MeshFilter, same-name isolation, shared multi-PFB Mesh and exact FBX preloads\n";
}
static void AddressChecks(){
 Fixture fixture;
 fixture.objects.erase(14);fixture.objects.erase(15);
 for(int64_t base:{40,50}){
  fixture.Add(base,0);SetString(fixture.T(base),Child(fixture.T(base),fixture.V(base),"m_Name"),"LOD");
  fixture.Add(base+1,1);fixture.Pointer(base+1,"m_GameObject",base);fixture.Pointer(base+1,"m_Father",11);
  fixture.Add(base+2,2);fixture.Pointer(base+2,"m_GameObject",base);fixture.Pointer(base+2,"m_Mesh",100);
  fixture.Add(base+3,3);fixture.Pointer(base+3,"m_GameObject",base);fixture.List(base,"m_Component",{base+1,base+2,base+3},"component");
 }
 fixture.List(11,"m_Children",{41,51});
 fixture.Add(14,3);fixture.Pointer(14,"m_GameObject",10);
 auto skin=fixture.types.size();fixture.types.push_back(Schema(137,{Ref("m_GameObject"),Ref("m_Mesh")}));
 fixture.Add(15,skin);fixture.Pointer(15,"m_GameObject",10);fixture.Pointer(15,"m_Mesh",100);
 fixture.SetList(10,"m_Component",{11,12,13,14,15},"component");
 auto proof=ReadProof(fixture,"assets/a.prefab",{{"assets/static.mesh","Static"}});
 Require(proof&&proof->uses.size()==5,"native proof lost inactive LOD-like children, skinned or static consumers");
 std::set<P::A::Address> addresses;for(const auto& use:proof->uses)addresses.insert(use.renderer);
 Require(addresses.size()==5&&addresses.count({"",0})&&addresses.count({"",1})&&addresses.count({"",2})&&
  addresses.count({P::A::Segment("LOD",0),0})&&addresses.count({P::A::Segment("LOD",1),0}),"native proof guessed same-name siblings or Renderer component ordinals");
 std::cout<<"PASS native structural addresses: skinned/static components, inactive LOD children and same-name sibling ordinals\n";
}
int main(){try{IdentityChecks();AddressChecks();return 0;}catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}}
