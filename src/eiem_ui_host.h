#pragma once
#include "eiem_lua_ui.h"
#include <wrl/client.h>

static void EiemToggleModManager() {
  LONG previous = InterlockedCompareExchange(&s_eiemModManagerOpen, 0, 0);
  while (InterlockedCompareExchange(&s_eiemModManagerOpen,
                                    previous ? 0 : 1, previous) != previous)
    previous = InterlockedCompareExchange(&s_eiemModManagerOpen, 0, 0);
}

// Dedicated native overlay, not a game swap-chain hook. Its worker owns all
// window and D3D calls; only immutable snapshots and input events cross threads.
class EiemUiHost {
  template<class T> using Com = Microsoft::WRL::ComPtr<T>;
  HWND hwnd = nullptr;
  ImGuiContext *context = nullptr;
  Com<ID3D11Device> device;
  Com<ID3D11DeviceContext> immediate;
  Com<IDXGISwapChain1> swap;
  Com<ID3D11RenderTargetView> target;
  Com<IDCompositionDevice> composition;
  Com<IDCompositionTarget> compositionTarget;
  Com<IDCompositionVisual> visual;
  LONG generation = -1;
  bool failed = false, shown = false, managerWasOpen = false;
  bool updateDismissedForOpen = false;
  int width = 0, height = 0;
  struct Entry { EiemUiSnapshot snapshot; std::unique_ptr<EiemLuaUi> script; bool reported = false; };
  std::vector<Entry> entries;
  struct ContextScope {
    ImGuiContext *previous;
    explicit ContextScope(ImGuiContext *next) : previous(ImGui::GetCurrentContext()) { ImGui::SetCurrentContext(next); }
    ~ContextScope() { ImGui::SetCurrentContext(previous); }
  };
  static LRESULT CALLBACK WndProc(HWND window, UINT message, WPARAM wp, LPARAM lp) {
    auto *self = reinterpret_cast<EiemUiHost *>(GetWindowLongPtrW(window,GWLP_USERDATA));
    if (message == WM_NCCREATE) {
      self = static_cast<EiemUiHost *>(reinterpret_cast<CREATESTRUCTW *>(lp)->lpCreateParams);
      SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));
    }
    if (self && self->context) {
      ContextScope scope(self->context);
      if (ImGui_ImplWin32_WndProcHandler(window,message,wp,lp)) return 1;
    }
    // This transparent host is infrastructure, not an authored Mod window.
    // Script-owned Begin/open values govern application windows.
    if (message == WM_CLOSE) {
      InterlockedExchange(&s_eiemModManagerOpen, 0);
      return 0;
    }
    if (message == WM_ERASEBKGND) return 1;
    return DefWindowProcW(window,message,wp,lp);
  }
  void DeleteContext() {
    if (!context) return;
    ContextScope scope(context);
    ImGui_ImplDX11_Shutdown(); ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext(context); context = nullptr;
  }
  bool NewContext() {
    context = ImGui::CreateContext();
    ImGui::SetCurrentContext(context);
    auto &io = ImGui::GetIO(); io.IniFilename = nullptr;
    ImGui::StyleColorsDark();
    // System font path is discovered, not tied to a user or drive.
    wchar_t windows[MAX_PATH]; GetWindowsDirectoryW(windows,MAX_PATH);
    auto fontPath = std::filesystem::path(windows) / L"Fonts" / L"msyh.ttc";
    if (std::filesystem::is_regular_file(fontPath))
      io.Fonts->AddFontFromFileTTF(fontPath.u8string().c_str(),18,nullptr,io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
    else io.Fonts->AddFontDefault();
    bool win32 = ImGui_ImplWin32_Init(hwnd);
    bool dx11 = win32 && ImGui_ImplDX11_Init(device.Get(),immediate.Get());
    if (!dx11) {
      if (win32) ImGui_ImplWin32_Shutdown();
      ImGui::DestroyContext(context); context = nullptr;
    }
    return dx11;
  }
  bool Create() {
    WNDCLASSW wc = {}; wc.lpfnWndProc = WndProc; wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"EFF_ModLuaUI"; wc.hCursor = LoadCursor(nullptr,IDC_ARROW);
    if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
    hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOREDIRECTIONBITMAP | WS_EX_TRANSPARENT,
      wc.lpszClassName,L"EFF Mod UI",WS_POPUP,0,0,1,1,nullptr,nullptr,wc.hInstance,this);
    if (!hwnd) return false;
    g_modUiHwnd = hwnd;
    D3D_FEATURE_LEVEL level;
    if (FAILED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        nullptr,0,D3D11_SDK_VERSION,&device,&level,&immediate))) return false;
    Com<IDXGIDevice> dxgi; Com<IDXGIAdapter> adapter; Com<IDXGIFactory2> factory;
    if (FAILED(device.As(&dxgi)) || FAILED(dxgi->GetAdapter(&adapter)) ||
        FAILED(adapter->GetParent(IID_PPV_ARGS(&factory)))) return false;
    DXGI_SWAP_CHAIN_DESC1 desc = {}; desc.Width = desc.Height = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; desc.BufferCount = 2;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL; desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
    if (FAILED(factory->CreateSwapChainForComposition(device.Get(),&desc,nullptr,&swap)) ||
        FAILED(DCompositionCreateDevice(dxgi.Get(),IID_PPV_ARGS(&composition))) ||
        FAILED(composition->CreateTargetForHwnd(hwnd,TRUE,&compositionTarget)) ||
        FAILED(composition->CreateVisual(&visual)) || FAILED(visual->SetContent(swap.Get())) ||
        FAILED(compositionTarget->SetRoot(visual.Get())) || FAILED(composition->Commit())) return false;
    return NewContext();
  }
  bool Resize(int w, int h) {
    if (width == w && height == h && target) return true;
    immediate->OMSetRenderTargets(0,nullptr,nullptr); target.Reset();
    if (FAILED(swap->ResizeBuffers(0,w,h,DXGI_FORMAT_UNKNOWN,0))) return false;
    Com<ID3D11Texture2D> buffer;
    if (FAILED(swap->GetBuffer(0,IID_PPV_ARGS(&buffer))) ||
        FAILED(device->CreateRenderTargetView(buffer.Get(),nullptr,&target))) return false;
    width = w; height = h; return true;
  }
  void Visible(bool value) {
    if (shown == value) return;
    shown = value; g_modUiVisible = value;
    ShowWindow(hwnd,value ? SW_SHOWNOACTIVATE : SW_HIDE);
    if (value) ReleaseCursorToGui();
    else {
      if (!g_guiVisible) ReturnCursorToGame();
      if (GetForegroundWindow() == hwnd && g_gameHwnd) SetForegroundWindow(g_gameHwnd);
    }
  }
  static std::string ModLabel(const std::string &path) {
    auto parent = std::filesystem::u8path(path).parent_path();
    std::string label = parent.filename().u8string();
    return label.empty() ? path : label;
  }
  void DrawManager(LONG generation) {
    if (!InterlockedCompareExchange(&s_eiemModManagerOpen, 0, 0)) return;
    LONG snapshotGeneration = 0, controlGeneration = 0;
    auto controls = EiemGetModControls(&snapshotGeneration, &controlGeneration);
    (void)controlGeneration;
    bool open = true;
    ImGui::SetNextWindowSize(ImVec2(620.0f, 420.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("EFF Mod Manager", &open)) {
      int selectedIndex = -1;
      for (size_t index = 0; index < controls.size(); ++index)
        if (controls[index].selected) { selectedIndex = (int)index; break; }
      const std::string selectedLabel = selectedIndex >= 0
          ? ModLabel(controls[(size_t)selectedIndex].modPath)
          : "Select a Mod";
      if (ImGui::BeginCombo("Mod", selectedLabel.c_str())) {
        for (size_t index = 0; index < controls.size(); ++index) {
          const auto &control = controls[index];
          const std::string label = ModLabel(control.modPath) + "##" + control.modPath;
          if (ImGui::Selectable(label.c_str(), selectedIndex == (int)index) &&
              EiemSelectControlledMod(control.modPath)) {
            selectedIndex = (int)index;
          }
          if (selectedIndex == (int)index) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
      }
      ImGui::SameLine();
      const auto update = EiemGetUpdateSnapshot();
      if (update.checking) ImGui::TextDisabled("Checking releases...");
      else if (update.available) ImGui::Text("Update v%s", update.version.c_str());
      else if (update.failed) ImGui::TextDisabled("Update check failed");
      else if (update.latest) ImGui::TextDisabled("Up to date");
      if (ImGui::SmallButton("Check updates")) {
        updateDismissedForOpen = false;
        EiemRequestUpdateCheck(true);
      }
      ImGui::Separator();
      const EiemModControlSnapshot *selected =
          selectedIndex >= 0 && selectedIndex < (int)controls.size()
              ? &controls[(size_t)selectedIndex]
              : nullptr;
      if (!selected) {
        ImGui::TextDisabled("Select a Mod to use its keys and shape sliders.");
      } else {
        ImGui::PushID(selected->modPath.c_str());
        ImGui::TextUnformatted(ModLabel(selected->modPath).c_str());
        ImGui::Separator();
        for (const auto &key : selected->keys) {
          ImGui::PushID(key.section.c_str());
          std::string label = key.section + "  [" + EiemFormatKeyChord(key.chord) + "]";
          const bool pressed = ImGui::Button(label.c_str(), ImVec2(-1.0f, 0.0f));
          const bool holding = key.behavior == EiemModKeyBehavior::Hold &&
                               ImGui::IsItemActive() && ImGui::IsMouseDown(0);
          if ((holding || (pressed && key.behavior != EiemModKeyBehavior::Hold)) &&
              snapshotGeneration == generation) {
            EiemModInputEvent event;
            event.chord = key.chord;
            event.generation = snapshotGeneration;
            event.modPath = selected->modPath;
            event.uiFocus = key.scope != EiemKeyScope::Game;
            event.keySection = key.section;
            event.holdTick = holding;
            event.holdSeconds = ImGui::GetIO().DeltaTime;
            EiemQueueModInput(std::move(event));
          }
          for (const auto &assignment : key.assignments) {
            auto value = selected->variables.find(assignment.variable);
            if (value != selected->variables.end())
              ImGui::Text("%s = %.3g", assignment.variable.c_str(), value->second);
          }
          ImGui::PopID();
        }
        if (!selected->shapeControls.empty() && !selected->keys.empty())
          ImGui::Separator();
        for (const auto &shape : selected->shapeControls) {
          auto found = selected->variables.find(shape.variable);
          if (found == selected->variables.end()) continue;
          float value = (float)found->second;
          ImGui::PushID(shape.variable.c_str());
          if (ImGui::SliderFloat(shape.label.c_str(), &value,
                                 (float)shape.minimum, (float)shape.maximum,
                                 "%.3f") && snapshotGeneration == generation) {
            EiemModInputEvent event;
            event.generation = snapshotGeneration;
            event.modPath = selected->modPath;
            event.values[shape.variable] = value;
            event.directValues = true;
            EiemQueueModInput(std::move(event));
          }
          ImGui::PopID();
        }
        ImGui::PopID();
      }
    }
    ImGui::End();
    const auto update = EiemGetUpdateSnapshot();
    if (update.available && !updateDismissedForOpen)
      ImGui::OpenPopup("EFF update");
    if (ImGui::BeginPopupModal("EFF update", nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::Text("EFF v%s is available (installed v%s).",
                  update.version.c_str(), EIEM_VERSION);
      if (ImGui::Button("Open Release")) {
        ShellExecuteA(nullptr, "open", update.url.c_str(), nullptr, nullptr,
                      SW_SHOWNORMAL);
        updateDismissedForOpen = true;
        ImGui::CloseCurrentPopup();
      }
      ImGui::SameLine();
      if (ImGui::Button("Later")) {
        updateDismissedForOpen = true;
        ImGui::CloseCurrentPopup();
      }
      ImGui::SameLine();
      if (ImGui::Button("Ignore this version")) {
        if (EiemIgnoreRelease(update.version)) {
          updateDismissedForOpen = true;
          ImGui::CloseCurrentPopup();
        }
      }
      ImGui::EndPopup();
    }
    if (!open) InterlockedExchange(&s_eiemModManagerOpen, 0);
  }
public:
  ~EiemUiHost() { Shutdown(); }
  bool InitializeHidden() {
    ContextScope restore(ImGui::GetCurrentContext());
    if (hwnd) return context != nullptr;
    if (Create()) return true;
    Shutdown(); return false;
  }
  void Shutdown() {
    entries.clear(); Visible(false); DeleteContext();
    target.Reset(); visual.Reset(); compositionTarget.Reset(); composition.Reset();
    swap.Reset(); immediate.Reset(); device.Reset();
    if (hwnd) DestroyWindow(hwnd);
    hwnd = nullptr; g_modUiHwnd = nullptr;
    UnregisterClassW(L"EFF_ModLuaUI",GetModuleHandleW(nullptr));
  }
  void Tick() {
    ContextScope restore(ImGui::GetCurrentContext());
    HWND foreground = GetForegroundWindow();
    const bool owned = foreground == g_gameHwnd || foreground == g_guiHwnd ||
                       foreground == hwnd;
    const bool managerRequested =
        InterlockedCompareExchange(&s_eiemModManagerOpen, 0, 0) != 0;
    if (managerRequested && !managerWasOpen) {
      updateDismissedForOpen = false;
      EiemRequestUpdateCheck(false);
    }
    // A hidden overlay does not need to copy every Mod's UI variables or
    // rebuild ImGui scripts. The generation is read when focus returns.
    if (!g_pluginActive || !owned || IsIconic(g_gameHwnd)) {
      managerWasOpen = false;
      Visible(false);
      return;
    }
    LONG nextGeneration; auto snapshots = EiemGetModUis(&nextGeneration);
    if (nextGeneration != generation) {
      if (failed) Shutdown(); // a failed device is rebuilt, never reused as a fallback
      entries.clear(); generation = nextGeneration; failed = false;
      // Full context replacement removes active widgets, drag state and backend
      // input belonging to the previous generation, even when titles match.
      if (context) { DeleteContext(); if (!NewContext()) failed = true; }
      for (const auto &snapshot : snapshots) {
        std::string id = EiemModIdentifier(snapshot.modPath.c_str(),snapshot.ui.section.c_str());
        auto script = std::make_unique<EiemLuaUi>(id);
        bool loaded = script->LoadFile(snapshot.modPath,snapshot.ui.path);
        if (!loaded)
          Log("[UI] %s/%s: %s",snapshot.modPath.c_str(),snapshot.ui.section.c_str(),script->error.c_str());
        entries.push_back({snapshot,std::move(script),!loaded});
      }
    } else {
      for (size_t i = 0; i < entries.size() && i < snapshots.size(); ++i)
        entries[i].snapshot.variables = std::move(snapshots[i].variables);
    }
    if (entries.empty() && !managerRequested) {
      managerWasOpen = managerRequested;
      Visible(false);
      return;
    }
    if (failed) return;
    if (!hwnd && !InitializeHidden()) {
      Log("[UI] Native host creation failed err=%lu; retry with Reload",GetLastError());
      Shutdown(); failed = true; return;
    }
    if (!context) { failed = true; return; }
    ImGui::SetCurrentContext(context);
    RECT rect; GetClientRect(g_gameHwnd,&rect); POINT origin = {}; ClientToScreen(g_gameHwnd,&origin);
    if (rect.right <= 0 || rect.bottom <= 0) { Visible(false); return; }
    if (!Resize(rect.right,rect.bottom)) { Log("[UI] Resize failed; retry with Reload"); Visible(false); failed = true; return; }
    SetWindowPos(hwnd,HWND_TOPMOST,origin.x,origin.y,rect.right,rect.bottom,SWP_NOACTIVATE);
    ImGui_ImplDX11_NewFrame(); ImGui_ImplWin32_NewFrame(); ImGui::NewFrame();
    DrawManager(nextGeneration);
    for (auto &entry : entries) {
      EiemVariables values;
      if (entry.script->Draw(entry.snapshot.variables,values,entry.snapshot.defaults) && !values.empty()) {
        // Validation at submission gives script authors immediate diagnostics;
        // publication validates again against the actual generation/state.
        std::string error;
        EiemVariables proposed;
        bool valid = false;
        AcquireSRWLockShared(&s_eiemModLock);
        for (size_t i = 0; i < s_eiemModProgram.states.size(); ++i)
          if (s_eiemModProgram.states[i].path == entry.snapshot.modPath)
            valid = EiemValidateUiValues(s_eiemModProgram,i,values,proposed,error);
        ReleaseSRWLockShared(&s_eiemModLock);
        if (valid) EiemQueueModInput({{},generation,entry.snapshot.modPath,entry.snapshot.ui.section,std::move(values)});
        else entry.script->error = error.empty() ? "UI Mod is no longer available" : error;
      }
      if (!entry.script->error.empty() && !entry.reported) {
        Log("[UI] %s/%s: %s",entry.snapshot.modPath.c_str(),entry.snapshot.ui.section.c_str(),entry.script->error.c_str());
        entry.reported = true;
      }
    }
    ImGui::Render();
    // Run callbacks even while nothing is shown: Lua decides whether to emit
    // windows. No private DLL "open" state can prevent a script from opening one.
    Visible(ImGui::GetDrawData()->TotalVtxCount > 0);
    const bool managerOpen =
        InterlockedCompareExchange(&s_eiemModManagerOpen, 0, 0) != 0;
    if (managerOpen && !managerWasOpen && shown) {
      ShowWindow(hwnd, SW_SHOW);
      SetForegroundWindow(hwnd);
      ReleaseCursorToGui();
    }
    managerWasOpen = managerOpen;
    POINT mouse; GetCursorPos(&mouse); ScreenToClient(hwnd,&mouse);
    bool hit = context->ActiveId != 0;
    for (auto *window : context->Windows)
      if (window->Active && !window->Hidden && !(window->Flags & ImGuiWindowFlags_NoMouseInputs) &&
          window->Rect().Contains(ImVec2((float)mouse.x,(float)mouse.y))) hit = true;
    auto ex = GetWindowLongPtrW(hwnd,GWL_EXSTYLE);
    auto desired = hit ? ex & ~WS_EX_TRANSPARENT : ex | WS_EX_TRANSPARENT;
    if (desired != ex) SetWindowLongPtrW(hwnd,GWL_EXSTYLE,desired);
    ID3D11RenderTargetView *rt = target.Get(); float clear[4] = {};
    immediate->OMSetRenderTargets(1,&rt,nullptr); immediate->ClearRenderTargetView(rt,clear);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    if (FAILED(swap->Present(0,0))) { Log("[UI] Present failed; retry with Reload"); Visible(false); failed = true; }
  }
};

static DWORD WINAPI EiemModUiThread(LPVOID) {
  Log("[MOD-UI] Thread started");
  while (g_guiRunning && !g_shutdownRequested && !g_gameHwnd) Sleep(100);
  if (!g_guiRunning || g_shutdownRequested || !g_gameHwnd) return 0;
  EiemUiHost host;
  MSG message = {};
  while (g_guiRunning && !g_shutdownRequested && IsWindow(g_gameHwnd)) {
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
      TranslateMessage(&message);
      DispatchMessageW(&message);
    }
    host.Tick();
    Sleep(g_modUiVisible ? 8 : 40);
  }
  host.Shutdown();
  Log("[MOD-UI] Thread exited");
  return 0;
}
