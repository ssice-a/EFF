#pragma once
// Temporary, version-locked observer. Never included in a production build.
// Records existing native binding arguments and memory before/after forwarding.
// No managed calls, script repairs, extra loads, object creation or fault handler.
#include <windows.h>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <filesystem>

namespace EiemScriptBindingProbe {
using BindingFn=void(*)(void*,void*,void*,void*,void*);
inline BindingFn original=nullptr;
constexpr unsigned capacity=512;
constexpr uintptr_t bindingRva=0x10efd0;
constexpr unsigned char prologue[]={0x40,0x55,0x53,0x57,0x41,0x56,0x41,0x57,0x48,0x8d,0x6c,0x24,0xd1,0x48,0x81,0xec,0xc0,0,0,0};
inline bool Copy(const void* from,void* to,size_t size) noexcept {
 if(!from||!to||uintptr_t(from)>UINTPTR_MAX-size)return false;
 // Do not consume a guard page while making a diagnostic observation.
 MEMORY_BASIC_INFORMATION page{};if(!VirtualQuery(from,&page,sizeof(page))||page.State!=MEM_COMMIT||
  (page.Protect&(PAGE_GUARD|PAGE_NOACCESS))||size>uintptr_t(page.BaseAddress)+page.RegionSize-uintptr_t(from))return false;
 __try{memcpy(to,from,size);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
template<class T> T At(const void* object,size_t offset) noexcept {
 T value{};if(object&&uintptr_t(object)<=UINTPTR_MAX-offset)Copy((const char*)object+offset,&value,sizeof(value));return value;
}
inline bool Target(void* script,char (&name)[64]) noexcept {
 auto text=At<const char*>(script,0x30);if(!text)return false;
 if(Copy(text,name,sizeof(name))&&memchr(name,0,sizeof(name)))return
  !strcmp(name,"BeyondBoneSphereCollider")||!strcmp(name,"EmotionShaderSyncMono");
 for(size_t i=0;i<sizeof(name);++i){if(!Copy(text+i,name+i,1))return false;if(!name[i])return
   !strcmp(name,"BeyondBoneSphereCollider")||!strcmp(name,"EmotionShaderSyncMono");}
 name[sizeof(name)-1]=0;return false;
}
struct State {
 uintptr_t scriptCache=0,scriptClass=0,componentCache=0,componentClass=0,managed=0;
 uint32_t scriptId=0,componentScriptId=0,componentState=0;
};
inline State Read(void* reference,void* component,void* script) noexcept {
 State s;s.scriptCache=At<uintptr_t>(script,0x78);s.scriptClass=At<uintptr_t>((void*)s.scriptCache,8);
 s.componentCache=At<uintptr_t>(reference,0x10);s.componentClass=At<uintptr_t>((void*)s.componentCache,8);
 s.scriptId=At<uint32_t>(script,8);s.componentScriptId=At<uint32_t>(reference,8);
 s.componentState=At<uint32_t>(component,0x20);s.managed=At<uintptr_t>(component,0x28);return s;
}
struct Record {
 uint64_t ticks=0,key=0;DWORD thread=0;unsigned kind=0,call=0,phase=0;
 uintptr_t reference=0,component=0,argumentClass=0,context=0,script=0;
 State state{};char name[64]{};unsigned stackCount=0;void* stack[12]{};
};
struct Slot {Record record{};std::atomic<bool> ready{false};};
inline Slot records[capacity];
inline std::atomic<unsigned> reserved{0},calls{0};
inline std::atomic<unsigned> unresolvedCalls{0};
inline std::atomic<bool> rootSubmitted{false};
inline std::atomic<bool> enabled{false},stopping{false};
inline HANDLE requested=nullptr,worker=nullptr,output=INVALID_HANDLE_VALUE;
inline uintptr_t moduleBase=0;
inline unsigned consumed=0;
inline unsigned Claim() noexcept {
 auto n=reserved.load(std::memory_order_relaxed);
 while(n<capacity&&!reserved.compare_exchange_weak(n,n+1,std::memory_order_relaxed)){}
 return n;
}
inline void Publish(Record record) noexcept {
 if(!enabled.load(std::memory_order_relaxed))return;auto index=Claim();if(index>=capacity)return;
 LARGE_INTEGER now{};QueryPerformanceCounter(&now);record.ticks=uint64_t(now.QuadPart);record.thread=GetCurrentThreadId();
 records[index].record=record;records[index].ready.store(true,std::memory_order_release);if(requested)SetEvent(requested);
}
inline void Input(unsigned phase,uint64_t key,const char* cab,bool ready=false,bool root=false) noexcept {
 if(!enabled.load(std::memory_order_relaxed))return;
 if(phase==0&&root)rootSubmitted=true;
 Record record;record.kind=2;record.phase=phase;record.key=key;record.state.componentState=unsigned(ready);
 if(cab)strncpy_s(record.name,cab,_TRUNCATE);Publish(record);
}
inline void Binding(void* reference,void* component,void* argumentClass,void* context,void* script) {
 char name[64]{};
 bool observe=false;
 if(enabled.load(std::memory_order_relaxed)&&reserved.load(std::memory_order_relaxed)<capacity){
  observe=Target(script,name);
  // A missing MonoScript argument is itself a prediction of the dependency
  // hypothesis. Keep a separate finite budget after root submission so these
  // calls cannot silently vanish from a name-based filter.
  if(!script&&rootSubmitted.load(std::memory_order_relaxed)&&unresolvedCalls.fetch_add(1,std::memory_order_relaxed)<128){
   strcpy_s(name,"unresolved-script");observe=true;
  }
 }
 unsigned call=0;
 if(observe){
  call=calls.fetch_add(1,std::memory_order_relaxed)+1;Record record;record.kind=1;record.call=call;
  record.reference=uintptr_t(reference);record.component=uintptr_t(component);record.argumentClass=uintptr_t(argumentClass);
  record.context=uintptr_t(context);record.script=uintptr_t(script);memcpy(record.name,name,sizeof(name));
  record.state=Read(reference,component,script);record.stackCount=CaptureStackBackTrace(1,12,record.stack,nullptr);Publish(record);
 }
 // The five-argument native routine returns void. Call once, without catching
 // or translating native exceptions. Incomplete pairs retain the before event.
 original(reference,component,argumentClass,context,script);
 if(observe){Record record;record.kind=1;record.call=call;record.phase=1;
  record.reference=uintptr_t(reference);record.component=uintptr_t(component);record.argumentClass=uintptr_t(argumentClass);
  record.context=uintptr_t(context);record.script=uintptr_t(script);memcpy(record.name,name,sizeof(name));
  record.state=Read(reference,component,script);Publish(record);
 }
}
inline bool Write(const char* text,size_t size) noexcept {
 DWORD written=0;return WriteFile(output,text,DWORD(size),&written,nullptr)&&written==size;
}
inline void Drain() noexcept {
 while(consumed<reserved.load(std::memory_order_acquire)&&consumed<capacity){
  auto& slot=records[consumed];if(!slot.ready.load(std::memory_order_acquire))break;const auto& r=slot.record;
  char row[4096];auto n=snprintf(row,sizeof(row),
   "{\"event\":%u,\"kind\":%u,\"phase\":%u,\"call\":%u,\"ticks\":%llu,\"tid\":%lu,\"name\":\"%s\",\"key\":\"%016llx\","
   "\"ref\":\"%llx\",\"component\":\"%llx\",\"argumentClass\":\"%llx\",\"context\":\"%llx\",\"script\":\"%llx\","
   "\"scriptCache\":\"%llx\",\"scriptClass\":\"%llx\",\"componentCache\":\"%llx\",\"componentClass\":\"%llx\","
   "\"scriptId\":%u,\"componentScriptId\":%u,\"componentState\":%u,\"managed\":\"%llx\",\"stack\":[",
   consumed,r.kind,r.phase,r.call,r.ticks,r.thread,r.name,r.key,r.reference,r.component,r.argumentClass,r.context,r.script,
   r.state.scriptCache,r.state.scriptClass,r.state.componentCache,r.state.componentClass,
   r.state.scriptId,r.state.componentScriptId,r.state.componentState,r.state.managed);
  if(n<=0||size_t(n)>=sizeof(row))return;size_t length=size_t(n);
  for(unsigned i=0;i<r.stackCount&&i<12;++i){n=snprintf(row+length,sizeof(row)-length,"%s\"%llx\"",i?",":"",uintptr_t(r.stack[i]));
   if(n<=0||size_t(n)>=sizeof(row)-length)return;length+=size_t(n);}
  memcpy(row+length,"]}\n",3);length+=3;if(!Write(row,length))return;++consumed;
 }
 if(output!=INVALID_HANDLE_VALUE)FlushFileBuffers(output);
}
inline DWORD WINAPI Worker(void*) {
 for(;;){WaitForSingleObject(requested,250);Drain();if(stopping.load(std::memory_order_acquire)){Drain();return 0;}}
}
inline bool Compatible(void* module) noexcept {
 IMAGE_DOS_HEADER dos{};if(!Copy(module,&dos,sizeof(dos))||dos.e_magic!=IMAGE_DOS_SIGNATURE||dos.e_lfanew<=0||dos.e_lfanew>0x1000)return false;
 IMAGE_NT_HEADERS64 pe{};if(!Copy((char*)module+dos.e_lfanew,&pe,sizeof(pe))||pe.Signature!=IMAGE_NT_SIGNATURE||
  pe.FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64||pe.OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC||
  pe.FileHeader.TimeDateStamp!=0x6aba59bd||pe.OptionalHeader.SizeOfImage!=0x22a2000)return false;
 unsigned char actual[sizeof(prologue)]{};return Copy((char*)module+bindingRva,actual,sizeof(actual))&&!memcmp(actual,prologue,sizeof(actual));
}
inline bool Start(const std::filesystem::path& path,uintptr_t base) {
 if(worker)return false;std::filesystem::create_directories(path.parent_path());
 output=CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
 if(output==INVALID_HANDLE_VALUE)return false;requested=CreateEventW(nullptr,FALSE,FALSE,nullptr);
 if(!requested){CloseHandle(output);output=INVALID_HANDLE_VALUE;return false;}
 moduleBase=base;LARGE_INTEGER frequency{};QueryPerformanceFrequency(&frequency);
 char header[256];int n=snprintf(header,sizeof(header),"{\"format\":\"EFF_SCRIPT_BINDING_PROBE/1\",\"pid\":%lu,\"unityBase\":\"%llx\",\"frequency\":%llu,\"limit\":%u}\n",
  GetCurrentProcessId(),base,uint64_t(frequency.QuadPart),capacity);Write(header,size_t(n));FlushFileBuffers(output);
 stopping=false;worker=CreateThread(nullptr,0,Worker,nullptr,0,nullptr);
 if(!worker){CloseHandle(requested);CloseHandle(output);requested=nullptr;output=INVALID_HANDLE_VALUE;return false;}
 enabled=true;return true;
}
inline void Stop() noexcept {
 enabled=false;if(worker){stopping=true;SetEvent(requested);WaitForSingleObject(worker,5000);CloseHandle(worker);worker=nullptr;}
 if(requested){CloseHandle(requested);requested=nullptr;}if(output!=INVALID_HANDLE_VALUE){CloseHandle(output);output=INVALID_HANDLE_VALUE;}
}
} // namespace EiemScriptBindingProbe
