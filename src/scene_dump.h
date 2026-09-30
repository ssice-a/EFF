#pragma once

#include <windows.h>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <io.h>
#include <fcntl.h>

// Dump requests are posted to the game window and executed by MmdWndProc on
// Unity's main thread. This keeps all interaction with observed IL2CPP object
// metadata on a thread where Unity's GC and object model are valid.
// Keep plugin-private commands outside the game's WM_USER range. Endfield
// uses several WM_USER messages internally, so reusing that range can invoke
// a Dump handler during unrelated startup traffic.
#define WM_EIEM_DUMP_CURRENT (WM_APP + 0x310)
#define WM_EIEM_DUMP_FULL    (WM_APP + 0x311)
#define WM_EIEM_DUMP_REFRESH (WM_APP + 0x312)
#define WM_EIEM_DUMP_DISABLED (WM_APP + 0x313)
#define WM_EIEM_MODEL_CURRENT (WM_APP + 0x314)
#define WM_EIEM_MODEL_FULL    (WM_APP + 0x315)
// 0x316/0x317 belong to Mod reconciliation/input (globals.h).
static constexpr UINT_PTR kEiemAutoSceneCensusTimer = 0xE15B;

static SRWLOCK s_dumpSelectionLock = SRWLOCK_INIT;
static void *s_dumpSelectedMeshes[4096] = {};
static size_t s_dumpSelectedMeshCount = 0;
static char g_dumpOutputDir[512] = "plugin\\dumps";
static char g_dumpSearch[256] = {};
static SRWLOCK s_dumpStatusLock = SRWLOCK_INIT;
static char s_dumpStatus[256] = "Ready";
static volatile LONG s_dumpInProgress = 0;
// These snapshots are deliberately static. A 4096-entry observation table is
// roughly 1.8 MB and must never be placed on the game or GUI thread stack.
static EiemMeshObservation s_dumpGameSnapshot[4096] = {};
static void *s_dumpSelectedSnapshot[4096] = {};
static EiemMeshObservation s_dumpGuiSnapshot[4096] = {};

struct EiemDisabledRendererEntry {
  void *renderer;
  bool originalEnabled;
};
static EiemDisabledRendererEntry s_dumpDisabledRenderers[4096] = {};
static size_t s_dumpDisabledRendererCount = 0;

static void TraceSkinnedMeshSetSharedMesh(void *self, void *mesh,
                                           void *methodInfo);
static void TraceMeshFilterSetSharedMesh(void *self, void *mesh,
                                          void *methodInfo);

static void EiemSetDumpStatus(const char *status) {
  AcquireSRWLockExclusive(&s_dumpStatusLock);
  strncpy_s(s_dumpStatus, sizeof(s_dumpStatus), status ? status : "", _TRUNCATE);
  ReleaseSRWLockExclusive(&s_dumpStatusLock);
}

static void EiemGetDumpStatus(char *out, size_t capacity) {
  if (!out || capacity == 0) return;
  AcquireSRWLockShared(&s_dumpStatusLock);
  strncpy_s(out, capacity, s_dumpStatus, _TRUNCATE);
  ReleaseSRWLockShared(&s_dumpStatusLock);
}

static bool EiemContainsText(const char *value, const char *query) {
  if (!query || !query[0]) return true;
  if (!value) return false;
  const size_t queryLength = strlen(query);
  for (const char *start = value; *start; ++start) {
    size_t i = 0;
    while (i < queryLength && start[i] &&
           tolower((unsigned char)start[i]) ==
               tolower((unsigned char)query[i]))
      ++i;
    if (i == queryLength) return true;
  }
  return false;
}

static void EiemWriteJsonString(FILE *file, const char *text);

static void EiemExtractObjectName(const char *description, char *out,
                                  size_t outSize) {
  if (!out || outSize == 0) return;
  out[0] = '\0';
  if (!description) return;
  const char *marker = strstr(description, "name=\"");
  if (!marker) return;
  marker += 6;
  const char *end = strchr(marker, '\"');
  if (!end || end <= marker) return;
  const size_t length = (size_t)(end - marker) < outSize - 1
                            ? (size_t)(end - marker)
                            : outSize - 1;
  memcpy(out, marker, length);
  out[length] = '\0';
}

static int EiemExtractLod(const char *hierarchy) {
  if (!hierarchy) return -1;
  for (const char *p = hierarchy; *p; ++p) {
    if ((p == hierarchy || p[-1] == '/' || p[-1] == '_') &&
        p[0] == 'l' && p[1] == 'o' && p[2] == 'd' &&
        p[3] >= '0' && p[3] <= '9')
      return p[3] - '0';
  }
  return -1;
}

static int EiemExtractLodFromName(const char *name) {
  if (!name) return -1;
  for (const char *p = name; *p; ++p) {
    if (p[0] == '_' && p[1] == 'l' && p[2] == 'o' && p[3] == 'd' &&
        p[4] >= '0' && p[4] <= '9')
      return p[4] - '0';
  }
  return -1;
}

static unsigned long long EiemSceneIdentityHash(const char *rendererType,
                                                const char *hierarchy,
                                                const char *meshName) {
  unsigned long long hash = 1469598103934665603ull;
  const char *parts[] = {rendererType ? rendererType : "", "|",
                         hierarchy ? hierarchy : "", "|",
                         meshName ? meshName : ""};
  for (const char *part : parts) {
    for (const unsigned char *p = (const unsigned char *)part; *p; ++p) {
      hash ^= *p;
      hash *= 1099511628211ull;
    }
  }
  return hash;
}

static int EiemManagedArrayCount(void *array) {
  if (!array) return -1;
  __try {
    const int count = *(int *)((char *)array + 24);
    return count >= 0 && count <= 4096 ? count : -1;
  } __except (1) {
    return -1;
  }
}

static void *EiemManagedArrayItem(void *array, int index) {
  if (!array || index < 0) return nullptr;
  __try {
    const int count = *(int *)((char *)array + 24);
    if (index >= count) return nullptr;
    return *(void **)((char *)array + 32 + ((size_t)index * sizeof(void *)));
  } __except (1) {
    return nullptr;
  }
}

static void EiemReadObjectLabel(void *object, char *out, size_t outSize) {
  if (!out || outSize == 0) return;
  out[0] = '\0';
  if (!object) return;
  if (g_object_get_name)
    ReadStrUtf8(Invoke(g_object_get_name, object), out, (int)outSize);
  if (!out[0]) TraceDescribeObject(object, out, (int)outSize);
}

static void EiemWriteAssetOrigin(FILE *file, void *object) {
  int64_t pathHash = 0;
  char container[768] = {};
  const bool found =
      TraceLookupAssetOrigin(object, &pathHash, container, sizeof(container));
  if (pathHash)
    fprintf(file, ",\"runtimePathHash\":\"%lld\"", (long long)pathHash);
  const bool logicalPath = container[0] &&
                           (strchr(container, '/') || strchr(container, '\\')) &&
                           container[0] != '#';
  if (container[0]) {
    fprintf(file, ",\"runtimePath\":");
    EiemWriteJsonString(file, container);
  }
  if (logicalPath) {
    fprintf(file, ",\"container\":");
    EiemWriteJsonString(file, container);
  }
  fprintf(file, ",\"resolution\":\"%s\"",
          found && logicalPath ? "runtime-path" : "name-only");
}

static void EiemWriteMaterialDependencies(FILE *file, void *renderer,
                                          bool skinned) {
  if (!file) return;
  void *materials = renderer && g_renderer_get_sharedMaterials
                        ? Invoke(g_renderer_get_sharedMaterials, renderer)
                        : nullptr;
  const int materialCount = EiemManagedArrayCount(materials);
  fprintf(file, "\"materialCount\":%d,\"materials\":[", materialCount < 0 ? 0 : materialCount);
  for (int slot = 0; slot < (materialCount < 0 ? 0 : materialCount); ++slot) {
    if (slot) fputc(',', file);
    void *material = EiemManagedArrayItem(materials, slot);
    char materialName[256] = {};
    EiemReadObjectLabel(material, materialName, sizeof(materialName));
    fprintf(file, "{\"slot\":%d,\"type\":\"Material\",\"name\":", slot);
    EiemWriteJsonString(file, materialName[0] ? materialName : "<unnamed material>");
    EiemWriteAssetOrigin(file, material);
    fprintf(file, ",\"source\":\"offline-index-required\",\"shader\":");
    char shaderName[256] = {};
    if (material && g_material_get_shader)
      EiemReadObjectLabel(Invoke(g_material_get_shader, material), shaderName,
                          sizeof(shaderName));
    EiemWriteJsonString(file, shaderName[0] ? shaderName : "<unresolved>");
    fprintf(file, ",\"textures\":[");
    void *properties = material && g_material_GetTexturePropertyNames
                           ? Invoke(g_material_GetTexturePropertyNames, material)
                           : nullptr;
    const int propertyCount = EiemManagedArrayCount(properties);
    bool emittedTexture = false;
    for (int propertyIndex = 0;
         propertyIndex < (propertyCount < 0 ? 0 : propertyCount);
         ++propertyIndex) {
      void *property = EiemManagedArrayItem(properties, propertyIndex);
      char propertyName[192] = {};
      ReadStrUtf8(property, propertyName, sizeof(propertyName));
      if (!propertyName[0]) continue;
      void *propertyString = il2cpp_string_new
                                 ? il2cpp_string_new(propertyName)
                                 : nullptr;
      void *texture = nullptr;
      if (propertyString && material && g_material_GetTexture) {
        void *params[] = {propertyString};
        texture = Invoke(g_material_GetTexture, material, params);
      }
      char textureName[256] = {};
      EiemReadObjectLabel(texture, textureName, sizeof(textureName));
      if (emittedTexture) fputc(',', file);
      emittedTexture = true;
      fprintf(file, "{\"property\":");
      EiemWriteJsonString(file, propertyName);
      fprintf(file, ",\"type\":\"Texture\",\"name\":");
      EiemWriteJsonString(file, textureName[0] ? textureName : "<null>");
      EiemWriteAssetOrigin(file, texture);
      fprintf(file, ",\"source\":\"offline-index-required\"}");
    }
    fprintf(file, "]}");
  }
  fprintf(file, "],\"bones\":[");
  if (skinned && renderer && g_smr_get_bones) {
    void *bones = Invoke(g_smr_get_bones, renderer);
    const int boneCount = EiemManagedArrayCount(bones);
    bool emittedBone = false;
    for (int index = 0; index < (boneCount < 0 ? 0 : boneCount); ++index) {
      char boneName[192] = {};
      EiemReadObjectLabel(EiemManagedArrayItem(bones, index), boneName,
                          sizeof(boneName));
      if (!boneName[0]) continue;
      if (emittedBone) fputc(',', file);
      emittedBone = true;
      EiemWriteJsonString(file, boneName);
    }
  }
  fprintf(file, "]");
  if (skinned && renderer && g_smr_get_rootBone) {
    char rootBone[192] = {};
    EiemReadObjectLabel(Invoke(g_smr_get_rootBone, renderer), rootBone,
                        sizeof(rootBone));
    fprintf(file, ",\"rootBone\":");
    EiemWriteJsonString(file, rootBone);
  }
}

static bool EiemObservationMatchesFilter(const EiemMeshObservation &entry) {
  return EiemContainsText(entry.meshName, g_dumpSearch) ||
         EiemContainsText(entry.hierarchyPath, g_dumpSearch) ||
         EiemContainsText(entry.rendererName, g_dumpSearch);
}

static bool EiemIsMeshSelected(void *mesh) {
  if (!mesh) return false;
  AcquireSRWLockShared(&s_dumpSelectionLock);
  bool selected = false;
  for (size_t i = 0; i < s_dumpSelectedMeshCount; ++i) {
    if (s_dumpSelectedMeshes[i] == mesh) {
      selected = true;
      break;
    }
  }
  ReleaseSRWLockShared(&s_dumpSelectionLock);
  return selected;
}

static void EiemSetMeshSelected(void *mesh, bool selected) {
  if (!mesh) return;
  AcquireSRWLockExclusive(&s_dumpSelectionLock);
  size_t found = s_dumpSelectedMeshCount;
  for (size_t i = 0; i < s_dumpSelectedMeshCount; ++i) {
    if (s_dumpSelectedMeshes[i] == mesh) {
      found = i;
      break;
    }
  }
  if (selected && found == s_dumpSelectedMeshCount) {
    if (s_dumpSelectedMeshCount < _countof(s_dumpSelectedMeshes))
      s_dumpSelectedMeshes[s_dumpSelectedMeshCount++] = mesh;
  } else if (!selected && found < s_dumpSelectedMeshCount) {
    s_dumpSelectedMeshes[found] =
        s_dumpSelectedMeshes[--s_dumpSelectedMeshCount];
  }
  ReleaseSRWLockExclusive(&s_dumpSelectionLock);
}

static void EiemClearDumpSelection() {
  AcquireSRWLockExclusive(&s_dumpSelectionLock);
  s_dumpSelectedMeshCount = 0;
  ReleaseSRWLockExclusive(&s_dumpSelectionLock);
}

static void EiemSelectAllDumpMeshes(bool selected) {
  const size_t count = TraceCopyMeshObservations(
      s_dumpGuiSnapshot, _countof(s_dumpGuiSnapshot));
  AcquireSRWLockExclusive(&s_dumpSelectionLock);
  s_dumpSelectedMeshCount = 0;
  if (selected) {
    for (size_t i = 0; i < count && i < _countof(s_dumpSelectedMeshes); ++i)
      s_dumpSelectedMeshes[s_dumpSelectedMeshCount++] = s_dumpGuiSnapshot[i].mesh;
  }
  ReleaseSRWLockExclusive(&s_dumpSelectionLock);
}

static void EiemSelectFilteredDumpMeshes(bool selected) {
  const size_t count = TraceCopyMeshObservations(
      s_dumpGuiSnapshot, _countof(s_dumpGuiSnapshot));
  AcquireSRWLockExclusive(&s_dumpSelectionLock);
  if (!selected) {
    for (size_t i = 0; i < count; ++i) {
      if (!EiemObservationMatchesFilter(s_dumpGuiSnapshot[i])) continue;
      for (size_t j = 0; j < s_dumpSelectedMeshCount; ++j) {
        if (s_dumpSelectedMeshes[j] == s_dumpGuiSnapshot[i].mesh) {
          s_dumpSelectedMeshes[j] =
              s_dumpSelectedMeshes[--s_dumpSelectedMeshCount];
          break;
        }
      }
    }
  } else {
    for (size_t i = 0; i < count &&
                       s_dumpSelectedMeshCount < _countof(s_dumpSelectedMeshes);
         ++i) {
      if (!EiemObservationMatchesFilter(s_dumpGuiSnapshot[i])) continue;
      bool exists = false;
      for (size_t j = 0; j < s_dumpSelectedMeshCount; ++j)
        if (s_dumpSelectedMeshes[j] == s_dumpGuiSnapshot[i].mesh) {
          exists = true;
          break;
        }
      if (!exists) s_dumpSelectedMeshes[s_dumpSelectedMeshCount++] =
          s_dumpGuiSnapshot[i].mesh;
    }
  }
  ReleaseSRWLockExclusive(&s_dumpSelectionLock);
}

static bool EiemIsRendererDisabled(void *renderer, size_t *index = nullptr) {
  for (size_t i = 0; i < s_dumpDisabledRendererCount; ++i) {
    if (s_dumpDisabledRenderers[i].renderer == renderer) {
      if (index) *index = i;
      return true;
    }
  }
  return false;
}

static void EiemRestoreDisabledRenderers() {
  if (g_renderer_set_enabled) {
    for (size_t i = 0; i < s_dumpDisabledRendererCount; ++i) {
      bool enabled = s_dumpDisabledRenderers[i].originalEnabled;
      void *params[] = {&enabled};
      Invoke(g_renderer_set_enabled, s_dumpDisabledRenderers[i].renderer,
             params);
    }
  }
  s_dumpDisabledRendererCount = 0;
  memset(s_dumpDisabledRenderers, 0, sizeof(s_dumpDisabledRenderers));
}

static void EiemApplyDisabledRenderers() {
  if (!g_renderer_set_enabled) return;
  const size_t count = TraceCopyMeshObservations(
      s_dumpGameSnapshot, _countof(s_dumpGameSnapshot));
  for (size_t i = 0; i < count; ++i) {
    const bool shouldDisable = EiemIsMeshSelected(
        s_dumpGameSnapshot[i].mesh);
    for (uint32_t r = 0; r < s_dumpGameSnapshot[i].rendererCount; ++r) {
      void *renderer = s_dumpGameSnapshot[i].renderers[r];
      size_t disabledIndex = 0;
      const bool isDisabled = EiemIsRendererDisabled(renderer, &disabledIndex);
      if (shouldDisable && !isDisabled) {
        bool originalEnabled = true;
        if (g_renderer_get_enabled) {
          void *boxed = Invoke(g_renderer_get_enabled, renderer);
          if (boxed) {
            __try { originalEnabled = *(bool *)((char *)boxed + 16); }
            __except (1) { originalEnabled = true; }
          }
        }
        bool disabled = false;
        void *params[] = {&disabled};
        Invoke(g_renderer_set_enabled, renderer, params);
        if (s_dumpDisabledRendererCount < _countof(s_dumpDisabledRenderers))
          s_dumpDisabledRenderers[s_dumpDisabledRendererCount++] =
              {renderer, originalEnabled};
      } else if (!shouldDisable && isDisabled) {
        bool enabled = s_dumpDisabledRenderers[disabledIndex].originalEnabled;
        void *params[] = {&enabled};
        Invoke(g_renderer_set_enabled, renderer, params);
        s_dumpDisabledRenderers[disabledIndex] =
            s_dumpDisabledRenderers[--s_dumpDisabledRendererCount];
      }
    }
  }
  Log("[DUMP] Disabled Renderer count: %zu", s_dumpDisabledRendererCount);
}

static void EiemWriteJsonString(FILE *file, const char *text) {
  if (!file) return;
  fputc('"', file);
  const unsigned char *p = (const unsigned char *)(text ? text : "");
  for (; *p; ++p) {
    switch (*p) {
    case '"': fputs("\\\"", file); break;
    case '\\': fputs("\\\\", file); break;
    case '\b': fputs("\\b", file); break;
    case '\f': fputs("\\f", file); break;
    case '\n': fputs("\\n", file); break;
    case '\r': fputs("\\r", file); break;
    case '\t': fputs("\\t", file); break;
    default:
      if (*p < 0x20) fprintf(file, "\\u%04x", (unsigned)*p);
      else fputc(*p, file);
      break;
    }
  }
  fputc('"', file);
}

static bool EiemEnsureDirectory(const char *path) {
  if (!path || !path[0]) return false;
  char buffer[512] = {};
  strncpy_s(buffer, sizeof(buffer), path, _TRUNCATE);
  for (char *p = buffer; *p; ++p) {
    if (*p != '\\' && *p != '/') continue;
    if (p == buffer || (p == buffer + 2 && buffer[1] == ':')) continue;
    char saved = *p;
    *p = '\0';
    CreateDirectoryA(buffer, nullptr);
    *p = saved;
  }
  CreateDirectoryA(buffer, nullptr);
  const DWORD attr = GetFileAttributesA(buffer);
  return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY);
}

static void EiemCopySelection(void **out, size_t capacity, size_t *count) {
  if (!out || !count) return;
  AcquireSRWLockShared(&s_dumpSelectionLock);
  const size_t n = s_dumpSelectedMeshCount < capacity ? s_dumpSelectedMeshCount
                                                       : capacity;
  if (n) memcpy(out, s_dumpSelectedMeshes, n * sizeof(void *));
  ReleaseSRWLockShared(&s_dumpSelectionLock);
  *count = n;
}

static bool EiemPointerInList(void *value, void **list, size_t count) {
  for (size_t i = 0; i < count; ++i)
    if (list[i] == value) return true;
  return false;
}

static void EiemDumpObservedMeshes(bool full) {
  if (InterlockedCompareExchange(&s_dumpInProgress, 1, 0) != 0) {
    EiemSetDumpStatus("Dump already in progress");
    return;
  }

  EiemSetDumpStatus("Collecting observed meshes...");
  EiemMeshObservation *observations = s_dumpGameSnapshot;
  const size_t observationCount =
      TraceCopyMeshObservations(observations, _countof(s_dumpGameSnapshot));
  void **selected = s_dumpSelectedSnapshot;
  size_t selectedCount = 0;
  EiemCopySelection(selected, _countof(s_dumpSelectedSnapshot), &selectedCount);


  char directory[512] = {};
  strncpy_s(directory, sizeof(directory), g_dumpOutputDir, _TRUNCATE);
  if (!EiemEnsureDirectory(directory)) {
    EiemSetDumpStatus("Cannot create output directory");
    InterlockedExchange(&s_dumpInProgress, 0);
    return;
  }

  char finalPath[768] = {};
  snprintf(finalPath, sizeof(finalPath), "%s\\scene_dump_%s.json", directory,
           full ? "full" : "current");
  char tempPath[800] = {};
  snprintf(tempPath, sizeof(tempPath), "%s.tmp", finalPath);
  FILE *file = fopen(tempPath, "wb");
  if (!file) {
    EiemSetDumpStatus("Cannot open dump output");
    InterlockedExchange(&s_dumpInProgress, 0);
    return;
  }

  const ULONGLONG now = GetTickCount64();
  fprintf(file, "{\n  \"schema\": 2,\n  \"kind\": ");
  EiemWriteJsonString(file, full ? "full_observed_mesh_dump"
                                 : "current_selected_mesh_dump");
  fprintf(file,
          ",\n  \"dependencyMode\": \"runtime-renderer-references\","
          "\n  \"capturedAtMs\": %llu,\n  \"meshCount\": ",
          (unsigned long long)now);
  size_t written = 0;
  for (size_t i = 0; i < observationCount; ++i) {
    if (!full && !EiemPointerInList(observations[i].mesh, selected,
                                    selectedCount))
      continue;
    ++written;
  }
  fprintf(file, "%zu,\n  \"meshes\": [\n", written);
  size_t emitted = 0;
  for (size_t i = 0; i < observationCount; ++i) {
    if (!full && !EiemPointerInList(observations[i].mesh, selected,
                                    selectedCount))
      continue;
    char lookupName[192] = {};
    EiemExtractObjectName(observations[i].meshName, lookupName,
                          sizeof(lookupName));
    int lod = EiemExtractLod(observations[i].hierarchyPath);
    if (lod < 0)
      lod = EiemExtractLodFromName(lookupName[0] ? lookupName
                                                   : observations[i].meshName);
    const unsigned long long identity = EiemSceneIdentityHash(
        observations[i].rendererType, observations[i].hierarchyPath,
        lookupName[0] ? lookupName : observations[i].meshName);
    fprintf(file, "    {\"rendererType\": ");
    EiemWriteJsonString(file, observations[i].rendererType);
    fprintf(file, ", \"renderer\": ");
    EiemWriteJsonString(file, observations[i].rendererName);
    fprintf(file, ", \"mesh\": ");
    EiemWriteJsonString(file, observations[i].meshName);
    fprintf(file, ", \"hierarchy\": ");
    EiemWriteJsonString(file, observations[i].hierarchyPath);
    fprintf(file, ", \"lookupType\": \"Mesh\", \"lookupName\": ");
    EiemWriteJsonString(file, lookupName[0] ? lookupName : observations[i].meshName);
    fprintf(file, ", \"lod\": %d, \"identityHash\": \"%016llx\", \"source\": \"runtime-observation\"",
            lod, identity);
    EiemWriteAssetOrigin(file, observations[i].mesh);
    fprintf(file, ", ");
    EiemWriteMaterialDependencies(
        file, observations[i].renderer,
        strcmp(observations[i].rendererType, "SkinnedMeshRenderer") == 0);
    fprintf(file, ", \"instanceCount\": %u, \"nativeMesh\": \"%p\", \"nativeRenderer\": \"%p\"}%s\n",
            observations[i].instanceCount,
            observations[i].mesh, observations[i].renderer,
            (++emitted < written) ? "," : "");
  }
  fprintf(file, "  ],\n  \"bundles\": [\n");
  AcquireSRWLockShared(&s_bundleManifestLock);
  for (size_t i = 0; i < s_bundleManifestEntries.size(); ++i) {
    fprintf(file, "    {\"path\": ");
    EiemWriteJsonString(file, s_bundleManifestEntries[i].path.c_str());
    fprintf(file, "}%s\n",
            i + 1 < s_bundleManifestEntries.size() ? "," : "");
  }
  const size_t bundleCount = s_bundleManifestEntries.size();
  ReleaseSRWLockShared(&s_bundleManifestLock);
  fprintf(file, "  ],\n  \"bundleCount\": %zu\n}\n", bundleCount);

  const bool ok = fflush(file) == 0;
  fclose(file);
  if (!ok || !MoveFileExA(tempPath, finalPath,
                          MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    DeleteFileA(tempPath);
    EiemSetDumpStatus("Dump write failed");
  } else {
    char status[256] = {};
    snprintf(status, sizeof(status), "Wrote %zu mesh records to %s", written,
             finalPath);
    EiemSetDumpStatus(status);
    Log("[DUMP] %s", status);
  }
  InterlockedExchange(&s_dumpInProgress, 0);
}

// Automatic, read-only census used for transient afterimage investigation.
// It runs after the game's renderer registry is built, so it does not depend
// on F10/Insert registration and does not alter any Renderer or Mesh state.
static volatile LONG s_eiemAutoSceneCensusCount = 0;
static ULONGLONG s_eiemAutoSceneCensusLastMs = 0;

static bool EiemSceneCensusLooksLikeLizhiyan(const EiemMeshObservation &entry) {
  return EiemContainsText(entry.rendererName, "lizhiyan") ||
         EiemContainsText(entry.meshName, "lizhiyan") ||
         EiemContainsText(entry.hierarchyPath, "lizhiyan");
}

static void EiemWriteAutoSceneCensus(const EiemMeshObservation *rows,
                                     size_t rowCount, LONG attempt) {
  char directory[512] = {};
  strncpy_s(directory, sizeof(directory), g_dumpOutputDir, _TRUNCATE);
  if (!EiemEnsureDirectory(directory)) {
    Log("[SCENE-LIZHIYAN] cannot create output directory=%s", directory);
    return;
  }
  char finalPath[768] = {};
  snprintf(finalPath, sizeof(finalPath), "%s\\lizhiyan_scene_census.json",
           directory);
  char tempPath[800] = {};
  snprintf(tempPath, sizeof(tempPath), "%s.tmp", finalPath);
  FILE *file = fopen(tempPath, "wb");
  if (!file) {
    Log("[SCENE-LIZHIYAN] cannot open output=%s", finalPath);
    return;
  }
  fprintf(file, "{\n  \"schema\": 1,\n  \"kind\": \"automatic_scene_mesh_census\",\n"
                "  \"attempt\": %ld,\n  \"capturedAtMs\": %llu,\n"
                "  \"meshCount\": %zu,\n  \"meshes\": [\n",
          attempt, (unsigned long long)GetTickCount64(), rowCount);
  for (size_t i = 0; i < rowCount; ++i) {
    const auto &entry = rows[i];
    if (i) fputs(",\n", file);
    fprintf(file, "    {\"rendererType\":");
    EiemWriteJsonString(file, entry.rendererType);
    fprintf(file, ",\"renderer\":");
    EiemWriteJsonString(file, entry.rendererName);
    fprintf(file, ",\"mesh\":");
    EiemWriteJsonString(file, entry.meshName);
    fprintf(file, ",\"hierarchy\":");
    EiemWriteJsonString(file, entry.hierarchyPath);
    fprintf(file, ",\"lizhiyanHint\":%s,\"instanceCount\":%u,"
                  "\"firstSeenMs\":%llu,\"lastSeenMs\":%llu,"
                  "\"originHash\":%lld,\"originPath\":",
            EiemSceneCensusLooksLikeLizhiyan(entry) ? "true" : "false",
            entry.instanceCount, (unsigned long long)entry.firstSeenMs,
            (unsigned long long)entry.lastSeenMs,
            (long long)entry.originHash);
    EiemWriteJsonString(file, entry.originPath);
    fprintf(file, ",\"vertexCount\":%d,\"subMeshCount\":%d,\"indexCounts\":[",
            entry.vertexCount, entry.subMeshCount);
    const int32_t indexCountLimit =
        entry.subMeshCount >= 0 && entry.subMeshCount <= 16
            ? entry.subMeshCount
            : 0;
    for (int32_t submesh = 0; submesh < indexCountLimit; ++submesh) {
      if (submesh) fputs(",", file);
      fprintf(file, "%d", entry.indexCounts[submesh]);
    }
    fprintf(file, "],\"materialCount\":%d,\"nativeRenderer\":\"%p\","
                  "\"nativeMesh\":\"%p\"}",
            entry.materialCount, entry.renderer, entry.mesh);
  }
  fputs("\n  ]\n}\n", file);
  const bool ok = fflush(file) == 0;
  fclose(file);
  if (!ok || !MoveFileExA(tempPath, finalPath,
                          MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    DeleteFileA(tempPath);
    Log("[SCENE-LIZHIYAN] write failed output=%s", finalPath);
    return;
  }
  Log("[SCENE-LIZHIYAN] wrote attempt=%ld rows=%zu output=%s", attempt,
      rowCount, finalPath);
}

static void EiemAutoSceneCensus() {
  // A registry boundary or the bounded window timer may call this repeatedly.
  // Keep one Unity-wide enumeration per second and stop after one minute.
  const ULONGLONG now = GetTickCount64();
  if (s_eiemAutoSceneCensusLastMs && now - s_eiemAutoSceneCensusLastMs < 1000)
    return;
  s_eiemAutoSceneCensusLastMs = now;
  const LONG attempt = InterlockedIncrement(&s_eiemAutoSceneCensusCount);
  if (attempt > 60) {
    if (g_gameHwnd && IsWindow(g_gameHwnd))
      KillTimer(g_gameHwnd, kEiemAutoSceneCensusTimer);
    return;
  }

  // Retain the union of all observations. A one-frame attack afterimage may
  // disappear before the next sample, so clearing here would lose it.
  TraceEnumerateRendererType(g_skinnedMeshRendererClass,
                             "SkinnedMeshRenderer");
  TraceEnumerateRendererType(g_meshFilterClass, "MeshFilter");
  const size_t rowCount = TraceCopyMeshObservations(
      s_dumpGameSnapshot, _countof(s_dumpGameSnapshot));
  size_t lizhiyanCount = 0;
  for (size_t i = 0; i < rowCount; ++i) {
    if (!EiemSceneCensusLooksLikeLizhiyan(s_dumpGameSnapshot[i])) continue;
    ++lizhiyanCount;
    Log("[SCENE-LIZHIYAN] attempt=%ld type=%s renderer=%s mesh=%s hierarchy=%s",
        attempt, s_dumpGameSnapshot[i].rendererType,
        s_dumpGameSnapshot[i].rendererName, s_dumpGameSnapshot[i].meshName,
        s_dumpGameSnapshot[i].hierarchyPath);
  }
  EiemWriteAutoSceneCensus(s_dumpGameSnapshot, rowCount, attempt);
  Log("[SCENE-LIZHIYAN] attempt=%ld rows=%zu lizhiyanHints=%zu", attempt,
      rowCount, lizhiyanCount);
  if (attempt >= 60 && g_gameHwnd && IsWindow(g_gameHwnd))
    KillTimer(g_gameHwnd, kEiemAutoSceneCensusTimer);
}

static void EiemRequestDump(bool full) {
  if (!g_gameHwnd || !IsWindow(g_gameHwnd)) {
    EiemSetDumpStatus("Game window is not available");
    return;
  }
  EiemSetDumpStatus(full ? "Full dump queued" : "Current dump queued");
  PostMessageW(g_gameHwnd, full ? WM_EIEM_DUMP_FULL : WM_EIEM_DUMP_CURRENT, 0,
               0);
}

static void EiemRequestDumpRefresh() {
  if (!g_gameHwnd || !IsWindow(g_gameHwnd)) {
    EiemSetDumpStatus("Game window is not available");
    return;
  }
  EiemSetDumpStatus("Refreshing current scene meshes...");
  PostMessageW(g_gameHwnd, WM_EIEM_DUMP_REFRESH, 0, 0);
}

static void EiemRequestDisabledRefresh() {
  if (!g_gameHwnd || !IsWindow(g_gameHwnd)) return;
  PostMessageW(g_gameHwnd, WM_EIEM_DUMP_DISABLED, 0, 0);
}

static void EiemRequestModelExport(bool full) {
  if (!g_gameHwnd || !IsWindow(g_gameHwnd)) {
    EiemSetDumpStatus("Game window is not available");
    return;
  }
  EiemSetDumpStatus(full ? "Full model export queued"
                         : "Current model export queued");
  PostMessageW(g_gameHwnd,
               full ? WM_EIEM_MODEL_FULL : WM_EIEM_MODEL_CURRENT, 0, 0);
}
