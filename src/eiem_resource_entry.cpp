#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX
#include <fcntl.h>
#include <io.h>
#include <share.h>
#include <windows.h>

#include <atomic>
#include <condition_variable>
#include <cstdarg>
#include <cstdio>
#include <deque>
#include <mutex>
#include <set>

#include "eiem_mesh_input_builder.h"
#include "eiem_resource_reload_plan.h"
#include "eiem_offline_resource_package.h"
#include "eiem_compiled_mod_set.h"
#include "eiem_prefab_resource_rules.h"
#include "eiem_loading_input_substitution.h"
#include "eiem_resource_directory.h"
#include "il2cpp_api.h"
#include "eiem_capture_skin_palette.h"
#include "eiem_entry_settings.h"
#include "eiem_camera_fade_core.h"
#include "eiem_package_protocol.h"
#include "eiem_control_panel_win32.h"
#if defined(EIEM_SHAPE_CAPTURE_GFXTRACE)
#include "../deps/renderdoc/renderdoc_app.h"
#endif
#define APPLEPIE_PLUGIN_IMPL
#include "applepie_mgr.h"

// Base resources are replaced only through serialized loading inputs. The
// optional shape path copies geometry for the capture consumer, through public
// Mesh bindings; it does not include retired base replacement/private fields.
namespace Entry {
using namespace EiemDirectory;
static_assert(sizeof(void *) == 8,
              "Resource entry observers require Windows x64");
FILE *OpenLog(const std::filesystem::path &path, bool exclusive = false) {
  // Readers can follow flushed output. Competing writers must fail before
  // truncation, which would otherwise leave holes at our existing file offset.
  HANDLE handle = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ,
                              nullptr, exclusive ? CREATE_NEW : CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
  if (handle == INVALID_HANDLE_VALUE)
    return nullptr;
  int descriptor = _open_osfhandle((intptr_t)handle, _O_WRONLY | _O_BINARY);
  if (descriptor < 0) {
    CloseHandle(handle);
    return nullptr;
  }
  auto file = _fdopen(descriptor, "wb");
  if (!file)
    _close(descriptor);
  return file;
}
struct ManagedAttachment {
  void *thread = nullptr;
  ManagedAttachment() = default;
  ManagedAttachment(const ManagedAttachment &) = delete;
  ManagedAttachment &operator=(const ManagedAttachment &) = delete;
  void Attach(void *domain) {
    if (thread)
      return;
    Require(domain && il2cpp_thread_attach && il2cpp_thread_detach,
            "managed thread lifetime exports unavailable");
    thread = il2cpp_thread_attach(domain);
    Require(thread != nullptr, "IL2CPP thread attachment failed");
  }
  void Detach() {
    if (!thread)
      return;
    auto attached = thread;
    thread = nullptr;
    il2cpp_thread_detach(attached);
  }
  ~ManagedAttachment() { Detach(); }
};
std::mutex logMutex;
FILE *logFile = nullptr;
FILE *logMirror = nullptr;
std::filesystem::path gameDirectory, sessionDirectory;
std::atomic_bool active{true}, reload{false};
std::atomic<uint64_t> reloadClock{0};
std::atomic<unsigned> generation{0};
EiemEntrySettings settings;
void CommitPendingReload();
namespace ReloadObservation {
uint64_t Clock();
void Asset(void*,void*,void*,uint64_t);
void Frame(int);
bool ShareMaterialInit();
void Material(void*,void*,void*,void*,bool,void*);
}
namespace LoadingObservation {
bool Active();
bool NativeStringObserved();
void Submitted(void*,const std::string&,unsigned,bool,const std::string&);
void Completed(void*,void*,const char*);
void Asset(void*,void*,const char*,void*,int64_t,void*,uint64_t);
void Unloaded(void*);
void Frame(int);
}
std::shared_ptr<const std::vector<Package>> published;
std::shared_ptr<const EiemNativeInput::InputSubstitutions> substitutions;
std::filesystem::path initialInputTable;
std::shared_ptr<EiemPrefabRules::Context> resourceRules;
std::shared_ptr<const EiemCompiledModSet::Set> preparedCompiledSet;
void* LoadRuleInput(void* path,uint32_t crc,uint64_t offset,bool asynchronous,bool& matched);
void CompleteRuleInput(void* request,void* bundle);
std::shared_ptr<EiemNativeInput::InputSubstitutions> pendingSubstitutions;
std::atomic<unsigned long long> submittedInputs{0}, completedInputs{0}, loadedAssets{0};
std::mutex submissionMutex;
std::map<void*, std::string> submittedRequests, submittedBundles;
std::atomic<unsigned long long> vfsCalls{0}, readCalls{0}, memoryCalls{0},
    fileCalls{0}, publicFileCalls{0}, streamCalls{0};
std::atomic<unsigned long long> helperCalls[6]{};
std::atomic<unsigned> publicFileSamples[2]{};
#if defined(EIEM_DIAGNOSTICS)
std::atomic<unsigned> fileSnapshotRequests{0}, fileSnapshots{0}, captureIds{0};
using SnapshotKey = std::pair<std::string, uint64_t>;
std::map<SnapshotKey, size_t> selectedSnapshots;
std::set<SnapshotKey> queuedSnapshots;
std::mutex snapshotMutex;
unsigned genericSnapshots = 0;
std::string SnapshotPath(std::string path) {
  std::replace(path.begin(), path.end(), '\\', '/');
  std::transform(path.begin(), path.end(), path.begin(), [](unsigned char ch) {
    return char(ch >= 'A' && ch <= 'Z' ? ch + ('a' - 'A') : ch);
  });
  return path;
}
void LoadSnapshotSelection() {
  // Temporary diagnostic selection, outside Mod contents. It selects copies
  // only; actual resource matches still use the decoded path + asset identity.
  auto path = gameDirectory / "plugin" / "resource-input-diagnostics" /
              "snapshot-ranges.tsv";
  if (!std::filesystem::exists(path))
    return;
  auto bytes = Read(path);
  Require(bytes.size() <= 128 * 1024, "diagnostic range selection too large");
  std::istringstream lines(std::string(bytes.begin(), bytes.end()));
  std::string line;
  Require(bool(std::getline(lines, line)) &&
              line == "EFF_DIAGNOSTIC_INPUT_RANGES\t1",
          "unsupported diagnostic range selection");
  std::map<SnapshotKey, size_t> selection;
  while (std::getline(lines, line)) {
    auto first = line.find('\t'), second = first == std::string::npos
                                               ? first
                                               : line.find('\t', first + 1);
    Require(first != std::string::npos && second != std::string::npos &&
                line.find('\t', second + 1) == std::string::npos,
            "invalid diagnostic range selection row");
    auto physical = line.substr(0, first);
    auto offsetText = line.substr(first + 1, second - first - 1),
         sizeText = line.substr(second + 1);
    Require(!offsetText.empty() && !sizeText.empty() &&
                offsetText.find_first_not_of("0123456789") ==
                    std::string::npos &&
                sizeText.find_first_not_of("0123456789") == std::string::npos,
            "invalid diagnostic range offset/size");
    uint64_t offset = std::stoull(offsetText);
    size_t size = size_t(std::stoull(sizeText));
    auto normalized = SnapshotPath(physical);
    auto root = SnapshotPath(gameDirectory.u8string()) + "/";
    Require(std::filesystem::u8path(physical).is_absolute() &&
                normalized.rfind(root, 0) == 0 &&
                normalized.find("/../") == std::string::npos && size >= 48 &&
                size <= 64ull * 1024 * 1024 && selection.size() < 8,
            "invalid diagnostic selection path/size/count");
    Require(selection.emplace(SnapshotKey{normalized, offset}, size).second,
            "duplicate diagnostic range selection");
  }
  selectedSnapshots = std::move(selection);
  Log("[ENTRY-SNAPSHOT-SELECTION] selected=%zu genericLimit=4 totalLimit=12 "
      "purpose=diagnostic-copies replacementIdentity=decoded-path-and-asset",
      selectedSnapshots.size());
}
#endif
std::mutex samplesMutex;
std::set<std::pair<uintptr_t, unsigned>> sampled;
std::atomic<unsigned> inputSamples{0}, pathSamples{0};
std::atomic<unsigned> hashLoadSamples{0}, hashStreamSamples{0};
std::atomic<unsigned> streamSamples{0};
thread_local const char *bundleContext = nullptr;
constexpr const char *vfsPathType = "Beyond.Resource.StringPathHash";
std::mutex hashMutex;
std::map<int64_t, std::string> hashPaths;
struct Context {
  const char *previous;
  explicit Context(const char *path) : previous(bundleContext) {
    bundleContext = path;
  }
  ~Context() { bundleContext = previous; }
};
#if defined(EIEM_DIAGNOSTICS)
constexpr size_t captureLimit = 64ull * 1024 * 1024,
                 queueLimit = 128ull * 1024 * 1024;
struct Job {
  std::string origin, kind;
  std::filesystem::path physicalPath;
  uint64_t offset = 0;
  size_t expectedBytes = 0;
  Bytes bytes;
  unsigned generation = 0, id = 0;
  std::shared_ptr<const std::vector<Package>> graph;
};
std::mutex queueMutex;
std::deque<Job> jobs;
size_t queuedBytes = 0;
#endif
HANDLE queueEvent = nullptr;
#if defined(EIEM_DIAGNOSTICS)
std::atomic<unsigned> captures{0};
#endif

bool CopyRaw(const void *input, void *output, size_t bytes) {
  if (!input || !output || uintptr_t(input) > UINTPTR_MAX - bytes)
    return false;
  __try {
    memcpy(output, input, bytes);
    return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}
bool ArrayLength(void *array, size_t *length) {
  uint64_t n = 0;
  if (!array || !CopyRaw((char *)array + 24, &n, 8))
    return false;
  *length = (size_t)n;
  return true;
}
std::string String(void *input) {
  char text[2048] = {};
  int n = ReadStrUtf8(input, text, sizeof(text));
  return n >= 0 ? std::string(text) : std::string("<unreadable-string>");
}
void Inventory(void *klass, const char *label) {
  if (!klass) {
    Log("[ENTRY-META] class=%s missing", label);
    return;
  }
  void *iterator = nullptr;
  while (void *method = il2cpp_class_get_methods(klass, &iterator)) {
    const char *name = il2cpp_method_get_name(method);
    if (!name)
      continue;
    // Only the load-input surface, never a dump of all classes/resources.
    if (!strstr(name, "Load") && !strstr(name, "Stream") &&
        strcmp(name, "Read") && strcmp(name, "get_Position") &&
        strcmp(name, "get_Length"))
      continue;
    std::string signature;
    unsigned count = il2cpp_method_get_param_count(method);
    for (unsigned i = 0; i < count; ++i) {
      auto type = il2cpp_method_get_param(method, i);
      auto text = type ? il2cpp_type_get_name(type) : nullptr;
      if (i)
        signature += ",";
      signature += text ? text : "?";
      if (text)
        il2cpp_free((void *)text);
    }
    auto type = il2cpp_method_get_return_type(method);
    auto result = type ? il2cpp_type_get_name(type) : nullptr;
    uint32_t impl = 0, flags = il2cpp_method_get_flags(method, &impl);
    void *address = ((MInfo *)method)->mp;
    Log("[ENTRY-META] %s.%s(%s)->%s static=%d address=%p rva=%llX", label, name,
        signature.c_str(), result ? result : "?", !!(flags & 0x10), address,
        (unsigned long long)(address ? (uintptr_t)address - (uintptr_t)hGA
                                     : 0));
    if (result)
      il2cpp_free((void *)result);
  }
}
void *Exact(void *klass, const char *name,
            std::initializer_list<const char *> arguments, const char *result,
            bool isStatic) {
  if (!klass)
    return nullptr;
  void *iterator = nullptr;
  void *found = nullptr;
  while (void *method = il2cpp_class_get_methods(klass, &iterator)) {
    const char *methodName = il2cpp_method_get_name(method);
    if (!methodName || strcmp(methodName, name) ||
        il2cpp_method_get_param_count(method) != arguments.size())
      continue;
    uint32_t impl = 0, flags = il2cpp_method_get_flags(method, &impl);
    if (bool(flags & 0x10) != isStatic)
      continue;
    auto type = il2cpp_method_get_return_type(method);
    auto text = type ? il2cpp_type_get_name(type) : nullptr;
    bool matches = text && strcmp(text, result) == 0;
    if (text)
      il2cpp_free((void *)text);
    unsigned i = 0;
    for (auto arg : arguments) {
      type = il2cpp_method_get_param(method, i++);
      text = type ? il2cpp_type_get_name(type) : nullptr;
      matches &= text && strcmp(text, arg) == 0;
      if (text)
        il2cpp_free((void *)text);
    }
    if (matches && ((MInfo *)method)->mp) {
      if (found) {
        Log("[ENTRY-HOOK] ambiguous signature %s; rejected", name);
        return nullptr;
      }
      found = method;
    }
  }
  return found;
}
bool HashAbi(void *klass) {
  if (!klass || !il2cpp_class_value_size || !il2cpp_class_get_type ||
      !il2cpp_type_get_type || !il2cpp_class_get_fields ||
      !il2cpp_field_get_flags || !il2cpp_field_get_type ||
      !il2cpp_field_get_offset)
    return false;
  uint32_t alignment = 0;
  auto size = il2cpp_class_value_size(klass, &alignment);
  auto type = il2cpp_class_get_type(klass);
  // Windows x64 passes an 8-byte aggregate in an integer register even when
  // packed. The actual game reports alignment=1, size=8 and one Int64 field.
  bool valid = type && il2cpp_type_get_type(type) == 0x11 && size == 8 &&
               (alignment == 1 || alignment == 8);
  unsigned fields = 0;
  void *iterator = nullptr;
  while (auto field = il2cpp_class_get_fields(klass, &iterator)) {
    if (il2cpp_field_get_flags(field) & 0x10)
      continue;
    ++fields;
    auto fieldType = il2cpp_field_get_type(field);
    auto name = fieldType ? il2cpp_type_get_name(fieldType) : nullptr;
    auto offset = il2cpp_field_get_offset(field);
    // IL2CPP reports value-type fields relative to the boxed object header.
    valid &=
        name &&
        (!strcmp(name, "System.Int64") || !strcmp(name, "System.UInt64")) &&
        offset == 16;
    Log("[ENTRY-ABI] StringPathHash field=%s offset=%zu", name ? name : "?",
        offset);
    if (name)
      il2cpp_free((void *)name);
  }
  valid &= fields == 1;
  Log("[ENTRY-ABI] StringPathHash size=%d alignment=%u instanceFields=%u "
      "supported=%d",
      size, alignment, fields, valid);
  return valid;
}
bool LoaderPosAbi(void *klass) {
  if (!klass || !il2cpp_class_value_size || !il2cpp_class_get_type ||
      !il2cpp_type_get_type || !il2cpp_class_get_fields ||
      !il2cpp_field_get_flags || !il2cpp_field_get_type ||
      !il2cpp_field_get_offset)
    return false;
  uint32_t alignment = 0;
  auto size = il2cpp_class_value_size(klass, &alignment);
  auto type = il2cpp_class_get_type(klass);
  bool valid =
      type && il2cpp_type_get_type(type) == 0x11 && size == 1 && alignment == 1;
  unsigned fields = 0;
  void *iterator = nullptr;
  while (auto field = il2cpp_class_get_fields(klass, &iterator)) {
    if (il2cpp_field_get_flags(field) & 0x10)
      continue;
    ++fields;
    auto fieldType = il2cpp_field_get_type(field);
    auto name = fieldType ? il2cpp_type_get_name(fieldType) : nullptr;
    valid &= name && !strcmp(name, "System.Byte") &&
             il2cpp_field_get_offset(field) == 16;
    if (name)
      il2cpp_free((void *)name);
  }
  valid &= fields == 1;
  Log("[ENTRY-ABI] EFileLoaderPosType size=%d alignment=%u fields=%u "
      "supported=%d",
      size, alignment, fields, valid);
  return valid;
}
std::set<void *> installed;
bool Install(void *klass, const char *name,
             std::initializer_list<const char *> args, const char *result,
             bool isStatic, void *detour, void **original) {
  auto method = Exact(klass, name, args, result, isStatic);
  if (!method) {
    Log("[ENTRY-HOOK] absent or incompatible %s argc=%zu static=%d", name,
        args.size(), isStatic);
    return false;
  }
  auto address = ((MInfo *)method)->mp;
  if (installed.count(address)) {
    Log("[ENTRY-HOOK] duplicate native address rejected %s address=%p", name,
        address);
    return false;
  }
  auto status = MH_CreateHook(address, detour, original);
  if (status == MH_OK) {
    status = MH_EnableHook(address);
    if (status != MH_OK)
      MH_RemoveHook(address);
  }
  if (status != MH_OK) {
    *original = nullptr;
    Log("[ENTRY-HOOK] failed %s status=%d", name, (int)status);
    return false;
  }
  installed.insert(address);
  Log("[ENTRY-HOOK] installed %s static=%d address=%p", name, isStatic,
      address);
  return true;
}
std::vector<std::filesystem::path> DiscoverModRoots() {
  return EiemComposedPackage::Roots(gameDirectory/"plugin/mods");
}
bool InstalledAuthorsMatch(const std::vector<Package>& graph) {
 return EiemComposedPackage::Matches(gameDirectory/"plugin/mods",graph);
}
std::shared_ptr<const std::vector<Package>> ReadPackages() {
  auto next = std::make_shared<std::vector<Package>>();
  std::set<Identity> targets;
  std::set<std::string> ids;
  const auto roots = DiscoverModRoots();
  for (const auto &root : roots) {
    auto p = Load(root);
    Require(ids.insert(p.id).second, "duplicate Mod id: " + p.id);
    for (const auto &row : p.targetIndex)
      Require(targets.insert(row.first).second,
              "conflicting Mesh path+asset: " + row.first.path + " / " +
                  row.first.asset);
    size_t bytes = 0;
    for (const auto &r : p.resources)
      bytes += r.second->bytes.size();
    Log("[ENTRY-PACKAGE] validated id=%s meshes=%zu parts=%zu materials=%zu "
        "textures=%zu "
        "rules=%zu targets=%zu resources=%zu bytes=%zu",
        p.id.c_str(), p.meshes.size(), p.parts.size(), p.materials.size(),
        p.textures.size(), p.rules.size(), p.targetIndex.size(),
        p.resources.size(), bytes);
    next->push_back(std::move(p));
  }
  EiemComposedPackage::Compose(*next,true);return next;
}
bool initialComposed=false;
void LoadInitialPackages() {
  auto config=EiemEntrySettings::Read(gameDirectory);
  std::shared_ptr<const std::vector<Package>> next;
  auto cache=gameDirectory/"plugin/resource-reload-cache";
  auto roots=DiscoverModRoots();
  if(!roots.empty()&&std::all_of(roots.begin(),roots.end(),[](const auto& root){return std::filesystem::is_regular_file(root/"compiled.bin");})){
   try{preparedCompiledSet=EiemCompiledModSet::Read(gameDirectory);resourceRules=EiemCompiledModSet::Context(preparedCompiledSet);next=std::make_shared<const std::vector<Package>>(preparedCompiledSet->manifest.packages);initialComposed=true;Log("[ENTRY-PACKAGE] compiledStartup=1 authorCompilation=0 perModArtifacts=%zu key=%s",next->size(),preparedCompiledSet->pointer.key.c_str());}
   catch(const std::exception& e){Log("[ENTRY-STARTUP-PENDING] reason=%s compiledModPreparationFailed=1",e.what());}
  }
  if(!next&&std::filesystem::exists(cache/"current.tsv")){
   try{
    auto pointer=EiemOfflinePackage::ReadCurrent(cache);
    auto manifest=EiemOfflinePackage::ReadManifest(cache,pointer,gameDirectory/"plugin/mods");
    auto authors=EiemOfflinePackage::Authors(manifest);
    Require(InstalledAuthorsMatch(authors),"offline generation author set differs from installed Mods");
    initialInputTable=cache/"generations"/pointer.key/"loading-inputs.tsv";
    if(manifest.plan.rulesOnly){auto plan=std::make_shared<const EiemResourceReloadPlan::Plan>(std::move(manifest.plan));resourceRules=std::make_shared<EiemPrefabRules::Context>(plan,std::move(manifest.package));initialInputTable.clear();}
    else Require(std::filesystem::exists(initialInputTable),"latest offline generation lacks startup loading inputs");
    initialComposed=manifest.composed;
    next=std::make_shared<const std::vector<Package>>(std::move(authors));
    Log("[ENTRY-PACKAGE] compiledStartup=1 authorINIParsed=0 latestCompleteGeneration=1 key=%s",pointer.key.c_str());
   }catch(const std::exception& e){
    initialInputTable.clear();initialComposed=false;
    Log("[ENTRY-STARTUP-PENDING] reason=%s authorINIParsed=1 staticSourcePreparationFailed=1",e.what());
   }
  }
  if(!next)next=ReadPackages();
  for(const auto& p:*next) for(const auto& key:p.bindings)
    Require(key.key.vk!=config.reload.vk && key.key.vk!=config.gui.vk,"Mod key overlaps reserved global shortcut: "+p.id+"/"+key.id);
  settings.Publish(config);
  std::atomic_store(&published,next);
  ++generation;
  Log("[ENTRY-GENERATION] initial=%u mods=%zu",generation.load(),next->size());
}
void PrepareInputSubstitutions() {
  if(std::atomic_load(&resourceRules)){Log("[ENTRY-PREFAB-RULES] startup=1 fullPrefabFiles=0 originalDependenciesCopied=0");return;}
  auto path = initialInputTable.empty()?gameDirectory / "plugin" / "resource-input-substitutions.tsv":initialInputTable;
  if (!std::filesystem::exists(path)) {
    Log("[ENTRY-SUBSTITUTIONS] absent mode=diagnostic-only nativeSubmission=0");
    return;
  }
  auto packages = std::atomic_load(&published);
  Require(bool(packages), "prepared loading input requires a validated Mod graph");
  Require(il2cpp_string_new && il2cpp_gchandle_new && il2cpp_gchandle_get_target,
          "prepared loading path lifetime exports unavailable");
  auto next = std::make_shared<EiemNativeInput::InputSubstitutions>(
      EiemNativeInput::ReadInputSubstitutions(path, gameDirectory, EiemComposedPackage::Compose(*packages,initialComposed||packages->size()!=1)));
  next->generation = generation.load();
  // Root paths for the process lifetime. Async native jobs may keep opening
  // these files after the managed wrapper has returned; neither is removed.
  for (auto& row : next->entries) {
    row.second.file=std::make_shared<EiemNativeInput::FileHold>(row.second.output);
    auto bytes=row.second.file->BytesLocked();
    auto hash=EiemDirectory::Sha(bytes.data(),bytes.size());
    Require(bytes.size()==row.second.outputBytes && EiemDirectory::Hex(hash.data(),hash.size())==row.second.outputHash,
            "startup input changed before lifetime lock");
    auto string = il2cpp_string_new(row.second.output.u8string().c_str());
    Require(string, "prepared loading path string allocation failed");
    row.second.pathHandle = il2cpp_gchandle_new(string, false);
    Require(row.second.pathHandle, "prepared loading path root failed");
  }
  pendingSubstitutions = std::move(next);
  Log("[ENTRY-SUBSTITUTIONS] validated=%zu generation=%u packageDigest=%s "
      "publication=pending-hooks source=current-input-sha256 output=verified-cache "
      "ownership=process-lifetime-files-and-rooted-paths nativeSubmission=0 mode=%s",
      pendingSubstitutions->entries.size(), pendingSubstitutions->generation,
      pendingSubstitutions->packageDigest.c_str(),
      pendingSubstitutions->control.empty() ? "replacement" : pendingSubstitutions->control.c_str());
}
#if defined(EIEM_DIAGNOSTICS)
void Sample(const char *kind, void *identity, const void *data, size_t length,
            const std::string &path) {
  if (!active.load() || !length || !data)
    return;
  unsigned char head[64] = {};
  size_t n = std::min(length, sizeof(head));
  if (!CopyRaw(data, head, n))
    return;
  bool unityFs = n >= 8 && !memcmp(head, "UnityFS\0", 8);
  // Startup JSON/config reads must not exhaust the bundle observation budget.
  // A recognizable bundle header is useful even outside a synchronous context.
  if (!strncmp(kind, "VFS.Read", 8) && !bundleContext && !unityFs)
    return;
  unsigned g = generation.load();
  {
    std::lock_guard<std::mutex> lock(samplesMutex);
    if (sampled.size() >= 512 ||
        !sampled.emplace((uintptr_t)identity, g).second)
      return;
  }
  unsigned slot = ++inputSamples;
  if (slot > 96)
    return;
  Log("[ENTRY-INPUT] sample=%u kind=%s identity=%p generation=%u bytes=%zu "
      "path=%s head=%s",
      slot, kind, identity, g, length, path.c_str(), Hex(head, n).c_str());
}
void CaptureMemory(const char *kind, void *array) {
  ++memoryCalls;
  size_t length = 0;
  if (!active.load() || !ArrayLength(array, &length))
    return;
  std::string path = bundleContext ? bundleContext : "<no-vfs-context>";
  Sample(kind, array, (char *)array + 32, length, path);
  unsigned char magic[8] = {};
  if (length < 8 || length > captureLimit ||
      !CopyRaw((char *)array + 32, magic, 8))
    return;
  // Capture only complete recognizable plaintext input. Hashes validate capture
  // integrity; they never choose a replacement target.
  if (memcmp(magic, "UnityFS\0", 8) != 0)
    return;
  unsigned slot = ++captures;
  if (slot > 12) {
    if (slot == 13)
      Log("[ENTRY-CAPTURE] complete-input budget exhausted");
    return;
  }
  Job job;
  job.kind = kind;
  job.origin = path;
  job.generation = generation.load();
  job.id = ++captureIds;
  job.graph = std::atomic_load(&published);
  {
    std::lock_guard<std::mutex> lock(queueMutex);
    if (length > queueLimit - queuedBytes) {
      Log("[ENTRY-CAPTURE] queue byte budget full bytes=%zu", length);
      return;
    }
    job.bytes.resize(length);
    if (!CopyRaw((char *)array + 32, job.bytes.data(), length)) {
      Log("[ENTRY-CAPTURE] copy failed bytes=%zu", length);
      return;
    }
    queuedBytes += length;
    jobs.push_back(std::move(job));
  }
  SetEvent(queueEvent);
}
void QueueFileSnapshot(const char *kind, void *path, uint64_t offset) {
  if (!active.load() || !queueEvent || selectedSnapshots.empty() || fileSnapshotRequests.load() >= 12)
    return;
  auto text = String(path);
  auto physical = std::filesystem::u8path(text);
  Require(physical.is_absolute(), "file input path is not absolute");
  SnapshotKey key{SnapshotPath(text), offset};
  auto selected = selectedSnapshots.find(key);
  if (selected == selectedSnapshots.end()) return;
  {
    std::lock_guard<std::mutex> lock(snapshotMutex);
    if (queuedSnapshots.count(key) ||
        (selected == selectedSnapshots.end() && genericSnapshots >= 4) ||
        fileSnapshotRequests >= 12)
      return;
    queuedSnapshots.insert(key);
    if (selected == selectedSnapshots.end())
      ++genericSnapshots;
    ++fileSnapshotRequests;
  }
  Job job;
  job.kind = std::string(kind) + ".disk-snapshot";
  job.origin = bundleContext ? bundleContext : "<no-vfs-context>";
  job.physicalPath = std::move(physical);
  job.offset = offset;
  job.expectedBytes =
      selected == selectedSnapshots.end() ? 0 : selected->second;
  job.id = ++captureIds;
  job.generation = generation.load();
  job.graph = std::atomic_load(&published);
  {
    std::lock_guard<std::mutex> lock(queueMutex);
    jobs.push_back(std::move(job));
  }
  SetEvent(queueEvent);
}
void PathEvent(const char *kind, void *path) {
  if (!active.load())
    return;
  unsigned slot = ++pathSamples;
  if (slot <= 160)
    Log("[ENTRY-LOAD] event=%s path=%s context=%s thread=%lu", kind,
        String(path).c_str(), bundleContext ? bundleContext : "<none>",
        GetCurrentThreadId());
}
#else
void Sample(const char*,void*,const void*,size_t,const std::string&) {}
void CaptureMemory(const char*,void*) {}
void QueueFileSnapshot(const char*,void*,uint64_t) {}
void PathEvent(const char*,void*) {}
#endif
void RememberHash(int64_t hash, void *path) {
  auto text = String(path);
  if (text.empty() || text[0] == '<' || text.size() > 2047)
    return;
  std::replace(text.begin(), text.end(), '\\', '/');
  std::lock_guard<std::mutex> lock(hashMutex);
  auto found = hashPaths.find(hash);
  if (found != hashPaths.end()) {
    if (found->second != text) {
      found->second = "<ambiguous-pathhash>";
      Log("[ENTRY-HASH] conflicting observed mapping hash=%016llX",
          (unsigned long long)hash);
    }
  } else if (hashPaths.size() < 4096) {
    hashPaths.emplace(hash, std::move(text));
  }
}
std::string HashPath(int64_t hash) {
  std::lock_guard<std::mutex> lock(hashMutex);
  auto found = hashPaths.find(hash);
  if (found != hashPaths.end())
    return found->second;
  char text[48];
  snprintf(text, sizeof(text), "<pathhash:%016llX>", (unsigned long long)hash);
  return text;
}
std::string HashEvent(const char *kind, int64_t hash,
                      std::atomic<unsigned> &budget) {
  auto text = HashPath(hash);
  if (active.load() && ++budget <= 160)
    Log("[ENTRY-LOAD] event=%s hash=%016llX path=%s thread=%lu", kind,
        (unsigned long long)hash, text.c_str(), GetCurrentThreadId());
  return text;
}
// Observer exceptions must never alter the game's return value or leave TLS
// pointing at a destroyed local string.
template <class F> void Observe(F f) {
  try {
    f();
  } catch (const std::exception &e) {
    Log("[ENTRY-OBSERVER] rejected reason=%s", e.what());
  } catch (...) {
    Log("[ENTRY-OBSERVER] rejected unknown C++ exception");
  }
}

using VfsFn = void *(__fastcall *)(void *, int64_t, void *);
using VfsPosFn = void *(__fastcall *)(void *, int64_t, void *, uint32_t,
                                      void *);
using StringVfsFn = void *(__fastcall *)(void *, void *, void *);
using HashStreamPosFn = void *(__fastcall *)(void *, int64_t, void *, void *);
using HashMapFn = bool(__fastcall *)(void *, int64_t, void **, void *);
using HashGetPathFn = void *(__fastcall *)(void *, void *);
using ReadFn = int(__fastcall *)(void *, void *, int, int, void *);
using SpanFn = int(__fastcall *)(void *, void *, void *);
using FileFn = void *(__fastcall *)(void *, uint32_t, uint64_t, void *);
using RequestBundleFn = void *(__fastcall *)(void *, void *);
using LoadAssetFn = void *(__fastcall *)(void *, void *, void *, void *);
using MemoryFn = void *(__fastcall *)(void *, uint32_t, void *);
using StreamFn = void *(__fastcall *)(void *, uint32_t, uint32_t, void *);
using HelperFn = void *(__fastcall *)(void *, void *, uint64_t, uint8_t,
                                      uint32_t, void *);
using HelperLeafFn = void *(__fastcall *)(void *, void *, uint64_t, uint32_t,
                                          void *);
VfsFn vfs = nullptr, vfsAsync = nullptr, getStreamHash = nullptr;
StringVfsFn getStream = nullptr;
HashStreamPosFn getStreamHashPos = nullptr;
VfsPosFn vfsPos = nullptr, vfsAsyncPos = nullptr;
HashMapFn hashMap = nullptr;
HashGetPathFn hashGetPath = nullptr;
ReadFn read = nullptr;
SpanFn readSpan = nullptr;
FileFn file = nullptr, fileAsync = nullptr;
FileFn filePublic = nullptr, fileAsyncPublic = nullptr;
void* ObservePublicFile(const char* kind,size_t index,FileFn call,void* path,uint32_t crc,uint64_t offset,void* info);
RequestBundleFn requestBundle = nullptr;
LoadAssetFn loadAsset = nullptr;
HelperFn helper = nullptr, helperAsync = nullptr;
HelperLeafFn helperStream = nullptr, helperStreamAsync = nullptr,
             helperPersist = nullptr, helperPersistAsync = nullptr;
MemoryFn memory = nullptr, memoryAsync = nullptr;
StreamFn stream = nullptr, streamAsync = nullptr;

// A pointer is kept only for comparison, never dereferenced or used to invoke
// a method. The witness is per thread and only records completed sync loads.
struct VfsLoadWitness {void* instance=nullptr;int64_t hash=0;};
thread_local VfsLoadWitness lastVfsSync;

void *__fastcall Vfs(void *self, int64_t path, void *info) {
#if defined(EIEM_DIAGNOSTICS)
  ++vfsCalls;
  std::string text;
  Observe([&] { text = HashEvent("VFS.sync", path, hashLoadSamples); });
  Context c(text.c_str());
#endif
  auto result=vfs(self, path, info);
  if(result)lastVfsSync={self,path};
  return result;
}
void *__fastcall VfsAsync(void *self, int64_t path, void *info) {
  ++vfsCalls;
  std::string text;
  Observe([&] { text = HashEvent("VFS.async", path, hashLoadSamples); });
  Context c(text.c_str());
  return vfsAsync(self, path, info);
}
void *__fastcall VfsPos(void *self, int64_t path, void *pos, uint32_t crc,
                        void *info) {
#if defined(EIEM_DIAGNOSTICS)
  ++vfsCalls;
  std::string text;
  Observe([&] { text = HashEvent("VFS.syncPos", path, hashLoadSamples); });
  Context c(text.c_str());
#endif
  auto result=vfsPos(self, path, pos, crc, info);
  if(result)lastVfsSync={self,path};
  return result;
}
void *__fastcall VfsAsyncPos(void *self, int64_t path, void *pos, uint32_t crc,
                             void *info) {
  ++vfsCalls;
  std::string text;
  Observe([&] { text = HashEvent("VFS.asyncPos", path, hashLoadSamples); });
  Context c(text.c_str());
  return vfsAsyncPos(self, path, pos, crc, info);
}
bool __fastcall HashMap(void *self, int64_t hash, void **path, void *info) {
  auto result = hashMap(self, hash, path, info);
  Observe([&] {
    void *string = nullptr;
    if (active.load() && result && CopyRaw(path, &string, sizeof(string)))
      RememberHash(hash, string);
  });
  return result;
}
void *__fastcall HashGetPath(void *self, void *info) {
  auto result = hashGetPath(self, info);
  Observe([&] {
    int64_t hash = 0;
    if (active.load() && result && CopyRaw(self, &hash, sizeof(hash)))
      RememberHash(hash, result);
  });
  return result;
}
void *__fastcall GetStreamHash(void *self, int64_t path, void *info) {
  std::string text;
  Observe([&] {
    text = HashEvent("VFS.GetAssetStreamHash", path, hashStreamSamples);
  });
  Context c(text.c_str());
  auto result = getStreamHash(self, path, info);
  Observe([&] {
    if (active.load() && result && ++streamSamples <= 160)
      Log("[ENTRY-STREAM] stream=%p hash=%016llX path=%s", result,
          (unsigned long long)path, HashPath(path).c_str());
  });
  return result;
}
void *__fastcall GetStreamHashPos(void *self, int64_t path, void *pos,
                                  void *info) {
  std::string text;
  Observe([&] {
    text = HashEvent("VFS.GetAssetStreamHashPos", path, hashStreamSamples);
  });
  Context c(text.c_str());
  auto result = getStreamHashPos(self, path, pos, info);
  Observe([&] {
    if (active.load() && result && ++streamSamples <= 160)
      Log("[ENTRY-STREAM] stream=%p hash=%016llX path=%s", result,
          (unsigned long long)path, HashPath(path).c_str());
  });
  return result;
}
void *__fastcall GetStream(void *self, void *path, void *info) {
  std::string text;
  Observe([&] {
    text = String(path);
    PathEvent("VFS.GetAssetStream", path);
  });
  Context c(text.c_str());
  auto result = getStream(self, path, info);
  Observe([&] {
    if (active.load() && result && ++streamSamples <= 160)
      Log("[ENTRY-STREAM] stream=%p path=%s", result, text.c_str());
  });
  return result;
}
int __fastcall Read(void *self, void *buffer, int offset, int count,
                    void *info) {
  ++readCalls;
  int result = read(self, buffer, offset, count, info);
  Observe([&] {
    size_t n = 0;
    if (result > 0 && offset >= 0 && count >= result &&
        ArrayLength(buffer, &n) && (size_t)offset <= n &&
        (size_t)result <= n - offset)
      Sample("VFS.Read.byte[]", self, (char *)buffer + 32 + offset,
             (size_t)result,
             bundleContext ? bundleContext : "<no-vfs-context>");
  });
  return result;
}
int __fastcall ReadSpan(void *self, void *span, void *info) {
  ++readCalls;
  int result = readSpan(self, span, info);
  Observe([&] {
    struct S {
      void *data;
      int length;
      int padding;
    };
    S s{};
    if (result > 0 && CopyRaw(span, &s, sizeof(s)) && s.length >= result)
      Sample("VFS.Read.Span", self, s.data, (size_t)result,
             bundleContext ? bundleContext : "<no-vfs-context>");
  });
  return result;
}
void *__fastcall File(void *path, uint32_t crc, uint64_t offset, void *info) {
  ++fileCalls;
  Observe([&] { PathEvent("Unity.LoadFromFile", path); });
  return ObservePublicFile("Unity.LoadFromFile.internal",0,file,path,crc,offset,info);
}
void *__fastcall FileAsync(void *path, uint32_t crc, uint64_t offset,
                           void *info) {
  ++fileCalls;
  Observe([&] { PathEvent("Unity.LoadFromFileAsync", path); });
  return ObservePublicFile("Unity.LoadFromFileAsync.internal",1,fileAsync,path,crc,offset,info);
}
template <class F>
void *ObserveHelper(const char *kind, size_t index, void *dir, void *name,
                    uint64_t offset, int pos, uint32_t crc, F call) {
  auto sequence = ++helperCalls[index];
  std::string directory, filename, context;
  bool observed = false;
  Observe([&] {
    if (!active.load() || sequence > 32)
      return;
    directory = String(dir);
    filename = String(name);
    context = bundleContext ? bundleContext : "<no-vfs-context>";
    Log("[ENTRY-HELPER] event=%s sequence=%llu phase=enter dir=%s name=%s "
        "offset=%llu pos=%d crc=%u context=%s",
        kind, sequence, directory.c_str(), filename.c_str(), offset, pos, crc,
        context.c_str());
    observed = true;
  });
  auto result = call();
  Observe([&] {
    if (observed)
      Log("[ENTRY-HELPER] event=%s sequence=%llu phase=return result=%p "
          "async=%d",
          kind, sequence, result, int(index % 2));
  });
  return result;
}
void *__fastcall Helper(void *dir, void *name, uint64_t offset, uint8_t pos,
                        uint32_t crc, void *info) {
  return ObserveHelper("ReadAssetBundle", 0, dir, name, offset, pos, crc, [&] {
    return helper(dir, name, offset, pos, crc, info);
  });
}
void *__fastcall HelperAsync(void *dir, void *name, uint64_t offset,
                             uint8_t pos, uint32_t crc, void *info) {
  return ObserveHelper(
      "ReadAssetBundleAsync", 1, dir, name, offset, pos, crc,
      [&] { return helperAsync(dir, name, offset, pos, crc, info); });
}
void *__fastcall HelperStream(void *dir, void *name, uint64_t offset,
                              uint32_t crc, void *info) {
  return ObserveHelper(
      "ReadStreamAssetBundle", 2, dir, name, offset, -1, crc,
      [&] { return helperStream(dir, name, offset, crc, info); });
}
void *__fastcall HelperStreamAsync(void *dir, void *name, uint64_t offset,
                                   uint32_t crc, void *info) {
  return ObserveHelper(
      "ReadStreamAssetBundleAsync", 3, dir, name, offset, -1, crc,
      [&] { return helperStreamAsync(dir, name, offset, crc, info); });
}
void *__fastcall HelperPersist(void *dir, void *name, uint64_t offset,
                               uint32_t crc, void *info) {
  return ObserveHelper(
      "ReadPersistAssetBundle", 4, dir, name, offset, -1, crc,
      [&] { return helperPersist(dir, name, offset, crc, info); });
}
void *__fastcall HelperPersistAsync(void *dir, void *name, uint64_t offset,
                                    uint32_t crc, void *info) {
  return ObserveHelper(
      "ReadPersistAssetBundleAsync", 5, dir, name, offset, -1, crc,
      [&] { return helperPersistAsync(dir, name, offset, crc, info); });
}
void *ObservePublicFile(const char *kind, size_t index, FileFn call, void *path,
                        uint32_t crc, uint64_t offset, void *info) {
  ++publicFileCalls;
#if defined(EIEM_DIAGNOSTICS)
  auto sequence = ++publicFileSamples[index];
  std::string text, context;
  bool observed = false;
  Observe([&] {
    if (!active.load() || sequence > 96)
      return;
    text = String(path);
    context = bundleContext ? bundleContext : "<no-vfs-context>";
    Log("[ENTRY-FILE] event=%s sequence=%u phase=enter path=%s offset=%llu "
        "crc=%u context=%s",
        kind, sequence, text.c_str(), offset, crc, context.c_str());
    observed = true;
  });
#endif
  void* selectedPath = path;
  uint64_t selectedOffset = offset;
  std::string selectedInput,selectedInputHash;
  unsigned selectedInputGeneration=0;
  Observe([&] {
    if (!active.load()) return;
    auto graph = std::atomic_load(&substitutions);
    if (!graph) return; // One immutable loading generation, independent of publication order.
    auto actualPath = String(path);
    auto selected = graph->entries.find({EiemNativeInput::InputPath(actualPath), offset});
    if (selected == graph->entries.end()) return;
    Require(crc == 0, "prepared loading input has unsupported nonzero caller CRC");
    const auto& entry = selected->second;
    Require(EiemNativeInput::SameStamp(entry.sourceStamp, EiemNativeInput::FileStamp(entry.source)) &&
                EiemNativeInput::SameStamp(entry.outputStamp, EiemNativeInput::FileStamp(entry.output)),
            "prepared loading source/output changed after publication");
    auto rooted = il2cpp_gchandle_get_target(entry.pathHandle);
    Require(rooted, "prepared loading path root lost");
    selectedInput = entry.output.filename().u8string();
    selectedInputGeneration=graph->generation;selectedInputHash=entry.outputHash;
    selectedPath = rooted; selectedOffset = 0;
    ++submittedInputs;
    Log("[ENTRY-SUBMIT] input=%s event=%s generation=%u original=%s offset=%llu "
        "output=%s bytes=%zu crc=0 methodInfo=preserved async=%d mode=%s",
        selectedInput.c_str(), kind, graph->generation, actualPath.c_str(), offset,
        entry.output.u8string().c_str(), entry.outputBytes, int(index % 2),
        graph->control.empty() ? "replacement" : graph->control.c_str());
  });
  bool ruleMatched=false;void* ruleResult=nullptr;
  Observe([&]{if(active.load())ruleResult=LoadRuleInput(path,crc,offset,bool(index%2),ruleMatched);});
  auto result = ruleMatched?ruleResult:call(selectedPath, crc, selectedOffset, info);
  Observe([&] {
    if (!selectedInput.empty()) {
      Log("[ENTRY-SUBMIT-RETURN] input=%s result=%p async=%d "
          "asyncCompletionObserved=0 assetCreationObserved=0", selectedInput.c_str(), result, int(index % 2));
      if (result) {
        std::lock_guard<std::mutex> lock(submissionMutex);
        if (index % 2) submittedRequests[result] = selectedInput;
        else { submittedBundles[result] = selectedInput; ++completedInputs; }
      }
#if defined(EIEM_DIAGNOSTICS)
      LoadingObservation::Submitted(result,selectedInput,selectedInputGeneration,bool(index%2),selectedInputHash);
#endif
    }
#if defined(EIEM_DIAGNOSTICS)
    if (result)
      QueueFileSnapshot(kind, path, offset);
    if (observed)
      Log("[ENTRY-FILE] event=%s sequence=%u phase=return result=%p async=%d",
          kind, sequence, result, int(index % 2));
#endif
  });
  return result;
}
void* __fastcall RequestBundle(void* self, void* info) {
  auto result = requestBundle(self, info);
  CompleteRuleInput(self,result);
  Observe([&] {
#if defined(EIEM_DIAGNOSTICS)
    if(LoadingObservation::Active()){LoadingObservation::Completed(self,result,"managed.get_assetBundle");return;}
#endif
    std::lock_guard<std::mutex> lock(submissionMutex);
    auto found = submittedRequests.find(self);
    if (found == submittedRequests.end()) return;
    Log("[ENTRY-SUBMIT-COMPLETE] input=%s request=%p bundle=%p assetCreationObserved=0",
        found->second.c_str(), self, result);
    if (result) { submittedBundles[result] = found->second; ++completedInputs; }
    submittedRequests.erase(found);
  });
  return result;
}
void* __fastcall LoadAsset(void* self, void* name, void* type, void* info) {
#if defined(EIEM_DIAGNOSTICS)
  auto probeStart=ReloadObservation::Clock();
#endif
  auto result = loadAsset(self, name, type, info);
#if defined(EIEM_DIAGNOSTICS)
  Observe([&]{ReloadObservation::Asset(self,name,result,probeStart);});
#endif
  Observe([&] {
#if defined(EIEM_DIAGNOSTICS)
    if(LoadingObservation::Active()){if(!LoadingObservation::NativeStringObserved())LoadingObservation::Asset(self,result,"managed.LoadAsset.String",name,0,type,probeStart);return;}
#endif
    std::string input;
    {
      std::lock_guard<std::mutex> lock(submissionMutex);
      auto found = submittedBundles.find(self);
      if (found == submittedBundles.end()) return;
      input = found->second;
    }
    if (result) ++loadedAssets;
    Log("[ENTRY-SUBMIT-ASSET] input=%s name=%s object=%p "
        "rendererRegistrationObserved=0 controlsBound=0", input.c_str(), String(name).c_str(), result);
  });
  return result;
}
void *__fastcall FilePublic(void *path, uint32_t crc, uint64_t offset,
                            void *info) {
  return ObservePublicFile("Unity.LoadFromFile.public", 0, filePublic, path,
                           crc, offset, info);
}
void *__fastcall FileAsyncPublic(void *path, uint32_t crc, uint64_t offset,
                                 void *info) {
  return ObservePublicFile("Unity.LoadFromFileAsync.public", 1, fileAsyncPublic,
                           path, crc, offset, info);
}
void *__fastcall Memory(void *array, uint32_t crc, void *info) {
  Observe([&] { CaptureMemory("Unity.LoadFromMemory", array); });
  return memory(array, crc, info);
}
void *__fastcall MemoryAsync(void *array, uint32_t crc, void *info) {
  Observe([&] { CaptureMemory("Unity.LoadFromMemoryAsync", array); });
  return memoryAsync(array, crc, info);
}
void *__fastcall Stream(void *input, uint32_t crc, uint32_t bufferSize,
                        void *info) {
  ++streamCalls;
  Observe([&] {
    if (streamCalls <= 32)
      Log("[ENTRY-LOAD] event=Unity.LoadFromStream stream=%p crc=%u "
          "readBuffer=%u context=%s",
          input, crc, bufferSize, bundleContext ? bundleContext : "<none>");
  });
  return stream(input, crc, bufferSize, info);
}
void *__fastcall StreamAsync(void *input, uint32_t crc, uint32_t bufferSize,
                             void *info) {
  ++streamCalls;
  Observe([&] {
    if (streamCalls <= 32)
      Log("[ENTRY-LOAD] event=Unity.LoadFromStreamAsync stream=%p crc=%u "
          "readBuffer=%u context=%s",
          input, crc, bufferSize, bundleContext ? bundleContext : "<none>");
  });
  return streamAsync(input, crc, bufferSize, info);
}

#include "eiem_entry_skinning_observation.h"
#include "eiem_entry_capture_observation.h"
#include "eiem_entry_runtime_shapes.h"
#include "eiem_entry_camera_fade.h"
#include "eiem_entry_reload_observation.h"
#include "eiem_entry_loading_observation.h"
#include "eiem_native_bundle_loader.h"
#include "eiem_entry_bundle_dependencies.h"
void CompleteRuleInput(void* request,void* bundle){NativeBundle::CompleteMemory(request,bundle);}
struct StartupArray {std::weak_ptr<const EiemDirectory::Bytes> packet;RuntimeShapes::Ref array;};
std::mutex startupArrayMutex;std::map<const EiemDirectory::Bytes*,StartupArray> startupArrays;
void* LoadRuleInput(void* path,uint32_t callerCrc,uint64_t offset,bool asynchronous,bool& matched){
 auto rules=std::atomic_load(&resourceRules);if(!rules)return nullptr;auto label=EiemNativeInput::InputPath(String(path));
 for(size_t i=0;i<rules->plan->originals.size();++i){const auto& source=rules->plan->originals[i];if(source.offset!=offset||EiemNativeInput::InputPath(source.physical.u8string())!=label)continue;
  if(rules->plan->startupHashes.at(i)==source.hash)return nullptr;
  auto begin=std::chrono::steady_clock::now();Require(callerCrc==0,"runtime rules require caller CRC zero");Require(NativeBundle::enabled,"runtime rule memory bindings not ready");
  auto ready=rules->startupInputs.find(i);RuntimeShapes::Ref array;bool reused=false;size_t nativeBytes=0;
  if(ready!=rules->startupInputs.end()){std::lock_guard<std::mutex> lock(startupArrayMutex);auto found=startupArrays.find(ready->second.get());if(found!=startupArrays.end()&&found->second.packet.lock()==ready->second){auto object=found->second.array.Get();if(object){array=RuntimeShapes::Ref::Make(object,false);reused=true;ArrayLength(object,&nativeBytes);}}}
  std::shared_ptr<const Bytes> bytes;if(!reused){bytes=ready==rules->startupInputs.end()?std::make_shared<const Bytes>(rules->Materialize(i,false)):rules->startupInputsPacked?std::make_shared<const Bytes>(EiemCompiledMod::Unpack(*ready->second)):ready->second;nativeBytes=bytes->size();}auto prepared=std::chrono::steady_clock::now();
  if(!reused)array=NativeBundle::Array(*bytes);auto uploaded=std::chrono::steady_clock::now();auto result=NativeBundle::LoadMemory(array.Get(),0,asynchronous);auto submitted=std::chrono::steady_clock::now();
  Require(result,"runtime rule Bundle load returned null");
  if(!reused&&ready!=rules->startupInputs.end()){std::lock_guard<std::mutex> lock(startupArrayMutex);for(auto it=startupArrays.begin();it!=startupArrays.end();)if(it->second.packet.expired())it=startupArrays.erase(it);else ++it;startupArrays[ready->second.get()]={ready->second,RuntimeShapes::Ref::Make(array.Get(),true)};}
  Observe([&]{{std::lock_guard<std::mutex> lock(submissionMutex);if(asynchronous)submittedRequests[result]=source.file;else{submittedBundles[result]=source.file;++completedInputs;}}++submittedInputs;
#if defined(EIEM_DIAGNOSTICS)
   LoadingObservation::Submitted(result,source.file,generation.load(),asynchronous,rules->plan->startupHashes.at(i));
#endif
  });
  auto ms=[](auto a,auto b){return std::chrono::duration<double,std::milli>(b-a).count();};
  Log("[ENTRY-RULE-SUBMIT] input=%s rendererRules=%zu sourceBytes=%llu nativeBytes=%zu startupCached=%d arrayReused=%d replayMs=%.3f uploadMs=%.3f nativeSubmitMs=%.3f fullPrefabFile=0 archiveMemoryStream=1 async=%d",source.file.c_str(),rules->plan->rendererRules[i].size(),source.size,nativeBytes,int(ready!=rules->startupInputs.end()),int(reused),ms(begin,prepared),ms(prepared,uploaded),ms(uploaded,submitted),int(asynchronous));matched=true;return result;
 }return nullptr;
}
#include "eiem_entry_full_resource_reload.h"
#include "eiem_entry_reload.h"

void InstallHooks(void **assemblies, size_t count) {
  RuntimeShapes::InstallOptional(assemblies,count);
  // Arm depends on the capture completion trampoline. Finish that binding
  // before enabling controls, memory submission, or file interception.
  CaptureObservation::InstallOptional(assemblies, count);
  RuntimeShapes::Arm();
  NativeBundle::Install(assemblies,count);
  StartupDependencies::Install(assemblies,count);
  auto bundleClass = FindClass("UnityEngine", "AssetBundle", assemblies, count);
  auto requestClass = FindClass("UnityEngine", "AssetBundleCreateRequest", assemblies, count);
#if defined(EIEM_DIAGNOSTICS)
  auto vfsClass =
      FindClass("Beyond.VFS", "VirtualFileSystem", assemblies, count);
  auto readClass =
      FindClass("Beyond.VFS", "VFSFileReadStream", assemblies, count);
  auto hashClass =
      FindClass("Beyond.Resource", "StringPathHash", assemblies, count);
  auto mappingClass =
      FindClass("Beyond.Resource", "StringPathHashBinary", assemblies, count);
  auto helperClass =
      FindClass("Beyond.VFS", "UnityFileLoaderHelper", assemblies, count);
  auto helperStreamClass =
      FindClass("Beyond.VFS", "UnityStreamingFileHelper", assemblies, count);
  auto helperPersistClass =
      FindClass("Beyond.VFS", "UnityPersistFileHelper", assemblies, count);
  auto posClass =
      FindClass("Beyond.VFS", "EFileLoaderPosType", assemblies, count);
  Inventory(vfsClass, "VirtualFileSystem");
  Inventory(readClass, "VFSFileReadStream");
  Inventory(bundleClass, "AssetBundle");
  Inventory(helperClass, "UnityFileLoaderHelper");
  Inventory(helperStreamClass, "UnityStreamingFileHelper");
  Inventory(helperPersistClass, "UnityPersistFileHelper");
  if (LoaderPosAbi(posClass)) {
    Install(helperClass, "ReadAssetBundle",
            {"System.String", "System.String", "System.UInt64",
             "Beyond.VFS.EFileLoaderPosType", "System.UInt32"},
            "UnityEngine.AssetBundle", true, (void *)Helper, (void **)&helper);
    Install(helperClass, "ReadAssetBundleAsync",
            {"System.String", "System.String", "System.UInt64",
             "Beyond.VFS.EFileLoaderPosType", "System.UInt32"},
            "UnityEngine.AssetBundleCreateRequest", true, (void *)HelperAsync,
            (void **)&helperAsync);
  }
  for (auto candidate : {std::make_pair(helperStreamClass, false),
                         std::make_pair(helperPersistClass, true)}) {
    bool persist = candidate.second;
    Install(
        candidate.first,
        persist ? "ReadPersistAssetBundle" : "ReadStreamAssetBundle",
        {"System.String", "System.String", "System.UInt64", "System.UInt32"},
        "UnityEngine.AssetBundle", true,
        persist ? (void *)HelperPersist : (void *)HelperStream,
        persist ? (void **)&helperPersist : (void **)&helperStream);
    Install(
        candidate.first,
        persist ? "ReadPersistAssetBundleAsync" : "ReadStreamAssetBundleAsync",
        {"System.String", "System.String", "System.UInt64", "System.UInt32"},
        "UnityEngine.AssetBundleCreateRequest", true,
        persist ? (void *)HelperPersistAsync : (void *)HelperStreamAsync,
        persist ? (void **)&helperPersistAsync : (void **)&helperStreamAsync);
  }
  if (HashAbi(hashClass)) {
    Install(hashClass, "get_path", {}, "System.String", false,
            (void *)HashGetPath, (void **)&hashGetPath);
    Install(mappingClass, "GetMappingStrByHash",
            {"System.Int64", "System.String&"}, "System.Boolean", false,
            (void *)HashMap, (void **)&hashMap);
    Install(vfsClass, "LoadBundleFromFile", {vfsPathType},
            "UnityEngine.AssetBundle", false, (void *)Vfs, (void **)&vfs);
    Install(vfsClass, "LoadBundleFromFileAsync", {vfsPathType},
            "UnityEngine.AssetBundleCreateRequest", false, (void *)VfsAsync,
            (void **)&vfsAsync);
    Install(vfsClass, "LoadBundleFromFile",
            {vfsPathType, "Beyond.VFS.EFileLoaderPosType&", "System.UInt32"},
            "UnityEngine.AssetBundle", false, (void *)VfsPos, (void **)&vfsPos);
    Install(vfsClass, "LoadBundleFromFileAsync",
            {vfsPathType, "Beyond.VFS.EFileLoaderPosType&", "System.UInt32"},
            "UnityEngine.AssetBundleCreateRequest", false, (void *)VfsAsyncPos,
            (void **)&vfsAsyncPos);
    Install(vfsClass, "GetAssetStream", {vfsPathType}, "System.IO.Stream",
            false, (void *)GetStreamHash, (void **)&getStreamHash);
    Install(vfsClass, "GetAssetStream",
            {vfsPathType, "Beyond.VFS.EFileLoaderPosType&"}, "System.IO.Stream",
            false, (void *)GetStreamHashPos, (void **)&getStreamHashPos);
  } else {
    Log("[ENTRY-HOOK] hash VFS observers rejected: unsupported StringPathHash "
        "ABI");
  }
  Install(vfsClass, "GetAssetStream", {"System.String"}, "System.IO.Stream",
          false, (void *)GetStream, (void **)&getStream);
  Install(readClass, "Read", {"System.Byte[]", "System.Int32", "System.Int32"},
          "System.Int32", false, (void *)Read, (void **)&read);
  Install(readClass, "Read", {"System.Span<System.Byte>"}, "System.Int32",
          false, (void *)ReadSpan, (void **)&readSpan);
#endif
  Install(bundleClass, "LoadFromFile_Internal",
          {"System.String", "System.UInt32", "System.UInt64"},
          "UnityEngine.AssetBundle", true, (void *)File, (void **)&file);
  Install(bundleClass, "LoadFromFileAsync_Internal",
          {"System.String", "System.UInt32", "System.UInt64"},
          "UnityEngine.AssetBundleCreateRequest", true, (void *)FileAsync,
          (void **)&fileAsync);
  // AOT public wrappers inline the icall dispatch and bypass the _Internal
  // method pointers. Resolve their real signatures, never a fixed file RVA.
  Install(bundleClass, "LoadFromFile",
          {"System.String", "System.UInt32", "System.UInt64"},
          "UnityEngine.AssetBundle", true, (void *)FilePublic,
          (void **)&filePublic);
  Install(bundleClass, "LoadFromFileAsync",
          {"System.String", "System.UInt32", "System.UInt64"},
          "UnityEngine.AssetBundleCreateRequest", true, (void *)FileAsyncPublic,
          (void **)&fileAsyncPublic);
  Install(requestClass, "get_assetBundle", {}, "UnityEngine.AssetBundle", false,
          (void*)RequestBundle, (void**)&requestBundle);
  Install(bundleClass, "LoadAsset", {"System.String", "System.Type"}, "UnityEngine.Object", false,
          (void*)LoadAsset, (void**)&loadAsset);
#if defined(EIEM_DIAGNOSTICS)
  Install(bundleClass, "LoadFromMemory_Internal",
          {"System.Byte[]", "System.UInt32"}, "UnityEngine.AssetBundle", true,
          (void *)Memory, (void **)&memory);
  Install(bundleClass, "LoadFromMemoryAsync_Internal",
          {"System.Byte[]", "System.UInt32"},
          "UnityEngine.AssetBundleCreateRequest", true, (void *)MemoryAsync,
          (void **)&memoryAsync);
  Install(bundleClass, "LoadFromStreamInternal",
          {"System.IO.Stream", "System.UInt32", "System.UInt32"},
          "UnityEngine.AssetBundle", true, (void *)Stream, (void **)&stream);
  Install(bundleClass, "LoadFromStreamAsyncInternal",
          {"System.IO.Stream", "System.UInt32", "System.UInt32"},
          "UnityEngine.AssetBundleCreateRequest", true, (void *)StreamAsync,
          (void **)&streamAsync);
#endif
  if (pendingSubstitutions) {
    Require(filePublic && fileAsyncPublic && requestBundle && loadAsset,
            "prepared loading input requires sync/async file and acceptance observer hooks");
    std::shared_ptr<const EiemNativeInput::InputSubstitutions> immutable = pendingSubstitutions;
    std::atomic_store(&substitutions, immutable);
    pendingSubstitutions.reset();
    Log("[ENTRY-SUBSTITUTIONS] published=%zu nativeSubmission=armed controlsBound=0 "
        "dynamicRegistrationVerified=0 shortcutNativeRebuild=0 mode=%s", immutable->entries.size(),
        immutable->control.empty() ? "replacement" : immutable->control.c_str());
  }
  // Optional diagnostics must never prevent validated inputs from being
  // published, nor unwind the production startup and thread-detach sequence.
  EntryCameraFade::InstallOptional(assemblies,count);
#if defined(EIEM_DIAGNOSTICS)
  ReloadObservation::InstallOptional(assemblies,count);
  LoadingObservation::InstallOptional(assemblies,count);
#endif
  FullResourceReload::Install(assemblies,count);
#if defined(EIEM_DIAGNOSTICS)
  SkinningObservation::StartOptionalReadback([&] {
    SkinningObservation::InstallReadback(assemblies, count);
  });
#endif
  auto armed = std::atomic_load(&substitutions);
  auto rules = std::atomic_load(&resourceRules);
  const bool ruleArmed = rules && NativeBundle::enabled;
  Log("[ENTRY-READY] installed=%zu vfs=%d memory=%d file=%d stream=%d read=%d "
      "publicFile=%d helper=%d "
      "replacementArmed=%d controlArmed=%d ruleMemoryArmed=%d "
      "RenderDocRequested=0",
      installed.size(), !!vfsPos || !!vfs, !!memory || !!memoryAsync,
      !!file || !!fileAsync || !!filePublic || !!fileAsyncPublic,
      !!stream || !!streamAsync, !!read || !!readSpan,
      !!filePublic || !!fileAsyncPublic,
      !!helper || !!helperAsync || !!helperStream || !!helperStreamAsync ||
          !!helperPersist || !!helperPersistAsync,
      (armed && armed->control.empty()) || ruleArmed,
      armed && !armed->control.empty(), int(ruleArmed));
}
DWORD WINAPI ProcessJobs(void *) {
  for (;;) {
    WaitForSingleObject(queueEvent, 1000);
    if (reload.exchange(false)) {
      if(FullResourceReload::configured) {
        if(FullResourceReload::enabled) FullResourceReload::Prepare();
        else Log("[FULL-RELOAD-REJECT] phase=trigger reason=requiredNativeBindingsUnavailable configurationFallbackRefused=1");
      } else {EntryReload::Prepare();}
    }
    if(FullResourceReload::configured) FullResourceReload::PollOffline();
#if defined(EIEM_DIAGNOSTICS)
    for (;;) {
      Job job;
      {
        std::lock_guard<std::mutex> lock(queueMutex);
        if (jobs.empty())
          break;
        job = std::move(jobs.front());
        jobs.pop_front();
        queuedBytes -= job.bytes.size();
      }
      try {
        if (!job.physicalPath.empty()) {
          job.bytes = EiemNativeInput::EndfieldInput::Snapshot(job.physicalPath,
                                                               job.offset);
          Require(!job.expectedBytes || job.bytes.size() == job.expectedBytes,
                  "diagnostic selected range size differs from current input");
          ++fileSnapshots;
          Log("[ENTRY-SNAPSHOT] input=%u path=%s offset=%llu bytes=%zu "
              "origin=independent-disk-read nativeReadBufferOwned=0",
              job.id, job.physicalPath.u8string().c_str(), job.offset,
              job.bytes.size());
        }
        auto hash = Sha(job.bytes.data(), job.bytes.size());
        auto folder = sessionDirectory;
        std::filesystem::create_directories(folder);
        auto output =
            folder /
            ("input-" + std::to_string(job.id) +
             (EiemNativeInput::EndfieldInput::Recognized(job.bytes) ? ".vfs"
                                                                    : ".ab"));
        std::ofstream out(output, std::ios::binary | std::ios::trunc);
        out.write((const char *)job.bytes.data(), job.bytes.size());
        Require(bool(out), "capture disk write failed");
        out.close();
        auto assets = EiemNativeInput::Inspect(job.bytes);
        auto packages = job.graph;
        size_t matches = 0;
        for (const auto &asset : assets)
          if (packages)
            for (const auto &package : *packages) {
              Identity id{asset.path, asset.asset};
              auto rule = package.targetIndex.find(id);
              if (rule != package.targetIndex.end()) {
                Require(asset.classId == 43,
                        "matched identity is not a native Mesh");
                const auto &replacement = package.rules.at(rule->second);
                for (const auto &partId : replacement.parts) {
                  const auto &mesh =
                      package.meshes.at(package.parts.at(partId).mesh);
                  Require(package.resources.at(mesh.resource)->type ==
                              asset.type,
                          "matched native Mesh schema hash is incompatible: " +
                              partId);
                }
                ++matches;
                Log("[ENTRY-MATCH] input=%u mod=%s rule=%s path=%s asset=%s "
                    "cab=%s pathId=%lld "
                    "typeHash=%s generation=%u replacement=0",
                    job.id, package.id.c_str(), rule->second.c_str(),
                    asset.path.c_str(), asset.asset.c_str(), asset.cab.c_str(),
                    (long long)asset.pathId, Hex(asset.type.data(), 16).c_str(),
                    job.generation);
              }
            }
        Log("[ENTRY-CAPTURE] input=%u kind=%s bytes=%zu sha256=%s assets=%zu "
            "meshMatches=%zu "
            "origin=%s file=%s generation=%u graphReady=%d replacement=0",
            job.id, job.kind.c_str(), job.bytes.size(),
            Hex(hash.data(), 32).c_str(), assets.size(), matches,
            job.origin.c_str(), output.u8string().c_str(), job.generation,
            !!packages);

      } catch (const std::exception &e) {
        Log("[ENTRY-CAPTURE] input=%u rejected reason=%s origin=%s", job.id,
            e.what(), job.origin.c_str());
      }
    }
#endif
  }
}
DWORD StartImpl() {
  wchar_t executable[32768] = {};
  GetModuleFileNameW(nullptr, executable, _countof(executable));
  gameDirectory = std::filesystem::path(executable).parent_path();
#if defined(EIEM_DIAGNOSTICS)
  FILETIME created{}, exited{}, kernel{}, user{};
  Require(
      GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user),
      "process creation time unavailable");
  char runName[80];
  snprintf(runName, sizeof(runName), "run-%lu-%08lX%08lX",
           GetCurrentProcessId(), created.dwHighDateTime,
           created.dwLowDateTime);
  sessionDirectory =
      gameDirectory / "plugin" / "resource-input-diagnostics" / runName;
  std::filesystem::create_directories(sessionDirectory);
  auto logPath = sessionDirectory / "entry.log";
  logFile = OpenLog(logPath, true);
#else
  std::filesystem::create_directories(gameDirectory / "plugin");
  auto logPath = gameDirectory / "plugin" / "eff_log.txt";
  logFile = OpenLog(logPath);
#endif
  if (!logFile) {
    OutputDebugStringA("EFF entry log could not be opened; observer "
                       "initialization cancelled\n");
    return 1;
  }
#if defined(EIEM_DIAGNOSTICS)
  logMirror = OpenLog(gameDirectory / "plugin" / "eff_log.txt");
#endif
  Log("[BUILD] resource-directory-entry %s %s pid=%lu abi=%u compiler=%s "
      "mode=serialized-loading-input startup=initializing replacement=not-yet-armed",
      __DATE__, __TIME__, GetCurrentProcessId(), EiemPackageProtocol::abi, EiemPackageProtocol::compiler);
  Log("[ENTRY-LOG] path=%s diagnostics=%d readers=shared writers=exclusive",
      logPath.u8string().c_str(), !!logMirror);
  while (!GetModuleHandleW(L"GameAssembly.dll"))
    Sleep(100);
  // Preserve the startup timing verified by the previous game run. Module
  // presence and assembly enumeration alone do not prove VFS/hotfix readiness.
  Sleep(3000);
  if (!Resolve()) {
    Log("[ENTRY-FATAL] IL2CPP exports unavailable");
    return 1;
  }
  void **assemblies = nullptr;
  size_t count = 0;
  ManagedAttachment managed;
  for (unsigned attempt = 0; attempt < 500; ++attempt) {
    auto domain = il2cpp_domain_get();
    if (domain)
      managed.Attach(domain);
    if (domain)
      assemblies = il2cpp_domain_get_assemblies(domain, &count);
    if (assemblies && count)
      break;
    Sleep(20);
  }
  if (!assemblies || !count) {
    Log("[ENTRY-FATAL] IL2CPP assemblies unavailable");
    return 1;
  }
  if (!il2cpp_free || !il2cpp_method_get_flags || !il2cpp_method_get_param ||
      !il2cpp_method_get_return_type) {
    Log("[ENTRY-FATAL] exact signature exports unavailable");
    return 1;
  }
  queueEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
  if (!queueEvent) {
    Log("[ENTRY-FATAL] queue event failed err=%lu", GetLastError());
    return 1;
  }
  auto status = MH_Initialize();
  if (status != MH_OK) {
    Log("[ENTRY-FATAL] MinHook init status=%d", (int)status);
    return 1;
  }
  // Read author-compiled rules before observing game loading requests.
  LoadInitialPackages();
  Observe([] { PrepareInputSubstitutions(); });
#if defined(EIEM_DIAGNOSTICS)
  Observe([] { LoadSnapshotSelection(); });
#endif
  InstallHooks(assemblies, count);
  managed.Detach();
  if(!RuntimeControls::panel.Read()) RuntimeControls::PublishPanel();
  HANDLE panelThread=CreateThread(nullptr,0,EiemControlPanel::Window::Run,&RuntimeControls::panel,0,nullptr);
  if(panelThread) {CloseHandle(panelThread);Log("[CONTROL-PANEL] workerStarted=1 shortcut=gui selectedModOnly=1");}
  else {RuntimeControls::panel.failed=true;Log("[CONTROL-PANEL-GAP] workerStartFailed=%lu",GetLastError());}
  Log("[ENTRY-LIFETIME] startup thread detached; poller=Win32-only "
      "worker=native-parser-only");
  HANDLE worker = CreateThread(nullptr, 0, ProcessJobs, nullptr, 0, nullptr);
  if (worker)
    CloseHandle(worker);
  else
    Log("[ENTRY-FATAL] evidence worker failed err=%lu", GetLastError());
  EiemControlPanel::Shortcut reloadKey, guiKey;
#if defined(EIEM_DIAGNOSTICS)
  ULONGLONG report = GetTickCount64();
#endif
  for (;;) {
    HWND window = GetForegroundWindow();
    DWORD process = 0;
    if (window)
      GetWindowThreadProcessId(window, &process);
    if(process==GetCurrentProcessId() && window!=RuntimeControls::panel.window.load() &&
       !(GetWindowLongPtrW(window,GWL_EXSTYLE)&WS_EX_TOOLWINDOW)) RuntimeControls::panel.gameWindow=window;
    auto gui=settings.Get().gui;
    bool guiPhysical=RuntimeControls::Down(gui);
    if(guiKey.Press(gui,guiPhysical,active && process==GetCurrentProcessId())) {
      if(RuntimeControls::panel.failed) Log("[CONTROL-PANEL-GAP] unavailable=1 restartRequired=1");
      else {
        RuntimeControls::panel.open=!RuntimeControls::panel.open.load();
        Log("[CONTROL-PANEL] requestedOpen=%d vk=%u modifiers=%u",int(RuntimeControls::panel.open.load()),gui.vk,gui.modifiers);
      }
    }
    if(!active) RuntimeControls::panel.open=false;
    auto shortcut=settings.Get().reload;
    bool physical=RuntimeControls::Down(shortcut);
    if (reloadKey.Press(shortcut,physical,active && process==GetCurrentProcessId())) {
      reloadClock.store(NativeBundle::Clock());
      reload.store(true);
      SetEvent(queueEvent);
      Log("[ENTRY-RELOAD] requested vk=%u modifiers=%u transaction=queued",shortcut.vk,shortcut.modifiers);
    }
#if defined(EIEM_DIAGNOSTICS)
    auto now = GetTickCount64();
    if (now - report >= 10000) {
      report = now;
      Log("[ENTRY-COUNTERS] vfs=%llu reads=%llu memory=%llu file=%llu "
          "stream=%llu samples=%u "
          "captures=%u generation=%u publicFile=%llu "
          "helperSync=%llu helperAsync=%llu streamingSync=%llu "
          "streamingAsync=%llu "
          "persistSync=%llu persistAsync=%llu",
          vfsCalls.load(), readCalls.load(), memoryCalls.load(),
          fileCalls.load(), streamCalls.load(), inputSamples.load(),
          captures.load(), generation.load(), publicFileCalls.load(),
          helperCalls[0].load(), helperCalls[1].load(), helperCalls[2].load(),
          helperCalls[3].load(), helperCalls[4].load(), helperCalls[5].load());
      Log("[ENTRY-SNAPSHOT-COUNTERS] requested=%u complete=%u limit=12",
          fileSnapshotRequests.load(), fileSnapshots.load());
      Log("[ENTRY-SUBMIT-COUNTERS] submitted=%llu bundlesReturned=%llu assetsReturned=%llu "
          "controlsBound=%u dynamicRegistrationVerified=0", submittedInputs.load(),
          completedInputs.load(), loadedAssets.load(), RuntimeShapes::boundCount.load());
      Observe([&]{LoadingObservation::Counters();});
    }
#endif
    RuntimeControls::preferences.Flush(Log);
    RuntimeControls::keySettings.Flush(Log);
    Sleep(25);
  }
}
DWORD WINAPI Start(void *) {
  try {
    return StartImpl();
  } catch (const std::exception &e) {
    Log("[ENTRY-FATAL] startup rejected reason=%s", e.what());
    return 1;
  } catch (...) {
    Log("[ENTRY-FATAL] startup rejected unknown C++ exception");
    return 1;
  }
}
} // namespace Entry

void Log(const char *format, ...) {
  char line[8192];
  va_list args;
  va_start(args, format);
  vsnprintf(line, sizeof(line), format, args);
  va_end(args);
  std::lock_guard<std::mutex> lock(Entry::logMutex);
  if (Entry::logFile) {
    SYSTEMTIME now;
    GetLocalTime(&now);
    for (auto output : {Entry::logFile, Entry::logMirror}) {
      if (!output)
        continue;
      fprintf(output, "[%02u:%02u:%02u.%03u] [pid=%lu tid=%lu] %s\n", now.wHour,
              now.wMinute, now.wSecond, now.wMilliseconds,
              GetCurrentProcessId(), GetCurrentThreadId(), line);
      fflush(output);
    }
  }
}
AP_PluginInfo info = {APPLEPIE_PLUGIN_API_VERSION, "eff",     "EFF",
                      "Resource input validation", "eff.ini", true};
APPLEPIE_PLUGIN_EXPORT AP_PluginInfo *AP_GetPluginInfo() { return &info; }
APPLEPIE_PLUGIN_EXPORT bool AP_PluginEnable() {
  Entry::active = true;
  Log("[ENTRY] enabled");
  Entry::EntryCameraFade::Report();
  return true;
}
APPLEPIE_PLUGIN_EXPORT bool AP_PluginDisable() {
  Entry::active = false;
  Entry::RuntimeControls::panel.open=false;
  Log("[ENTRY] disabled");
  Entry::EntryCameraFade::Report();
  return true;
}
APPLEPIE_PLUGIN_EXPORT bool AP_ReloadConfig() {
  Entry::reload = true;
  if (Entry::queueEvent)
    SetEvent(Entry::queueEvent);
  return true;
}
APPLEPIE_PLUGIN_EXPORT int AP_GetHotkeys(AP_HotkeyInfo *output, int maximum) {
  if (maximum > 0 && output) {
    output[0] = {"Reload Mod configuration", "reload", int(Entry::settings.Get().reload.vk)};
    if(maximum>1) {
      output[1]={"Open Mod controls", "gui", int(Entry::settings.Get().gui.vk)};
      return 2;
    }
    return 1;
  }
  return 0;
}
APPLEPIE_PLUGIN_EXPORT void AP_SetLanguage(const char *) {}
BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
  if (reason == DLL_PROCESS_DETACH) {Entry::RuntimeShapes::managedAlive=false;Entry::RuntimeControls::panel.stopping=true;}
  if (reason == DLL_PROCESS_ATTACH) {
    DisableThreadLibraryCalls(module);
    HANDLE thread = CreateThread(nullptr, 0, Entry::Start, nullptr, 0, nullptr);
    if (thread)
      CloseHandle(thread);
  }
  return TRUE;
}
