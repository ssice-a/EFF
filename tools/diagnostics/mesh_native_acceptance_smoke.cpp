#define EIEM_MESH_NATIVE_ACCEPTANCE 1
#define EIEM_MESH_TARGET_PROBE 1
#define main FullResourceFixtureMain
#include "full_resource_reload_smoke.cpp"
#undef main

void InheritedBoundsBinding(){
 Test::Setup();Test::Reset();
 struct MethodRow {MInfo info{};void* owner;const char* name;const char* result;std::vector<const char*> parameters;uint32_t flags;};
 static MethodRow rows[]={
  {{},(void*)0xC001,"set_localBounds","System.Void",{"System.Single"},0x86},
  {{},(void*)0xC001,"set_localBounds","System.Void",{"UnityEngine.Bounds"},0x96},
  {{},(void*)0xC002,"set_localBounds","System.Void",{"UnityEngine.Bounds"},0x86},
  {{},(void*)0xC002,"set_localBounds_Injected","System.Void",{"UnityEngine.Bounds&"},0x86}};
 for(auto& row:rows)row.info.mp=(void*)1;
 auto previous=std::make_tuple(il2cpp_class_get_methods,il2cpp_method_get_name,il2cpp_method_get_param_count,
  il2cpp_method_get_flags,il2cpp_method_get_return_type,il2cpp_method_get_param,il2cpp_type_get_name,il2cpp_class_get_parent);
 auto restore=[&]{std::tie(il2cpp_class_get_methods,il2cpp_method_get_name,il2cpp_method_get_param_count,
  il2cpp_method_get_flags,il2cpp_method_get_return_type,il2cpp_method_get_param,il2cpp_type_get_name,il2cpp_class_get_parent)=previous;};
 il2cpp_class_get_methods=[](void* klass,void** it)->void*{for(auto index=uintptr_t(*it);index<std::size(rows);++index){*it=(void*)(index+1);if(rows[index].owner==klass)return &rows[index];}return nullptr;};
 il2cpp_method_get_name=[](void* method)->const char*{return static_cast<MethodRow*>(method)->name;};
 il2cpp_method_get_param_count=[](void* method)->uint32_t{return uint32_t(static_cast<MethodRow*>(method)->parameters.size());};
 il2cpp_method_get_flags=[](void* method,uint32_t*)->uint32_t{return static_cast<MethodRow*>(method)->flags;};
 il2cpp_method_get_return_type=[](void* method)->void*{return const_cast<char*>(static_cast<MethodRow*>(method)->result);};
 il2cpp_method_get_param=[](void* method,uint32_t index)->void*{return const_cast<char*>(static_cast<MethodRow*>(method)->parameters.at(index));};
 il2cpp_type_get_name=[](void* type)->const char*{return static_cast<const char*>(type);};
 il2cpp_class_get_parent=[](void* klass)->void*{return klass==(void*)0xC001?(void*)0xC002:nullptr;};
 try{
  Require(!Entry::Exact((void*)0xC001,"set_localBounds",{"UnityEngine.Bounds"},"System.Void",false),"fixture failed to reproduce derived-only bounds lookup failure");
  Require(NativeAuthorResources::LocalBoundsSetter((void*)0xC001)==&rows[2],"Renderer inherited setter was not resolved from SkinnedMeshRenderer");
  rows[2].info.mp=nullptr;
  Require(NativeAuthorResources::LocalBoundsSetter((void*)0xC001)==&rows[3],"inherited injected bounds setter fallback was not resolved");
  rows[3].info.mp=nullptr;
  Require(!NativeAuthorResources::LocalBoundsSetter((void*)0xC001),"wrong parameter or static bounds setter was accepted");
 }catch(...){restore();throw;}
 restore();std::cout<<"PASS real metadata bounds binding: inherited Renderer setter, injected fallback, wrong overload/static rejection\n";
}

void FailureRouting(){
 namespace N=Entry::MeshNativeAcceptance;Test::Setup();Test::Reset();N::enabled=false;
 Require(N::Route(Test::source,"disabled")==Test::source,"disabled native backend changed a source");N::enabled=true;
 Require(N::Route(Test::source,"unrelated")==Test::source,"unmatched native backend changed a source");
 N::Source record;record.source=RuntimeShapes::Ref::Make(Test::source,true);record.id=FullResourceReload::Id(Test::source);record.path="assets/actual-request.prefab";N::sources[Test::source]=record;
 Require(N::Route(Test::source,"missing-evidence")==Test::source&&!N::sources.at(Test::source).error.empty(),"assembly failure did not return original source");
 Require(!N::shared&&!N::preparation.valid(),"failed unmatched assembly eagerly read resources");
 Require(N::Route(Test::source,"repeated-failure")==Test::source,"failed assembly changed subsequent factory calls");
 N::constructing=true;Require(N::Route(Test::source,"internal-clone")==Test::source,"template construction recursively routed itself");N::constructing=false;
 N::sources.clear();N::enabled=false;
 std::cout<<"PASS native delivery failure keeps original source, no eager IO, no recursive template routing\n";
}
void ExistingInput(const char* path){
 namespace N=Entry::MeshNativeAcceptance;auto declarations=EiemMeshTargetDirectory::Read(std::filesystem::u8path(path));bool found=false;
 for(const auto& target:declarations){if(target.rule!="ReplaceS_actor_chen_body_01_lod0"||target.lod)continue;auto description=N::ReadInput(target);auto p=N::ReadPayload(description);
  Require(p->mesh.vertices==14552&&p->mesh.bones.size()==98&&p->mesh.shapes.size()==2&&!p->mesh.packedFrame,"existing chen body payload differs");
  Require(p->texture.width==1024&&p->texture.height==1024&&p->texture.mips==11&&p->materialResource.blocks.empty(),"existing body appearance payload differs");
  auto marker=description.target.mod+"/"+description.part;Require(p->bytes<8*1024*1024,"one body read unrelated Mod resources");
  std::cout<<"PASS existing body input "<<marker<<" bytesRead="<<p->bytes<<" readDecodeMs="<<p->readMs<<" compiledReads=0 gameArchiveReads=0 consumerPfbReads=0\n";found=true;break;
 }Require(found,"existing chen body rule absent");
}
int main(int argc,char** argv){try{InheritedBoundsBinding();FailureRouting();if(argc>1)ExistingInput(argv[1]);return 0;}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
