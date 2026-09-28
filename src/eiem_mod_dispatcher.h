#pragma once

// Static resource replacement needs a Unity-thread dispatcher for Mod keys and
// F10, but it must not reactivate the historical animation/physics window
// procedure. This procedure owns only the Mod transaction messages.
static LRESULT CALLBACK EiemModWndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                       LPARAM lParam) {
  if (!s_eiemUnityThreadId) s_eiemUnityThreadId = GetCurrentThreadId();
  WNDPROC original = g_origWndProc;
  if (msg == WM_CLOSE || msg == WM_DESTROY || msg == WM_NCDESTROY ||
      msg == WM_ENDSESSION) {
    EiemFinishPhysicsManualCaptureOnUnityThread(hwnd,"shutdown");
    g_shutdownRequested = true;
    g_guiRunning = false;
    KillTimer(hwnd, kEiemShapeTransitionTimer);
    KillTimer(hwnd, kEiemModRetryTimer);
    KillTimer(hwnd, kEiemSkinTimingProbeTimer);
    KillTimer(hwnd, kEiemPhysicsCaptureTimer);
    KillTimer(hwnd, kEiemVisibilityForceOffTimer);
    EiemClearVisibilityForceOffProbe();
    s_eiemShapeTransitionTick = 0;
    if (original &&
        GetWindowLongPtrW(hwnd, GWLP_WNDPROC) == (LONG_PTR)EiemModWndProc) {
      SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)original);
      g_origWndProc = nullptr;
    }
    const LRESULT result =
        original ? CallWindowProcW(original, hwnd, msg, wParam, lParam)
                 : DefWindowProcW(hwnd, msg, wParam, lParam);
    InterlockedExchange(&g_shutdownWndProcDone, 1);
    return result;
  }
  if (msg == WM_EIEM_MOD_RECONCILE) {
    InterlockedExchange(&s_eiemModUpdateMessagePosted, 0);
    EiemRunModReconcile();
    return 0;
  }
  if (msg == WM_TIMER && wParam == kEiemModRetryTimer) {
    KillTimer(hwnd, kEiemModRetryTimer);
    EiemRunModReconcile();
    return 0;
  }
  if (msg == WM_TIMER && wParam == kEiemShapeTransitionTimer) {
    EiemRunShapeTransitions();
    return 0;
  }
  if (msg == WM_EIEM_PHYSICS_CAPTURE) {
    EiemStartPhysicsManualCaptureOnUnityThread(hwnd);
    return 0;
  }
  if (msg == WM_TIMER && wParam == kEiemSkinTimingProbeTimer) {
    KillTimer(hwnd, kEiemSkinTimingProbeTimer);
    EiemRunSkinTimingProbe();
    return 0;
  }
  if (msg == WM_TIMER && wParam == kEiemPhysicsCaptureTimer) {
    EiemFinishPhysicsManualCaptureOnUnityThread(hwnd,"bounded-window-complete");
    return 0;
  }
  if (msg == WM_TIMER && wParam == kEiemVisibilityForceOffTimer) {
    EiemRunVisibilityForceOffProbe();
    return 0;
  }
  if (msg == WM_EIEM_MOD_KEY) {
    EiemQueueModKey({LOWORD(wParam), HIWORD(wParam)}, (LONG)lParam);
    return 0;
  }
  if (msg == WM_EIEM_MOD_HOLD) {
    EiemQueueModKey({LOWORD(wParam), HIWORD(wParam)}, (LONG)lParam,
                    true, 0.02);
    return 0;
  }
  return original ? CallWindowProcW(original, hwnd, msg, wParam, lParam)
                  : DefWindowProcW(hwnd, msg, wParam, lParam);
}

static DWORD WINAPI HotkeyThread(LPVOID) {
  Log("[OK] Hotkey thread started");

  void *domain = il2cpp_domain_get();
  if (domain)
    il2cpp_thread_attach(domain);

  HWND hwnd = nullptr;
  while (!hwnd && !g_shutdownRequested) {
    hwnd = FindGameWindow();
    if (!hwnd) Sleep(200);
  }
  if (g_shutdownRequested) return 0;
  g_gameHwnd = hwnd;
  Log("[OK] Game window found: %p (pid=%lu)", hwnd, GetCurrentProcessId());

  const WNDPROC dispatcher =
      kEiemEnableLegacyWorkers ? MmdWndProc : EiemModWndProc;
  g_origWndProc =
      (WNDPROC)SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)dispatcher);
  if (g_origWndProc) {
    Log("[OK] Game window subclassed for %s execution",
        kEiemEnableLegacyWorkers ? "legacy/main-thread" : "Mod transaction");
  } else {
    Log("[WARN] Failed to subclass game window (err=%lu)", GetLastError());
  }
  if (kEiemEnableVisibilityForceOffProbe) {
    if (!SetTimer(hwnd, kEiemVisibilityForceOffTimer, 16, nullptr))
      Log("[TEMP-VIS-FORCEOFF-v1] SetTimer failed err=%lu", GetLastError());
    else
      Log("[TEMP-VIS-SYNC-v3] probe armed interval=16ms");
  }
  EiemPostPendingModUpdate("game window attached");

  // Cycle keys use OS edges. Hold keys use the same registration only as a
  // capture gate, then are sampled below so their speed is independent of the
  // keyboard's repeat delay/rate.
  enum class KeyAction { Manager, Reload, Cycle, PhysicsCapture };
  struct Binding {
    int id; EiemKeyChord chord; EiemModKeyBehavior behavior;
    KeyAction action; LONG generation; ULONGLONG holdTick = 0;
  };
  std::vector<Binding> registered;
  // RegisterHotKey's MOD_NOREPEAT state belongs to one OS registration.  A
  // reload publishes a new Mod generation and rebuilds those registrations;
  // if the user still holds F10, Windows can emit a fresh WM_HOTKEY for the
  // new registration and start an unbounded reload/rebind loop.  Keep the
  // physical edge latch outside `registered`, so it survives every rebind and
  // is cleared only after the complete chord has actually been released.
  std::vector<EiemKeyChord> pressedChords;
  auto chordDown = [](const EiemKeyChord &chord) {
    return (GetAsyncKeyState((int)chord.vk) & 0x8000) != 0 &&
           (!(chord.modifiers & MOD_CONTROL) ||
            (GetAsyncKeyState(VK_CONTROL) & 0x8000)) &&
           (!(chord.modifiers & MOD_SHIFT) ||
            (GetAsyncKeyState(VK_SHIFT) & 0x8000)) &&
           (!(chord.modifiers & MOD_ALT) ||
            (GetAsyncKeyState(VK_MENU) & 0x8000));
  };
  auto takePressEdge = [&](const EiemKeyChord &chord) {
    for (const auto &pressed : pressedChords)
      if (pressed == chord) return false;
    pressedChords.push_back(chord);
    return true;
  };
  LONG boundModGeneration = -1, boundControlGeneration = -1;
  LONG boundConfigGeneration = -1;
  bool boundManagerOpen = false;
  HWND boundForeground = nullptr;
  MSG message = {};
  PeekMessageW(&message, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
  auto clearBindings = [&] {
    for (const auto &binding : registered) UnregisterHotKey(nullptr, binding.id);
    registered.clear();
  };
  while (g_guiRunning && !g_shutdownRequested && IsWindowAlive(hwnd)) {
    s_eiemPersistentStates.Flush(); // batched writes only; no disk polling or Unity calls
    pressedChords.erase(
        std::remove_if(pressedChords.begin(), pressedChords.end(),
                       [&](const EiemKeyChord &chord) {
                         return !chordDown(chord);
                       }),
        pressedChords.end());
    HWND foreground = GetForegroundWindow();
    LONG modGeneration = InterlockedCompareExchange(&s_eiemModGeneration, 0, 0);
    LONG controlGeneration =
        InterlockedCompareExchange(&s_eiemModControlGeneration, 0, 0);
    LONG configGeneration = InterlockedCompareExchange(&s_eiemGlobalConfigGeneration, 0, 0);
    const bool managerOpen =
        InterlockedCompareExchange(&s_eiemModManagerOpen, 0, 0) != 0;
    if (foreground != boundForeground || modGeneration != boundModGeneration ||
        controlGeneration != boundControlGeneration ||
        configGeneration != boundConfigGeneration || managerOpen != boundManagerOpen) {
      clearBindings();
      // Discard messages from the old registration before reusing OS IDs.
      while (PeekMessageW(&message, nullptr, WM_HOTKEY, WM_HOTKEY, PM_REMOVE)) {}
      EiemGlobalConfig config = EiemGetGlobalConfig();
      std::string selectedModPath;
      std::vector<EiemModHotkeyBinding> modKeys = EiemGetModKeyBindings(
          &modGeneration, &controlGeneration, &selectedModPath,
          EiemModUsesUiKeyScope(foreground == hwnd,
                                foreground == g_modUiHwnd, managerOpen));
      int nextId = 1;
      auto bind = [&](EiemKeyChord chord, EiemModKeyBehavior behavior,
                      KeyAction action) -> bool {
        int id = nextId++;
        if (id > 0xBFFF || !RegisterHotKey(nullptr, id, chord.modifiers | MOD_NOREPEAT, chord.vk)) {
          Log("[HOTKEY] registration failed vk=%u modifiers=%u err=%lu; change key in INI",
              chord.vk, chord.modifiers, GetLastError());
          return false;
        }
        registered.push_back({id, chord, behavior, action, modGeneration});
        return true;
      };
      const bool ownedForeground = foreground == hwnd || (g_guiHwnd && foreground == g_guiHwnd) ||
                                  (g_modUiHwnd && foreground == g_modUiHwnd);
      if (ownedForeground) bind(config.gui, EiemModKeyBehavior::Cycle,
                                KeyAction::Manager);
      if (ownedForeground) {
        bind(config.reload, EiemModKeyBehavior::Cycle, KeyAction::Reload);
        const EiemKeyChord physicsCapture{VK_F12,0};
        const EiemKeyChord physicsFallback{VK_F12,MOD_CONTROL};
        bool fallbackBound=false;
        if (kEiemEnableNativePhysicsObservation &&
            !(physicsCapture == config.reload) && !(physicsCapture == config.gui)) {
          if (!bind(physicsCapture,EiemModKeyBehavior::Cycle,
                    KeyAction::PhysicsCapture)) {
            bool fallbackConflict=physicsFallback == config.reload ||
                                  physicsFallback == config.gui;
            for (const auto &binding:modKeys)
              fallbackConflict |= binding.chord == physicsFallback;
            if (!fallbackConflict)
              fallbackBound=bind(physicsFallback,EiemModKeyBehavior::Cycle,
                                 KeyAction::PhysicsCapture);
            if (fallbackBound)
              Log("[HOTKEY] F12 unavailable; physics capture fallback=Ctrl+F12");
          }
        }
        for (const auto &binding : modKeys) {
          if (binding.chord == config.reload || binding.chord == config.gui ||
              (kEiemEnableNativePhysicsObservation &&
               (binding.chord == physicsCapture ||
                (fallbackBound && binding.chord == physicsFallback)))) {
            Log("[HOTKEY] mod key conflicts with global shortcut vk=%u modifiers=%u; mod shortcut disabled",
                binding.chord.vk, binding.chord.modifiers);
            continue;
          }
          bind(binding.chord, binding.behavior, KeyAction::Cycle);
        }
      }
      boundForeground = foreground;
      boundModGeneration = modGeneration;
      boundControlGeneration = controlGeneration;
      boundConfigGeneration = configGeneration;
      boundManagerOpen = managerOpen;
      Log("[HOTKEY] bindings updated count=%zu modGeneration=%ld configGeneration=%ld",
          registered.size(), modGeneration, configGeneration);
    }
    while (PeekMessageW(&message, nullptr, WM_HOTKEY, WM_HOTKEY, PM_REMOVE)) {
      if (!g_pluginActive) continue;
      for (const auto &binding : registered) {
        if (binding.id != (int)message.wParam) continue;
        if (!takePressEdge(binding.chord)) break;
        HWND currentForeground = GetForegroundWindow();
        if (binding.action == KeyAction::Manager) {
          if (currentForeground == hwnd || currentForeground == g_guiHwnd || currentForeground == g_modUiHwnd)
            EiemToggleModManager();
        } else if (currentForeground == hwnd || currentForeground == g_guiHwnd || currentForeground == g_modUiHwnd) {
          if (binding.action == KeyAction::Reload)
            EiemRequestModUpdate(EiemModUpdate::Reload, "global reload hotkey");
          else if (binding.action == KeyAction::PhysicsCapture) {
            if (!PostMessageW(hwnd,WM_EIEM_PHYSICS_CAPTURE,0,0))
              Log("[HOTKEY] could not queue F12 physics capture err=%lu",
                  GetLastError());
          }
          else if (!PostMessageW(hwnd, WM_EIEM_MOD_KEY,
                                 MAKEWPARAM(binding.chord.vk, binding.chord.modifiers),
                                 binding.generation))
            Log("[HOTKEY] could not queue mod key err=%lu", GetLastError());
        }
        break;
      }
    }
    if (foreground == hwnd || (g_guiHwnd && foreground == g_guiHwnd) ||
        (g_modUiHwnd && foreground == g_modUiHwnd)) {
      const ULONGLONG now = GetTickCount64();
      for (auto &binding : registered) {
        if (binding.action != KeyAction::Cycle ||
            binding.behavior != EiemModKeyBehavior::Hold)
          continue;
        const bool down = chordDown(binding.chord);
        if (!down) {
          binding.holdTick = 0;
          continue;
        }
        if (!binding.holdTick) binding.holdTick = now;
        const ULONGLONG elapsed = now - binding.holdTick;
        if (elapsed < 16) continue;
        if (!PostMessageW(hwnd, WM_EIEM_MOD_HOLD,
                          MAKEWPARAM(binding.chord.vk, binding.chord.modifiers),
                          binding.generation))
          Log("[HOTKEY] could not queue hold key err=%lu", GetLastError());
        binding.holdTick = now;
      }
    }
    Sleep(20);
  }
  clearBindings();

  // The game owns this window. Restore its original procedure before the
  // plugin thread exits so late close/destroy messages bypass EIEM.
  if (g_origWndProc && IsWindow(hwnd) &&
      GetWindowLongPtrW(hwnd, GWLP_WNDPROC) == (LONG_PTR)dispatcher) {
    SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)g_origWndProc);
    g_origWndProc = nullptr;
  }
  s_eiemPersistentStates.Flush(true);
  g_guiRunning = false;
  Log("[INFO] Game window closed, hotkey thread exiting");
  return 0;
}
