// This fixture exercises explicit developer observation, not the released DLL.
#define EIEM_DIAGNOSTICS 1
#define DllMain ResourceEntryFixtureDllMain
#include "../../src/eiem_resource_entry.cpp"
#undef DllMain
#include <cassert>
#include <iostream>
#include <functional>
#include "../../src/eiem_texture_input_builder.h"
#include "../../src/eiem_renderer_input_builder.h"
#include "eiem_packed_uv_probe.h"
using namespace EiemDirectory;
namespace Fixture {
struct Method {
  void *address;
  const char *name;
  const char *result;
  std::vector<const char *> params;
  uint32_t flags;
};
struct Class {
  std::vector<Method *> methods;
};
struct Field {
  const char *type;
  size_t offset;
  int flags = 0;
};
int valueSize = 8, valueKind = 0x11;
uint32_t valueAlignment = 8;
std::vector<Field> fields{{"System.Int64", 16}};
unsigned attachCalls = 0, detachCalls = 0, attached = 0;
void *__cdecl Attach(void *) {
  ++attachCalls;
  ++attached;
  return (void *)0xAABBCC;
}
void __cdecl Detach(void *thread) {
  Require(thread == (void *)0xAABBCC && attached > 0,
          "invalid/double thread detach");
  ++detachCalls;
  --attached;
}
int32_t __cdecl ValueSize(void *, uint32_t *alignment) {
  *alignment = valueAlignment;
  return valueSize;
}
void *__cdecl ClassType(void *klass) { return klass; }
int __cdecl TypeKind(void *) { return valueKind; }
void *__cdecl NextField(void *, void **iterator) {
  size_t i = (size_t)*iterator;
  if (i >= fields.size())
    return nullptr;
  *iterator = (void *)(i + 1);
  return &fields[i];
}
int __cdecl FieldFlags(void *f) { return ((Field *)f)->flags; }
void *__cdecl FieldType(void *f) { return (void *)((Field *)f)->type; }
size_t __cdecl FieldOffset(void *f) { return ((Field *)f)->offset; }
void *__cdecl Next(void *klass, void **iterator) {
  auto c = (Class *)klass;
  size_t i = (size_t)*iterator;
  if (i >= c->methods.size())
    return nullptr;
  *iterator = (void *)(i + 1);
  return c->methods[i];
}
const char *__cdecl Name(void *m) { return ((Method *)m)->name; }
uint32_t __cdecl Count(void *m) {
  return (unsigned)((Method *)m)->params.size();
}
uint32_t __cdecl Flags(void *m, uint32_t *impl) {
  *impl = 0;
  return ((Method *)m)->flags;
}
void *__cdecl Return(void *m) { return (void *)((Method *)m)->result; }
void *__cdecl Param(void *m, uint32_t i) {
  return (void *)((Method *)m)->params.at(i);
}
const char *__cdecl TypeName(void *t) { return _strdup((const char *)t); }
void __cdecl Free(void *p) { free(p); }
void *lastArray = nullptr;
void *lastInfo = nullptr;
unsigned calls = 0;
unsigned lastCrc = 0;
int64_t lastHash = 0;
void *lastSelf = nullptr;
void *lastPos = nullptr;
void *stringResult = nullptr;
void *rootedPath = nullptr;
void* __cdecl HandleTarget(uint32_t handle) {
  Require(handle == 23, "fixture path handle changed");
  return rootedPath;
}
int result = 0;
void *lastDir = nullptr, *lastName = nullptr;
uint64_t lastOffset = 0;
uint8_t lastLoaderPos = 0;
void *__fastcall FileOriginal(void *path, uint32_t crc, uint64_t offset,
                              void *info) {
  ++calls;
  lastName = path;
  lastCrc = crc;
  lastOffset = offset;
  lastInfo = info;
  return (void *)0x45670;
}
void *__fastcall HelperOriginal(void *dir, void *name, uint64_t offset,
                                uint8_t pos, uint32_t crc, void *info) {
  lastDir = dir;
  lastLoaderPos = pos;
  return FileOriginal(name, crc, offset, info);
}
void *__fastcall HelperLeafOriginal(void *dir, void *name, uint64_t offset,
                                    uint32_t crc, void *info) {
  lastDir = dir;
  return FileOriginal(name, crc, offset, info);
}
void *__fastcall MemoryOriginal(void *array, uint32_t crc, void *info) {
  ++calls;
  lastArray = array;
  lastCrc = crc;
  lastInfo = info;
  return (void *)0x12340;
}
int __fastcall ReadOriginal(void *, void *, int, int, void *info) {
  ++calls;
  lastInfo = info;
  return result;
}
int __fastcall SpanOriginal(void *, void *, void *info) {
  ++calls;
  lastInfo = info;
  return result;
}
void* vfsResult=(void*)0x34560;
void *__fastcall VfsOriginal(void *self, int64_t hash, void *info) {
  ++calls;
  lastSelf = self;
  lastHash = hash;
  lastInfo = info;
  Require(Entry::bundleContext != nullptr,
          "VFS context absent during original");
  return vfsResult;
}
void *__fastcall VfsPosOriginal(void *self, int64_t hash, void *pos,
                                uint32_t crc, void *info) {
  lastPos = pos;
  lastCrc = crc;
  if (pos)
    *(unsigned char *)pos = 2;
  return VfsOriginal(self, hash, info);
}
void *__fastcall StreamPosOriginal(void *self, int64_t hash, void *pos,
                                   void *info) {
  lastPos = pos;
  if (pos)
    *(unsigned char *)pos = 2;
  return VfsOriginal(self, hash, info);
}
bool __fastcall MappingOriginal(void *self, int64_t hash, void **outPath,
                                void *info) {
  ++calls;
  lastSelf = self;
  lastHash = hash;
  lastInfo = info;
  if (outPath)
    *outPath = stringResult;
  return stringResult != nullptr;
}
void *__fastcall GetterOriginal(void *self, void *info) {
  ++calls;
  lastSelf = self;
  lastInfo = info;
  return stringResult;
}
void* __fastcall AssetOriginal(void* self, void* name, void* type, void* info) {
  ++calls; lastSelf = self; lastName = name; lastArray = type; lastInfo = info;
  return stringResult;
}
Bytes ManagedString(const std::wstring &text) {
  Bytes bytes(20 + text.size() * 2);
  int32_t length = (int32_t)text.size();
  memcpy(bytes.data() + 16, &length, 4);
  memcpy(bytes.data() + 20, text.data(), text.size() * 2);
  return bytes;
}
void *__cdecl SkinObjectClass(void *) { return (void *)0x781; }
const char *__cdecl SkinClassName(void *) { return "SkinnedMeshRenderer"; }
const char *__cdecl SkinClassNamespace(void *) { return "UnityEngine"; }
void *__cdecl SkinReadException(void *, void *, void **, void **exception) {
  *exception = (void *)0xEF;
  return nullptr;
}
bool lastBounds = false;
bool readinessResult = false;
bool __fastcall SkinReadiness(void *self, void *info) {
  ++calls; lastSelf = self; lastInfo = info; return readinessResult;
}
void *lastProperties = nullptr;
void __fastcall RendererRegistration(void *self, void *renderer, void *configs, void *info) {
  ++calls; lastSelf = self; lastArray = renderer; lastName = configs; lastInfo = info;
}
void __fastcall ControllerRegistration(void *self, void *renderers, void *configs, void *properties, bool bounds, void *info) {
  RendererRegistration(self, renderers, configs, info); lastProperties = properties; lastBounds = bounds;
}
void __fastcall BoneAssignment(void *self, void *bones, void *info) {
  ++calls; lastSelf = self; lastArray = bones; lastInfo = info;
}
uint64_t lastMatrixCount = 0;
bool packMatrixOutput = false;
void __fastcall MatrixUpload(void *renderer, const float *matrices, uint64_t count) {
  ++calls; lastSelf = renderer; lastArray = (void *)matrices; lastMatrixCount = count;
  if (!packMatrixOutput) return;
  uintptr_t buffer = 0;
  memcpy(&buffer, (char *)renderer + 0x388, sizeof(buffer));
  for (size_t bone = 0; bone < count; ++bone)
    for (size_t row = 0; row < 3; ++row) for (size_t col = 0; col < 4; ++col)
      memcpy((void *)(buffer + 48 + (bone * 12 + row * 4 + col) * 4), matrices + bone * 16 + row + col * 4, 4);
}
uint64_t __fastcall NativeRecord(void *renderer) {
  ++calls; lastSelf = renderer;
  return 0xCAFEBABEDEADBEEFull;
}
uint64_t __fastcall NativeFormat(void *device, void *vertexData, void *selected, uint32_t *changed) {
  ++calls; lastSelf = device; lastArray = vertexData; lastName = selected; lastInfo = changed;
  if (selected) memset(selected, 0x34, 28);
  if (changed) *changed = 0x1003;
  return 0xC0FFEEAABBCCDDEEull;
}
} // namespace Fixture
int main(int argc, char **argv) {
  try {
    Require(argc >= 2 && (argc <= 5 || argc == 7),
            "provide UnityFS fixture, optional Endfield input, Mod, Texture fixture directory");
    auto rejects = [](const std::function<void()>& call, const char* message) {
      bool rejected = false;
      try { call(); } catch (const std::exception& e) {
        rejected = std::string(e.what()).find(message) != std::string::npos;
      }
      Require(rejected, std::string("missing writer rejection: ") + message);
    };
    {
      // Capture eligibility is evidence only, including when a managed getter
      // throws or the observation budget is exhausted.
      using namespace Entry::CaptureObservation;
      originalFinished = Fixture::SkinReadiness;
      meshMethod = (void*)0x991;
      il2cpp_runtime_invoke = Fixture::SkinReadException;
      samples = 0; seen.clear();
      auto calls = Fixture::calls;
      void *self = (void*)0x101, *info = (void*)0x505;
      for (bool ready : {false, true, false, true}) {
        Fixture::readinessResult = ready;
        Require(Finished(self, info) == ready && Fixture::calls == ++calls &&
                    Fixture::lastSelf == self && Fixture::lastInfo == info,
                "capture observation changes original result/call/MethodInfo");
      }
      Require(samples == 2 && seen.size() == 2, "capture observation did not bound renderer states");
      samples = 256;
      Require(Finished(self, info) && Fixture::calls == ++calls,
              "capture observation budget changed original callback");
      Require(Finished(nullptr, info) && Fixture::calls == ++calls && Fixture::lastSelf == nullptr,
              "null renderer changed original callback");
      samples = 0; seen.clear(); originalFinished = nullptr; meshMethod = nullptr;
      il2cpp_runtime_invoke = nullptr;
      std::cout << "PASS capture eligibility observation: true/false preserved; managed getter failure; exact original call/MethodInfo; bounded deduplication; null renderer\n";
    }
    {
      // Startup failure in the optional probe must not unwind past the
      // already installed production hooks or unpublish their loading table.
      using namespace Entry::SkinningObservation;
      auto saved = std::atomic_load(&Entry::substitutions);
      auto table = std::make_shared<EiemNativeInput::InputSubstitutions>();
      table->generation = 1;
      std::shared_ptr<const EiemNativeInput::InputSubstitutions> armed = table;
      std::atomic_store(&Entry::substitutions, armed);
      bool failed = false, startupContinued = false;
      enabled = true;
      try {
        StartOptionalReadback([] { throw std::runtime_error("bounded skinning observation API incomplete"); });
        startupContinued = true;
      } catch (...) { failed = true; }
      Require(!failed && startupContinued && !enabled && std::atomic_load(&Entry::substitutions) == armed,
              "optional readback failure aborted replacement startup");
      std::atomic_store(&Entry::substitutions, saved);
      std::cout << "PASS unavailable optional readback APIs preserve armed replacement table and startup completion\n";
    }
    {
      // A managed getter failure must leave the original game callbacks and
      // every pointer/boolean/MethodInfo argument intact, exactly once.
      using namespace Entry::SkinningObservation;
      il2cpp_object_get_class = Fixture::SkinObjectClass;
      il2cpp_class_get_name = Fixture::SkinClassName;
      il2cpp_class_get_namespace = Fixture::SkinClassNamespace;
      il2cpp_runtime_invoke = Fixture::SkinReadException;
      api.mesh = (void *)0x991;
      rendererInit = Fixture::RendererRegistration;
      controllerInit = Fixture::ControllerRegistration;
      setBones = Fixture::BoneAssignment;
      enabled = true;
      void *self = (void *)0x101, *renderer = (void *)0x202, *configs = (void *)0x303,
           *properties = (void *)0x404, *info = (void *)0x505;
      auto calls = Fixture::calls;
      RendererInit(self, renderer, configs, info);
      Require(Fixture::calls == calls + 1 && Fixture::lastSelf == self && Fixture::lastArray == renderer &&
                  Fixture::lastName == configs && Fixture::lastInfo == info,
              "readback getter exception changed Renderer registration");
      SetBones(renderer, configs, info);
      Require(Fixture::calls == calls + 2 && Fixture::lastSelf == renderer &&
                  Fixture::lastArray == configs && Fixture::lastInfo == info,
              "readback getter exception changed native bone assignment");
      ControllerInit(self, nullptr, configs, properties, true, info);
      Require(Fixture::calls == calls + 3 && Fixture::lastSelf == self && Fixture::lastArray == nullptr &&
                  Fixture::lastName == configs && Fixture::lastProperties == properties && Fixture::lastBounds &&
                  Fixture::lastInfo == info, "readback changed controller ABI");
      enabled = false; api = {}; rendererInit = nullptr; controllerInit = nullptr; setBones = nullptr;
      il2cpp_object_get_class = nullptr; il2cpp_class_get_name = nullptr; il2cpp_class_get_namespace = nullptr;
      il2cpp_runtime_invoke = nullptr;
      Require(SuffixPath("/instance/Root/Spine", "Root/Spine") &&
                  !SuffixPath("/instance/OtherRoot/Spine", "Root/Spine") &&
                  !SuffixPath("/instance/Root/SpineExtra", "Root/Spine"), "readback path boundary comparison");
      std::cout << "PASS bounded readback: managed getter exceptions preserve Renderer/controller/bone callback arguments exactly once\n";
    }
    {
      using namespace EiemNativeInput;
      Type type;
      type.nodes = {{"Base", "Base", false, {1, 2, 3, 4}},
                    {"bool", "flag", false, {}},
                    {"string", "label", false, {}},
                    {"float", "sample", false, {}},
                    {"vector", "values", false, {5}},
                    {"Array", "Array", true, {6, 7}},
                    {"int", "size", false, {}},
                    {"UInt16", "data", false, {}}};
      Bytes source{1, 2, 0, 0, 0, 'a', 'b', 0x7d,
                   0x34, 0x12, 0xc0, 0x7f, 2, 0, 0, 0, 1, 0, 0xff, 0xff};
      auto value = DecodeValue(type, 0, source);
      Require(EncodeValue(type, value) == source, "writer changed padding/NaN bits on no-op");
      SetString(type, Child(type, value, "label"), "abcde");
      auto changed = EncodeValue(type, value);
      Require(changed.size() == 24 && changed[10] == 0 && changed[11] == 0 &&
                  Le(changed, 12, 4) == 0x7fc01234 && Le(changed, 20, 4) == 0xffff0001,
              "writer lost alignment/untouched atom bits after changing a preceding field");
      Require(EncodeValue(type, DecodeValue(type, 0, changed)) == changed,
              "changed variable-size value does not roundtrip");
      source.push_back(1);
      rejects([&] { DecodeValue(type, 0, source); }, "complete value");
      source.pop_back();
      WriteInteger(source, 12, 1000000, 4);
      rejects([&] { DecodeValue(type, 0, source); }, "count exceeds input extent");
    }
    il2cpp_thread_attach = Fixture::Attach;
    il2cpp_thread_detach = Fixture::Detach;
    {
      Entry::ManagedAttachment managed;
      managed.Attach((void *)0xABC);
      managed.Attach((void *)0xABC);
      Require(Fixture::attachCalls == 1 && Fixture::attached == 1,
              "startup polling attached the same thread repeatedly");
      managed.Detach();
      Require(Fixture::attached == 0 && Fixture::detachCalls == 1,
              "native poller still registered with IL2CPP after startup");
      managed.Detach();
    }
    Require(Fixture::detachCalls == 1,
            "thread lifetime cleanup duplicated detach");
    try {
      Entry::ManagedAttachment managed;
      managed.Attach((void *)0xABC);
      throw std::runtime_error("startup failure fixture");
    } catch (const std::runtime_error &) {
    }
    Require(Fixture::attached == 0 && Fixture::detachCalls == 2,
            "startup failure leaked managed attachment");
    auto detach = il2cpp_thread_detach;
    il2cpp_thread_detach = nullptr;
    bool lifetimeRejected = false;
    try {
      Entry::ManagedAttachment managed;
      managed.Attach((void *)0xABC);
    } catch (const std::exception &) {
      lifetimeRejected = true;
    }
    il2cpp_thread_detach = detach;
    Require(lifetimeRejected && Fixture::attachCalls == 2 &&
                Fixture::attached == 0,
            "missing detach export still attached a native thread");
    auto logPath =
        std::filesystem::temp_directory_path() /
        ("eff-log-sharing-" + std::to_string(GetCurrentProcessId()) + ".txt");
    auto writer = Entry::OpenLog(logPath);
    Require(writer != nullptr, "fixture log open failed");
    fputs("live-log-fixture", writer);
    fflush(writer);
    HANDLE reader = CreateFileW(logPath.c_str(), GENERIC_READ,
                                FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (reader == INVALID_HANDLE_VALUE) {
      fclose(writer);
      std::filesystem::remove(logPath);
      throw std::runtime_error(
          "log cannot be read while DLL writer remains open");
    }
    char logText[32] = {};
    DWORD readBytes = 0;
    bool readable = ReadFile(reader, logText, sizeof(logText), &readBytes,
                             nullptr) != FALSE;
    CloseHandle(reader);
    auto competingWriter = Entry::OpenLog(logPath);
    bool competingRejected = competingWriter == nullptr;
    if (competingWriter)
      fclose(competingWriter);
    fputs("-tail", writer);
    fflush(writer);
    fclose(writer);
    auto sharedLogBytes = Read(logPath);
    auto exclusiveReopen = Entry::OpenLog(logPath, true);
    if (exclusiveReopen)
      fclose(exclusiveReopen);
    std::filesystem::remove(logPath);
    Require(readable && std::string(logText, readBytes) == "live-log-fixture",
            "live log bytes missing");
    Require(competingRejected &&
                std::string(sharedLogBytes.begin(), sharedLogBytes.end()) ==
                    "live-log-fixture-tail",
            "a second log writer can truncate the active startup log");
    Require(exclusiveReopen == nullptr,
            "session log creation overwrites an existing run");
    auto mirrorPath = logPath;
    mirrorPath += ".mirror";
    Entry::logFile = Entry::OpenLog(logPath, true);
    Entry::logMirror = Entry::OpenLog(mirrorPath, true);
    Require(Entry::logFile && Entry::logMirror,
            "session log fixture open failed");
    Log("[ENTRY-FIXTURE] session evidence");
    fclose(Entry::logFile);
    fclose(Entry::logMirror);
    Entry::logFile = Entry::logMirror = nullptr;
    auto sessionLog = Read(logPath), mirrorLog = Read(mirrorPath);
    std::filesystem::remove(logPath);
    std::filesystem::remove(mirrorPath);
    Require(sessionLog == mirrorLog &&
                std::string(sessionLog.begin(), sessionLog.end())
                        .find("[pid=" + std::to_string(GetCurrentProcessId()) +
                              " tid=") != std::string::npos,
            "session/mirror logs disagree or lose process/thread identity");
    const unsigned char golden[] = {0x35, 'a', 'b', 'c', 3, 0};
    auto decoded = EiemNativeInput::Lz4(golden, sizeof(golden), 12);
    Require(std::string(decoded.begin(), decoded.end()) == "abcabcabcabc",
            "LZ4 overlapping match decoded incorrectly");
    const unsigned char inverted[] = {0x47, 'a', 'b', 'c', 0, 3};
    Require(EiemNativeInput::Lz4(inverted, sizeof(inverted), 12, true) ==
                decoded,
            "Endfield inverse LZ4 token/offset/overlap differs");
    const unsigned char invalid[] = {0x35, 'a', 'b', 'c', 0, 0};
    bool rejected = false;
    try {
      EiemNativeInput::Lz4(invalid, sizeof(invalid), 12);
    } catch (const std::exception &) {
      rejected = true;
    }
    Require(rejected, "invalid LZ4 offset was accepted");
    il2cpp_class_get_methods = Fixture::Next;
    il2cpp_method_get_name = Fixture::Name;
    il2cpp_method_get_param_count = Fixture::Count;
    il2cpp_method_get_flags = Fixture::Flags;
    il2cpp_method_get_return_type = Fixture::Return;
    il2cpp_method_get_param = Fixture::Param;
    il2cpp_type_get_name = Fixture::TypeName;
    il2cpp_free = Fixture::Free;
    il2cpp_class_value_size = Fixture::ValueSize;
    il2cpp_class_get_type = Fixture::ClassType;
    il2cpp_type_get_type = Fixture::TypeKind;
    il2cpp_class_get_fields = Fixture::NextField;
    il2cpp_field_get_flags = Fixture::FieldFlags;
    il2cpp_field_get_type = Fixture::FieldType;
    il2cpp_field_get_offset = Fixture::FieldOffset;
    Fixture::Method hashMethod{(void *)Fixture::VfsOriginal,
                               "LoadBundleFromFile",
                               "UnityEngine.AssetBundle",
                               {"Beyond.Resource.StringPathHash"},
                               0};
    Fixture::Class hashClass{{&hashMethod}};
    Require(Entry::Exact(&hashClass, "LoadBundleFromFile", {Entry::vfsPathType},
                         "UnityEngine.AssetBundle", false) == &hashMethod,
            "production VFS signature rejects the observed StringPathHash "
            "overload");
    Require(Entry::HashAbi(&hashClass), "8-byte hash value ABI rejected");
    Fixture::valueAlignment = 1;
    Require(Entry::HashAbi(&hashClass),
            "observed packed 8-byte hash value ABI rejected");
    Fixture::valueAlignment = 16;
    Require(!Entry::HashAbi(&hashClass),
            "unverified over-aligned hash ABI accepted");
    Fixture::valueAlignment = 8;
    Fixture::valueSize = 16;
    Require(!Entry::HashAbi(&hashClass), "large hash value ABI accepted");
    Fixture::valueSize = 8;
    Fixture::valueKind = 0x12;
    Require(!Entry::HashAbi(&hashClass), "reference-type hash ABI accepted");
    Fixture::valueKind = 0x11;
    Fixture::fields[0].offset = 24;
    Require(!Entry::HashAbi(&hashClass),
            "incompatible hash field layout accepted");
    Fixture::fields[0].offset = 16;
    Fixture::fields[0].type = "System.Double";
    Require(!Entry::HashAbi(&hashClass), "floating-point hash ABI accepted");
    Fixture::fields[0].type = "System.Int64";
    Fixture::fields.push_back({"System.Int64", 24});
    Require(!Entry::HashAbi(&hashClass), "extra hash instance field accepted");
    Fixture::fields.back().flags = 0x10;
    Require(Entry::HashAbi(&hashClass),
            "static field invalidates compatible hash ABI");
    Fixture::fields.pop_back();
    Fixture::valueSize = 1;
    Fixture::valueAlignment = 1;
    Fixture::fields[0].type = "System.Byte";
    Require(Entry::LoaderPosAbi(&hashClass),
            "byte loader-position enum ABI rejected");
    Fixture::valueSize = 4;
    Require(!Entry::LoaderPosAbi(&hashClass),
            "unverified loader enum ABI accepted");
    Fixture::valueSize = 1;
    Fixture::fields[0].type = "System.Boolean";
    Require(!Entry::LoaderPosAbi(&hashClass),
            "non-byte loader enum ABI accepted");
    Fixture::valueSize = 8;
    Fixture::valueAlignment = 8;
    Fixture::fields[0].type = "System.Int64";
    Fixture::Method exact{(void *)Fixture::MemoryOriginal,
                          "LoadFromMemory_Internal",
                          "UnityEngine.AssetBundle",
                          {"System.Byte[]", "System.UInt32"},
                          0x10};
    Fixture::Class klass{{&exact}};
    Require(Entry::Exact(&klass, "LoadFromMemory_Internal",
                         {"System.Byte[]", "System.UInt32"},
                         "UnityEngine.AssetBundle", true) == &exact,
            "exact signature not resolved");
    Require(!Entry::Exact(&klass, "LoadFromMemory_Internal",
                          {"System.Byte[]", "System.UInt32"},
                          "UnityEngine.AssetBundle", false),
            "static/instance mismatch accepted");
    Require(!Entry::Exact(&klass, "LoadFromMemory_Internal",
                          {"System.Byte[]", "System.Int32"},
                          "UnityEngine.AssetBundle", true),
            "parameter mismatch accepted");
    Require(!Entry::Exact(&klass, "LoadFromMemory_Internal",
                          {"System.Byte[]", "System.UInt32"},
                          "UnityEngine.AssetBundleCreateRequest", true),
            "return mismatch accepted");
    klass.methods.push_back(&exact);
    Require(!Entry::Exact(&klass, "LoadFromMemory_Internal",
                          {"System.Byte[]", "System.UInt32"},
                          "UnityEngine.AssetBundle", true),
            "ambiguous metadata accepted");
    auto bytes = Read(std::filesystem::u8path(argv[1]));
    Bytes array(32 + bytes.size());
    uint64_t length = bytes.size();
    memcpy(array.data() + 24, &length, 8);
    memcpy(array.data() + 32, bytes.data(), bytes.size());
    auto before = array;
    Entry::queueEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    Entry::memory = Fixture::MemoryOriginal;
    Entry::read = Fixture::ReadOriginal;
    Entry::readSpan = Fixture::SpanOriginal;
    Entry::vfs = Fixture::VfsOriginal;
    Entry::vfsAsync = Fixture::VfsOriginal;
    Entry::vfsPos = Fixture::VfsPosOriginal;
    Entry::vfsAsyncPos = Fixture::VfsPosOriginal;
    Entry::getStreamHash = Fixture::VfsOriginal;
    Entry::getStreamHashPos = Fixture::StreamPosOriginal;
    Entry::hashMap = Fixture::MappingOriginal;
    Entry::hashGetPath = Fixture::GetterOriginal;
    Entry::helper = Entry::helperAsync = Fixture::HelperOriginal;
    Entry::helperStream = Entry::helperStreamAsync = Entry::helperPersist =
        Entry::helperPersistAsync = Fixture::HelperLeafOriginal;
    Entry::filePublic = Entry::fileAsyncPublic = Fixture::FileOriginal;
    void *method = (void *)0x67890;
    Fixture::calls = 0;
    auto out = Entry::Memory(array.data(), 927, method);
    Require(out == (void *)0x12340 && Fixture::calls == 1 &&
                Fixture::lastArray == array.data() && Fixture::lastCrc == 927 &&
                Fixture::lastInfo == method,
            "memory hook changed original call/return");
    Require(array == before, "memory hook modified original bytes");
    Require(Entry::jobs.size() == 1 && Entry::jobs.front().bytes == bytes,
            "complete input not safely captured");
    Require(Entry::jobs.front().origin == "<no-vfs-context>",
            "invented resource identity");
    Require(!Entry::CopyRaw((void *)1, array.data(), 8),
            "invalid memory did not fail safely");
    if (argc >= 3) {
      using namespace EiemNativeInput::EndfieldInput;
      auto native = Read(std::filesystem::u8path(argv[2]));
      auto original = native;
      auto bundle = Decode(native);
      Require(!bundle.members.empty(), "real Endfield input has no members");
      auto members = bundle.members;
      members.push_back({"example.resS", 0, Bytes{1, 2, 3, 4, 5, 6, 7}});
      auto encoded = Encode(native, members);
      auto roundtrip = Decode(encoded);
      Require(roundtrip.members.size() == members.size(),
              "Endfield writer loses members");
      for (size_t i = 0; i < members.size(); ++i)
        Require(roundtrip.members[i].name == members[i].name &&
                    roundtrip.members[i].flags == members[i].flags &&
                    roundtrip.members[i].bytes == members[i].bytes,
                "Endfield multi-member/raw-stream rewrite changes bytes or "
                "identities");
      Require(native == original && !EiemNativeInput::Inspect(native).empty(),
              "Endfield decoding changes source or loses native identities");
      std::vector<Bytes> corrupt;
      auto broken = encoded;
      broken[0] ^= 1;
      corrupt.push_back(broken);
      broken = encoded;
      broken.pop_back();
      corrupt.push_back(broken);
      broken = encoded;
      EiemNativeInput::Reader reader(broken, 10, true);
      auto f2 = uint32_t(reader.Get(4));
      Put(broken, 14, 9 ^ f2, 4);
      corrupt.push_back(broken);
      broken = encoded;
      Put(broken, 48, EncodeCount(1000001, 0x23F77B8A, 0x91CE0A4F), 4);
      corrupt.push_back(broken);
      for (const auto &bytes : corrupt) {
        bool rejected = false;
        try {
          Decode(bytes);
        } catch (const std::exception &) {
          rejected = true;
        }
        Require(rejected, "invalid Endfield input accepted");
      }
      bool snapshotRejected = false;
      try {
        Snapshot(std::filesystem::u8path(argv[2]), UINT64_MAX);
      } catch (const std::exception &) {
        snapshotRejected = true;
      }
      Require(snapshotRejected, "file snapshot accepted an out-of-file offset");
    }
    Fixture::result = 64;
    Fixture::calls = 0;
    Require(Entry::Read((void *)0x11200, array.data(), 0, 64, method) == 64 &&
                Fixture::calls == 1 && Fixture::lastInfo == method &&
                array == before,
            "Read hook changes call/result/buffer");
    Fixture::result = -9;
    Require(Entry::Read((void *)0x11200, array.data(), 0, 64, method) == -9,
            "negative Read result changed");
    Fixture::result = 65;
    Require(Entry::Read((void *)0x11200, array.data(), -1, 64, method) == 65,
            "invalid Read result changed");
    struct Span {
      void *data;
      int length;
      int padding;
    };
    Span span{array.data() + 32, 64, 0};
    Fixture::result = 64;
    Require(Entry::ReadSpan((void *)0x11300, &span, method) == 64 &&
                array == before,
            "Span hook changes original");
    Entry::active = false;
    auto count = Entry::jobs.size();
    Entry::Memory(array.data(), 12, method);
    Require(count == Entry::jobs.size(), "disabled capture still enqueued");
    Entry::active = true;
    Entry::Observe(
        [] { throw std::runtime_error("expected fixture rejection"); });
    Require(Entry::bundleContext == nullptr, "unexpected TLS context");
    Fixture::calls = 0;
    int64_t pathHash = (int64_t)0xFEDCBA9876543210ULL;
    void *owner = (void *)0x12340;
    Require(Entry::Vfs(owner, pathHash, method) == (void *)0x34560 &&
                Fixture::calls == 1 && Fixture::lastSelf == owner &&
                Fixture::lastHash == pathHash && Fixture::lastInfo == method &&
                Entry::bundleContext == nullptr,
            "VFS hash value/call/result or TLS restore changed");
    Require(Entry::lastVfsSync.instance==owner&&Entry::lastVfsSync.hash==pathHash,
            "completed sync load did not record its query context");
    Fixture::vfsResult=nullptr;
    Require(Entry::Vfs((void*)0x99880,42,method)==nullptr&&
                Entry::lastVfsSync.instance==owner&&Entry::lastVfsSync.hash==pathHash,
            "failed sync load replaced the successful query context");
    Fixture::vfsResult=(void*)0x34560;
    for (auto hook : {Entry::VfsPos, Entry::VfsAsyncPos}) {
      unsigned char pos = 0;
      Fixture::calls = 0;
      Entry::Context outer("outer-call");
      Require(hook(owner, pathHash, &pos, 0xAB12CD34, method) ==
                      (void *)0x34560 &&
                  Fixture::calls == 1 && Fixture::lastHash == pathHash &&
                  Fixture::lastPos == &pos && Fixture::lastCrc == 0xAB12CD34 &&
                  pos == 2 && Fixture::lastInfo == method &&
                  !strcmp(Entry::bundleContext, "outer-call"),
              "VFS position/CRC forwarding or nested TLS restore changed");
    }
    for (auto hook : {Entry::VfsAsync, Entry::GetStreamHash}) {
      Fixture::calls = 0;
      Require(hook(owner, pathHash, method) == (void *)0x34560 &&
                  Fixture::calls == 1 && Fixture::lastHash == pathHash &&
                  Entry::bundleContext == nullptr,
              "async/stream hash forwarding or TLS restore changed");
    }
    unsigned char streamPos = 0;
    Fixture::calls = 0;
    Require(Entry::GetStreamHashPos(owner, pathHash, &streamPos, method) ==
                    (void *)0x34560 &&
                Fixture::calls == 1 && Fixture::lastHash == pathHash &&
                streamPos == 2 && Fixture::lastPos == &streamPos &&
                Fixture::lastInfo == method && Entry::bundleContext == nullptr,
            "stream hash position forwarding or TLS restore changed");
    auto pathString = Fixture::ManagedString(L"assets/example.bundle");
    auto dirString = Fixture::ManagedString(L"Data/Bundles/Windows");
    uint64_t fileOffset = 0xFEDCBA9876543210ULL;
    for (auto hook : {Entry::Helper, Entry::HelperAsync}) {
      Fixture::calls = 0;
      Entry::Context outer("outer-call");
      Require(hook(dirString.data(), pathString.data(), fileOffset, 0xFE,
                   0xAB12CD34, method) == (void *)0x45670 &&
                  Fixture::calls == 1 && Fixture::lastDir == dirString.data() &&
                  Fixture::lastName == pathString.data() &&
                  Fixture::lastOffset == fileOffset &&
                  Fixture::lastLoaderPos == 0xFE &&
                  Fixture::lastCrc == 0xAB12CD34 &&
                  Fixture::lastInfo == method &&
                  !strcmp(Entry::bundleContext, "outer-call"),
              "helper changes enum/offset/CRC/MethodInfo/call/result/context");
    }
    for (auto hook : {Entry::HelperStream, Entry::HelperStreamAsync,
                      Entry::HelperPersist, Entry::HelperPersistAsync}) {
      Fixture::calls = 0;
      Require(hook(dirString.data(), pathString.data(), fileOffset, 0xAB12CD34,
                   method) == (void *)0x45670 &&
                  Fixture::calls == 1 && Fixture::lastDir == dirString.data() &&
                  Fixture::lastName == pathString.data() &&
                  Fixture::lastOffset == fileOffset &&
                  Fixture::lastCrc == 0xAB12CD34 &&
                  Fixture::lastInfo == method &&
                  Entry::bundleContext == nullptr,
              "streaming/persist helper changes original call/result/context");
    }
    for (auto hook : {Entry::FilePublic, Entry::FileAsyncPublic}) {
      Fixture::calls = 0;
      Require(
          hook(pathString.data(), 0xAB12CD34, fileOffset, method) ==
                  (void *)0x45670 &&
              Fixture::calls == 1 && Fixture::lastName == pathString.data() &&
              Fixture::lastCrc == 0xAB12CD34 &&
              Fixture::lastOffset == fileOffset &&
              Fixture::lastInfo == method && Entry::bundleContext == nullptr,
          "public Unity file observer changes "
          "path/offset/CRC/MethodInfo/return");
    }
    Entry::hashLoadSamples = 0;
    Entry::hashStreamSamples = 0;
    for (unsigned i = 0; i < 200; ++i)
      Entry::GetStreamHash(owner, pathHash, method);
    Require(Entry::hashLoadSamples == 0 && Entry::hashStreamSamples == 200,
            "configuration hash streams consume bundle-event budget");
    Entry::Vfs(owner, pathHash, method);
    Require(Entry::hashLoadSamples == 1,
            "bundle events lost their separate sampling budget");
    Fixture::stringResult = pathString.data();
    void *mapped = nullptr;
    Fixture::calls = 0;
    Require(Entry::HashMap(owner, pathHash, &mapped, method) &&
                Fixture::calls == 1 && mapped == pathString.data() &&
                Fixture::lastHash == pathHash &&
                Entry::HashPath(pathHash) == "assets/example.bundle",
            "passive hash mapping changes original or loses observed path");
    Require(Entry::HashGetPath(&pathHash, method) == pathString.data() &&
                Fixture::lastSelf == &pathHash,
            "passive getter changes original result/receiver");
    Fixture::stringResult = nullptr;
    Require(!Entry::HashMap(owner, pathHash, &mapped, method) &&
                mapped == nullptr,
            "failed mapping changes original result/output");
    Require(Entry::HashPath(999).find("<pathhash:") == 0,
            "unknown hash invented a path");
    auto conflicting = Fixture::ManagedString(L"assets/conflicting.bundle");
    Entry::RememberHash(pathHash, conflicting.data());
    Require(Entry::HashPath(pathHash) == "<ambiguous-pathhash>",
            "conflicting mapping accepted");
    auto sampleCount = Entry::inputSamples.load();
    const char config[] = "{\"configuration\":1}";
    Entry::Sample("VFS.Read.byte[]", (void *)0x22300, config, sizeof(config),
                  "<none>");
    Require(Entry::inputSamples.load() == sampleCount,
            "config reads exhaust bundle samples");
    Entry::Sample("VFS.Read.byte[]", (void *)0x22400, bytes.data(),
                  bytes.size(), "<none>");
    Require(Entry::inputSamples.load() == sampleCount + 1,
            "recognizable bundle header outside context was discarded");
    Entry::jobs.clear();
    Entry::queuedBytes = 0;
    Entry::fileSnapshotRequests = 0;
    Entry::genericSnapshots = 0;
    Entry::queuedSnapshots.clear();
    Entry::selectedSnapshots.clear();
    auto absoluteString = Fixture::ManagedString(L"C:\\fixture\\observed.chk");
    auto normalized = Entry::SnapshotPath("C:/fixture/observed.chk");
    for (unsigned i = 0; i < 8; ++i)
      Entry::selectedSnapshots.emplace(Entry::SnapshotKey{normalized, 100 + i},
                                       48 + i);
    for (unsigned i = 0; i < 20; ++i)
      Entry::QueueFileSnapshot("fixture", absoluteString.data(), i);
    Require(Entry::fileSnapshotRequests == 0 && Entry::jobs.empty(),
            "unselected diagnostic inputs were captured");
    for (unsigned i = 0; i < 8; ++i) {
      Entry::QueueFileSnapshot("fixture", absoluteString.data(), 100 + i);
      Entry::QueueFileSnapshot("fixture", absoluteString.data(), 100 + i);
    }
    Require(Entry::fileSnapshotRequests == 8 && Entry::jobs.size() == 8 &&
                Entry::jobs.back().expectedBytes == 55 &&
                Entry::queuedBytes == 0,
            "selected copies lost to generic events, duplicated, or read in "
            "callback");
    Entry::jobs.clear();
    AP_ReloadConfig();
    Require(Entry::reload.exchange(false), "manager refresh request lost");
    CloseHandle(Entry::queueEvent);
    Entry::queueEvent = nullptr;
    std::cout << "PASS managed attachment cleanup; live log readers; exclusive "
                 "writers/session creation; session mirror/PID/TID; separate "
                 "bundle/stream budgets; public Unity/helper argument/result "
                 "preservation; byte-enum ABI; packed "
                 "hash ABI; "
                 "hash signature/ABI gates; hash/position/CRC forwarding; "
                 "passive mappings; "
                 "bundle-specific sampling; exact-signature gates; "
                 "call/return/buffer preservation; TLS restoration; "
                 "guarded reads; complete-input copy; disabled observer; "
                 "refresh request\n";
    if (argc >= 3)
      std::cout
          << "PASS real Endfield decode/identity; multi-member/raw-stream "
             "writer; bad header/truncated/revision/count rejection; "
             "out-of-file snapshot rejection; selected/generic capture "
             "budgets; deduplication; callback owns path only\n";
    if (argc >= 4) {
      using namespace EiemNativeInput;
      auto source = Read(std::filesystem::u8path(argv[2]));
      auto package = Load(std::filesystem::u8path(argv[3]));
      {
        using namespace Entry::SkinningObservation;
        size_t audited = 0;
        for (const auto &row : package.meshes) {
          const auto &mesh = row.second;
          const auto &resource = *package.resources.at(mesh.resource);
          auto input = AuthorVertices(resource);
          Require(input.count == mesh.vertices, "native geometry readback expected vertex count");
          for (size_t v = 0; v < input.count; ++v) {
            auto weight = input.Skin(v);
            Require(WeightError(weight, weight, mesh.bones.size()) == 0, "native weight comparison rejects valid payload");
            for (float p : input.Position(v)) Require(std::isfinite(p), "native geometry readback expected position");
          }
          auto truncated = resource;
          --truncated.blocks.at("m_VertexData").size;
          rejects([&] { AuthorVertices(truncated); }, "stream extent");
          ++audited;
        }
        NativeWeight expected{{.25f, .75f, 0, 0}, {1, 3, 0, 0}};
        NativeWeight reordered{{.75f, .25f, 0, 0}, {3, 1, 999, -1}};
        Require(WeightError(expected, reordered, 4) == 0, "weight comparison depends on tuple order or padding");
        reordered.bones[0] = 2;
        Require(WeightError(expected, reordered, 4) == .75, "weight comparison missed palette slot permutation");
        reordered.bones[0] = 4;
        rejects([&] { WeightError(expected, reordered, 4); }, "bone index range");
        reordered.bones[0] = 3; reordered.weights[0] = -1;
        rejects([&] { WeightError(expected, reordered, 4); }, "weight range");
        std::cout << "PASS native geometry readback expected streams: " << audited
                  << " real parts; bounded extent rejection; semantic per-bone weight comparison; invalid palette/range rejection\n";
      }
      auto bundle = EndfieldInput::Decode(source);
      {
        auto root = std::filesystem::temp_directory_path() /
            ("eff-loading-control-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(GetTickCount64()));
        Require(!std::filesystem::exists(root), "loading control fixture directory exists");
        auto original = root / "Endfield_Data" / "source.chk";
        auto cache = root / "plugin" / "resource-export-staging" / "control" / "output.vfs";
        auto tablePath = root / "control.tsv";
        std::filesystem::create_directories(original.parent_path());
        std::filesystem::create_directories(cache.parent_path());
        auto writeBytes = [](const auto& path, const Bytes& bytes) {
          std::ofstream file(path, std::ios::binary | std::ios::trunc);
          file.write((const char*)bytes.data(), bytes.size());
          Require(bool(file), "loading control fixture write failed");
        };
        Bytes disk(16);
        disk.insert(disk.end(), source.begin(), source.end());
        writeBytes(original, disk);
        auto hashText = [](const Bytes& bytes) {
          auto hash = Sha(bytes.data(), bytes.size());
          return Hex(hash.data(), hash.size());
        };
        auto writeTable = [&](const std::string& version, const std::string& mode, const Bytes& output) {
          writeBytes(cache, output);
          std::ofstream file(tablePath, std::ios::binary | std::ios::trunc);
          file << "EFF_LOADING_INPUT_SUBSTITUTIONS\t" << version << "\t" << package.id << "\t"
               << PackageDigest(package) << (mode.empty() ? "" : "\t" + mode) << "\n"
               << original.u8string() << "\t16\t" << source.size() << "\t" << hashText(source) << "\t"
               << cache.u8string() << "\t" << output.size() << "\t" << hashText(output) << "\n";
          Require(bool(file), "loading control table fixture write failed");
        };
        auto readTable = [&] { return ReadInputSubstitutions(tablePath, root, package); };
        writeTable("1", "", source);
        rejects([&] { readTable(); }, "byte contract");
        writeTable("2", "raw-copy", source);
        auto raw = readTable();
        Require(raw.control == "raw-copy" && raw.entries.size() == 1 && raw.entries.begin()->second.offset == 16,
                "raw loading control lost its mode/range");
        auto reencoded = EndfieldInput::Encode(source, bundle.members);
        Require(reencoded != source, "loading control fixture must use an encoded original block");
        writeTable("2", "raw-copy", reencoded);
        rejects([&] { readTable(); }, "byte contract");
        writeTable("2", "reencoded-unchanged", reencoded);
        Require(readTable().control == "reencoded-unchanged", "unchanged reencoded control rejected");
        auto changed = bundle;
        changed.members[0].bytes.back() ^= 1;
        writeTable("2", "reencoded-unchanged", EndfieldInput::Encode(source, changed.members));
        rejects([&] { readTable(); }, "changed decoded content");
        writeTable("2", "unknown-mode", source);
        rejects([&] { readTable(); }, "version/package generation");
        std::filesystem::remove_all(root);
        std::cout << "PASS native loading controls: exact raw bytes/range; unchanged reencoded members; "
                     "production no-op rejection; control byte/content/mode rejection\n";
      }
      auto member = std::find_if(bundle.members.begin(), bundle.members.end(), [](const auto& m) {
        return m.bytes.size() >= 48 && Le(m.bytes, 8, 4) == 0x16000000;
      });
      Require(member != bundle.members.end(), "real writer fixture has no SerializedFile");
      auto layout = ParseSerialized(member->bytes);
      Require(BuildSerialized(member->bytes, layout, {}) == member->bytes,
              "serialized no-op changed complete current input");
      auto first = layout.objects.begin()->second;
      Bytes payload(member->bytes.begin() + first.offset,
                    member->bytes.begin() + first.offset + first.size);
      rejects([&] { BuildSerialized(member->bytes, layout, {}, {{first.id, first.type, payload}}); },
              "identity/type/size");
      rejects([&] { BuildSerialized(member->bytes, layout, {{0, payload}}); }, "PathID absent");
      auto assets = SerializedAssets(member->bytes, member->name, layout);
      auto asset = std::find_if(assets.begin(), assets.end(), [&](const auto& a) {
        auto rule = package.targetIndex.find({a.path, a.asset});
        return rule != package.targetIndex.end() && !package.rules.at(rule->second).skip;
      });
      Require(asset != assets.end(), "real writer fixture must contain a selected non-skip Mesh");
      const auto& rule = package.rules.at(package.targetIndex.at({asset->path, asset->asset}));
      auto meshId = package.parts.at(rule.parts.at(0)).mesh;
      auto& mesh = package.meshes.at(meshId);
      auto original = package.resources.at(mesh.resource);
      auto bad = std::make_shared<Resource>(*original);
      bad->type[0] ^= 1;
      package.resources[mesh.resource] = bad;
      rejects([&] { BuildMeshInput(source, package); }, "class/schema mismatch");
      package.resources[mesh.resource] = original;
      ++mesh.vertices;
      rejects([&] { BuildMeshInput(source, package); }, "vertex directory count");
      --mesh.vertices;
      bad = std::make_shared<Resource>(*original);
      auto indexBlock = bad->blocks.at("m_IndexBuffer");
      // Published resource integrity is gated by the directory reader. This
      // fixture bypasses that gate to exercise native geometry validation.
      WriteInteger(bad->bytes, indexBlock.offset + 4, 0xffffffff, 4);
      package.resources[mesh.resource] = bad;
      rejects([&] { BuildMeshInput(source, package); }, "index exceeds vertex count");
      package.resources[mesh.resource] = original;
      auto rebuilt = BuildMeshInput(source, package);
      {
        size_t tested = 0;
        auto native = EndfieldInput::Decode(rebuilt.bytes);
        for (const auto &member : native.members) {
          if (member.bytes.size() < 48 || member.bytes[11] != 22) continue;
          auto layout = ParseSerialized(member.bytes);
          for (auto id : layout.order) {
            const auto &type = layout.types.at(layout.objects.at(id).type);
            if (type.classId != 43) continue;
            const auto original = ObjectValue(member.bytes, layout, id);
            auto result = original;
            auto &v = Child(type, result, "m_VertexData");
            auto &cs = ArrayValue(type, Child(type, v, "m_Channels")).children;
            if (Integer(Child(type, cs[6], "dimension")) != 0x24) continue;
            const auto before = PackedUvProbe::Describe(type, v);
            const auto data = Child(type, v, "m_DataSize").raw;
            const auto offset = Integer(Child(type, cs[6], "offset"));
            Require(PackedUvProbe::Repack(type, result), "flagged UV2 was not repacked");
            const auto after = PackedUvProbe::Describe(type, v);
            const auto &output = Child(type, v, "m_DataSize").raw;
            Require(Integer(Child(type, cs[6], "format")) == 3 &&
                        Integer(Child(type, cs[6], "dimension")) == 0x24 &&
                        std::equal(data.begin(), data.begin() + before.start[1], output.begin()) &&
                        std::equal(data.begin() + before.start[2], data.end(), output.begin() + after.start[2]),
                    "UV probe changed packed positions/normals or skin weights/indices");
            for (size_t vertex = 0; vertex < before.count; ++vertex) {
              size_t from = before.start[1] + vertex * before.stride[1];
              size_t to = after.start[1] + vertex * after.stride[1];
              Require(std::equal(data.begin() + from, data.begin() + from + offset, output.begin() + to) &&
                          std::equal(data.begin() + from + offset + 16, data.begin() + from + before.stride[1],
                                     output.begin() + to + offset + 4), "UV probe changed other UV bytes");
              for (size_t component = 0; component < 4; ++component) {
                float expected = 0;
                std::memcpy(&expected, data.data() + from + offset + component * 4, 4);
                double actual = double(static_cast<signed char>(output[to + offset + component])) / 127;
                Require(std::abs(actual - expected) <= 1.0 / 254 + 1e-7, "UV probe UV2 error exceeds half SNorm8 step");
              }
            }
            for (const auto &field : original.children) {
              const auto &name = type.nodes.at(field.node).name;
              if (name != "m_VertexData")
                Require(EncodeValue(type, field) == EncodeValue(type, Child(type, result, name)),
                        "UV probe changed non-vertex Mesh field");
            }
            auto invalid = original;
            auto &invalidVertex = Child(type, invalid, "m_VertexData");
            Child(type, invalidVertex, "m_DataSize").raw.pop_back();
            rejects([&] { PackedUvProbe::Repack(type, invalid); }, "stream data extent");
            for (float bad : {2.f, -2.f, std::numeric_limits<float>::quiet_NaN()}) {
              invalid = original;
              auto &bytes = Child(type, Child(type, invalid, "m_VertexData"), "m_DataSize").raw;
              std::memcpy(bytes.data() + before.start[1] + offset, &bad, 4);
              rejects([&] { PackedUvProbe::Repack(type, invalid); }, "SNorm8 range");
            }
            for (float signedValue : {-.5f, .5f}) {
              auto rounded = original;
              auto &bytes = Child(type, Child(type, rounded, "m_VertexData"), "m_DataSize").raw;
              std::memcpy(bytes.data() + before.start[1] + offset, &signedValue, 4);
              PackedUvProbe::Repack(type, rounded);
              Require(static_cast<signed char>(bytes[after.start[1] + offset]) == (signedValue < 0 ? -64 : 64),
                      "UV probe signed half-step quantization");
            }
            invalid = original;
            auto &unflagged = ArrayValue(type, Child(type, Child(type, invalid, "m_VertexData"), "m_Channels")).children[6];
            SetInteger(type, Child(type, unflagged, "dimension"), 4);
            auto unchanged = EncodeValue(type, invalid);
            Require(!PackedUvProbe::Repack(type, invalid) && EncodeValue(type, invalid) == unchanged,
                    "UV probe modified unflagged channel");
            ++tested;
          }
        }
        Require(tested > 0, "packed-UV probe real fixture absent");
        std::cout << "PASS packed UV2 SNorm8 probe: " << tested
                  << " real Meshes; signed values and half-step quantization; positions/normals/skin/other UV preservation; truncated/range rejection; unflagged no-op\n";
      }
      Require(rebuilt.matches && !rebuilt.parts.empty() && rebuilt.bytes != source,
              "real selected Mesh content did not rebuild");
      auto rebuiltBundle = EndfieldInput::Decode(rebuilt.bytes);
      for (const auto& rebuiltMember : rebuiltBundle.members) {
        if (rebuiltMember.bytes.size() < 48 || Le(rebuiltMember.bytes, 8, 4) != 0x16000000) continue;
        auto rebuiltLayout = ParseSerialized(rebuiltMember.bytes);
        Require(std::is_sorted(rebuiltLayout.order.begin(), rebuiltLayout.order.end()),
                "rebuilt Mesh object table is not in signed PathID order");
      }
      std::cout << "PASS variable-size alignment; nonzero source padding/NaN preservation; "
                   "complete-field consumption; bad array rejection; original-input no-op; "
                   "PathID collision/absent-target rejection; schema/count/index-range rejection; "
                   "real selected Mesh content rebuild\n";
    }
    if (argc >= 5) {
      using namespace EiemNativeInput;
      auto package = Load(std::filesystem::u8path(argv[3]));
      std::vector<std::pair<std::string, Bytes>> inputs;
      AssetIndex index;
      for (const auto& entry : std::filesystem::directory_iterator(std::filesystem::u8path(argv[4]))) {
        if (entry.path().extension() != ".vfs") continue;
        auto input = Read(entry.path());
        AddAssets(index, input);
        inputs.emplace_back(entry.path().filename().u8string(), std::move(input));
      }
      size_t textures = 0, materials = 0, bindings = 0;
      bool streamRejection = false, materialRejection = false, largeOutput = false;
      for (const auto& input : inputs) {
        auto built = BuildTextureInput(input.second, package, index);
        if (!built.textures.empty()) {
          auto before = EndfieldInput::Decode(input.second);
          auto after = EndfieldInput::Decode(built.bytes);
          for (const auto& texture : built.textures) {
            auto find = [&](const auto& members) -> const Bytes& {
              auto row = std::find_if(members.begin(), members.end(),
                  [&](const auto& item) { return item.name == texture.cab; });
              Require(row != members.end(), "Texture regression CAB absent"); return row->bytes;
            };
            const auto& oldBytes = find(before.members); const auto& newBytes = find(after.members);
            auto oldLayout = ParseSerialized(oldBytes), newLayout = ParseSerialized(newBytes);
            const auto& type = oldLayout.types.at(oldLayout.objects.at(texture.sourceId).type);
            Require(texture.id != texture.sourceId && !oldLayout.objects.count(texture.id) &&
                newLayout.objects.count(texture.id), "Texture clone identity not isolated");
            Require(EncodeValue(type, ObjectValue(oldBytes, oldLayout, texture.sourceId)) ==
                EncodeValue(type, ObjectValue(newBytes, newLayout, texture.sourceId)),
                "original Texture changed by authored binding");
          }
        }
        textures += built.textures.size();
        materials += built.materialMatches;
        bindings += built.textureBindings;
        if (built.bytes.size() > 64ull * 1024 * 1024) {
          largeOutput = true;
          rejects([&] { EndfieldInput::Decode(built.bytes); }, "size exceeds bounds");
          Require(!EndfieldInput::Decode(built.bytes).members.empty(),
                  "explicit bounded Texture output cannot decode");
        }
        if (!built.textures.empty() && !streamRejection) {
          streamRejection = true;
          auto missing = EndfieldInput::Decode(input.second);
          auto streamName = built.textures[0].stream;
          missing.members.erase(std::remove_if(missing.members.begin(), missing.members.end(),
              [&](const auto& member) { return member.name == streamName; }), missing.members.end());
          auto absentStream = EndfieldInput::Encode(input.second, missing.members);
          rejects([&] { BuildTextureInput(absentStream, package, index); }, "stream member absent");
          auto texture = std::find_if(package.textures.begin(), package.textures.end(), [&](const auto& row) {
            return row.second.source.path == built.textures[0].target.path &&
                   row.second.source.asset == built.textures[0].target.asset;
          });
          Require(texture != package.textures.end(), "fixture Texture declaration missing");
          ++texture->second.width;
          rejects([&] { BuildTextureInput(input.second, package, index); }, "pixels differ from INI mip size");
          --texture->second.width;
          auto source = EndfieldInput::Decode(input.second);
          auto selectedMember = std::find_if(source.members.begin(), source.members.end(), [&](const auto& member) {
            return member.name == built.textures[0].cab;
          });
          auto layout = ParseSerialized(selectedMember->bytes);
          auto id = built.textures[0].sourceId;
          auto value = ObjectValue(selectedMember->bytes, layout, id);
          const auto& type = layout.types.at(layout.objects.at(id).type);
          SetInteger(type, Child(type, Child(type, value, "m_StreamData"), "offset"), UINT64_MAX);
          selectedMember->bytes = BuildSerialized(selectedMember->bytes, layout,
                                                    {{id, EncodeValue(type, value)}});
          auto invalidRange = EndfieldInput::Encode(input.second, source.members);
          rejects([&] { BuildTextureInput(invalidRange, package, index); }, "current Texture stream range");
        }
        if (built.textureBindings && !materialRejection) {
          materialRejection = true;
          AssetIndex empty;
          rejects([&] { BuildTextureInput(input.second, package, empty); }, "identity absent from current input graph");
          auto fakeInput = input.second;
          auto material = std::find_if(package.materials.begin(), package.materials.end(), [&](const auto& row) {
            auto assets = Inspect(fakeInput);
            return !row.second.textures.empty() && std::any_of(assets.begin(), assets.end(), [&](const auto& a) {
              return a.path == row.second.source.path && a.asset == row.second.source.asset;
            });
          });
          Require(material != package.materials.end(), "fixture Material declaration missing");
          auto& edit = *material->second.textures.begin();
          auto selected = package.textures.at(edit.second).source;
          auto ambiguous = index;
          ambiguous.at(selected).pathId += 1;
          rejects([&] { BuildTextureInput(input.second, package, ambiguous); }, "requires preload registration");
          auto binding = *material->second.textures.begin();
          material->second.textures.erase(binding.first);
          material->second.textures.emplace("_fixture_absent_property", binding.second);
          rejects([&] { BuildTextureInput(input.second, package, index); }, "texture property absent");
          material->second.textures.erase("_fixture_absent_property");
          material->second.textures.emplace(binding);
        }
      }
      Require(textures == 11 && materials == 5 && bindings == 14 && streamRejection &&
                  materialRejection && largeOutput, "Texture/Material regression fixture coverage");
      {
        auto dual = package;
        auto original = std::find_if(dual.textures.begin(), dual.textures.end(), [](const auto& row) {
          return row.second.source.asset == "T_actor_lizhiyan_cloth_01_D";
        });
        Require(original != dual.textures.end(), "shared head Texture fixture absent");
        auto variant = original->second; variant.resource += ".fixture";
        auto pixels = std::make_shared<Resource>(*dual.resources.at(original->second.resource));
        pixels->bytes.at(pixels->blocks.at("pixels").offset) ^= 1;
        dual.resources[variant.resource] = pixels;
        auto section = original->first + "Variant"; dual.textures[section] = variant;
        auto head = std::find_if(dual.materials.begin(), dual.materials.end(), [](const auto& row) {
          return row.second.source.asset == "M_actor_lizhiyan_cloth_01";
        });
        Require(head != dual.materials.end(), "head Material fixture absent");
        head->second.textures["_BaseMap"] = section;
        size_t sharedClones = 0, dualBindings = 0;
        std::set<int64_t> ids;
        for (const auto& input : inputs) {
          auto result = BuildTextureInput(input.second, dual, index);
          dualBindings += result.textureBindings;
          for (const auto& row : result.textures)
            if (row.target.asset == variant.source.asset) { ++sharedClones; ids.insert(row.id); }
        }
        Require(sharedClones == 2 && ids.size() == 2 && dualBindings == 15,
                "independent authored variants of one native Texture template collide");
      }
      std::cout << "PASS current Material/Texture graph identities; all mip pixel bytes; "
                   "original Texture objects preserved; independent shared-template clones; "
                   "missing stream and invalid source range rejection; missing identity/property rejection; "
                   "new dependency preload gate; INI/pixel count rejection; "
                   "capture 64 MiB limit retained; explicit 256 MiB rebuilt output bound\n";
    }
    if (argc == 7) {
      using namespace EiemNativeInput;
      auto package = Load(std::filesystem::u8path(argv[3]));
      AssetIndex assets;
      MeshSchemaIndex schemas;
      std::vector<BuiltMeshPart> meshes;
      for (const auto& entry : std::filesystem::directory_iterator(std::filesystem::u8path(argv[5]))) {
        if (entry.path().extension() != ".vfs") continue;
        auto bytes = Read(entry.path());
        AddAssets(assets, bytes); AddMeshSchemas(schemas, bytes);
        auto rebuilt = BuildMeshInput(bytes, package);
        meshes.insert(meshes.end(), rebuilt.parts.begin(), rebuilt.parts.end());
      }
      auto source = Read(std::filesystem::u8path(argv[6]));
      auto rebuilt = BuildRendererInput(source, package, assets, meshes, schemas);
      Require(rebuilt.matches && rebuilt.addedObjects && rebuilt.skipped && rebuilt.lodGroups &&
                  rebuilt.preloadReferences, "Renderer real fixture expansion coverage");
      auto rebuiltBundle = EndfieldInput::Decode(rebuilt.bytes);
      for (const auto& rebuiltMember : rebuiltBundle.members) {
        if (rebuiltMember.bytes.size() < 48 || Le(rebuiltMember.bytes, 8, 4) != 0x16000000) continue;
        auto rebuiltLayout = ParseSerialized(rebuiltMember.bytes);
        Require(std::is_sorted(rebuiltLayout.order.begin(), rebuiltLayout.order.end()),
                "expanded Renderer object table is not in signed PathID order");
      }
      rejects([&] { BuildRendererInput(source, package, assets, {}, schemas); }, "Mesh dependency not rebuilt");
      rejects([&] { BuildRendererInput(source, package, assets, meshes, {}); }, "current Mesh TypeTree");
      const auto& part = package.parts.at(rebuilt.parts[0].part);
      auto& bones = package.meshes.at(part.mesh).bones;
      auto original = bones.at(0);
      bones[0] = "fixture/missing/bone";
      rejects([&] { BuildRendererInput(source, package, assets, meshes, schemas); }, "unique local hierarchy anchor");
      bones[0] = original;
      auto material = assets.find(package.materials.at(part.material).source);
      auto originalMaterial = material->second;
      material->second.cab = "CAB-fixture-unregistered-external";
      rejects([&] { BuildRendererInput(source, package, assets, meshes, schemas); }, "external registration");
      material->second = originalMaterial;
      auto bundle = EndfieldInput::Decode(source);
      auto member = std::find_if(bundle.members.begin(), bundle.members.end(), [&](const auto& m) {
        return m.name == rebuilt.parts[0].cab;
      });
      Require(member != bundle.members.end(), "Renderer fixture member missing");
      auto layout = ParseSerialized(member->bytes);
      auto tid = rebuilt.parts[0].parent;
      auto value = ObjectValue(member->bytes, layout, tid);
      const auto& type = layout.types.at(layout.objects.at(tid).type);
      SetReference(type, Child(type, value, "m_Father"), {member->name, tid}, layout, member->name);
      auto invalid = bundle;
      auto broken = std::find_if(invalid.members.begin(), invalid.members.end(), [&](const auto& m) { return m.name == member->name; });
      broken->bytes = BuildSerialized(member->bytes, layout, {{tid, EncodeValue(type, value)}});
      auto cycle = EndfieldInput::Encode(source, invalid.members);
      rejects([&] { BuildRendererInput(cycle, package, assets, meshes, schemas); }, "hierarchy cycle/depth");
      auto noRules = package;
      noRules.targetIndex.clear();
      Require(BuildRendererInput(source, noRules, assets, meshes, schemas).bytes == source,
              "Renderer no-match changed complete input bytes");
      std::cout << "PASS current Renderer/PFB parts; nested character hierarchy anchors; LOD/preload expansion; "
                   "missing Mesh/schema/bone rejection; unregistered Material external rejection; "
                   "Transform cycle rejection; byte-identical no-match input\n";
      auto table = std::make_shared<InputSubstitutions>();
      table->generation = Entry::generation.load();
      InputSubstitution replacement;
      replacement.source = std::filesystem::canonical(std::filesystem::u8path(argv[2]));
      replacement.output = std::filesystem::canonical(std::filesystem::u8path(argv[6]));
      replacement.sourceStamp = FileStamp(replacement.source); replacement.outputStamp = FileStamp(replacement.output);
      replacement.offset = 1234; replacement.pathHandle = 23;
      replacement.outputBytes = replacement.outputStamp.bytes;
      auto key = std::make_pair(InputPath(replacement.source.u8string()), replacement.offset);
      table->entries.emplace(key, replacement);
      auto oldSubstitutions = std::atomic_load(&Entry::substitutions);
      std::shared_ptr<const InputSubstitutions> immutable = table;
      std::atomic_store(&Entry::substitutions, immutable);
      auto sourceName = Fixture::ManagedString(replacement.source.wstring());
      auto outputName = Fixture::ManagedString(replacement.output.wstring());
      Fixture::rootedPath = outputName.data();
      il2cpp_gchandle_get_target = Fixture::HandleTarget;
      Entry::active = true;
      Entry::filePublic = Entry::fileAsyncPublic = Fixture::FileOriginal;
      void* info = (void*)0x88221;
      auto submissionCount = Entry::submittedInputs.load();
      Require(Entry::FilePublic(sourceName.data(), 0, 1234, info) == (void*)0x45670 &&
                  Fixture::lastName == outputName.data() && Fixture::lastCrc == 0 &&
                  Fixture::lastOffset == 0 && Fixture::lastInfo == info,
              "prepared sync loading arguments/result changed");
      Entry::FileAsyncPublic(sourceName.data(), 0, 1234, info);
      Require(Fixture::lastName == outputName.data() && Fixture::lastOffset == 0 && Fixture::lastInfo == info &&
                  Entry::submittedInputs == submissionCount + 2, "prepared async loading arguments changed");
      Entry::requestBundle = Fixture::GetterOriginal;
      Fixture::stringResult = (void*)0x71330;
      Require(Entry::RequestBundle((void*)0x45670, info) == Fixture::stringResult &&
                  Fixture::lastSelf == (void*)0x45670 && Fixture::lastInfo == info,
              "async bundle getter arguments/result changed");
      Entry::loadAsset = Fixture::AssetOriginal;
      void* assetType = (void*)0xAAA88;
      auto assetName = Fixture::ManagedString(L"current_fixture_asset");
      Fixture::stringResult = (void*)0x91310;
      auto loaded = Entry::loadedAssets.load();
      Require(Entry::LoadAsset((void*)0x71330, assetName.data(), assetType, info) == Fixture::stringResult &&
                  Fixture::lastSelf == (void*)0x71330 && Fixture::lastName == assetName.data() &&
                  Fixture::lastArray == assetType && Fixture::lastInfo == info && Entry::loadedAssets == loaded + 1,
              "prepared asset load observer changes arguments/result");
      Entry::FilePublic(sourceName.data(), 777, 1234, info);
      Require(Fixture::lastName == sourceName.data() && Fixture::lastCrc == 777 && Fixture::lastOffset == 1234,
              "unsupported caller CRC was discarded");
      ++table->entries.at(key).outputStamp.written;
      Entry::FilePublic(sourceName.data(), 0, 1234, info);
      Require(Fixture::lastName == sourceName.data() && Fixture::lastOffset == 1234,
              "stale prepared output still submitted");
      --table->entries.at(key).outputStamp.written;
      auto refreshed=std::make_shared<InputSubstitutions>(*table);
      ++refreshed->generation;
      std::atomic_store(&Entry::substitutions,std::shared_ptr<const InputSubstitutions>(refreshed));
      Entry::FilePublic(sourceName.data(), 0, 1234, info);
      Require(Fixture::lastName == outputName.data() && Fixture::lastOffset == 0,
              "configuration publication temporarily loses unchanged verified native input");
      std::atomic_store(&Entry::substitutions, oldSubstitutions);
      Fixture::rootedPath = nullptr;
      std::cout << "PASS prepared sync/async file submission; rooted path/offset/CRC/MethodInfo forwarding; "
                   "async bundle/asset acceptance observers preserve calls/results; "
                   "unsupported CRC/stale output preserves original input; configuration generation retains unchanged verified paths\n";
    }
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL " << e.what() << "\n";
    return 1;
  }
}
