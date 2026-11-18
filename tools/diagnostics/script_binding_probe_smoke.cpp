#define NOMINMAX
#include "script_binding_probe.h"
#include <MinHook.h>
#include <array>
#include <iostream>
#include <set>
#include <stdexcept>
#include <thread>
#include <vector>
namespace P=EiemScriptBindingProbe;
void Require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
template<class T,size_t N>void Set(std::array<unsigned char,N>& bytes,size_t at,T value){memcpy(bytes.data()+at,&value,sizeof(value));}
std::array<unsigned char,0x100> script{},reference{},component{},cache{},nestedReference{},nestedComponent{};
char target[64]="BeyondBoneSphereCollider";
struct Invocation{void *ref,*component,*klass,*context,*script;};
std::vector<Invocation> invocations;
bool fail=false;
__declspec(noinline) void Native(void* ref,void* owner,void* klass,void* context,void* source){
 invocations.push_back({ref,owner,klass,context,source});if(fail)throw std::runtime_error("native exception");
 if(context==(void*)0x2222){P::BindingFn volatile nested=Native;nested(nestedReference.data(),nestedComponent.data(),klass,(void*)0x3333,source);}
 if(owner&&ref){Set(reference,0x10,cache.data());Set(cache,8,(void*)0x12345678);Set(component,0x20,uint32_t(2));}
}
int main(int argc,char** argv){
 try{
  Require(argc==2||argc==3,"output path argument required");
  if(argc==3){auto path=std::filesystem::u8path(argv[2]);auto actual=LoadLibraryExW(path.c_str(),nullptr,DONT_RESOLVE_DLL_REFERENCES);
   Require(actual&&P::Compatible(actual),"actual Unity image refused");FreeLibrary(actual);}
  // Validate the same PE/prologue gate used by the actual diagnostic install.
  auto image=(unsigned char*)VirtualAlloc(nullptr,0x22a2000,MEM_RESERVE,PAGE_NOACCESS);Require(image,"image reserve failed");
  Require(VirtualAlloc(image,4096,MEM_COMMIT,PAGE_READWRITE)!=nullptr,"header commit failed");
  Require(VirtualAlloc(image+(P::bindingRva&~uintptr_t(4095)),4096,MEM_COMMIT,PAGE_READWRITE)!=nullptr,"code commit failed");
  auto dos=(IMAGE_DOS_HEADER*)image;dos->e_magic=IMAGE_DOS_SIGNATURE;dos->e_lfanew=0x80;
  auto pe=(IMAGE_NT_HEADERS64*)(image+0x80);pe->Signature=IMAGE_NT_SIGNATURE;pe->FileHeader.Machine=IMAGE_FILE_MACHINE_AMD64;
  pe->OptionalHeader.Magic=IMAGE_NT_OPTIONAL_HDR64_MAGIC;pe->FileHeader.TimeDateStamp=0x6aba59bd;pe->OptionalHeader.SizeOfImage=0x22a2000;
  memcpy(image+P::bindingRva,P::prologue,sizeof(P::prologue));Require(P::Compatible(image),"known module refused");
  ++pe->FileHeader.TimeDateStamp;Require(!P::Compatible(image),"changed module timestamp accepted");--pe->FileHeader.TimeDateStamp;
  image[P::bindingRva]^=1;Require(!P::Compatible(image),"changed code accepted");VirtualFree(image,0,MEM_RELEASE);
  auto guard=VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);DWORD old=0;VirtualProtect(guard,4096,PAGE_READWRITE|PAGE_GUARD,&old);
  char byte=0;Require(!P::Copy(guard,&byte,1),"guard page read");MEMORY_BASIC_INFORMATION page{};VirtualQuery(guard,&page,sizeof(page));
  Require(page.Protect&PAGE_GUARD,"observer consumed guard state");VirtualFree(guard,0,MEM_RELEASE);
  Set(script,0x30,target);Set(script,0x78,cache.data());Set(script,8,uint32_t(42));Set(reference,8,uint32_t(42));
  Require(P::Start(std::filesystem::u8path(argv[1]),0x12340000),"observer output failed");
  Require(MH_Initialize()==MH_OK,"MinHook initialize failed");Require(MH_CreateHook((void*)Native,(void*)P::Binding,(void**)&P::original)==MH_OK,"binding hook failed");
  Require(MH_EnableHook((void*)Native)==MH_OK,"binding hook enable failed");P::BindingFn volatile invoke=Native;
  invoke(reference.data(),component.data(),(void*)0x1111,(void*)0x4444,script.data());
  Require(invocations.size()==1&&invocations[0].ref==reference.data()&&invocations[0].component==component.data()&&
   invocations[0].klass==(void*)0x1111&&invocations[0].context==(void*)0x4444&&invocations[0].script==script.data(),"five native arguments changed");
  Require(P::records[0].record.state.scriptClass==0&&P::records[0].record.state.componentClass==0&&
   P::records[1].record.state.scriptClass==0x12345678&&P::records[1].record.state.componentClass==0x12345678,"before/after evidence lost");
  Require(P::records[0].record.state.componentScriptId==42&&P::records[0].record.stackCount>0,"script id or stack missing");
  invoke(reference.data(),component.data(),(void*)0x1111,(void*)0x2222,script.data());
  Require(invocations.size()==3&&invocations[2].ref==nestedReference.data()&&invocations[2].context==(void*)0x3333,"nested original changed");
  const auto count=P::reserved.load();fail=true;bool caught=false;
  try{invoke(reference.data(),component.data(),(void*)0x1111,(void*)0x4444,script.data());}catch(const std::runtime_error&){caught=true;}fail=false;
  Require(caught&&invocations.size()==4&&P::reserved.load()==count+1,"native exception swallowed, retried or post-event invented");
  strcpy_s(target,"UnrelatedScript");const auto unmatched=P::reserved.load();invoke(nullptr,nullptr,nullptr,nullptr,script.data());
  Require(P::reserved.load()==unmatched&&invocations.size()==5,"unrelated native binding observed or changed");
  P::enabled=false;invoke(nullptr,nullptr,nullptr,nullptr,(void*)1);Require(invocations.size()==6,"disabled observer inspected invalid script");P::enabled=true;
  P::Input(0,123,"cab-fixture",false,true);
  const auto nullBefore=P::reserved.load();invoke(nullptr,nullptr,nullptr,nullptr,nullptr);
  Require(P::reserved.load()==nullBefore+2&&!strcmp(P::records[nullBefore].record.name,"unresolved-script"),"missing MonoScript argument filtered out");
  auto concurrentStart=P::reserved.load();std::vector<std::thread> writers;
  for(unsigned writer=0;writer<4;++writer)writers.emplace_back([writer]{for(unsigned i=0;i<16;++i)P::Input(3,1000+writer*16+i,"concurrent",true);});
  for(auto& writer:writers)writer.join();Require(P::reserved.load()==concurrentStart+64,"concurrent events lost");
  std::set<uint64_t> keys;
  for(unsigned i=concurrentStart;i<P::reserved.load();++i){Require(P::records[i].ready.load(),"concurrent publication missing");keys.insert(P::records[i].record.key);}
  Require(keys.size()==64&&*keys.begin()==1000&&*keys.rbegin()==1063,"concurrent events duplicated");
  strcpy_s(target,"BeyondBoneSphereCollider");
  for(unsigned i=0;i<P::capacity+10;++i)invoke(nullptr,nullptr,nullptr,nullptr,script.data());
  Require(P::reserved.load()==P::capacity&&invocations.size()==7+P::capacity+10,"record limit changed native call count");
  P::Stop();Require(P::consumed==P::capacity,"records not drained before stop");
  Require(MH_DisableHook((void*)Native)==MH_OK&&MH_Uninitialize()==MH_OK,"hook cleanup failed");
  std::cout<<"PASS version/prologue rejection, guard preservation, real MinHook five-argument ABI, original once, null class before/after, nested forwarding, native exception propagation, non-target/disabled forwarding, concurrent publication, finite records and worker drain\n";
  return 0;
 }catch(const std::exception& e){P::Stop();std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
