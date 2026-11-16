// Exercise real ImGui/D3D failures in an isolated host, never in the game.
#include <windows.h>
#include <d3d11.h>
#include <cstdlib>
#include <iostream>
#include <string>
#include "../../deps/imgui/imgui.h"
#include "../../deps/imgui/imgui_impl_dx11.h"
#include "../../deps/minhook_lib/include/MinHook.h"
#pragma comment(lib,"d3d11.lib")
#pragma comment(lib,"bin/imgui/eiem_imgui.lib")
HRESULT WINAPI FailVertex(ID3D11Device*,const void*,SIZE_T,ID3D11ClassLinkage*,ID3D11VertexShader** out){if(out)*out=nullptr;return E_FAIL;}
HRESULT WINAPI FailTexture(ID3D11Device*,const D3D11_TEXTURE2D_DESC*,const D3D11_SUBRESOURCE_DATA*,ID3D11Texture2D** out){if(out)*out=nullptr;return E_OUTOFMEMORY;}
HRESULT WINAPI FailView(ID3D11Device*,ID3D11Resource*,const D3D11_SHADER_RESOURCE_VIEW_DESC*,ID3D11ShaderResourceView** out){if(out)*out=nullptr;return E_FAIL;}
using MapFunction=HRESULT(WINAPI*)(ID3D11DeviceContext*,ID3D11Resource*,UINT,D3D11_MAP,UINT,D3D11_MAPPED_SUBRESOURCE*);
MapFunction originalMap=nullptr;bool constantOnly=false;
HRESULT WINAPI FailMap(ID3D11DeviceContext* ctx,ID3D11Resource* resource,UINT sub,D3D11_MAP mode,UINT flags,D3D11_MAPPED_SUBRESOURCE* output){
 if(constantOnly){ID3D11Buffer* buffer=nullptr;bool constant=false;if(SUCCEEDED(resource->QueryInterface(IID_PPV_ARGS(&buffer)))){D3D11_BUFFER_DESC desc{};buffer->GetDesc(&desc);constant=(desc.BindFlags&D3D11_BIND_CONSTANT_BUFFER)!=0;buffer->Release();}if(!constant)return originalMap(ctx,resource,sub,mode,flags,output);}
 return DXGI_ERROR_DEVICE_REMOVED;
}
void Frame(){ImGui_ImplDX11_NewFrame();auto& io=ImGui::GetIO();io.DisplaySize={600,400};io.DeltaTime=1.f/60;ImGui::NewFrame();ImGui::Begin("Failure fixture");ImGui::TextUnformatted("GPU failure must not abort the host");ImGui::End();ImGui::Render();ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());}
int main(int argc,char** argv){
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
 _set_error_mode(_OUT_TO_STDERR);_set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
 ID3D11Device* device=nullptr;ID3D11DeviceContext* context=nullptr;
 auto hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context);
 if(FAILED(hr))return 10;
 std::string mode=argc>1?argv[1]:"shader";void* hook=nullptr;void* replacement=nullptr;std::string expected;
 auto entries=*reinterpret_cast<void***>(device);
 if(mode=="shader"){hook=entries[12];replacement=reinterpret_cast<void*>(&FailVertex);expected="imgui-vertex-shader";}
 else if(mode=="texture"){hook=entries[5];replacement=reinterpret_cast<void*>(&FailTexture);expected="imgui-texture-create";}
 else if(mode=="view"){hook=entries[7];replacement=reinterpret_cast<void*>(&FailView);expected="imgui-texture-view";}
 else if(mode=="map"||mode=="constant-map"){constantOnly=mode=="constant-map";hook=(*reinterpret_cast<void***>(context))[14];replacement=reinterpret_cast<void*>(&FailMap);expected=constantOnly?"imgui-constant-map":"imgui-vertex-map";}
 else return 11;
 if(MH_Initialize()!=MH_OK||MH_CreateHook(hook,replacement,reinterpret_cast<void**>(&originalMap))!=MH_OK||MH_EnableHook(hook)!=MH_OK)return 12;
 ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;ImGui::GetIO().LogFilename=nullptr;ImGui_ImplDX11_Init(device,context);
 bool caught=false;
 try{Frame();}catch(const std::exception& e){std::string error=e.what();std::cout<<"CAUGHT "<<error<<'\n';caught=error.find(expected)!=std::string::npos;}
 MH_DisableHook(MH_ALL_HOOKS);MH_Uninitialize();
 ImGui_ImplDX11_Shutdown();ImGui::DestroyContext();
 // Recreate actual backend objects and render after fault removal.
 ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;ImGui::GetIO().LogFilename=nullptr;ImGui_ImplDX11_Init(device,context);
 try{Frame();Frame();}catch(const std::exception& e){std::cerr<<"RECOVERY_FAIL "<<e.what()<<'\n';return 13;}
 ImGui_ImplDX11_Shutdown();ImGui::DestroyContext();context->Release();device->Release();
 if(!caught){std::cerr<<"FAIL missing stage-specific exception "<<expected<<'\n';return 1;}
 std::cout<<"PASS injected "<<mode<<" failure caught without abort; real backend recreation/render succeeds\n";return 0;
}
