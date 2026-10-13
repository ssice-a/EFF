#pragma once
#include "eiem_control_panel.h"
#include "../deps/imgui/imgui.h"
#include "../deps/imgui/imgui_impl_win32.h"
#include "../deps/imgui/imgui_impl_dx11.h"
#include <d3d11.h>
#include <dxgi1_2.h>
#include <dcomp.h>
#include <dwmapi.h>
#include <wrl/client.h>
#include <sstream>
#if defined(EIEM_STATIC_RUNTIME)
#pragma comment(lib,"bin/imgui-static/eiem_imgui.lib")
#else
#pragma comment(lib,"bin/imgui/eiem_imgui.lib")
#endif
#pragma comment(lib,"d3d11.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"dcomp.lib")
#pragma comment(lib,"dwmapi.lib")
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND,UINT,WPARAM,LPARAM);

// A separate compositor window works with both game render backends.
// Only this UI thread owns ImGui/D3D; immutable Store messages reach Unity.
namespace EiemControlPanel {
class Window {
  template<class T> using Ptr=Microsoft::WRL::ComPtr<T>;
  Store& store;HWND hwnd=nullptr;bool shown=false,acrylic=false,win32Ready=false,rendererReady=false;
  ImGuiContext* gui=nullptr;
  Ptr<ID3D11Device> device;Ptr<ID3D11DeviceContext> context;Ptr<IDXGISwapChain1> swap;
  Ptr<ID3D11RenderTargetView> target;Ptr<IDCompositionDevice> compositor;
  Ptr<IDCompositionTarget> compositionTarget;Ptr<IDCompositionVisual> visual;
  std::shared_ptr<const Snapshot> view;
  UINT width=604,height=640;
  std::string holdPackage,holdName;
#ifdef EIEM_PANEL_TEST
  std::map<std::tuple<Action,std::string,std::string>,RECT> testItems;
  std::vector<std::tuple<int,int,bool>> testMouse;
#endif
  void Item(Action action,const std::string& package,const std::string& name={}) {
#ifdef EIEM_PANEL_TEST
    auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();
    testItems[{action,package,name}]={LONG(a.x),LONG(a.y),LONG(b.x),LONG(b.y)};
#else
    (void)action;(void)package;(void)name;
#endif
  }
  void StopHold() {
    if(!holdName.empty())Send(Action::Hold,holdPackage,holdName,0);
    holdPackage.clear();holdName.clear();
  }
  bool Send(Action action,const std::string& package,const std::string& name={},double value=0) {
    return view&&store.Submit({view->revision,view->scope,action,package,name,value});
  }
  void Backdrop() {
    BOOL dark=TRUE;DwmSetWindowAttribute(hwnd,DWMWA_USE_IMMERSIVE_DARK_MODE,&dark,sizeof(dark));
    DWM_WINDOW_CORNER_PREFERENCE corners=DWMWCP_ROUND;
    DwmSetWindowAttribute(hwnd,DWMWA_WINDOW_CORNER_PREFERENCE,&corners,sizeof(corners));
    DWM_SYSTEMBACKDROP_TYPE type=DWMSBT_TRANSIENTWINDOW;
    acrylic=SUCCEEDED(DwmSetWindowAttribute(hwnd,DWMWA_SYSTEMBACKDROP_TYPE,&type,sizeof(type)));
    MARGINS glass{-1,-1,-1,-1};DwmExtendFrameIntoClientArea(hwnd,&glass);
  }
  static LRESULT CALLBACK Procedure(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    auto* self=reinterpret_cast<Window*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
    if(msg==WM_NCCREATE){self=static_cast<Window*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);self->hwnd=hwnd;SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}
    if(!self)return DefWindowProcW(hwnd,msg,wp,lp);
    if(self->gui){ImGui::SetCurrentContext(self->gui);if(ImGui_ImplWin32_WndProcHandler(hwnd,msg,wp,lp))return 1;}
    if(msg==WM_CLOSE){self->store.open=false;self->StopHold();self->Visible(false);return 0;}
    if(msg==WM_SIZE&&wp!=SIZE_MINIMIZED){self->width=LOWORD(lp);self->height=HIWORD(lp);return 0;}
    if(msg==WM_ERASEBKGND)return 1;
    if(msg==WM_PAINT){PAINTSTRUCT paint{};BeginPaint(hwnd,&paint);EndPaint(hwnd,&paint);return 0;}
    if(msg==WM_DWMCOMPOSITIONCHANGED){self->Backdrop();return 0;}
    return DefWindowProcW(hwnd,msg,wp,lp);
  }
  bool Graphics() {
    if(FAILED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)))return false;
    Ptr<IDXGIDevice> dxgi;Ptr<IDXGIAdapter> adapter;Ptr<IDXGIFactory2> factory;
    if(FAILED(device.As(&dxgi))||FAILED(dxgi->GetAdapter(&adapter))||FAILED(adapter->GetParent(IID_PPV_ARGS(&factory))))return false;
    DXGI_SWAP_CHAIN_DESC1 desc{};desc.Width=width;desc.Height=height;desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;desc.SampleDesc.Count=1;
    desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.BufferCount=2;desc.SwapEffect=DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;desc.AlphaMode=DXGI_ALPHA_MODE_PREMULTIPLIED;
    if(FAILED(factory->CreateSwapChainForComposition(device.Get(),&desc,nullptr,&swap))||
       FAILED(DCompositionCreateDevice(dxgi.Get(),IID_PPV_ARGS(&compositor)))||
       FAILED(compositor->CreateTargetForHwnd(hwnd,TRUE,&compositionTarget))||
       FAILED(compositor->CreateVisual(&visual))||FAILED(visual->SetContent(swap.Get()))||
       FAILED(compositionTarget->SetRoot(visual.Get()))||FAILED(compositor->Commit()))return false;
    return true;
  }
  bool RenderTarget() {
    DXGI_SWAP_CHAIN_DESC1 desc{};if(FAILED(swap->GetDesc1(&desc)))return false;
    if(desc.Width!=width||desc.Height!=height){context->OMSetRenderTargets(0,nullptr,nullptr);target.Reset();if(FAILED(swap->ResizeBuffers(0,width,height,DXGI_FORMAT_UNKNOWN,0)))return false;}
    if(!target){Ptr<ID3D11Texture2D> buffer;if(FAILED(swap->GetBuffer(0,IID_PPV_ARGS(&buffer)))||FAILED(device->CreateRenderTargetView(buffer.Get(),nullptr,&target)))return false;}
    return true;
  }
  static std::string Label(const EiemDirectory::Package& p){return p.root.filename().u8string()+"  ("+p.id+")";}
  void Draw() {
#ifdef EIEM_PANEL_TEST
    testItems.clear();
#endif
    ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({float(width),float(height)});
    ImGui::SetNextWindowBgAlpha(acrylic?.66f:1.f);
    ImGui::Begin("EFF controls",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings);
    ImGui::TextUnformatted(u8"MOD 按键控制");ImGui::Spacing();
    ImGui::TextDisabled("%s",u8"勾选响应按键的 Mod，同一个键可以同时控制多个 Mod。");
    ImGui::Separator();ImGui::Spacing();
    if(view&&view->graph)for(const auto& p:*view->graph)if(!p.bindings.empty()||std::any_of(p.shapeAnimations.begin(),p.shapeAnimations.end(),[](const auto& c){return c.second.key.vk!=0;})){
      ImGui::PushID(p.id.c_str());bool active=view->keySwitchEnabled.at(p.id);
      if(ImGui::Checkbox(Label(p).c_str(),&active))Send(Action::Keyboard,p.id,{},active?1:0);
      Item(Action::Keyboard,p.id);ImGui::PopID();
    }
    ImGui::Spacing();ImGui::SeparatorText(u8"手动调整");
    const EiemDirectory::Package* selected=nullptr;
    if(view&&view->graph)for(const auto& p:*view->graph)if(p.id==view->selected)selected=&p;
    bool opened=ImGui::BeginCombo(u8"Mod",selected?Label(*selected).c_str():u8"无可控制的 Mod");
    Item(Action::Select,"");
    if(opened){if(view&&view->graph)for(const auto& p:*view->graph)if(!p.bindings.empty()||!p.controls.empty()||!p.shapeAnimations.empty()){
      if(ImGui::Selectable(Label(p).c_str(),p.id==view->selected)){StopHold();Send(Action::Select,p.id);}
      Item(Action::Select,p.id);
    }ImGui::EndCombo();}
    ImGui::TextDisabled("%s",u8"面板打开时暂停物理按键，按钮和滑块仍可使用。");
    std::string nextHold;
    if(selected){
      const auto& p=*selected;
      for(const auto& b:p.bindings){
        ImGui::PushID(b.id.c_str());auto control=p.controls.find(b.variable);
        std::ostringstream label;label<<'['<<EiemFormatKeyChord(b.key)<<"]  "<<(control==p.controls.end()?b.id:control->second.label);
        if(b.type=="hold")label<<u8"  按住 -> "<<b.values.front();label<<"    "<<view->values.at(p.id).at(b.variable);
        bool clicked=ImGui::Button(label.str().c_str(),{-1,32});
        Item(b.type=="hold"?Action::Hold:Action::Invoke,p.id,b.id);
        if(b.type=="hold"){if(ImGui::IsItemActive()&&ImGui::IsMouseDown(ImGuiMouseButton_Left))nextHold=b.id;}
        else if(clicked)Send(Action::Invoke,p.id,b.id);
        ImGui::PopID();
      }
      for(const auto& row:p.controls){
        ImGui::PushID(row.first.c_str());double value=view->values.at(p.id).at(row.first);const auto& c=row.second;
        if(c.minimum==c.maximum)ImGui::Text("%s = %.3f",c.label.c_str(),value);
        else{ImGui::SetNextItemWidth(-1);if(ImGui::SliderScalar(c.label.c_str(),ImGuiDataType_Double,&value,&c.minimum,&c.maximum,"%.3f"))Send(Action::Value,p.id,row.first,value);Item(Action::Value,p.id,row.first);}
        ImGui::PopID();
      }
      if(!p.shapeAnimations.empty())ImGui::SeparatorText(u8"形态动画");
      for(const auto& row:p.shapeAnimations){const auto& clip=row.second;ImGui::PushID(clip.id.c_str());const auto& playback=view->animations.at(p.id).at(clip.id);
        ImGui::Text("%s  [%s]",clip.label.c_str(),playback.playing?(playback.paused?u8"暂停":u8"播放中"):u8"停止");
        if(ImGui::Button(playback.paused?u8"继续":u8"播放"))Send(Action::AnimationPlay,p.id,clip.id);Item(Action::AnimationPlay,p.id,clip.id);ImGui::SameLine();
        if(ImGui::Button(u8"暂停"))Send(Action::AnimationPause,p.id,clip.id);Item(Action::AnimationPause,p.id,clip.id);ImGui::SameLine();
        if(ImGui::Button(u8"停止"))Send(Action::AnimationStop,p.id,clip.id);Item(Action::AnimationStop,p.id,clip.id);
        double speed=view->values.at(p.id).at(clip.speedVariable),low=.1,high=4;
        if(ImGui::SliderScalar(u8"速度倍率",ImGuiDataType_Double,&speed,&low,&high,"%.2fx"))Send(Action::AnimationSpeed,p.id,clip.id,speed);Item(Action::AnimationSpeed,p.id,clip.id);
        if(ImGui::InputDouble(u8"输入倍率",&speed,.1,1,"%.3f")&&std::isfinite(speed)&&speed>0)Send(Action::AnimationSpeed,p.id,clip.id,speed);
        if(ImGui::Button(u8"恢复作者默认"))Send(Action::AnimationReset,p.id,clip.id);Item(Action::AnimationReset,p.id,clip.id);ImGui::PopID();
      }
      if(ImGui::CollapsingHeader(u8"资源来源"))for(const auto& row:p.targetIndex)ImGui::TextWrapped("%s",row.first.path.c_str());
      if(nextHold!=holdName){StopHold();if(!nextHold.empty()&&Send(Action::Hold,p.id,nextHold,1)){holdPackage=p.id;holdName=nextHold;}}
    }else StopHold();
    ImGui::Spacing();ImGui::Separator();
    ImGui::TextWrapped("%s",u8"按键开关自动保存到 mod.ini。手改 key_switch_enabled 后按 F10 生效。");
    ImGui::End();
  }
  void Visible(bool visible) {
    if(shown==visible)return;shown=visible;
    if(visible){ClipCursor(nullptr);ShowWindow(hwnd,SW_SHOW);SetForegroundWindow(hwnd);}
    else{StopHold();bool owned=GetForegroundWindow()==hwnd;ShowWindow(hwnd,SW_HIDE);if(owned&&IsWindow(store.gameWindow.load()))SetForegroundWindow(store.gameWindow.load());}
  }
public:
  explicit Window(Store& store):store(store){}
  ~Window(){Visible(false);store.window=nullptr;if(gui){ImGui::SetCurrentContext(gui);if(rendererReady)ImGui_ImplDX11_Shutdown();if(win32Ready)ImGui_ImplWin32_Shutdown();ImGui::DestroyContext(gui);gui=nullptr;}if(hwnd)DestroyWindow(hwnd);}
  HWND Handle()const{return hwnd;}
#ifdef EIEM_PANEL_TEST
  RECT TestRect(Action action,const std::string& package,const std::string& name={})const{return testItems.at({action,package,name});}
  bool TestAcrylic()const{return acrylic;}
  void TestMouse(int x,int y,bool down){testMouse.emplace_back(x,y,down);}
#endif
  bool Create() {
    WNDCLASSW wc{};wc.lpfnWndProc=Procedure;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"EFF_ResourceControls";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);
    if(!RegisterClassW(&wc)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return false;
    RECT r{0,0,LONG(width),LONG(height)};DWORD style=WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_THICKFRAME;
    AdjustWindowRectEx(&r,style,FALSE,WS_EX_TOOLWINDOW|WS_EX_TOPMOST);
    // Cross-thread owned windows share activation/hide work with their owner.
    // Keep the panel independent so closing it never waits for Unity's HWND.
    hwnd=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_TOPMOST,wc.lpszClassName,L"EFF Mod 按键控制",style,CW_USEDEFAULT,CW_USEDEFAULT,r.right-r.left,r.bottom-r.top,nullptr,nullptr,wc.hInstance,this);
    if(!hwnd)return false;Backdrop();if(!Graphics())return false;
    IMGUI_CHECKVERSION();gui=ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;
    char windows[MAX_PATH]{};GetWindowsDirectoryA(windows,MAX_PATH);auto font=std::string(windows)+"/Fonts/msyh.ttc";
    if(std::filesystem::is_regular_file(std::filesystem::u8path(font)))io.Fonts->AddFontFromFileTTF(font.c_str(),17.f,nullptr,io.Fonts->GetGlyphRangesChineseFull());
    ImGui::StyleColorsDark();auto& styleSettings=ImGui::GetStyle();styleSettings.WindowPadding={18,18};styleSettings.ItemSpacing={10,10};styleSettings.FramePadding={9,6};styleSettings.FrameRounding=5;styleSettings.PopupRounding=8;styleSettings.GrabRounding=4;styleSettings.WindowBorderSize=0;
    styleSettings.Colors[ImGuiCol_WindowBg]={.045f,.055f,.075f,.8f};styleSettings.Colors[ImGuiCol_CheckMark]={.40f,.72f,1.f,1.f};
    styleSettings.Colors[ImGuiCol_Button]={.16f,.26f,.38f,.9f};styleSettings.Colors[ImGuiCol_ButtonHovered]={.22f,.37f,.54f,1.f};styleSettings.Colors[ImGuiCol_FrameBg]={.12f,.16f,.23f,.85f};
    win32Ready=ImGui_ImplWin32_Init(hwnd);rendererReady=win32Ready&&ImGui_ImplDX11_Init(device.Get(),context.Get());
    if(!rendererReady)return false;store.window=hwnd;return true;
  }
  void Tick(HWND foreground=GetForegroundWindow()) {
    HWND game=store.gameWindow.load();
    bool wanted=store.open&&IsWindow(game)&&!IsIconic(game)&&(foreground==hwnd||foreground==game);
    auto next=store.Read();if(view&&next&&(view->scope!=next->scope||view->revision!=next->revision))StopHold();view=std::move(next);
    Visible(wanted&&bool(view));if(!shown)return;
    if(!width||!height||!RenderTarget())throw std::runtime_error("control panel graphics target unavailable");
    ImGui::SetCurrentContext(gui);ImGui_ImplDX11_NewFrame();ImGui_ImplWin32_NewFrame();
#ifdef EIEM_PANEL_TEST
    if(!testMouse.empty()){auto& io=ImGui::GetIO();io.ConfigInputTrickleEventQueue=false;io.AddFocusEvent(true);for(const auto& event:testMouse){io.AddMousePosEvent(float(std::get<0>(event)),float(std::get<1>(event)));io.AddMouseButtonEvent(0,std::get<2>(event));}testMouse.clear();}
#endif
    ImGui::NewFrame();Draw();
    if(ImGui::IsKeyPressed(ImGuiKey_Escape)){store.open=false;StopHold();}
    ImGui::Render();const float clear[4]={0,0,0,0};auto rt=target.Get();context->OMSetRenderTargets(1,&rt,nullptr);context->ClearRenderTargetView(rt,clear);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());if(FAILED(swap->Present(1,0)))throw std::runtime_error("control panel present failed");
  }
  static DWORD WINAPI Run(void* arg) {
    auto& store=*static_cast<Store*>(arg);
    try{Window panel(store);MSG message{};
      while(!store.stopping&&!store.failed){
        if(!panel.Handle()){if(!store.open){Sleep(60);continue;}if(!panel.Create()){store.open=false;store.failed=true;return 1;}}
        while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}
        panel.Tick();Sleep(store.open?8:60);
      }
    }catch(const std::exception&){store.open=false;store.failed=true;return 1;}
    store.open=false;return 0;
  }
};
} // namespace EiemControlPanel
