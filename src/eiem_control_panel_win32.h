#pragma once
#include "eiem_control_panel.h"
#include "eiem_gui_images.h"
#include "../deps/imgui/imgui.h"
#include "../deps/imgui/imgui_impl_win32.h"
#include "../deps/imgui/imgui_impl_dx11.h"
#include <d3d11.h>
#include <dxgi1_2.h>
#include <dcomp.h>
#include <dwmapi.h>
#include <wrl/client.h>
#include <limits>
#include <sstream>
#if defined(EIEM_STATIC_RUNTIME)
#pragma comment(lib,"bin/imgui-static/eiem_imgui.lib")
#pragma comment(lib,"bin/imgui-static/eiem_lua.lib")
#else
#pragma comment(lib,"bin/imgui/eiem_imgui.lib")
#pragma comment(lib,"bin/imgui/eiem_lua.lib")
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
  bool drewIns=false;
  std::shared_ptr<const Snapshot> view;
  EiemGuiImages customImages;
  EiemGuiState customState;
  struct CustomPanel {
    EiemLuaUi script;
    uint64_t session;
    ULONGLONG queueNoticeUntil=0;
    CustomPanel(const std::string& id,EiemGuiState& state,uint64_t serial):script(id,&state,serial),session(serial){}
  };
  std::map<std::string,CustomPanel> custom;
  std::map<std::string,std::string> customErrors;
  ULONGLONG insQueueNoticeUntil=0;
  static constexpr const char* queueFullMessage=u8"操作队列已满，本次操作未提交。请稍后重试。";
  UINT width=604,height=640;
  std::exception_ptr pendingError;
  bool composition=true;
  void Check(HRESULT result,const char* stage) {
#ifdef EIEM_PANEL_TEST
    if(store.graphicsFailure){auto injected=store.graphicsFailure(stage);if(FAILED(injected))result=injected;}
#endif
    if(SUCCEEDED(result))return;
    std::ostringstream message;message<<"stage="<<stage<<" hr=0x"<<std::hex<<uint32_t(result);
    if(device)message<<" removed=0x"<<uint32_t(device->GetDeviceRemovedReason());
    throw std::runtime_error(message.str());
  }
  static constexpr UINT_PTR resizeTimer=1;
  std::string holdPackage,holdName;
#ifdef EIEM_PANEL_TEST
  std::map<std::tuple<Action,std::string,std::string>,RECT> testItems;
  std::vector<std::tuple<int,int,bool>> testMouse;
  float testScrollMax=0;
  bool testCaptureBackground=false;
  UINT testBackgroundX=0,testBackgroundY=0,testBackgroundAlpha=0;
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
    if(!view)return false;
    if(store.Submit({view->revision,view->scope,action,package,name,value}))return true;
    QueueFull(insQueueNoticeUntil,"insMod=",package);
    return false;
  }
  void QueueFull(ULONGLONG& noticeUntil,const char* source,const std::string& package) {
    const auto now=GetTickCount64();
    if(now>=noticeUntil)store.Report(std::string(source)+package+" reason=command-queue-full");
    noticeUntil=now+4000;
  }
  void DrawQueueNotice(bool ins) {
    const auto now=GetTickCount64();
    if(!(ins&&now<insQueueNoticeUntil)&&std::none_of(custom.begin(),custom.end(),[&](const auto& row){
      return now<row.second.queueNoticeUntil;
    }))return;
    ImGui::SetNextWindowPos({float(width)/2,12},ImGuiCond_Always,{.5f,0});
    if(ImGui::Begin("##EFF command queue",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoInputs|
        ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoFocusOnAppearing|ImGuiWindowFlags_AlwaysAutoResize)){
      ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
      ImGui::TextUnformatted(queueFullMessage);
    }
    ImGui::End();
  }
  void Backdrop() {
    BOOL dark=TRUE;DwmSetWindowAttribute(hwnd,DWMWA_USE_IMMERSIVE_DARK_MODE,&dark,sizeof(dark));
    DWM_WINDOW_CORNER_PREFERENCE corners=DWMWCP_ROUND;
    DwmSetWindowAttribute(hwnd,DWMWA_WINDOW_CORNER_PREFERENCE,&corners,sizeof(corners));
    DWM_SYSTEMBACKDROP_TYPE type=DWMSBT_TRANSIENTWINDOW;
    acrylic=SUCCEEDED(DwmSetWindowAttribute(hwnd,DWMWA_SYSTEMBACKDROP_TYPE,&type,sizeof(type)));
    MARGINS glass{-1,-1,-1,-1};DwmExtendFrameIntoClientArea(hwnd,&glass);
  }
  int GripSize() const {return MulDiv(20,int(GetDpiForWindow(hwnd)),96);}
  static LRESULT CALLBACK Procedure(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    auto* self=reinterpret_cast<Window*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
    if(msg==WM_NCCREATE){self=static_cast<Window*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);self->hwnd=hwnd;SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}
    if(!self)return DefWindowProcW(hwnd,msg,wp,lp);
    if(msg==WM_GETMINMAXINFO){
      RECT minimum{0,0,420,320};
      AdjustWindowRectExForDpi(&minimum,DWORD(GetWindowLongPtrW(hwnd,GWL_STYLE)),FALSE,DWORD(GetWindowLongPtrW(hwnd,GWL_EXSTYLE)),GetDpiForWindow(hwnd));
      auto info=reinterpret_cast<MINMAXINFO*>(lp);info->ptMinTrackSize={minimum.right-minimum.left,minimum.bottom-minimum.top};return 0;
    }
    if(msg==WM_NCHITTEST){
      auto hit=DefWindowProcW(hwnd,msg,wp,lp);if(hit!=HTCLIENT||IsZoomed(hwnd))return hit;
      POINT p{static_cast<short>(LOWORD(lp)),static_cast<short>(HIWORD(lp))};ScreenToClient(hwnd,&p);RECT client{};GetClientRect(hwnd,&client);
      if(p.x>=client.right-self->GripSize()&&p.y>=client.bottom-self->GripSize())return HTBOTTOMRIGHT;
      return hit;
    }
    // The native move/resize loop owns message dispatch until the drag ends.
    // Keep rendering here on the UI thread, including swap-chain resizing.
    if(msg==WM_ENTERSIZEMOVE){self->StopHold();SetTimer(hwnd,resizeTimer,16,nullptr);return 0;}
    if(msg==WM_EXITSIZEMOVE){KillTimer(hwnd,resizeTimer);return 0;}
    if(msg==WM_TIMER&&wp==resizeTimer){
      try{if(self->rendererReady)self->Tick();}catch(...){self->pendingError=std::current_exception();KillTimer(hwnd,resizeTimer);}
      return 0;
    }
    if(msg==WM_DESTROY)KillTimer(hwnd,resizeTimer);
    if(self->gui){ImGui::SetCurrentContext(self->gui);if(ImGui_ImplWin32_WndProcHandler(hwnd,msg,wp,lp))return 1;}
    if(msg==WM_CLOSE){self->store.Close();self->StopHold();self->Visible(false);return 0;}
    if(msg==WM_SIZE&&wp!=SIZE_MINIMIZED){self->width=LOWORD(lp);self->height=HIWORD(lp);return 0;}
    if(msg==WM_ERASEBKGND)return 1;
    if(msg==WM_PAINT){PAINTSTRUCT paint{};BeginPaint(hwnd,&paint);EndPaint(hwnd,&paint);return 0;}
    if(msg==WM_DWMCOMPOSITIONCHANGED){if(self->composition)self->Backdrop();return 0;}
    return DefWindowProcW(hwnd,msg,wp,lp);
  }
  bool Graphics() {
    store.Report(std::string("stage=device-create backend=")+(composition?"composition":"hwnd"));
    Check(D3D11CreateDevice(nullptr,driver,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context),"device-create");
    Ptr<IDXGIDevice> dxgi;Ptr<IDXGIAdapter> adapter;Ptr<IDXGIFactory2> factory;
    Check(device.As(&dxgi),"dxgi-device");Check(dxgi->GetAdapter(&adapter),"adapter");Check(adapter->GetParent(IID_PPV_ARGS(&factory)),"factory");
    DXGI_ADAPTER_DESC adapterInfo{};Check(adapter->GetDesc(&adapterInfo),"adapter-info");
    store.Report("stage=device-ready vendor="+std::to_string(adapterInfo.VendorId)+" device="+std::to_string(adapterInfo.DeviceId)+" driver="+std::to_string(driver));
    DXGI_SWAP_CHAIN_DESC1 desc{};desc.Width=width;desc.Height=height;desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;desc.SampleDesc.Count=1;
    desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.BufferCount=2;desc.SwapEffect=DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;desc.AlphaMode=DXGI_ALPHA_MODE_PREMULTIPLIED;
    if(composition){
      store.Report("stage=composition-create");
      Check(factory->CreateSwapChainForComposition(device.Get(),&desc,nullptr,&swap),"composition-swap");
      Check(DCompositionCreateDevice(dxgi.Get(),IID_PPV_ARGS(&compositor)),"composition-device");
      Check(compositor->CreateTargetForHwnd(hwnd,TRUE,&compositionTarget),"composition-target");
      Check(compositor->CreateVisual(&visual),"composition-visual");Check(visual->SetContent(swap.Get()),"composition-content");
      Check(compositionTarget->SetRoot(visual.Get()),"composition-root");Check(compositor->Commit(),"composition-commit");
    }else{
      desc.AlphaMode=DXGI_ALPHA_MODE_IGNORE;desc.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
      store.Report("stage=hwnd-swap-create");
      Check(factory->CreateSwapChainForHwnd(device.Get(),hwnd,&desc,nullptr,nullptr,&swap),"hwnd-swap");
      factory->MakeWindowAssociation(hwnd,DXGI_MWA_NO_ALT_ENTER);
    }
    return true;
  }
  D3D_DRIVER_TYPE driver=D3D_DRIVER_TYPE_HARDWARE;
  void ReleaseGraphics() {
    custom.clear();
    customImages.Clear();
    if(gui)ImGui::SetCurrentContext(gui);
    if(rendererReady){ImGui_ImplDX11_Shutdown();rendererReady=false;}
    if(context)context->ClearState();target.Reset();visual.Reset();compositionTarget.Reset();compositor.Reset();swap.Reset();context.Reset();device.Reset();
  }
  bool RenderTarget() {
    DXGI_SWAP_CHAIN_DESC1 desc{};Check(swap->GetDesc1(&desc),"swap-description");
    if(desc.Width!=width||desc.Height!=height){context->OMSetRenderTargets(0,nullptr,nullptr);target.Reset();Check(swap->ResizeBuffers(0,width,height,DXGI_FORMAT_UNKNOWN,0),"resize");}
    if(!target){Ptr<ID3D11Texture2D> buffer;Check(swap->GetBuffer(0,IID_PPV_ARGS(&buffer)),"backbuffer");Check(device->CreateRenderTargetView(buffer.Get(),nullptr,&target),"render-target");}
    return true;
  }
  static std::string Label(const EiemDirectory::Package& p){return ModLabel(p);}
  static bool EffectToggle(const EiemDirectory::Package& p,const std::string& variable){
    return std::any_of(p.effects.begin(),p.effects.end(),[&](const auto& row){return row.second.variable==variable;});
  }
  bool SliderWithNumber(const char* label,double& value,double low,double high,
                        const char* format,Action action,const std::string& package,const std::string& name){
    ImGui::TextWrapped("%s",label);
    // ImGui asserts outside this range. The numeric input still supports the
    // author's full domain and uses the usual runtime validation.
    constexpr double sliderLimit=std::numeric_limits<double>::max()/2.0;
    const bool canSlide=low>=-sliderLimit&&high<=sliderLimit;
    float inputWidth=-1.f;
    bool changed=false;
    if(canSlide){
      const float width=ImGui::GetContentRegionAvail().x;
      const float numberWidth=ImGui::CalcTextSize("-0.000000").x+2*ImGui::GetStyle().FramePadding.x;
      const float spacing=ImGui::GetStyle().ItemSpacing.x;
      const bool inlineNumber=width>=2*numberWidth+spacing;
      ImGui::SetNextItemWidth(inlineNumber?width-numberWidth-spacing:-1.f);
      changed=ImGui::SliderScalar("##slider",ImGuiDataType_Double,&value,&low,&high,format,ImGuiSliderFlags_NoInput);
      Item(action,package,name);
      if(inlineNumber){ImGui::SameLine();inputWidth=numberWidth;}
    }
    ImGui::SetNextItemWidth(inputWidth);
    const bool typed=ImGui::InputDouble("##number",&value,0,0,"%.9g");
    Item(action,package,name+"##number");
    if(ImGui::IsItemHovered())ImGui::SetTooltip(u8"直接输入数值");
    return typed||changed;
  }
  void Parameter(const EiemDirectory::Package& p,const EiemControlProgram::Parameter& d,const std::string& type,const std::string& label){
    double actual=view->effective.at(p.id).at(d.variable);
    double value=d.computed?actual:view->values.at(p.id).at(d.variable);
    ImGui::BeginDisabled(bool(d.computed));bool changed=false;
    if(type=="checkbox"||(type.empty()&&d.type==EiemControlProgram::Type::Boolean)){bool b=value!=0;changed=ImGui::Checkbox(label.c_str(),&b);value=b?1:0;}
    else if(type=="number"){ImGui::TextUnformatted(label.c_str());ImGui::SetNextItemWidth(-1);changed=ImGui::InputDouble("##value",&value,0,0,"%.6g");}
    else changed=SliderWithNumber(label.c_str(),value,d.softMin,d.softMax,"%.4g",Action::Value,p.id,d.variable);
    if(d.type==EiemControlProgram::Type::Integer)value=std::round(value);
    if(changed)Send(Action::Value,p.id,d.variable,value);
    if(type=="number"||type=="checkbox"||(type.empty()&&d.type==EiemControlProgram::Type::Boolean))Item(Action::Value,p.id,d.variable);
    ImGui::EndDisabled();
    if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)){ImGui::BeginTooltip();ImGui::Text("%s",d.variable.c_str());ImGui::Text(u8"设定值: %.6g",view->values.at(p.id).at(d.variable));ImGui::Text(u8"生效值: %.6g",view->effective.at(p.id).at(d.variable));if(d.computed)ImGui::TextUnformatted(u8"由表达式计算（只读）");else{auto animation=view->animations.find(p.id);if(animation!=view->animations.end())for(const auto& clip:p.shapeAnimations)if(clip.second.tracks.count(d.variable)&&animation->second.at(clip.first).playing)ImGui::Text(u8"动画驱动: %s",clip.first.c_str());}if(!d.description.empty())ImGui::TextWrapped("%s",d.description.c_str());ImGui::EndTooltip();}
    if(d.computed||actual!=view->values.at(p.id).at(d.variable))ImGui::TextDisabled(u8"生效值: %.6g%s",actual,d.computed?u8"（表达式）":u8"（动画）");
  }
  bool ProgramNode(const EiemDirectory::Package& p,const std::string& id){
    const auto& program=*p.program;const auto& u=program.ui.at(id);const auto& values=view->effective.at(p.id);
    bool disabled=false;try{if(u.visible&&!u.visible->Evaluate(values))return false;disabled=u.enabled&&!u.enabled->Evaluate(values);}catch(const std::exception& e){ImGui::TextWrapped("%s: %s",id.c_str(),e.what());return false;}
    ImGui::PushID(id.c_str());ImGui::BeginDisabled(disabled);
    auto children=[&](){auto f=program.children.find(id);if(f==program.children.end()||f->second.empty())return;
      if(u.type=="row"){if(ImGui::BeginTable("##row",int(f->second.size()),ImGuiTableFlags_SizingStretchSame)){for(const auto& child:f->second){ImGui::TableNextColumn();ProgramNode(p,child);}ImGui::EndTable();}}
      else for(const auto& child:f->second)ProgramNode(p,child);};
    if(u.type=="group"){if(ImGui::CollapsingHeader(u.label.c_str(),u.expanded?ImGuiTreeNodeFlags_DefaultOpen:0))children();}
    else if(u.type=="row")children();
    else if(u.type=="tabs"){if(ImGui::BeginTabBar("##tabs")){children();ImGui::EndTabBar();}}
    else if(u.type=="tab"){if(ImGui::BeginTabItem(u.label.c_str())){children();ImGui::EndTabItem();}}
    else if(u.type=="button"){if(ImGui::Button(u.label.c_str()))Send(Action::InvokeAction,p.id,u.action);Item(Action::InvokeAction,p.id,u.action);}
    else if(u.type=="text"){if(u.variable.empty())ImGui::TextWrapped("%s",u.label.c_str());else ImGui::Text("%s: %.6g",u.label.c_str(),values.at(u.variable));}
    else Parameter(p,program.parameters.at(u.variable),u.type,u.label);
    ImGui::EndDisabled();ImGui::PopID();return true;
  }
  void Program(const EiemDirectory::Package& p){
    auto error=view->errors.find(p.id);if(error!=view->errors.end())ImGui::TextWrapped("%s",error->second.c_str());
    try{const auto& program=*p.program;
      if(!program.ui.empty()){auto root=program.children.find("");if(root!=program.children.end())for(const auto& id:root->second)ProgramNode(p,id);}
      for(const auto& row:program.parameters){const auto& parameter=row.second;
        if(!parameter.exposed||(!program.ui.empty()&&(parameter.uiType.empty()||program.placedParameters.count(row.first))))continue;
        ImGui::PushID(row.first.c_str());Parameter(p,parameter,parameter.uiType,parameter.label);ImGui::PopID();}
      if(program.ui.empty())for(const auto& row:program.actions)if(row.second.exposed){ImGui::PushID(row.first.c_str());if(ImGui::Button(row.second.label.c_str()))Send(Action::InvokeAction,p.id,row.first);Item(Action::InvokeAction,p.id,row.first);ImGui::PopID();}
    }catch(const std::exception& e){ImGui::TextWrapped(u8"UI 表达式错误: %s",e.what());}
  }
  void CustomSettings(const EiemDirectory::Package& p) {
    bool enabled=view->customGuiEnabled.at(p.id);
    ImGui::PushID(p.id.c_str());
    if(ImGui::Checkbox(u8"允许作者面板",&enabled))Send(Action::GuiEnabled,p.id,{},enabled?1:0);
    ImGui::SameLine();
    bool opened=store.GuiSession(p.id)!=0;
    ImGui::BeginDisabled(!enabled||!p.customGui.error.empty()||!p.loadError.empty());
    if(ImGui::Button(opened?u8"关闭作者面板":u8"打开作者面板")&&!store.ToggleGui(*view,p.id))
      customErrors[p.id]=u8"作者面板打开数量已达上限";
    ImGui::EndDisabled();
    if(p.customGui.key.vk)ImGui::TextDisabled("%s",EiemFormatKeyChord(p.customGui.key).c_str());
    if(!p.customGui.error.empty())ImGui::TextWrapped("%s",p.customGui.error.c_str());
    if(auto failure=customErrors.find(p.id);failure!=customErrors.end())
      ImGui::TextWrapped("%s",failure->second.c_str());
    if(auto editor=custom.find(p.id);editor!=custom.end()&&GetTickCount64()<editor->second.queueNoticeUntil)
      ImGui::TextWrapped("%s",queueFullMessage);
    if(!p.program)if(auto failure=view->errors.find(p.id);failure!=view->errors.end())
      ImGui::TextWrapped("%s",failure->second.c_str());
    ImGui::PopID();
  }
  void DrawCustom(const std::map<std::string,uint64_t>& open) {
    if(!view||!view->graph||open.empty())return;
    for(const auto& p:*view->graph)if(open.count(p.id)) {
      if(!view->customGuiEnabled.count(p.id)||!view->customGuiEnabled.at(p.id)||
         !p.customGui.error.empty()||!p.loadError.empty()){store.CloseGui(p.id);continue;}
      auto [item,created]=custom.try_emplace(p.id,p.id,customState,open.at(p.id));
      auto& editor=item->second;
      if(created)editor.script.LoadFile((p.root/"mod.ini").u8string(),p.customGui.script);
      EiemLuaUi::ImageLoader images=[&](const char* path,ImVec2 sampleSize){
        return customImages.Load(device.Get(),editor.session,p.root,path,sampleSize);
      };
      EiemVariables writes;
      if(editor.script.Draw(view->effective.at(p.id),writes,p.defaults,&images,view->animations.at(p.id))) {
        customErrors.erase(p.id);
        auto requests=editor.script.TakeRequests();
        if(!requests.empty()&&!store.Submit({view->revision,editor.session,Action::GuiRequests,p.id,{},0,std::move(requests)}))
          QueueFull(editor.queueNoticeUntil,"customGui=",p.id);
      } else {
        customErrors[p.id]=editor.script.error;
        store.Report("customGui="+p.id+" reason="+editor.script.error);
        store.CloseGui(p.id);
      }
      if(editor.script.closeRequested)store.CloseGui(p.id);
    }
  }
  void Draw() {
#ifdef EIEM_PANEL_TEST
    testItems.clear();
#endif
    ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({float(width),float(height)});
    ImGui::SetNextWindowBgAlpha(1.f);
    ImGui::Begin("EFF controls",nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings);
    ImGui::TextUnformatted(u8"MOD 按键控制");ImGui::Spacing();
    ImGui::PushTextWrapPos(0);ImGui::TextDisabled("%s",u8"勾选响应按键的 Mod，同一个键可以同时控制多个 Mod。");ImGui::PopTextWrapPos();
    ImGui::Separator();ImGui::Spacing();
    if(view&&view->graph)for(const auto& p:*view->graph)if(p.customGui.key.vk||(p.program&&!p.program->events.empty())||!p.bindings.empty()||std::any_of(p.shapeAnimations.begin(),p.shapeAnimations.end(),[](const auto& c){return c.second.key.vk!=0;})){
      ImGui::PushID(p.id.c_str());bool active=view->keySwitchEnabled.at(p.id);
      if(ImGui::Checkbox(Label(p).c_str(),&active))Send(Action::Keyboard,p.id,{},active?1:0);
      Item(Action::Keyboard,p.id);ImGui::PopID();
    }
    ImGui::Spacing();ImGui::SeparatorText(u8"手动调整");
    const EiemDirectory::Package* selected=nullptr;
    if(view&&view->graph)for(const auto& p:*view->graph)if(p.id==view->selected)selected=&p;
    ImGui::TextUnformatted("Mod");ImGui::SetNextItemWidth(-1);
    bool opened=ImGui::BeginCombo("##Mod",selected?Label(*selected).c_str():u8"无可控制的 Mod");
    Item(Action::Select,"");
    if(opened){if(view&&view->graph)for(const auto& p:*view->graph)if(!p.customGui.script.empty()||p.program||!p.bindings.empty()||!p.controls.empty()||!p.shapeAnimations.empty()){
      ImGui::PushID(p.id.c_str());
      if(ImGui::Selectable(Label(p).c_str(),p.id==view->selected)){StopHold();Send(Action::Select,p.id);}
      Item(Action::Select,p.id);
      ImGui::PopID();
    }ImGui::EndCombo();}
    ImGui::PushTextWrapPos(0);ImGui::TextDisabled("%s",u8"面板打开时暂停物理按键，按钮和滑块仍可使用。");ImGui::PopTextWrapPos();
    std::string nextHold;
    if(selected){
      const auto& p=*selected;
      if(!p.loadError.empty())ImGui::TextWrapped(u8"保留上次有效配置: %s",p.loadError.c_str());
      if(!p.customGui.script.empty())CustomSettings(p);
      if(p.program)Program(p);
      if(!p.program||p.program->ui.empty()){
      for(const auto& b:p.bindings){
        ImGui::PushID(b.id.c_str());auto control=p.controls.find(b.variable);
        const auto& name=!b.label.empty()?b.label:(control==p.controls.end()?b.id:control->second.label);
        std::ostringstream label;label<<'['<<EiemFormatKeyChord(b.key)<<"]  "<<name;
        if(b.type=="hold")label<<u8"  按住 -> "<<b.values.front();label<<"    "<<view->values.at(p.id).at(b.variable);
        bool clicked=ImGui::Button(label.str().c_str(),{-1,32});
        Item(b.type=="hold"?Action::Hold:Action::Invoke,p.id,name);
        if(b.type=="hold"){if(ImGui::IsItemActive()&&ImGui::IsMouseDown(ImGuiMouseButton_Left))nextHold=b.id;}
        else if(clicked)Send(Action::Invoke,p.id,b.id);
        ImGui::PopID();
      }
      for(const auto& row:p.controls){
        if(p.program&&p.program->parameters.count(row.first))continue;
        if(p.materialTracks.count(row.first))continue;
        ImGui::PushID(row.first.c_str());double value=view->values.at(p.id).at(row.first);const auto& c=row.second;
        if(EffectToggle(p,row.first)){bool enabled=value>=.5;if(ImGui::Checkbox(c.label.c_str(),&enabled))Send(Action::Value,p.id,row.first,enabled?1:0);Item(Action::Value,p.id,row.first);}
        else if(c.minimum==c.maximum)ImGui::Text("%s = %.3f",c.label.c_str(),value);
        else if(SliderWithNumber(c.label.c_str(),value,c.minimum,c.maximum,"%.3f",Action::Value,p.id,row.first))
          Send(Action::Value,p.id,row.first,value);
        ImGui::PopID();
      }
      for(bool material:{false,true}){
      bool heading=false;
      for(const auto& row:p.shapeAnimations){if(bool(p.materialAnimationIds.count(row.first))!=material)continue;
        if(!heading){ImGui::SeparatorText(material?u8"材质参数动画":u8"形态动画");heading=true;}
        const auto& clip=row.second;ImGui::PushID(clip.id.c_str());const auto& playback=view->animations.at(p.id).at(clip.id);
        ImGui::Text("%s  [%s]",clip.label.c_str(),playback.playing?(playback.paused?u8"暂停":u8"播放中"):u8"停止");
        if(ImGui::Button(playback.paused?u8"继续":u8"播放"))Send(Action::AnimationPlay,p.id,clip.id);Item(Action::AnimationPlay,p.id,clip.id);ImGui::SameLine();
        if(ImGui::Button(u8"暂停"))Send(Action::AnimationPause,p.id,clip.id);Item(Action::AnimationPause,p.id,clip.id);ImGui::SameLine();
        if(ImGui::Button(u8"停止"))Send(Action::AnimationStop,p.id,clip.id);Item(Action::AnimationStop,p.id,clip.id);
        double speed=view->values.at(p.id).at(clip.speedVariable),low=.1,high=4;
        if(SliderWithNumber(u8"速度倍率",speed,low,high,"%.2fx",Action::AnimationSpeed,p.id,clip.id)&&std::isfinite(speed)&&speed>0)
          Send(Action::AnimationSpeed,p.id,clip.id,speed);
        if(ImGui::Button(u8"恢复作者默认"))Send(Action::AnimationReset,p.id,clip.id);Item(Action::AnimationReset,p.id,clip.id);ImGui::PopID();
      }
      }
      }
      if(ImGui::CollapsingHeader(u8"资源来源"))for(const auto& row:p.targetIndex)ImGui::TextWrapped("%s",row.first.path.c_str());
      if(nextHold!=holdName){StopHold();if(!nextHold.empty()&&Send(Action::Hold,p.id,nextHold,1)){holdPackage=p.id;holdName=nextHold;}}
    }else StopHold();
    ImGui::Spacing();ImGui::Separator();
    ImGui::TextWrapped("%s",u8"按键开关自动保存到 mod.ini。手改 key_switch_enabled 后按 F10 生效。");
#ifdef EIEM_PANEL_TEST
    testScrollMax=ImGui::GetScrollMaxY();
#endif
    ImGui::End();
    if(!IsZoomed(hwnd)){
      auto draw=ImGui::GetForegroundDrawList();float size=float(GripSize());auto color=ImGui::GetColorU32(ImGuiCol_TextDisabled);
      for(float inset:{4.f,9.f,14.f})draw->AddLine({float(width)-size+inset,float(height)-3},{float(width)-3,float(height)-size+inset},color);
    }
  }
  void Visible(bool visible) {
    if(shown==visible)return;shown=visible;
    if(visible){ClipCursor(nullptr);ShowWindow(hwnd,SW_SHOW);SetForegroundWindow(hwnd);}
    else{StopHold();ShowWindow(hwnd,SW_HIDE);}
  }
public:
  explicit Window(Store& store):store(store){}
  ~Window(){Visible(false);store.window=nullptr;ReleaseGraphics();if(gui){if(win32Ready)ImGui_ImplWin32_Shutdown();ImGui::DestroyContext(gui);gui=nullptr;}if(hwnd)DestroyWindow(hwnd);}
  HWND Handle()const{return hwnd;}
#ifdef EIEM_PANEL_TEST
  ID3D11Device* TestDevice()const{return device.Get();}
  RECT TestRect(Action action,const std::string& package,const std::string& name={})const{return testItems.at({action,package,name});}
  bool TestAcrylic()const{return acrylic;}
  std::pair<UINT,UINT> TestBufferSize()const{DXGI_SWAP_CHAIN_DESC1 desc{};if(FAILED(swap->GetDesc1(&desc)))throw std::runtime_error("test swap-chain unavailable");return {desc.Width,desc.Height};}
  void TestCaptureBackgroundAlpha(UINT x,UINT y){testBackgroundX=x;testBackgroundY=y;testCaptureBackground=true;}
  UINT TestBackgroundAlpha()const{return testBackgroundAlpha;}
  float TestScrollMax()const{return testScrollMax;}
  void TestMouse(int x,int y,bool down){testMouse.emplace_back(x,y,down);}
#endif
  unsigned Backend() const {return composition?0:driver==D3D_DRIVER_TYPE_HARDWARE?1:2;}
  bool Create(unsigned firstAttempt=0) {
    store.Report("stage=window-create");
    WNDCLASSW wc{};wc.lpfnWndProc=Procedure;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"EFF_ResourceControls";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);
    if(!RegisterClassW(&wc)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return false;
    RECT r{0,0,LONG(width),LONG(height)};DWORD style=WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_THICKFRAME|WS_MAXIMIZEBOX;
    AdjustWindowRectEx(&r,style,FALSE,WS_EX_TOOLWINDOW|WS_EX_TOPMOST);
    // Cross-thread owned windows share activation/hide work with their owner.
    // Keep the panel independent so closing it never waits for Unity's HWND.
    hwnd=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_TOPMOST,wc.lpszClassName,L"EFF Mod 按键控制",style,CW_USEDEFAULT,CW_USEDEFAULT,r.right-r.left,r.bottom-r.top,nullptr,nullptr,wc.hInstance,this);
    if(!hwnd){store.Report("stage=window-create win32="+std::to_string(GetLastError()));return false;}
    store.Report("stage=imgui-context-create");
    IMGUI_CHECKVERSION();gui=ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;
    store.Report("stage=font-load");
    char windows[MAX_PATH]{};GetWindowsDirectoryA(windows,MAX_PATH);auto font=std::string(windows)+"/Fonts/msyh.ttc";
    if(std::filesystem::is_regular_file(std::filesystem::u8path(font)))io.Fonts->AddFontFromFileTTF(font.c_str(),17.f,nullptr,io.Fonts->GetGlyphRangesChineseFull());
    ImGui::StyleColorsDark();auto& styleSettings=ImGui::GetStyle();styleSettings.WindowPadding={18,18};styleSettings.ItemSpacing={10,10};styleSettings.FramePadding={9,6};styleSettings.FrameRounding=5;styleSettings.PopupRounding=8;styleSettings.GrabRounding=4;styleSettings.WindowBorderSize=0;
    styleSettings.Colors[ImGuiCol_WindowBg]={.045f,.055f,.075f,1.f};styleSettings.Colors[ImGuiCol_CheckMark]={.40f,.72f,1.f,1.f};
    styleSettings.Colors[ImGuiCol_Button]={.16f,.26f,.38f,.9f};styleSettings.Colors[ImGuiCol_ButtonHovered]={.22f,.37f,.54f,1.f};styleSettings.Colors[ImGuiCol_FrameBg]={.12f,.16f,.23f,.85f};
    store.Report("stage=imgui-win32-init");
    win32Ready=ImGui_ImplWin32_Init(hwnd);if(!win32Ready)throw std::runtime_error("stage=imgui-win32");
    bool wine=GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"wine_get_version")!=nullptr;
    for(unsigned attempt=std::max(firstAttempt,wine?1u:0u);attempt<3;++attempt){
      composition=attempt==0;driver=attempt==2?D3D_DRIVER_TYPE_WARP:D3D_DRIVER_TYPE_HARDWARE;
      try{
        if(composition)Backdrop();else{acrylic=false;MARGINS margins{};DwmExtendFrameIntoClientArea(hwnd,&margins);DWM_SYSTEMBACKDROP_TYPE type=DWMSBT_NONE;DwmSetWindowAttribute(hwnd,DWMWA_SYSTEMBACKDROP_TYPE,&type,sizeof(type));}
        Graphics();store.Report("stage=imgui-dx11-init");rendererReady=ImGui_ImplDX11_Init(device.Get(),context.Get());
        if(!rendererReady)throw std::runtime_error("stage=imgui-dx11");
        if(!ImGui_ImplDX11_CreateDeviceObjects())throw std::runtime_error("stage=imgui-device-objects");
        store.failed=false;store.window=hwnd;store.Report(std::string("stage=ready backend=")+(composition?"composition":"hwnd"));return true;
      }catch(const std::exception& error){store.Report(std::string("fallback=1 ")+error.what());ReleaseGraphics();}
    }return false;
  }
  void Tick(HWND foreground=GetForegroundWindow()) {
    if(pendingError){auto error=pendingError;pendingError=nullptr;std::rethrow_exception(error);}
    HWND game=store.gameWindow.load();
    (void)foreground;
    auto next=store.Read();
    if(view&&next&&(view->scope!=next->scope||view->revision!=next->revision))StopHold();
    if(view&&next&&view->revision!=next->revision){custom.clear();customImages.Clear();customErrors.clear();insQueueNoticeUntil=0;}
    view=std::move(next);
    auto open=store.OpenGuis();
    for(auto it=open.begin();it!=open.end();){
      if(!view||!view->customGuiEnabled.count(it->first)||!view->customGuiEnabled.at(it->first)){
        store.CloseGui(it->first);it=open.erase(it);
      }else ++it;
    }
    std::vector<uint64_t> retained;
    for(auto it=custom.begin();it!=custom.end();) {
      auto session=open.find(it->first);
      if(session==open.end()||session->second!=it->second.session){
        customImages.Release(it->second.session);
        it=custom.erase(it);
      }else {retained.push_back(it->second.session);++it;}
    }
    ImGui::SetCurrentContext(gui);
    customState.Collect(retained);
    bool wanted=(store.open||!open.empty())&&IsWindow(game)&&!IsIconic(game);
    const bool visible=wanted&&bool(view);
    const bool firstFrame=visible&&!shown;
    if(firstFrame)store.Report("stage=window-show");
    Visible(visible);if(!shown)return;
    if(firstFrame)store.Report("stage=first-frame-begin");
    if(!width||!height||!RenderTarget())throw std::runtime_error("control panel graphics target unavailable");
    ImGui_ImplDX11_NewFrame();ImGui_ImplWin32_NewFrame();
#ifdef EIEM_PANEL_TEST
    if(!testMouse.empty()){auto& io=ImGui::GetIO();io.ConfigInputTrickleEventQueue=false;io.AddFocusEvent(true);for(const auto& event:testMouse){io.AddMousePosEvent(float(std::get<0>(event)),float(std::get<1>(event)));io.AddMouseButtonEvent(0,std::get<2>(event));}testMouse.clear();}
#endif
    ImGui::NewFrame();
    if(firstFrame)store.Report("stage=first-frame-draw");
    const bool ins=store.open;
    if(ins)Draw();
    customImages.BeginFrame(open.size());
    DrawCustom(open);
    customImages.EndFrame();
    if(ins&&!drewIns&&!open.empty())ImGui::SetWindowFocus("EFF controls");
    drewIns=ins;
    DrawQueueNotice(ins);
    if(ImGui::IsKeyPressed(ImGuiKey_Escape)){store.Close();StopHold();}
    if(firstFrame)store.Report("stage=first-frame-render");
    ImGui::Render();const float clear[4]={0,0,0,0};auto rt=target.Get();context->OMSetRenderTargets(1,&rt,nullptr);context->ClearRenderTargetView(rt,clear);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
#ifdef EIEM_PANEL_TEST
    if(testCaptureBackground){
      context->OMSetRenderTargets(0,nullptr,nullptr);Ptr<ID3D11Texture2D> buffer,staging;
      if(FAILED(swap->GetBuffer(0,IID_PPV_ARGS(&buffer))))throw std::runtime_error("test backbuffer unavailable");
      D3D11_TEXTURE2D_DESC desc{};buffer->GetDesc(&desc);desc.Width=desc.Height=1;desc.MipLevels=desc.ArraySize=1;desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.MiscFlags=0;
      if(FAILED(device->CreateTexture2D(&desc,nullptr,&staging)))throw std::runtime_error("test staging texture unavailable");
      D3D11_BOX box{testBackgroundX,testBackgroundY,0,testBackgroundX+1,testBackgroundY+1,1};context->CopySubresourceRegion(staging.Get(),0,0,0,0,buffer.Get(),0,&box);
      D3D11_MAPPED_SUBRESOURCE mapped{};if(FAILED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped)))throw std::runtime_error("test staging texture map failed");
      testBackgroundAlpha=static_cast<const unsigned char*>(mapped.pData)[3];context->Unmap(staging.Get(),0);testCaptureBackground=false;
    }
#endif
    if(firstFrame)store.Report("stage=first-present");
    Check(swap->Present(1,0),"present");
    if(firstFrame)store.Report("stage=first-present-returned");
  }
  static DWORD WINAPI Run(void* arg) {
    auto& store=*static_cast<Store*>(arg);
    std::unique_ptr<Window> panel;MSG message{};ULONGLONG retryAt=0;unsigned firstAttempt=0;
    while(!store.stopping){
      while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}
      try{
        if(!panel){if(!store.Visible()||GetTickCount64()<retryAt){Sleep(60);continue;}panel=std::make_unique<Window>(store);if(!panel->Create(firstAttempt))throw std::runtime_error("stage=graphics-unavailable");}
        panel->Tick();Sleep(store.Visible()?8:60);
      }catch(const std::exception& error){store.Report(std::string("retryMs=3000 ")+error.what());store.failed=true;if(panel)firstAttempt=std::min(2u,panel->Backend()+1);panel.reset();retryAt=GetTickCount64()+3000;}
    }
    store.Close();return 0;
  }
};
} // namespace EiemControlPanel
