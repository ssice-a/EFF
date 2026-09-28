#pragma once

static void LoadAndResolveVmd(HWND hwnd) {
  char filePath[MAX_PATH] = {0};
  OPENFILENAMEA ofn = {};
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = hwnd;
  ofn.lpstrFilter = "VMD Files (*.vmd)\0*.vmd\0All Files\0*.*\0";
  ofn.lpstrFile = filePath;
  ofn.nMaxFile = MAX_PATH;
  ofn.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
  ofn.lpstrTitle = "EIEM - Select VMD File";

  if (!GetOpenFileNameA(&ofn)) {
    Log("[VMD] File dialog cancelled");
    return;
  }

  Log("[VMD] Loading: %s", filePath);

  if (g_vmd) {
    FreeVmd(g_vmd);
    g_vmd = nullptr;
  }
  if (!g_resolvedMappings)
    g_resolvedMappings = new std::vector<ResolvedBoneMapping>();
  g_resolvedMappings->clear();

  g_vmd = LoadVmd(filePath);
  if (!g_vmd->loaded) {
    Log("[VMD] ERROR: %s", g_vmd->error.c_str());
    return;
  }

  Log("[VMD] Loaded OK: model=%s, frames=%u (%.1f sec), %zu bone timelines",
      g_vmd->modelName, g_vmd->totalFrames, g_vmd->totalFrames / 30.0f,
      g_vmd->boneTimelines.size());

  FILE *dumpFile = fopen("plugin/eiem_vmd_dump.txt", "w");
  if (dumpFile) {
    DumpVmd(g_vmd, dumpFile);

    fprintf(dumpFile, "\n=== Bone Mapping Resolution ===\n");
    RefreshEntityAnimator();

    void *charRootTransform = nullptr;
    if (g_cachedAnimator && g_component_get_transform) {
      charRootTransform = SafeGetComponentTransform(g_cachedAnimator);
    }
    if (charRootTransform) {
      Log("[VMD] Character root Transform: %p", charRootTransform);
    } else {
      Log("[VMD] WARNING: No character root Transform, finger bones won't "
          "resolve");
    }
    int mapped = 0, unmapped = 0;
    for (const auto &pair : g_vmd->boneTimelines) {
      const std::string &mmdName = pair.first;
      int humanBone = LookupBoneMapping(mmdName);
      bool isPos = IsBonePositionMapped(mmdName);

      ResolvedBoneMapping rm;
      rm.mmdName = mmdName;
      rm.humanBone = humanBone;
      rm.isPositionBone = isPos;
      rm.transform = nullptr;
      rm.valid = false;

      if (humanBone >= 0 && g_cachedAnimator && g_animator_GetBoneTransform) {
        void *t = SafeGetBoneTransform(humanBone);
        if (t) {
          rm.transform = t;
          rm.valid = true;
        }
      }

      if (!rm.valid) {
        const char *fingerName = LookupFingerMapping(mmdName);
        if (fingerName && charRootTransform) {
          void *t = SafeFindChildRecursive(charRootTransform, fingerName, 15);
          if (t) {
            rm.transform = t;
            rm.transformName = fingerName;
            rm.isFingerBone = true;
            rm.valid = true;
          }
        }
      }

      if (rm.valid) {
        char bname[256] = {0};
        if (g_object_get_name)
          SafeGetBoneName(rm.transform, bname, sizeof(bname));
        fprintf(dumpFile, "  [OK] \"%s\" -> %s[%s] -> \"%s\" (%zu keys)%s\n",
                mmdName.c_str(), rm.isFingerBone ? "Finger" : "HumanBone",
                rm.isFingerBone ? rm.transformName.c_str()
                                : std::to_string(humanBone).c_str(),
                bname, pair.second.keys.size(), isPos ? " [POS]" : "");
        mapped++;
      } else if (humanBone >= 0 || LookupFingerMapping(mmdName)) {
        fprintf(dumpFile,
                "  [--] \"%s\" -> mapped but no Transform (%zu keys)\n",
                mmdName.c_str(), pair.second.keys.size());
        unmapped++;
      } else {
        fprintf(dumpFile, "  [??] \"%s\" -> NO MAPPING (%zu keys)\n",
                mmdName.c_str(), pair.second.keys.size());
        unmapped++;
      }

      g_resolvedMappings->push_back(rm);
    }

    fprintf(dumpFile, "\nMapped: %d, Unmapped: %d, Total: %d\n", mapped,
            unmapped, mapped + unmapped);
    fclose(dumpFile);

    Log("[VMD] Bone mapping: %d mapped, %d unmapped. See eiem_vmd_dump.txt",
        mapped, unmapped);
  }
}


#include "camera_control.h"

static bool IsWindowAlive(HWND hwnd) {
  if (!IsWindow(hwnd))
    return false;
  return true;
}

// Animation playback can call into audio, calibration and Unity-facing code.
// Keep it isolated from the input message pump so a stalled animation update
// can never disable the GUI toggle handling.
static DWORD WINAPI AnimationWorkerThread(LPVOID) {
  Log("[OK] Animation worker thread started");
  void *domain = il2cpp_domain_get ? il2cpp_domain_get() : nullptr;
  if (domain && il2cpp_thread_attach)
    il2cpp_thread_attach(domain);

  while (g_guiRunning && !g_shutdownRequested) {
    __try {
      AnimationTick();
      MuscleAnimationTick();
      if (g_camTestMode && g_cameraActive)
        ApplyCameraFrame(0.0f);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
      Log("[ANIMATION] Worker update failed with exception=0x%08lX",
          GetExceptionCode());
      Sleep(100);
    }
    Sleep((g_musclePlayer && g_musclePlayer->playing) || g_camTestMode ? 0 : 50);
  }
  Log("[INFO] Animation worker thread exiting");
  return 0;
}

#include "eiem_mod_dispatcher.h"

static void DumpTransformHierarchy(void *transform, int depth, FILE *dumpFile) {
  if (!transform || depth > 15)
    return;

  __try {
    char name[256] = "?";
    if (g_object_get_name) {
      void *nameStr = Invoke(g_object_get_name, transform);
      if (nameStr)
        ReadStrUtf8(nameStr, name, sizeof(name));
    }

    Vector3 lp = {0, 0, 0};
    if (g_transform_get_localPosition) {
      void *boxed = Invoke(g_transform_get_localPosition, transform);
      if (boxed)
        lp = *(Vector3 *)((char *)boxed + 16);
    }

    Quaternion lr = {0, 0, 0, 1};
    if (g_transform_get_localRotation) {
      void *boxed = Invoke(g_transform_get_localRotation, transform);
      if (boxed)
        lr = *(Quaternion *)((char *)boxed + 16);
    }

    char indent[64] = {};
    for (int i = 0; i < depth && i < 30; i++) {
      indent[i * 2] = ' ';
      indent[i * 2 + 1] = ' ';
    }

    fprintf(dumpFile, "%s[%s] pos(%.3f,%.3f,%.3f) rot(%.4f,%.4f,%.4f,%.4f)\n",
            indent, name, lp.x, lp.y, lp.z, lr.x, lr.y, lr.z, lr.w);

    if (g_transform_get_childCount && g_transform_GetChild) {
      int childCount = UnboxInt(Invoke(g_transform_get_childCount, transform));
      for (int i = 0; i < childCount && i < 200; i++) {
        int32_t idx = i;
        void *params[] = {&idx};
        void *child = Invoke(g_transform_GetChild, transform, params);
        if (child)
          DumpTransformHierarchy(child, depth + 1, dumpFile);
      }
    }
  } __except (1) {
    fprintf(dumpFile, "[EXCEPTION at depth %d]\n", depth);
  }
}

static void DiscoverSkeleton() {
  Log("=== EIEM Phase 2: Bone Discovery ===");

  FILE *df = fopen("plugin\\eiem_skeleton_dump.txt", "w");
  if (!df) {
    Log("[ERROR] Cannot create dump file");
    return;
  }
  fprintf(df, "=== EIEM Phase 2: Bone Discovery ===\n\n");

  void *domain = il2cpp_domain_get();
  il2cpp_thread_attach(domain);

  fprintf(df, "=== Captured Instances ===\n");
  fprintf(df, "PlayerController: %p\n", g_playerController);
  fprintf(df, "MainCharacter Entity: %p\n", g_mainCharEntity);
  fprintf(df, "Cached Animator: %p\n", g_cachedAnimator);

  if (g_playerController && !g_mainCharEntity) {
    __try {
      int pcOff = SafeOff(OFF_pcEntity, 0x70, "pcEntity");
      g_mainCharEntity = *(void **)((char *)g_playerController + pcOff);
      fprintf(df, "Read mainCharacter from PC: %p\n", g_mainCharEntity);
    } __except (1) {
      fprintf(df, "[ERROR] Failed to read mainCharacter\n");
    }
  }

  if (g_mainCharEntity && !g_cachedAnimator) {
    __try {
      int ecOff = SafeOff(OFF_entityComplexAnim, 0x110, "entityComplexAnim");
      void *complexAnimCom = *(void **)((char *)g_mainCharEntity + ecOff);
      fprintf(df, "ComplexAnimatorComponent: %p\n", complexAnimCom);
      if (complexAnimCom) {
        int caOff =
            SafeOff(OFF_complexAnimAnimator, 0x148, "complexAnimAnimator");
        void *animator = *(void **)((char *)complexAnimCom + caOff);
        fprintf(df, "Animator: %p\n", animator);
        g_cachedAnimator = animator;
      }
    } __except (1) {
      fprintf(df, "[ERROR] Failed to navigate animatorCom chain\n");
    }
  }

  if (g_cachedAnimator) {
    fprintf(df, "\n=== Animator Analysis ===\n");
    fprintf(df, "Animator ptr: %p\n", g_cachedAnimator);

    if (g_animator_get_isHuman) {
      void *boxed = Invoke(g_animator_get_isHuman, g_cachedAnimator);
      bool isHuman = UnboxBool(boxed);
      fprintf(df, "isHuman: %s\n", isHuman ? "YES" : "NO");
      Log("Animator.isHuman = %s", isHuman ? "YES" : "NO");
    }

    if (g_animator_get_avatar) {
      void *avatar = Invoke(g_animator_get_avatar, g_cachedAnimator);
      fprintf(df, "Avatar: %p\n", avatar);
      if (avatar && g_object_get_name) {
        char avatarName[256] = "?";
        void *nameStr = Invoke(g_object_get_name, avatar);
        if (nameStr)
          ReadStrUtf8(nameStr, avatarName, sizeof(avatarName));
        fprintf(df, "Avatar name: %s\n", avatarName);
      }
    }

    if (g_animator_GetBoneTransform) {
      fprintf(df, "\n=== HumanBodyBones Enumeration ===\n");
      int foundCount = 0;
      for (int bone = 0; bone < g_humanBoneCount; bone++) {
        __try {
          int32_t boneId = bone;
          void *params[] = {&boneId};
          void *boneTransform =
              Invoke(g_animator_GetBoneTransform, g_cachedAnimator, params);
          if (boneTransform) {
            foundCount++;
            char boneName[256] = "?";
            if (g_object_get_name) {
              void *nameStr = Invoke(g_object_get_name, boneTransform);
              if (nameStr)
                ReadStrUtf8(nameStr, boneName, sizeof(boneName));
            }

            Vector3 lp = {0, 0, 0};
            if (g_transform_get_localPosition) {
              void *boxed =
                  Invoke(g_transform_get_localPosition, boneTransform);
              if (boxed)
                lp = *(Vector3 *)((char *)boxed + 16);
            }
            Quaternion lr = {0, 0, 0, 1};
            if (g_transform_get_localRotation) {
              void *boxed =
                  Invoke(g_transform_get_localRotation, boneTransform);
              if (boxed)
                lr = *(Quaternion *)((char *)boxed + 16);
            }

            fprintf(df,
                    "  [%2d] %-30s —\"%s\" pos(%.3f,%.3f,%.3f) "
                    "rot(%.4f,%.4f,%.4f,%.4f)\n",
                    bone, g_humanBoneNames[bone], boneName, lp.x, lp.y, lp.z,
                    lr.x, lr.y, lr.z, lr.w);
          } else {
            fprintf(df, "  [%2d] %-30s —NULL\n", bone,
                    g_humanBoneNames[bone]);
          }
        } __except (1) {
          fprintf(df, "  [%2d] %-30s —EXCEPTION\n", bone,
                  g_humanBoneNames[bone]);
        }
      }
      fprintf(df, "\nTotal bones found: %d / %d\n", foundCount,
              g_humanBoneCount);
    }

    if (g_component_get_transform) {
      void *animTransform = Invoke(g_component_get_transform, g_cachedAnimator);
      if (animTransform) {
        fprintf(df, "\n=== Character Transform Hierarchy ===\n");
        DumpTransformHierarchy(animTransform, 0, df);
      }
    }

    fprintf(df, "\n=== BlendShape Discovery ===\n");
    if (g_component_get_gameObject && g_smr_get_sharedMesh &&
        g_mesh_get_blendShapeCount && g_mesh_GetBlendShapeName) {
      void *go = nullptr;
      __try {
        go = Invoke(g_component_get_gameObject, g_cachedAnimator);
      } __except (1) {
      }
      if (go) {
        void *animTransform = nullptr;
        __try {
          animTransform = Invoke(g_component_get_transform, g_cachedAnimator);
        } __except (1) {
        }
        if (animTransform) {
          const char *faceMeshNames[] = {"S_actor_endminf_face_01_lod0",
                                         "S_actor_endminf_body_01_lod0",
                                         nullptr};
          for (int mi = 0; faceMeshNames[mi]; mi++) {
            void *meshTransform =
                SafeFindChildRecursive(animTransform, faceMeshNames[mi], 5);
            if (!meshTransform)
              continue;

            void *meshGo = nullptr;
            __try {
              meshGo = Invoke(g_component_get_gameObject, meshTransform);
            } __except (1) {
              continue;
            }
            if (!meshGo)
              continue;

            void *smrType = il2cpp_class_get_type(g_skinnedMeshRendererClass);
            void *smrTypeObj = il2cpp_type_get_object(smrType);
            void *smr = nullptr;
            __try {
              void *params[] = {smrTypeObj};
              smr = Invoke(g_gameObject_GetComponent, meshGo, params);
            } __except (1) {
              continue;
            }
            if (!smr)
              continue;

            void *mesh = nullptr;
            __try {
              mesh = Invoke(g_smr_get_sharedMesh, smr);
            } __except (1) {
              continue;
            }
            if (!mesh)
              continue;

            void *countBoxed = nullptr;
            __try {
              countBoxed = Invoke(g_mesh_get_blendShapeCount, mesh);
            } __except (1) {
              continue;
            }
            int bsCount = countBoxed ? *(int *)((char *)countBoxed + 16) : 0;

            fprintf(df, "\n[%s] %d blend shapes:\n", faceMeshNames[mi],
                    bsCount);
            for (int bs = 0; bs < bsCount; bs++) {
              __try {
                void *params[] = {&bs};
                void *nameStr = Invoke(g_mesh_GetBlendShapeName, mesh, params);
                char bsName[256] = "?";
                if (nameStr)
                  ReadStrUtf8(nameStr, bsName, sizeof(bsName));

                void *wParams[] = {&bs};
                void *wBoxed = Invoke(g_smr_GetBlendShapeWeight, smr, wParams);
                float weight = wBoxed ? *(float *)((char *)wBoxed + 16) : 0.0f;

                fprintf(df, "  [%3d] %s (weight=%.1f)\n", bs, bsName, weight);
              } __except (1) {
                fprintf(df, "  [%3d] EXCEPTION\n", bs);
              }
            }
          }
        }
      }
    } else {
      fprintf(df, "  (SkinnedMeshRenderer API not resolved)\n");
    }

  } else {
    fprintf(df, "\n[WARN] No Animator captured yet.\n");
    fprintf(df, "Make sure you're in-game with a character loaded.\n");
    fprintf(df, "The GetMainCharacter hook needs to fire first.\n");
  }

  fprintf(df, "\n=== Unity API Status ===\n");
  fprintf(df, "Animator.GetBoneTransform: %s\n",
          g_animator_GetBoneTransform ? "YES" : "NO");
  fprintf(df, "Animator.get_isHuman: %s\n",
          g_animator_get_isHuman ? "YES" : "NO");
  fprintf(df, "Transform.set_localRotation: %s\n",
          g_transform_set_localRotation ? "YES" : "NO");
  fprintf(df, "Transform.set_localPosition: %s\n",
          g_transform_set_localPosition ? "YES" : "NO");

  fflush(df);
  fclose(df);
  Log("[OK] Phase 2 bone discovery written to eiem_skeleton_dump.txt");
}


static DWORD WINAPI InitThread(LPVOID) {
  while (!GetModuleHandleW(L"GameAssembly.dll"))
    Sleep(500);

  // GameAssembly can be present before Unity's GC and managed thread table
  // are fully initialized. Keep the startup barrier before any IL2CPP call.
  Sleep(3000);

  if (g_shutdownRequested)
    return 0;

  InitializeCriticalSection(&g_logLock);
  g_logHandle =
      CreateFileA("plugin\\eiem_log.txt", GENERIC_WRITE, FILE_SHARE_READ, NULL,
                  CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  Log("=== EIEM Phase 1: Skeleton Discovery ===");
  Log("[DIAG-LOOP-v113] process=%lu thread=%lu tick=%llu stage=startup "
      "generation=%ld",
      (unsigned long)GetCurrentProcessId(), (unsigned long)GetCurrentThreadId(),
      (unsigned long long)GetTickCount64(),
      InterlockedCompareExchange(&s_eiemModGeneration, 0, 0));
  Log("[BUILD] assembly-complete-mesh-20260917 dll=%s %s", __DATE__,
      __TIME__);

  if (!Resolve()) {
    Log("[FATAL] Failed to resolve IL2CPP API");
    return 1;
  }
  Log("[OK] IL2CPP API resolved");

  InitHardcodedMuscleMap();

  void *domain = nullptr;
  void **asms = nullptr;
  size_t ac = 0;
  // Wait only for a usable assembly list. The resource pass must be hooked
  // before the expensive metadata dump and other plugin initialization.
  for (int attempt = 0; attempt < 250 && !g_shutdownRequested; ++attempt) {
    domain = il2cpp_domain_get ? il2cpp_domain_get() : nullptr;
    if (domain && il2cpp_thread_attach)
      il2cpp_thread_attach(domain);
    if (domain && il2cpp_domain_get_assemblies)
      asms = il2cpp_domain_get_assemblies(domain, &ac);
    if (asms && ac > 0)
      break;
    Sleep(20);
  }
  if (!asms || ac == 0) {
    Log("[FATAL] IL2CPP assemblies were not ready");
    return 1;
  }
  Log("[OK] Domain attached, %zu assemblies", ac);

  if (MH_Initialize() != MH_OK) {
    Log("[FATAL] MinHook initialization failed");
    return 1;
  }

  // Resource completion can begin before the game window and hotkey thread
  // exist. Load the rules before enabling any resource hook so the first
  // observed completion cannot commit an unmodified object due to startup
  // ordering. Later reload requests are dispatched on Unity's thread.
  LoadEiemConfig();
  // The stable replacement profile keeps camera, face and legacy animation
  // hooks out of the process.  ForceClearDither is an experimental global
  // visual operation: it runs after every camera pitch evaluation and can
  // interfere with the game's temporary dodge/occlusion dither state.  Keep
  // the implementation and config available for a dedicated visual test, but
  // never install it in the production static-replacement baseline.
  if (!kEiemStaticReplacementBaseline &&
      EiemGetGlobalConfig().disableCameraFade) {
    if (!InitEiemCameraFade(asms, ac))
      Log("[CAMERA-FADE] enabled in config, but initialization was unavailable");
  } else if (kEiemStaticReplacementBaseline) {
    Log("[CAMERA-FADE] disabled in static replacement baseline");
  } else {
    Log("[CAMERA-FADE] disabled by config");
  }
  EiemReloadMods();

  Log("[RES-TRACE] Installing startup resource hooks before metadata dump");
  // Resolve the Mesh managed methods before installing any diagnostic
  // detours.  The setter-trace build consumes these addresses directly;
  // installing the trace first silently skipped every entry because the
  // backend resolver had not populated the method pointers yet.
  EiemResolveResourceBackend(asms, ac);
  InitIl2CppResourceTrace(asms, ac);
  if (!kEiemStaticReplacementBaseline)
    DumpIl2CppMetadata(asms, ac);


  g_transformClass = FindClass("UnityEngine", "Transform", asms, ac);
  if (g_transformClass) {
    Log("[OK] Transform class found");
    g_transform_get_localRotation =
        FindMethod(g_transformClass, "get_localRotation", 0);
    g_transform_set_localRotation =
        FindMethod(g_transformClass, "set_localRotation", 1);
    g_transform_get_localPosition =
        FindMethod(g_transformClass, "get_localPosition", 0);
    g_transform_set_localPosition =
        FindMethod(g_transformClass, "set_localPosition", 1);
    g_transform_get_localScale =
        FindMethod(g_transformClass, "get_localScale", 0);
    g_transform_set_localScale =
        FindMethod(g_transformClass, "set_localScale", 1);
    g_transform_get_childCount =
        FindMethod(g_transformClass, "get_childCount", 0);
    g_transform_GetChild = FindMethod(g_transformClass, "GetChild", 1);
    g_transform_Find = FindMethod(g_transformClass, "Find", 1);
    g_transform_get_parent = FindMethod(g_transformClass, "get_parent", 0);
    g_transform_set_parent = FindMethod(g_transformClass, "SetParent", 2);
    g_transform_get_position = FindMethod(g_transformClass, "get_position", 0);
    g_transform_get_localToWorldMatrix =
        FindMethod(g_transformClass, "get_localToWorldMatrix", 0);
    Log("  get_localRotation: %p", g_transform_get_localRotation);
    Log("  set_localRotation: %p", g_transform_set_localRotation);
    Log("  get_localPosition: %p", g_transform_get_localPosition);
    Log("  set_localPosition: %p", g_transform_set_localPosition);
    Log("  get_localScale: %p", g_transform_get_localScale);
    Log("  set_localScale: %p", g_transform_set_localScale);
    Log("  get_childCount: %p", g_transform_get_childCount);
    Log("  GetChild: %p", g_transform_GetChild);
    Log("  Find: %p", g_transform_Find);
    Log("  get_localToWorldMatrix: %p", g_transform_get_localToWorldMatrix);
    Log("  get_position: %p, SetParent(Transform,bool): %p",
        g_transform_get_position, g_transform_set_parent);
  } else {
    Log("[WARN] Transform class NOT found");
  }

  void *objectClass = FindClass("UnityEngine", "Object", asms, ac);
  if (objectClass) {
    g_object_get_name = FindMethod(objectClass, "get_name", 0);
    static const char *const typeParameter[] = {"System.Type"};
    g_object_find_objects_of_type = EiemFindMethodWithParamTypes(
        objectClass, "FindObjectsOfType", typeParameter,
        _countof(typeParameter));
    g_object_destroy = FindMethod(objectClass, "Destroy", 1);
    Log("[OK] Object.get_name: %p, FindObjectsOfType(Type): %p, Destroy: %p",
        g_object_get_name, g_object_find_objects_of_type, g_object_destroy);
  }

  void *resourcesClass = FindClass("UnityEngine", "Resources", asms, ac);
  if (resourcesClass) {
    static const char *const typeParameter[] = {"System.Type"};
    g_resources_find_objects_of_type_all = EiemFindMethodWithParamTypes(
        resourcesClass, "FindObjectsOfTypeAll", typeParameter,
        _countof(typeParameter));
    Log("[DUMP] Resources.FindObjectsOfTypeAll(Type): %p",
        g_resources_find_objects_of_type_all);
  }

  g_animatorClass = FindClass("UnityEngine", "Animator", asms, ac);
  if (g_animatorClass) {
    Log("[OK] Animator class found");
    g_animator_GetBoneTransform =
        FindMethod(g_animatorClass, "GetBoneTransform", 1);
    g_animator_get_avatar = FindMethod(g_animatorClass, "get_avatar", 0);
    g_animator_get_isHuman = FindMethod(g_animatorClass, "get_isHuman", 0);
    g_animator_Rebind = FindMethod(g_animatorClass, "Rebind", 0);
    g_animator_Update = FindMethod(g_animatorClass, "Update", 1);
    g_animator_SetBoneLocalRotation =
        FindMethod(g_animatorClass, "SetBoneLocalRotation", 2);
    Log("  GetBoneTransform: %p", g_animator_GetBoneTransform);
    Log("  get_avatar: %p", g_animator_get_avatar);
    Log("  get_isHuman: %p", g_animator_get_isHuman);
    Log("  Rebind: %p  Update: %p", g_animator_Rebind, g_animator_Update);
    Log("  SetBoneLocalRotation: %p", g_animator_SetBoneLocalRotation);

    void *behaviourClass = FindClass("UnityEngine", "Behaviour", asms, ac);
    if (behaviourClass) {
      g_animator_get_enabled = FindMethod(behaviourClass, "get_enabled", 0);
      g_animator_set_enabled = FindMethod(behaviourClass, "set_enabled", 1);
    }
    Log("  get/set_enabled: %p / %p (from Behaviour)", g_animator_get_enabled,
        g_animator_set_enabled);
  } else {
    Log("[WARN] Animator class NOT found");
  }

  g_humanPoseHandlerClass =
      FindClass("UnityEngine", "HumanPoseHandler", asms, ac);
  if (!kEiemStaticReplacementBaseline && g_humanPoseHandlerClass) {
    void *iter = nullptr;
    void *method;
    Log("[HPH] Enumerating HumanPoseHandler methods:");
    while ((method = il2cpp_class_get_methods(g_humanPoseHandlerClass,
                                              &iter)) != nullptr) {
      const char *name = il2cpp_method_get_name(method);
      int nparams = il2cpp_method_get_param_count(method);
      Log("[HPH]   %s (%d params) = %p", name, nparams, method);

      if (strcmp(name, ".ctor") == 0 && nparams == 2)
        g_humanPoseHandler_ctor = method;
      if (strcmp(name, "SetHumanPose") == 0)
        g_humanPoseHandler_SetHumanPose = method;
      if (strcmp(name, "GetHumanPose") == 0)
        g_humanPoseHandler_GetHumanPose = method;
      if (strcmp(name, "Dispose") == 0)
        g_humanPoseHandler_Dispose = method;
      if (strcmp(name, "GetInternalAvatarPose") == 0) {
        Log("[HPH]   *** Found GetInternalAvatarPose as MethodInfo! ***");
      }
    }

    __try {
      il2cpp_class_get_method_from_name(g_humanPoseHandlerClass,
                                        "SetInternalAvatarPose", 1);
    } __except (1) {
      Log("[HPH]   get_method_from_name crashed for SetInternalAvatarPose");
    }
    __try {
      il2cpp_class_get_method_from_name(g_humanPoseHandlerClass,
                                        "SetInternalHumanPose", 2);
    } __except (1) {
      Log("[HPH]   get_method_from_name crashed for SetInternalHumanPose");
    }

    if (!g_humanPoseHandler_SetHumanPose && il2cpp_resolve_icall) {
      Log("[HPH] SetHumanPose stripped! Resolving native icalls...");

      void *fn1 = il2cpp_resolve_icall(
          "UnityEngine.HumanPoseHandler::SetInternalAvatarPose");
      if (fn1) {
        g_icall_SetInternalAvatarPose = (fn_InternalAvatarPose)fn1;
        Log("[HPH]   SetInternalAvatarPose = %p", fn1);
      }

      void *fn2 = il2cpp_resolve_icall(
          "UnityEngine.HumanPoseHandler::SetInternalHumanPose");
      if (fn2) {
        g_icall_SetInternalHumanPose = (fn_InternalHumanPose)fn2;
        Log("[HPH]   SetInternalHumanPose = %p", fn2);
      }

      void *fn3 =
          il2cpp_resolve_icall("UnityEngine.HumanPoseHandler::SetHumanPose");
      if (fn3) {
        g_icall_SetHumanPose = (fn_SetHumanPose_compiled)fn3;
        Log("[HPH]   SetHumanPose compiled = %p", fn3);
      }

      auto dumpBytes64 = [](const char *name, void *fn) {
        if (!fn)
          return;
        unsigned char *p = (unsigned char *)fn;
        for (int row = 0; row < 4; row++) {
          int o = row * 16;
          Log("[HPH]   %s +%02X: %02X %02X %02X %02X %02X %02X %02X %02X %02X "
              "%02X %02X %02X %02X %02X %02X %02X",
              name, o, p[o + 0], p[o + 1], p[o + 2], p[o + 3], p[o + 4],
              p[o + 5], p[o + 6], p[o + 7], p[o + 8], p[o + 9], p[o + 10],
              p[o + 11], p[o + 12], p[o + 13], p[o + 14], p[o + 15]);
        }
      };
      void *fnGet = il2cpp_resolve_icall(
          "UnityEngine.HumanPoseHandler::GetInternalAvatarPose");
      g_icall_GetInternalAvatarPose = (fn_InternalAvatarPose)fnGet;
      Log("[HPH]   GetInternalAvatarPose = %p", fnGet);

      if (fnGet && g_icall_SetInternalAvatarPose) {
        MH_STATUS st =
            MH_CreateHook(fnGet, (void *)Hooked_GetInternalAvatarPose,
                          (void **)&orig_GetInternalAvatarPose);
        if (st == MH_OK) {
          MH_EnableHook(fnGet);
          g_trojanHookTarget = fnGet;
          Log("[TROJAN] Hooked GetInternalAvatarPose! orig=%p",
              orig_GetInternalAvatarPose);
        } else {
          Log("[TROJAN] Hook failed: MH_STATUS=%d", st);
        }
      }
      dumpBytes64("SetAvatarPose", fn1);
      dumpBytes64("GetAvatarPose", fnGet);
      dumpBytes64("SetHumanPose", fn3);

      void *compiledGet = ((void **)g_humanPoseHandler_GetHumanPose)[0];
      if (compiledGet) {
        Log("[SCAN] Compiled GetHumanPose at %p", compiledGet);
        unsigned char *p = (unsigned char *)compiledGet;

        uintptr_t gaBase = (uintptr_t)GetModuleHandleW(L"GameAssembly.dll");
        int callOffset = -1;
        int movOffset = -1;
        int leaOffset = -1;
        unsigned char *h = nullptr;

        for (int i = 0; i < 128; i++) {
          if (p[i] != 0xE8)
            continue;
          int32_t disp = *(int32_t *)(p + i + 1);
          uintptr_t target = (uintptr_t)(p + i + 5) + disp;
          if (target <= gaBase || target >= gaBase + 0x10000000)
            continue;

          unsigned char *candidate = (unsigned char *)target;
          int foundMov = -1;
          __try {
            for (int j = 0; j < 96; j++) {
              if (candidate[j] == 0x48 && candidate[j+1] == 0x8B && candidate[j+2] == 0x05) {
                foundMov = j;
                break;
              }
            }
          } __except (1) {
            continue; 
          }

          if (foundMov >= 0) {
            callOffset = i;
            movOffset = foundMov;
            h = candidate;
            Log("[SCAN] Helper thunk at %p (found CALL at +0x%X, MOV at +0x%X)",
                (void *)target, i, foundMov);

            for (int j = 0; j < 96; j++) {
              if (h[j] == 0x48 && h[j+1] == 0x8D && h[j+2] == 0x0D) {
                leaOffset = j;
                break;
              }
            }
            break; 
          }
        }

        if (callOffset >= 0 && movOffset >= 0 && h) {
          int32_t slotDisp = *(int32_t *)(h + movOffset + 3);
          uintptr_t slotAddr = (uintptr_t)(h + movOffset + 7) + slotDisp;
          void *cachedFn = nullptr;
          __try {
            cachedFn = *(void **)slotAddr;
          } __except (1) {
          }
          Log("[SLOT] Cached icall slot at %p fn=%p", (void *)slotAddr,
              cachedFn);

          if (leaOffset >= 0) {
            int32_t nameDisp = *(int32_t *)(h + leaOffset + 3);
            const char *nameStr =
                (const char *)((uintptr_t)(h + leaOffset + 7) + nameDisp);
            char nameBuf[256] = {};
            __try {
              strncpy(nameBuf, nameStr, sizeof(nameBuf) - 1);
              nameBuf[sizeof(nameBuf) - 1] = '\0';
            } __except (1) {
              strncpy(nameBuf, "<unreadable>", sizeof(nameBuf) - 1);
              nameBuf[sizeof(nameBuf) - 1] = '\0';
            }
            Log("[SLOT] icall name: \"%s\"", nameBuf);

            char setName[256] = {};
            strncpy(setName, nameBuf, sizeof(setName) - 1);
            setName[sizeof(setName) - 1] = '\0';
            char *getPos = strstr(setName, "Get");
            if (getPos) {
              memcpy(getPos, "Set", 3);
              Log("[SLOT] Resolving Set version: \"%s\"", setName);
              void *setFn = il2cpp_resolve_icall(setName);
              Log("[SLOT] Set function resolved: %p", setFn);

              if (setFn) {
                g_slotAddr = (void **)slotAddr;
                g_slotOrigGet = cachedFn;
                g_slotSetFn = setFn;
                Log("[SLOT] *** READY TO PATCH! slot=%p get=%p set=%p ***",
                    (void *)slotAddr, cachedFn, setFn);
              }
            }
          } else {
            Log("[SCAN] LEA RCX pattern (48 8D 0D) not found in helper");
          }
        } else {
          Log("[SCAN] No E8 CALL with icall slot pattern found in first 128 "
              "bytes of compiled GetHumanPose");
        }
      }
    }

    Log("[OK] HumanPoseHandler: ctor=%p Get=%p SetHP=%p SetAvatar=%p "
        "SetHuman=%p",
        g_humanPoseHandler_ctor, g_humanPoseHandler_GetHumanPose,
        g_icall_SetHumanPose, g_icall_SetInternalAvatarPose,
        g_icall_SetInternalHumanPose);
  } else if (!kEiemStaticReplacementBaseline) {
    Log("[WARN] HumanPoseHandler NOT found");
  } else {
    Log("[VALIDATION] HumanPoseHandler/Trojan hooks disabled");
  }

  g_gameObjectClass = FindClass("UnityEngine", "GameObject", asms, ac);
  if (g_gameObjectClass) {
    g_gameObject_get_transform =
        FindMethod(g_gameObjectClass, "get_transform", 0);
    g_gameObject_get_activeSelf =
        FindMethod(g_gameObjectClass, "get_activeSelf", 0);
    g_gameObject_get_activeInHierarchy =
        FindMethod(g_gameObjectClass, "get_activeInHierarchy", 0);
    g_gameObject_set_active = FindMethod(g_gameObjectClass, "SetActive", 1);
    g_gameObject_get_layer = FindMethod(g_gameObjectClass, "get_layer", 0);
    g_gameObject_set_layer = FindMethod(g_gameObjectClass, "set_layer", 1);
    g_gameObject_ctor = FindMethod(g_gameObjectClass, ".ctor", 1);
    g_gameObject_ctorDefault = FindMethod(g_gameObjectClass, ".ctor", 0);
    g_gameObject_set_name = FindMethod(g_gameObjectClass, "set_name", 1);
    g_gameObject_AddComponent = FindMethod(g_gameObjectClass, "AddComponent", 1);
    Log("[OK] GameObject: transform=%p active=%p/%p setActive=%p layer=%p/%p "
        "ctor(string)=%p ctor()=%p set_name=%p AddComponent=%p",
        g_gameObject_get_transform, g_gameObject_get_activeSelf,
        g_gameObject_get_activeInHierarchy, g_gameObject_set_active,
        g_gameObject_get_layer, g_gameObject_set_layer,
        g_gameObject_ctor, g_gameObject_ctorDefault, g_gameObject_set_name,
        g_gameObject_AddComponent);
  }

  g_componentClass = FindClass("UnityEngine", "Component", asms, ac);
  if (g_componentClass) {
    g_component_get_gameObject =
        FindMethod(g_componentClass, "get_gameObject", 0);
    g_component_get_transform =
        FindMethod(g_componentClass, "get_transform", 0);
    Log("[OK] Component.get_transform: %p", g_component_get_transform);
  }

  // BaseModelViewPart stores HGMeshRenderer[] separately from its ordinary
  // Renderer[]/SkinnedMeshRenderer[] arrays.  Resolve the type once so the
  // runtime probe can tell whether a source renderer participates in that
  // parallel registration path.  No HG component is created or modified.
  g_hgMeshRendererClass =
      FindClass("UnityEngine", "HGMeshRenderer", asms, ac);
  Log("[RES-TRACE] UnityEngine.HGMeshRenderer class=%p",
      g_hgMeshRendererClass);

  if (g_gameObjectClass) {
    g_gameObject_GetComponent =
        FindMethod(g_gameObjectClass, "GetComponent", 1);
    g_gameObject_GetComponentsInChildren =
        FindMethod(g_gameObjectClass, "GetComponentsInChildren", 2);
    Log("[OK] GameObject.GetComponent: %p", g_gameObject_GetComponent);
    Log("[OK] GameObject.GetComponentsInChildren(Type,bool): %p",
        g_gameObject_GetComponentsInChildren);
  }

  g_skinnedMeshRendererClass =
      FindClass("UnityEngine", "SkinnedMeshRenderer", asms, ac);
  if (g_skinnedMeshRendererClass) {
    g_smr_get_sharedMesh =
        FindMethod(g_skinnedMeshRendererClass, "get_sharedMesh", 0);
    g_smr_set_sharedMesh =
        FindMethod(g_skinnedMeshRendererClass, "set_sharedMesh", 1);
    g_smr_GetBlendShapeWeight =
        FindMethod(g_skinnedMeshRendererClass, "GetBlendShapeWeight", 1);
    g_smr_SetBlendShapeWeight =
        FindMethod(g_skinnedMeshRendererClass, "SetBlendShapeWeight", 2);
    g_smr_get_bones = FindMethod(g_skinnedMeshRendererClass, "get_bones", 0);
    g_smr_set_bones = FindMethod(g_skinnedMeshRendererClass, "set_bones", 1);
    g_smr_get_rootBone = FindMethod(g_skinnedMeshRendererClass, "get_rootBone", 0);
    g_smr_set_rootBone = FindMethod(g_skinnedMeshRendererClass, "set_rootBone", 1);
    g_smr_get_skinningRoot =
        FindMethod(g_skinnedMeshRendererClass, "get_skinningRoot", 0);
    g_smr_set_skinningRoot =
        FindMethod(g_skinnedMeshRendererClass, "set_skinningRoot", 1);
    g_smr_get_quality = FindMethod(g_skinnedMeshRendererClass, "get_quality", 0);
    g_smr_set_quality = FindMethod(g_skinnedMeshRendererClass, "set_quality", 1);
    g_smr_get_updateWhenOffscreen =
        FindMethod(g_skinnedMeshRendererClass, "get_updateWhenOffscreen", 0);
    g_smr_set_updateWhenOffscreen =
        FindMethod(g_skinnedMeshRendererClass, "set_updateWhenOffscreen", 1);
    g_smr_get_forceMatrixRecalculationPerRender = FindMethod(
        g_skinnedMeshRendererClass, "get_forceMatrixRecalculationPerRender", 0);
    g_smr_set_forceMatrixRecalculationPerRender = FindMethod(
        g_skinnedMeshRendererClass, "set_forceMatrixRecalculationPerRender", 1);
    g_smr_get_skinnedMotionVectors =
        FindMethod(g_skinnedMeshRendererClass, "get_skinnedMotionVectors", 0);
    g_smr_set_skinnedMotionVectors =
        FindMethod(g_skinnedMeshRendererClass, "set_skinnedMotionVectors", 1);
    // localBounds is declared by Renderer, not SkinnedMeshRenderer. A direct
    // class lookup returned null in v74 and silently disabled replacement
    // bounds updates.
    g_smr_get_localBounds =
        FindMethodInHierarchy(g_skinnedMeshRendererClass, "get_localBounds", 0);
    g_smr_set_localBounds =
        FindMethodInHierarchy(g_skinnedMeshRendererClass, "set_localBounds", 1);
    Log("[OK] SkinnedMeshRenderer: get/set sharedMesh=%p/%p, GetWeight=%p, SetWeight=%p, "
        "get/set bones=%p/%p rootBone=%p/%p skinningRoot=%p/%p "
        "quality=%p/%p offscreen=%p/%p forceMatrix=%p/%p motion=%p/%p "
        "localBounds=%p/%p",
        g_smr_get_sharedMesh, g_smr_set_sharedMesh, g_smr_GetBlendShapeWeight,
        g_smr_SetBlendShapeWeight, g_smr_get_bones, g_smr_set_bones,
        g_smr_get_rootBone, g_smr_set_rootBone, g_smr_get_skinningRoot,
        g_smr_set_skinningRoot, g_smr_get_quality, g_smr_set_quality,
        g_smr_get_updateWhenOffscreen, g_smr_set_updateWhenOffscreen,
        g_smr_get_forceMatrixRecalculationPerRender,
        g_smr_set_forceMatrixRecalculationPerRender,
        g_smr_get_skinnedMotionVectors, g_smr_set_skinnedMotionVectors,
        g_smr_get_localBounds, g_smr_set_localBounds);
  } else {
    Log("[WARN] SkinnedMeshRenderer class NOT found");
  }

  void *meshFilterClass = FindClass("UnityEngine", "MeshFilter", asms, ac);
  if (meshFilterClass) {
    g_meshFilterClass = meshFilterClass;
    g_meshFilter_get_sharedMesh =
        FindMethod(meshFilterClass, "get_sharedMesh", 0);
    g_meshFilter_set_sharedMesh =
        FindMethod(meshFilterClass, "set_sharedMesh", 1);
    Log("[OK] MeshFilter: get/set sharedMesh=%p/%p", g_meshFilter_get_sharedMesh,
        g_meshFilter_set_sharedMesh);
  }
  g_meshRendererClass =
      FindClass("UnityEngine", "MeshRenderer", asms, ac);
  Log("[OK] MeshRenderer class: %p", g_meshRendererClass);

  void *rendererClass = FindClass("UnityEngine", "Renderer", asms, ac);
  if (rendererClass) {
    g_rendererClass = rendererClass;
    g_renderer_get_enabled =
        FindMethodInHierarchy(rendererClass, "get_enabled", 0);
    g_renderer_set_enabled =
        FindMethodInHierarchy(rendererClass, "set_enabled", 1);
    g_renderer_get_isVisible =
        FindMethodInHierarchy(rendererClass, "get_isVisible", 0);
    g_renderer_get_forceRenderingOff =
        FindMethodInHierarchy(rendererClass, "get_forceRenderingOff", 0);
    g_renderer_get_sharedMaterials =
        FindMethodInHierarchy(rendererClass, "get_sharedMaterials", 0);
    // World-space AABB. This is the only cheap measurement that can tell a
    // renderer that is posed correctly from one that has collapsed into its
    // bind pose somewhere below the model.
    g_renderer_get_bounds =
        FindMethodInHierarchy(rendererClass, "get_bounds", 0);
    Log("[MOD-LOD] Renderer enabled get/set/isVisible/forceRenderingOff: %p / %p / %p / %p",
        g_renderer_get_enabled, g_renderer_set_enabled,
        g_renderer_get_isVisible, g_renderer_get_forceRenderingOff);
  }

  g_materialClass = FindClass("UnityEngine", "Material", asms, ac);
  if (g_materialClass) {
    g_material_get_shader = FindMethodInHierarchy(g_materialClass, "get_shader", 0);
    g_material_GetTexturePropertyNames = FindMethod(g_materialClass, "GetTexturePropertyNames", 0);
    static const char *const materialStringType[] = {"System.String"};
    g_material_GetTexture = FindMethodWithParamTypes(
        g_materialClass, "GetTexture", materialStringType, 1);
    g_material_GetColor = FindMethodWithParamTypes(
        g_materialClass, "GetColor", materialStringType, 1);
    g_material_GetVector = FindMethodWithParamTypes(
        g_materialClass, "GetVector", materialStringType, 1);
    g_material_GetFloat = FindMethodWithParamTypes(
        g_materialClass, "GetFloat", materialStringType, 1);
    g_material_GetInt = FindMethodWithParamTypes(
        g_materialClass, "GetInt", materialStringType, 1);
    Log("[DUMP] Material APIs: shader=%p textureNames=%p texture=%p color=%p vector=%p float=%p int=%p",
        g_material_get_shader, g_material_GetTexturePropertyNames,
        g_material_GetTexture, g_material_GetColor, g_material_GetVector,
        g_material_GetFloat, g_material_GetInt);
  }
  g_textureClass = FindClass("UnityEngine", "Texture", asms, ac);
  if (g_textureClass) {
    g_texture_get_width = FindMethod(g_textureClass, "get_width", 0);
    g_texture_get_height = FindMethod(g_textureClass, "get_height", 0);
  }

  void *meshClass = FindClass("UnityEngine", "Mesh", asms, ac);
  if (meshClass) {
    g_mesh_get_vertices = FindMethod(meshClass, "get_vertices", 0);
    g_mesh_get_normals = FindMethod(meshClass, "get_normals", 0);
    g_mesh_get_tangents = FindMethod(meshClass, "get_tangents", 0);
    g_mesh_get_uv = FindMethod(meshClass, "get_uv", 0);
    g_mesh_get_colors = FindMethod(meshClass, "get_colors", 0);
    g_mesh_get_triangles = FindMethod(meshClass, "get_triangles", 0);
    g_mesh_GetTriangles = FindMethod(meshClass, "GetTriangles", 1);
    g_mesh_get_vertexCount = FindMethod(meshClass, "get_vertexCount", 0);
    g_mesh_GetIndexCount = FindMethod(meshClass, "GetIndexCount", 1);
    g_mesh_get_subMeshCount = FindMethod(meshClass, "get_subMeshCount", 0);
    g_mesh_get_boneWeights = FindMethod(meshClass, "GetBoneWeightsImpl", 0);
    g_mesh_get_bindposes = FindMethod(meshClass, "get_bindposes", 0);
    g_mesh_get_blendShapeCount =
        FindMethod(meshClass, "get_blendShapeCount", 0);
    g_mesh_GetBlendShapeName = FindMethod(meshClass, "GetBlendShapeName", 1);
    Log("[OK] Mesh export API: vertices=%p normals=%p tangents=%p uv=%p "
        "colors=%p triangles=%p vertexCount=%p indexCount=%p subMeshes=%p bones=%p bindposes=%p",
        g_mesh_get_vertices, g_mesh_get_normals, g_mesh_get_tangents,
        g_mesh_get_uv, g_mesh_get_colors, g_mesh_get_triangles,
        g_mesh_GetTriangles, g_mesh_get_vertexCount, g_mesh_GetIndexCount,
        g_mesh_get_subMeshCount, g_mesh_get_boneWeights,
        g_mesh_get_bindposes);

    void *meshDataClass = FindClass("", "MeshData", asms, ac);
    void *meshDataArrayClass = FindClass("", "MeshDataArray", asms, ac);
    if (meshDataClass) {
      g_meshData_get_vertexCount = FindMethod(meshDataClass, "get_vertexCount", 0);
      g_meshData_get_subMeshCount = FindMethod(meshDataClass, "get_subMeshCount", 0);
      g_meshData_GetIndexCount = FindMethod(meshDataClass, "GetIndexCount", 1);
      g_meshData_CopyAttributeIntoPtr =
          FindMethod(meshDataClass, "CopyAttributeIntoPtr", 5);
      g_meshData_CopyIndicesIntoPtr =
          FindMethod(meshDataClass, "CopyIndicesIntoPtr", 5);
    }
    if (meshDataArrayClass) {
      g_meshDataArray_get_Item = FindMethod(meshDataArrayClass, "get_Item", 1);
      g_meshDataArray_Dispose = FindMethod(meshDataArrayClass, "Dispose", 0);
    }
    g_mesh_acquireReadOnlyMeshData =
        FindMethod(meshClass, "AcquireReadOnlyMeshData", 1);
    Log("[DUMP] MeshData fallback: acquire=%p item=%p dispose=%p vertexCount=%p "
        "subMeshCount=%p indexCount=%p attrCopy=%p indexCopy=%p",
        g_mesh_acquireReadOnlyMeshData, g_meshDataArray_get_Item,
        g_meshDataArray_Dispose, g_meshData_get_vertexCount,
        g_meshData_get_subMeshCount, g_meshData_GetIndexCount,
        g_meshData_CopyAttributeIntoPtr, g_meshData_CopyIndicesIntoPtr);
  } else {
    Log("[WARN] Mesh class NOT found");
  }

  g_cameraClass = FindClass("UnityEngine", "Camera", asms, ac);
  if (g_cameraClass) {
    g_camera_get_main = FindMethod(g_cameraClass, "get_main", 0);
    g_camera_get_fieldOfView = FindMethod(g_cameraClass, "get_fieldOfView", 0);
    g_camera_set_fieldOfView = FindMethod(g_cameraClass, "set_fieldOfView", 1);
    Log("[CAM] Camera class found: get_main=%p get_fov=%p set_fov=%p",
        g_camera_get_main, g_camera_get_fieldOfView, g_camera_set_fieldOfView);
  } else {
    Log("[WARN] Camera class NOT found");
  }

  if (!kEiemStaticReplacementBaseline && !g_camSetPos) {
    g_camSetPos = (void (*)(void *, float *))il2cpp_resolve_icall(
        "UnityEngine.Transform::set_position_Injected(UnityEngine.Vector3&)");
    g_camSetRot = (void (*)(void *, float *))il2cpp_resolve_icall(
        "UnityEngine.Transform::set_rotation_Injected(UnityEngine.Quaternion&)");
    g_camGetPos = (CamGetPosRot_t)il2cpp_resolve_icall(
        "UnityEngine.Transform::get_position_Injected(UnityEngine.Vector3&)");
    g_camGetRot = (CamGetPosRot_t)il2cpp_resolve_icall(
        "UnityEngine.Transform::get_rotation_Injected(UnityEngine.Quaternion&)");
    Log("[CAM] Transform icalls: setPos=%p setRot=%p getPos=%p getRot=%p",
        g_camSetPos, g_camSetRot, g_camGetPos, g_camGetRot);

    void *setLocalPos = il2cpp_resolve_icall(
        "UnityEngine.Transform::set_localPosition_Injected(UnityEngine.Vector3&)");
    void *setLocalRot = il2cpp_resolve_icall(
        "UnityEngine.Transform::set_localRotation_Injected(UnityEngine.Quaternion&)");

    if (g_camSetPos &&
        MH_CreateHook((void *)g_camSetPos, (void *)Hook_SetPos,
                      (void **)&g_origSetPos) == MH_OK)
      MH_EnableHook((void *)g_camSetPos);
    if (g_camSetRot &&
        MH_CreateHook((void *)g_camSetRot, (void *)Hook_SetRot,
                      (void **)&g_origSetRot) == MH_OK)
      MH_EnableHook((void *)g_camSetRot);
    if (setLocalPos &&
        MH_CreateHook(setLocalPos, (void *)Hook_SetLocalPos,
                      (void **)&g_origSetLocalPos) == MH_OK)
      MH_EnableHook(setLocalPos);
    if (setLocalRot &&
        MH_CreateHook(setLocalRot, (void *)Hook_SetLocalRot,
                      (void **)&g_origSetLocalRot) == MH_OK)
      MH_EnableHook(setLocalRot);
    Log("[CAM] Transform write-hooks: setPos=%p setRot=%p setLPos=%p setLRot=%p",
        g_origSetPos, g_origSetRot, g_origSetLocalPos, g_origSetLocalRot);
  }

  // The former MovementComponent/FinalIK detours belonged to the abandoned
  // per-frame MMD foot-IK experiment. Do not resolve or install those hooks:
  // resource lifecycle work must leave the game's animation solver untouched.
  Log("[LEGACY] Movement and FinalIK detours disabled; game-owned IK remains untouched");

  void *pcClass =
      FindClass("Beyond.Gameplay.Core", "PlayerController", asms, ac);
  if (pcClass) {
    Log("[OK] PlayerController class found");

    {
      const char *entityNames[] = {"mainCharacter", "m_entity", "_entity",
                                   "m_mainCharacter", "entity",
                                   "m_controlledEntity", "controlledEntity"};
      const char *matchedName = nullptr;
      OFF_pcEntity = FindFieldInHierarchy(pcClass, entityNames, 7, &matchedName);
      if (OFF_pcEntity >= 0) {
        Log("[OFFSET] PlayerController.%s = %d (0x%X)", matchedName,
            OFF_pcEntity, OFF_pcEntity);
      } else {
        Log("[WARN] PlayerController entity field not found in hierarchy, "
            "using fallback 0x70");
        Log("[WARN] PlayerController full hierarchy:");
        DumpFieldsHierarchy(pcClass);
      }
    }


    void *setMainChar = FindMethod(pcClass, "SetMainCharacter", 2);
    if (!kEiemStaticReplacementBaseline && setMainChar) {
      typedef void (*SetMainCharacter_t)(void *self, void *entity, bool flag);
      static SetMainCharacter_t orig_SetMainCharacter = nullptr;

      struct SetMainCharHook {
        static void Hooked(void *self, void *entity, bool flag) {
          // During host teardown the game may reset the active entity after
          // WM_CLOSE. Forward that reset, but do not touch IL2CPP metadata or
          // plugin caches while the runtime is shutting down.
          if (g_shutdownRequested) {
            if (orig_SetMainCharacter)
              orig_SetMainCharacter(self, entity, flag);
            return;
          }
          if (self && !g_playerController) {
            g_playerController = self;
            Log("[HOOK] Captured PlayerController: %p", self);
          }
          if (entity) {
            g_mainCharEntity = entity;
            Log("[HOOK] SetMainCharacter: Entity=%p", entity);

            if (OFF_entityComplexAnim < 0) {
              __try {
                void *entClass = il2cpp_object_get_class(entity);
                if (entClass) {
                  const char *caNames[] = {
                      "<animatorCom>k__BackingField",
                      "animatorCom",
                      "complexAnimationComponent",
                      "m_complexAnimationComponent",
                      "_complexAnimationComponent"};
                  const char *matchedName = nullptr;
                  OFF_entityComplexAnim =
                      FindFieldInHierarchy(entClass, caNames, 5, &matchedName);
                  if (OFF_entityComplexAnim >= 0) {
                    Log("[OFFSET] Entity.%s = %d (0x%X)", matchedName,
                        OFF_entityComplexAnim, OFF_entityComplexAnim);
                  } else {
                    Log("[WARN] Entity complexAnimationComponent not found in "
                        "hierarchy, fallback 0x110");
                    Log("[WARN] Entity full hierarchy:");
                    DumpFieldsHierarchy(entClass);
                  }
                }
              } __except (1) {
              }
            }

            __try {
              int ecOff =
                  SafeOff(OFF_entityComplexAnim, 0x110, "entityComplexAnim");
              void *complexAnimCom = *(void **)((char *)entity + ecOff);
              if (complexAnimCom) {
                if (OFF_complexAnimAnimator < 0) {
                  __try {
                    void *cacClass = il2cpp_object_get_class(complexAnimCom);
                    if (cacClass) {
                      const char *animNames[] = {"animator", "m_animator",
                                                 "_animator",
                                                 "<animator>k__BackingField"};
                      const char *matchedName2 = nullptr;
                      OFF_complexAnimAnimator = FindFieldInHierarchy(
                          cacClass, animNames, 4, &matchedName2);
                      if (OFF_complexAnimAnimator >= 0) {
                        Log("[OFFSET] ComplexAnimComp.%s = %d (0x%X)",
                            matchedName2, OFF_complexAnimAnimator,
                            OFF_complexAnimAnimator);
                      } else {
                        Log("[WARN] ComplexAnimComp animator not found in "
                            "hierarchy, fallback 0x148");
                        Log("[WARN] ComplexAnimComp full hierarchy:");
                        DumpFieldsHierarchy(cacClass);
                      }
                    }
                  } __except (1) {
                  }
                }

                int caOff = (OFF_complexAnimAnimator >= 0)
                                ? OFF_complexAnimAnimator
                                : 0x148;
                void *animator = *(void **)((char *)complexAnimCom + caOff);
                if (animator) {
                  g_cachedAnimator = animator;
                  Log("[HOOK] Got Animator: %p", animator);
                }
              }
            } __except (1) {
            }
          }
          orig_SetMainCharacter(self, entity, flag);

          s_ikDisabled = false;
          memset(s_bipedIK, 0, sizeof(s_bipedIK));
          s_bipedIKCount = 0;
          memset(s_grounderIK, 0, sizeof(s_grounderIK));
          s_grounderIKCount = 0;
          memset(s_followDamper, 0, sizeof(s_followDamper));
          s_followDamperCount = 0;
          s_animatorMono = nullptr;
          s_bbcCount = 0;
          s_bbcMethodsResolved = false;
          s_skirtBBCIndex = -1;
          memset(s_bbcInstances, 0, sizeof(s_bbcInstances));

          s_cachedMovementComp = nullptr;
          s_cachedEntity = nullptr;
          s_footIKFirstCaptured = false;
          s_footIKCalibrated = false;
          s_footPosBaseCaptured = false;
          s_initialRootCaptured = false;
          s_baseGroundCaptured = false;
          s_baseGroundY = 0.0f;
          s_smoothedGroundDelta = 0.0f;
          s_lfBaseCaptured = false;
          s_lfBaseGround = 0.0f;
          s_rfBaseCaptured = false;
          s_rfBaseGround = 0.0f;
          g_groundDeltaY = 0.0f;
          g_activeLfSolver = nullptr;
          g_activeRfSolver = nullptr;
          g_mmdIKActive = false;

          g_fingerTransformsResolved = false;
          g_fingerRestCaptured = false;
          memset(g_fingerTransforms, 0, sizeof(g_fingerTransforms));

          g_confirmedSMC = nullptr;
          g_skeletalMorphCore = nullptr;
          g_boneMapReady = false;
          ResetFaceCache();
          ResetSkirtState();
          s_firstFrame = true;
          Log("[SWITCH] Full character state reset for new character: Entity=%p Animator=%p",
              g_mainCharEntity, g_cachedAnimator);
        }
      };

      if (Hook(setMainChar, "PlayerController.SetMainCharacter",
               (void *)SetMainCharHook::Hooked,
               (void **)&orig_SetMainCharacter)) {
        Log("[OK] SetMainCharacter hooked (captures Entity on char switch)");
      }
    } else {
      Log("[WARN] SetMainCharacter not found");
    }
  } else {
    Log("[WARN] PlayerController class NOT found");
  }
  void *smcClass = FindClass("Beyond.Gameplay.View.SkeletalMorph",
                             "SkeletalMorphCore", asms, ac);
  if (!smcClass)
    smcClass = FindClass("Beyond.Gameplay.View", "SkeletalMorphCore", asms, ac);
  if (!smcClass)
    smcClass = FindClass("Beyond.Gameplay.Core", "SkeletalMorphCore", asms, ac);
  if (!kEiemStaticReplacementBaseline && smcClass) {
    g_skeletalMorphCoreClass = smcClass;
    Log("[FACE] SkeletalMorphCore class found");

    void *updateMethod = nullptr;
    void *miter = nullptr;
    void *m;
    while ((m = il2cpp_class_get_methods(smcClass, &miter))) {
      const char *mn = il2cpp_method_get_name(m);
      if (mn && strcmp(mn, "Update") == 0 &&
          il2cpp_method_get_param_count(m) == 1) {
        updateMethod = m;
        break;
      }
    }
    if (updateMethod) {
      if (Hook(updateMethod, "SkeletalMorphCore.Update",
               (void *)Hooked_SMCUpdate, (void **)&g_origSMCUpdate)) {
        Log("[FACE] SMCUpdate hooked —face expressions ready");
      }
    } else {
      Log("[FACE] Update method not found in SkeletalMorphCore");
    }

    void *jobMethod = nullptr;
    void *specialJobMethod = nullptr;
    miter = nullptr;
    while ((m = il2cpp_class_get_methods(smcClass, &miter))) {
      const char *mn = il2cpp_method_get_name(m);
      if (!mn)
        continue;
      int pc = il2cpp_method_get_param_count(m);
      if (strcmp(mn, "DoEvaluateMorphToBoneJob") == 0 && pc == 2)
        jobMethod = m;
      if (strcmp(mn, "DoEvaluateSpecialMorphToBoneJob") == 0 && pc == 2)
        specialJobMethod = m;
    }

    if (jobMethod) {
      if (Hook(jobMethod, "DoEvaluateMorphToBoneJob",
               (void *)Hooked_MorphToBoneJob, (void **)&g_origMorphToBoneJob)) {
        Log("[JOB] DoEvaluateMorphToBoneJob hooked");
      }
    } else {
      Log("[JOB] DoEvaluateMorphToBoneJob not found");
    }

    if (specialJobMethod) {
      if (Hook(specialJobMethod, "DoEvaluateSpecialMorphToBoneJob",
               (void *)Hooked_SpecialMorphJob,
               (void **)&g_origSpecialMorphJob)) {
        Log("[JOB] DoEvaluateSpecialMorphToBoneJob hooked");
      }
    }

    Log("[FACE] ApplyBoneToTransJob hook DISABLED (unstable ABI)");
  } else if (!kEiemStaticReplacementBaseline) {
    Log("[FACE] SkeletalMorphCore class not found");
  } else {
    Log("[VALIDATION] Player/face/IK hooks disabled");
  }


  Log("\n=== Phase 2 Init Complete ===");
  Log("Hooks installed. Use the GUI Dump tab for resource capture.");

  g_guiRunning = true;
  g_hotkeyThread = CreateThread(NULL, 0, HotkeyThread, NULL, 0, NULL);
  g_modUiThread = CreateThread(NULL, 0, EiemModUiThread, NULL, 0, NULL);

  if (kEiemEnableLegacyWorkers) {
    DumpCursorMethods();

    g_animationThread =
        CreateThread(NULL, 0, AnimationWorkerThread, NULL, 0, NULL);
    g_guiThread = CreateThread(NULL, 0, GuiThread, NULL, 0, NULL);
  } else {
    Log("[VALIDATION] Mod hotkey/reload/manager workers enabled; "
        "animation/legacy-GUI/update workers disabled");
  }

  return 0;
}

