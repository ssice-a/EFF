#pragma once
#include "eiem_control_panel.h"
#include <commctrl.h>
#include <sstream>
#pragma comment(lib,"comctl32.lib")
#pragma comment(lib,"gdi32.lib")

// A small independent Windows panel also works when the game uses Vulkan.
// All HWNDs and controls belong to this worker; Store is the only bridge.
namespace EiemControlPanel {
class Window {
  Store& store;
  HWND hwnd=nullptr, combo=nullptr, checkbox=nullptr;
  HFONT font=nullptr;
  std::shared_ptr<const Snapshot> view;
  std::vector<std::string> choices;
  struct Widget { HWND hwnd;int y,height;std::string name;Action action;bool held=false; };
  std::vector<Widget> widgets;
  int content=0, scroll=0;
  bool shown=false;
  static constexpr int comboId=100, keyboardId=101, firstWidget=200;
  static std::wstring Wide(const std::string& text) {
    if(text.empty()) return {};
    int n=MultiByteToWideChar(CP_UTF8,0,text.data(),int(text.size()),nullptr,0);
    std::wstring result(n,L'\0');MultiByteToWideChar(CP_UTF8,0,text.data(),int(text.size()),result.data(),n);return result;
  }
  static LRESULT CALLBACK Procedure(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    auto* self=reinterpret_cast<Window*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
    if(msg==WM_NCCREATE) {
      self=static_cast<Window*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
      SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));self->hwnd=hwnd;
    }
    if(!self) return DefWindowProcW(hwnd,msg,wp,lp);
    try {
      if(msg==WM_CLOSE) {self->store.open=false;self->Visible(false);return 0;}
      if(msg==WM_COMMAND) {self->Command(LOWORD(wp),HIWORD(wp));return 0;}
      if(msg==WM_HSCROLL) {self->Slider(reinterpret_cast<HWND>(lp));return 0;}
      if(msg==WM_VSCROLL) {self->Scroll(LOWORD(wp));return 0;}
      if(msg==WM_MOUSEWHEEL) {self->scroll-=GET_WHEEL_DELTA_WPARAM(wp)/WHEEL_DELTA*72;self->Layout();return 0;}
    } catch(const std::exception&) {
      self->store.open=false;self->store.failed=true;self->Visible(false);return 0;
    }
    return DefWindowProcW(hwnd,msg,wp,lp);
  }
  HWND Add(const wchar_t* type,const std::wstring& text,DWORD style,int y,int height,int id,
           const std::string& name={},Action action=Action::Invoke) {
    auto child=CreateWindowExW(0,type,text.c_str(),WS_CHILD|WS_VISIBLE|style,16,y,566,height,
                              hwnd,reinterpret_cast<HMENU>(INT_PTR(id)),GetModuleHandleW(nullptr),nullptr);
    if(!child) throw std::runtime_error("control panel widget creation failed");
    SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);
    widgets.push_back({child,y,height,name,action});return child;
  }
  const EiemDirectory::Package* Selected() const {
    if(view && view->graph) for(const auto& p:*view->graph) if(p.id==view->selected) return &p;
    return nullptr;
  }
  bool Send(Action action,const std::string& package,const std::string& name={},double value=0) {
    return view && store.Submit({view->revision,view->scope,action,package,name,value});
  }
  std::wstring KeyText(const EiemDirectory::Binding& b,const EiemDirectory::Package& p) const {
    auto control=p.controls.find(b.variable);
    std::string label=control==p.controls.end()?b.id:control->second.label;
    double value=view->values.at(p.id).at(b.variable);
    std::ostringstream out;
    out<<(b.type=="hold"?u8"按住 ":"")<<'['<<EiemFormatKeyChord(b.key)<<"]  "<<label;
    if(b.type=="hold") out<<" -> "<<b.values.front();
    out<<"    = "<<value;
    return Wide(out.str());
  }
  void Build() {
    for(const auto& w:widgets) DestroyWindow(w.hwnd);
    widgets.clear();choices.clear();scroll=0;combo=checkbox=nullptr;
    Add(L"STATIC",L"快捷键只控制下面选中的 Mod；同一 Mod 的角色实例共享状态。",0,12,24,0);
    combo=Add(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,40,220,comboId);
    int chosen=-1;
    if(view && view->graph) for(const auto& p:*view->graph) if(!p.bindings.empty() || !p.controls.empty()) {
      std::string label=p.root.filename().u8string()+"  ("+p.id+")";
      if(p.id==view->selected) chosen=int(choices.size());choices.push_back(p.id);
      auto wide=Wide(label);SendMessageW(combo,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(wide.c_str()));
    }
    SendMessageW(combo,CB_SETCURSEL,chosen,0);
    checkbox=Add(L"BUTTON",L"启用所选 Mod 的物理快捷键",BS_AUTOCHECKBOX|WS_TABSTOP,76,24,keyboardId);
    SendMessageW(checkbox,BM_SETCHECK,view && view->keyboard?BST_CHECKED:BST_UNCHECKED,0);
    const auto* p=Selected();
    std::string targets=u8"资源目标（作者路径）：\r\n";
    if(p) for(const auto& target:p->targetIndex) targets+=target.first.path+"\r\n";
    Add(L"EDIT",Wide(targets),ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL|WS_VSCROLL|WS_BORDER,106,68,0);
    Add(L"STATIC",L"面板打开时暂停物理快捷键，请用按钮和滑块。\n关闭面板后恢复；使用 UI 快捷键再次打开或关闭。",0,182,40,0);
    int y=228,id=firstWidget;
    if(p) {
      for(const auto& b:p->bindings) {
        Add(L"BUTTON",KeyText(b,*p),BS_PUSHBUTTON|WS_TABSTOP,y,32,id++,b.id,b.type=="hold"?Action::Hold:Action::Invoke);y+=38;
      }
      for(const auto& c:p->controls) {
        Add(L"STATIC",L"",0,y,22,0,c.first,Action::Value);y+=22;
        auto slider=Add(TRACKBAR_CLASSW,L"",TBS_HORZ|TBS_NOTICKS|WS_TABSTOP,y,30,id++,c.first,Action::Value);
        SendMessageW(slider,TBM_SETRANGE,TRUE,MAKELPARAM(0,10000));y+=36;
      }
    } else {Add(L"STATIC",L"当前没有可控制的 Mod。",0,y,32,0);y+=38;}
    Add(L"STATIC",L"快捷键只控制所选 Mod。增删或修改 Mod 后，按重载键生效（默认 F10）。",0,y,28,0);content=y+40;
    Layout();Refresh();
  }
  void Layout() {
    RECT r{};GetClientRect(hwnd,&r);scroll=std::clamp(scroll,0,std::max(0,content-int(r.bottom)));
    SCROLLINFO info{sizeof(info),SIF_RANGE|SIF_PAGE|SIF_POS,0,content-1,UINT(r.bottom),scroll,0};
    SetScrollInfo(hwnd,SB_VERT,&info,TRUE);
    for(const auto& w:widgets) SetWindowPos(w.hwnd,nullptr,16,w.y-scroll,566,w.height,SWP_NOZORDER|SWP_NOACTIVATE);
  }
  void Scroll(int code) {
    if(code==SB_LINEUP) scroll-=32;else if(code==SB_LINEDOWN) scroll+=32;
    else if(code==SB_PAGEUP) scroll-=240;else if(code==SB_PAGEDOWN) scroll+=240;
    else if(code==SB_THUMBTRACK || code==SB_THUMBPOSITION) {
      SCROLLINFO info{sizeof(info),SIF_TRACKPOS};GetScrollInfo(hwnd,SB_VERT,&info);scroll=info.nTrackPos;
    }
    Layout();
  }
  void Refresh() {
    const auto* p=Selected();if(!p) return;
    for(const auto& w:widgets) {
      if(w.name.empty()) continue;
      if(w.action==Action::Invoke || w.action==Action::Hold) {
        for(const auto& b:p->bindings) if(b.id==w.name) {auto text=KeyText(b,*p);SetWindowTextW(w.hwnd,text.c_str());}
      } else {
        const auto& c=p->controls.at(w.name);double value=view->values.at(p->id).at(w.name);
        wchar_t type[32]{};GetClassNameW(w.hwnd,type,32);
        if(!wcscmp(type,TRACKBAR_CLASSW)) {
          // Never move a thumb underneath a live mouse drag.
          if(GetCapture()!=w.hwnd) {
            int position=c.maximum==c.minimum?0:int(std::lround((value-c.minimum)/(c.maximum-c.minimum)*10000));
            SendMessageW(w.hwnd,TBM_SETPOS,TRUE,position);
          }
        } else {
          std::ostringstream out;out<<c.label<<" = "<<value<<"   ["<<c.minimum<<", "<<c.maximum<<"]";
          auto text=Wide(out.str());SetWindowTextW(w.hwnd,text.c_str());
        }
      }
    }
  }
  void Command(int id,int notification) {
    if(!view) return;
    if(id==comboId && notification==CBN_SELCHANGE) {
      auto index=SendMessageW(combo,CB_GETCURSEL,0,0);
      if(index>=0 && size_t(index)<choices.size()) Send(Action::Select,choices[size_t(index)]);
    } else if(id==keyboardId && notification==BN_CLICKED) {
      Send(Action::Keyboard,view->selected,{},SendMessageW(checkbox,BM_GETCHECK,0,0)==BST_CHECKED?1:0);
    } else if(notification==BN_CLICKED) for(const auto& w:widgets)
      if(GetDlgCtrlID(w.hwnd)==id && w.action==Action::Invoke && !w.name.empty()) Send(Action::Invoke,view->selected,w.name);
  }
  void Slider(HWND slider) {
    const auto* p=Selected();if(!p) return;
    for(const auto& w:widgets) if(w.hwnd==slider && w.action==Action::Value) {
      const auto& c=p->controls.at(w.name);
      double value=c.minimum+(c.maximum-c.minimum)*double(SendMessageW(slider,TBM_GETPOS,0,0))/10000;
      Send(Action::Value,p->id,w.name,value);
    }
  }
  void Visible(bool visible) {
    if(shown==visible) return;shown=visible;
    if(visible) {
      ClipCursor(nullptr);ShowWindow(hwnd,SW_SHOW);SetForegroundWindow(hwnd);
    } else {
      bool owned=GetForegroundWindow()==hwnd;ShowWindow(hwnd,SW_HIDE);
      if(owned && IsWindow(store.gameWindow.load())) SetForegroundWindow(store.gameWindow.load());
    }
  }
public:
  explicit Window(Store& store):store(store) {}
  ~Window() { Visible(false);store.window=nullptr;if(hwnd) DestroyWindow(hwnd);if(font) DeleteObject(font); }
  HWND Handle() const {return hwnd;}
  bool Create() {
    INITCOMMONCONTROLSEX common{sizeof(common),ICC_BAR_CLASSES};if(!InitCommonControlsEx(&common)) return false;
    WNDCLASSW wc{};wc.lpfnWndProc=Procedure;wc.hInstance=GetModuleHandleW(nullptr);
    wc.lpszClassName=L"EFF_ResourceControls";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_BTNFACE+1);
    if(!RegisterClassW(&wc) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS) return false;
    font=CreateFontW(-17,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH,L"Microsoft YaHei UI");
    RECT r{0,0,604,640};DWORD style=WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_VSCROLL;
    AdjustWindowRectEx(&r,style,FALSE,WS_EX_TOOLWINDOW|WS_EX_TOPMOST);
    hwnd=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_TOPMOST,wc.lpszClassName,L"EFF 角色 / Mod 按键控制",style,
      CW_USEDEFAULT,CW_USEDEFAULT,r.right-r.left,r.bottom-r.top,store.gameWindow.load(),nullptr,wc.hInstance,this);
    if(!hwnd) return false;store.window=hwnd;return true;
  }
  void Tick(HWND foreground=GetForegroundWindow()) {
    HWND game=store.gameWindow.load();
    if(IsWindow(game) && GetWindow(hwnd,GW_OWNER)!=game) SetWindowLongPtrW(hwnd,GWLP_HWNDPARENT,reinterpret_cast<LONG_PTR>(game));
    bool owned=foreground==hwnd || foreground==game;
    bool wanted=store.open && IsWindow(game) && !IsIconic(game) && owned;
    auto next=store.Read();
    if(wanted && next) {
      bool rebuild=!view || view->revision!=next->revision || view->scope!=next->scope;
      view=std::move(next);if(rebuild) Build();else Refresh();
    }
    Visible(wanted && bool(view));
    for(auto& w:widgets) if(w.action==Action::Hold) {
      bool down=shown && (SendMessageW(w.hwnd,BM_GETSTATE,0,0)&BST_PUSHED)!=0 && (GetAsyncKeyState(VK_LBUTTON)&0x8000);
      if(down!=w.held && Send(Action::Hold,view->selected,w.name,down?1:0)) w.held=down;
    }
  }
  static DWORD WINAPI Run(void* arg) {
    auto& store=*static_cast<Store*>(arg);
    try {
      Window panel(store);if(!panel.Create()) {store.open=false;store.failed=true;return 1;}
      MSG message{};
      while(!store.stopping && !store.failed) {
        while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) {TranslateMessage(&message);DispatchMessageW(&message);}
        panel.Tick();Sleep(store.open?16:60);
      }
    } catch(const std::exception&) {store.open=false;store.failed=true;return 1;}
    store.open=false;return 0;
  }
};
} // namespace EiemControlPanel
