// Runs the real diagnostic detours against a mock segmented game reader.
// The fixture DLL supplies only a module identity; no game code is executed.
#define EIEM_NATIVE_MESH_ENTRY_TRACE_BUILD
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "MinHook.h"

struct EiemSkinIdentity {};
struct EiemMeshSubmeshVisibilityState {};
struct EiemModRule { bool hasMesh = false; char asset[192] = {}; };
static void EiemFindStandaloneRenderRules(std::vector<EiemModRule> *) {}
static void TraceLookupStreamPath(void *, char *out, int) { out[0] = 0; }
static std::mutex logsMutex;
static std::vector<std::string> logs;
static void Log(const char *format, ...) {
  char line[4096];
  va_list args;
  va_start(args, format);
  vsnprintf(line, sizeof(line), format, args);
  va_end(args);
  std::lock_guard<std::mutex> guard(logsMutex);
  logs.emplace_back(line);
}
#include "eiem_native_mesh_entry_probe.h"

struct Fixture {
  unsigned char native[512] = {};
  unsigned char owner[512] = {};
  unsigned char stream[192] = {};
  unsigned char firstBlock[8192] = {};
  unsigned char lastBlock[8192] = {};
  EiemSerializedObjectReadRecord records[3] = {
      {-2, 0, 8, 0}, {0x123456789LL, 1000, 45000, 7},
      {0x234567890LL, 90000, 64, 8}};
  Fixture *nested = nullptr;
  bool raise = false;
};
template <typename T> static void Put(unsigned char *base, size_t offset, T value) {
  memcpy(base + offset, &value, sizeof(value));
}
static Fixture *FromNative(uintptr_t native) { return reinterpret_cast<Fixture *>(native); }
static void Prepare(Fixture &f) {
  const uintptr_t unity = (uintptr_t)GetModuleHandleW(L"UnityPlayer.dll");
  Put(f.native, 0, unity + kEiemNativeMeshVtableRva);
  Put(f.owner, 0x50, uint64_t{10000});
  Put(f.owner, 0xA8, (uintptr_t)&f.records[0]);
  Put(f.owner, 0xB0, (uintptr_t)&f.records[3]);
  Put(f.owner, 0x168, (uintptr_t)&f.owner[0x188]);
  unsigned char *provider = &f.owner[0x188];
  Put(provider, 0, unity + kEiemNativeMeshFileProviderVtableRva);
  static const char filePath[] = "archive:/test-bundle/CAB-test-resource";
  Put(provider, 0x30, (uintptr_t)filePath);
  Put(provider, 0x40, uint64_t{sizeof(filePath) - 1});
  provider[0x50] = 0;
  Put(f.stream, 0, uint64_t{0x800101});
  Put(f.stream, 0x30, (uintptr_t)f.firstBlock + 2808);
  Put(f.stream, 0x38, (uintptr_t)f.firstBlock);
  Put(f.stream, 0x40, (uintptr_t)f.firstBlock + 8192);
  Put(f.stream, 0x48, (uintptr_t)&f.owner[0x188]);
  Put(f.stream, 0x50, int32_t{1});
  Put(f.stream, 0x58, uint64_t{8192});
  Put(f.stream, 0x60, uint64_t{11000});
  Put(f.stream, 0x68, uint64_t{56000});
}
static uintptr_t RunOuter(Fixture &f);
static uintptr_t __fastcall FakeDeserialize(uintptr_t native, uintptr_t stream) {
  Fixture &f = *FromNative(native);
  assert(stream == (uintptr_t)f.stream);
  assert(s_eiemNativeMeshReadContext && s_eiemNativeMeshReadContext->native == native);
  if (f.raise) RaiseException(0xE0424242, 0, 0, nullptr);
  if (f.nested) {
    EiemNativeMeshReadContext *parent = s_eiemNativeMeshReadContext;
    assert(RunOuter(*f.nested) == 0xFEDCBA9876543210ULL);
    assert(s_eiemNativeMeshReadContext == parent);
  }
  Put(f.native, 0x30, (uintptr_t)"TargetMesh");
  Put(f.stream, 0x30, (uintptr_t)f.lastBlock + 6848);
  Put(f.stream, 0x38, (uintptr_t)f.lastBlock);
  Put(f.stream, 0x40, (uintptr_t)f.lastBlock + 8192);
  Put(f.stream, 0x50, int32_t{6});
  return 0xABCD;
}
static uintptr_t __fastcall FakeOuter(
    uintptr_t owner, int64_t key, uintptr_t unused, uint8_t flags,
    uintptr_t *typeOutput, uint8_t *readOutput, uintptr_t native) {
  Fixture &f = *FromNative(native);
  assert(owner == (uintptr_t)f.owner && key == f.records[1].key);
  assert(unused == 0x8765432112345678ULL && flags == 0xA5);
  assert(typeOutput && readOutput);
  *typeOutput = 0x1234;
  *readOutput = 0; // The game's output denotes the type-tree branch, not success.
  assert(EiemNativeMeshDeserializeHook(native, (uintptr_t)f.stream) == 0xABCD);
  // Original stream teardown invalidates its index after the inner observation.
  Put(f.stream, 0x50, int32_t{-1});
  return 0xFEDCBA9876543210ULL;
}
static uintptr_t RunOuter(Fixture &f) {
  uintptr_t type = 0;
  uint8_t read = 255;
  const uintptr_t result = EiemNativeObjectReadHook(
      (uintptr_t)f.owner, f.records[1].key, 0x8765432112345678ULL,
      0xA5, &type, &read, (uintptr_t)f.native);
  assert(type == 0x1234 && read == 0);
  return result;
}
static bool CatchFault(Fixture *f) {
  __try { RunOuter(*f); }
  __except (GetExceptionCode() == 0xE0424242
                ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
    return true;
  }
  return false;
}
static size_t CountLog(const char *needle) {
  size_t count = 0;
  for (const auto &line : logs) if (line.find(needle) != std::string::npos) ++count;
  return count;
}
int main(int argc, char **argv) {
  if (argc == 3 && strcmp(argv[1], "--verify-install") == 0) {
    // Map the inspected player without running DllMain or resolving imports.
    // Only MinHook's instruction relocation is exercised; no game method runs.
    const uintptr_t base = (uintptr_t)LoadLibraryExA(
        argv[2], nullptr, DONT_RESOLVE_DLL_REFERENCES);
    assert(base && EiemNativeMeshReadVerifyPlayer(base));
    unsigned char outerBefore[32], innerBefore[32];
    memcpy(outerBefore, (void *)(base+kEiemNativeObjectReadRva), sizeof(outerBefore));
    memcpy(innerBefore, (void *)(base+kEiemNativeMeshDeserializeRva), sizeof(innerBefore));
    assert(MH_Initialize() == MH_OK);
    assert(EiemInstallNativeMeshReadOwnerTrace(base));
    assert(s_eiemNativeObjectReadOriginal && s_eiemNativeMeshDeserializeOriginal);
    assert(MH_DisableHook((void *)(base+kEiemNativeObjectReadRva)) == MH_OK);
    assert(MH_DisableHook((void *)(base+kEiemNativeMeshDeserializeRva)) == MH_OK);
    assert(MH_RemoveHook((void *)(base+kEiemNativeObjectReadRva)) == MH_OK);
    assert(MH_RemoveHook((void *)(base+kEiemNativeMeshDeserializeRva)) == MH_OK);
    assert(memcmp(outerBefore, (void *)(base+kEiemNativeObjectReadRva), sizeof(outerBefore)) == 0);
    assert(memcmp(innerBefore, (void *)(base+kEiemNativeMeshDeserializeRva), sizeof(innerBefore)) == 0);
    assert(MH_Uninitialize() == MH_OK);
    puts("PASS: actual inspected player signatures accepted; paired MinHook installation/relocation succeeds; original instructions restored; no game initialization or Mesh code executed");
    return 0;
  }
  assert(argc == 2 && LoadLibraryA(argv[1]));
  assert(!EiemNativeMeshReadVerifyPlayer(
      (uintptr_t)GetModuleHandleW(L"UnityPlayer.dll")));
  assert(CountLog("reason=player-version-mismatch") == 1);
  logs.clear();
  s_eiemNativeObjectReadOriginal = FakeOuter;
  s_eiemNativeMeshDeserializeOriginal = FakeDeserialize;
  EiemNativeMeshEntryTraceTarget target;
  strcpy_s(target.asset, "TargetMesh");
  s_eiemNativeMeshEntryTraceTargets.push_back(target);
  auto parent = std::make_unique<Fixture>();
  auto child = std::make_unique<Fixture>();
  Prepare(*parent); Prepare(*child); parent->nested = child.get();
  assert(RunOuter(*parent) == 0xFEDCBA9876543210ULL);
  assert(!s_eiemNativeMeshReadContext && !s_eiemNativeMeshEntryInHook);
  assert(CountLog("fullRangeConsumed=1") == 2);
  assert(CountLog("recordStartMatch=1 recordLengthMatch=1") == 2);
  assert(CountLog("nestedObject=1") == 1);
  assert(CountLog("consumed=45000") == 2);
  assert(CountLog("blockBefore=1 blockAfter=6") == 2);
  assert(CountLog("resourceFileStatus=ok resourceFile=\"archive:/test-bundle/CAB-test-resource\"") == 2);

  auto failing = std::make_unique<Fixture>();
  Prepare(*failing); failing->raise = true;
  assert(CatchFault(failing.get()));
  assert(!s_eiemNativeMeshReadContext && !s_eiemNativeMeshEntryInHook &&
         !s_eiemNativeMeshEntryActiveNative);

  auto first = std::make_unique<Fixture>();
  auto second = std::make_unique<Fixture>();
  Prepare(*first); Prepare(*second);
  std::thread t1([&] { RunOuter(*first); assert(!s_eiemNativeMeshReadContext); });
  std::thread t2([&] { RunOuter(*second); assert(!s_eiemNativeMeshReadContext); });
  t1.join(); t2.join();
  assert(CountLog("fullRangeConsumed=1") == 4);
  assert(CountLog("[MESH-READ-FAIL]") == 0);

  Prepare(*first);
  Put(first->owner, 0x98, uintptr_t{0x1});
  RunOuter(*first);
  assert(CountLog("recordStatus=record-lock-contract-unverified") == 1);

  EiemSerializedObjectReadRecord found;
  uintptr_t address = 0;
  assert(strcmp(EiemFindSerializedObjectReadRecord(
      (uintptr_t)first->records, (uintptr_t)(first->records+3), 99,
      EiemNativeMeshEntryReadBytes, &found, &address), "record-key-not-found") == 0);
  assert(strcmp(EiemFindSerializedObjectReadRecord(
      1, 25, 1, EiemNativeMeshEntryReadBytes, &found, &address),
      "record-key-unreadable") == 0);
  EiemNativeReadWindow invalid;
  invalid.provider = 1; invalid.base = 1; invalid.cursor = 1;
  invalid.limit = 1; invalid.blockIndex = 2;
  invalid.blockSize = UINT64_MAX;
  assert(!EiemNativeReadWindowPosition(&invalid));

  // Test the exact game string representation, including bounded fault cases.
  unsigned char fileString[40] = {};
  char path[64] = {};
  memcpy(fileString, "CAB-short", 9);
  fileString[0x18] = 24 - 9;
  fileString[0x20] = 1;
  assert(!EiemReadPlayerFileString((uintptr_t)fileString,
      EiemNativeMeshEntryReadBytes, path, sizeof(path)));
  assert(strcmp(path, "CAB-short") == 0);
  fileString[0x18] = 25;
  assert(strcmp(EiemReadPlayerFileString((uintptr_t)fileString,
      EiemNativeMeshEntryReadBytes, path, sizeof(path)),
      "file-string-inline-invalid") == 0 && path[0] == 0);
  Put(fileString, 0, (uintptr_t)"CAB-borrowed");
  Put(fileString, 0x10, uint64_t{12});
  fileString[0x20] = 2;
  assert(!EiemReadPlayerFileString((uintptr_t)fileString,
      EiemNativeMeshEntryReadBytes, path, sizeof(path)));
  assert(strcmp(path, "CAB-borrowed") == 0);
  Put(fileString, 0x10, uint64_t{64});
  assert(strcmp(EiemReadPlayerFileString((uintptr_t)fileString,
      EiemNativeMeshEntryReadBytes, path, sizeof(path)),
      "file-string-too-long") == 0 && path[0] == 0);
  Put(fileString, 0, uintptr_t{1});
  Put(fileString, 0x10, uint64_t{12});
  assert(strcmp(EiemReadPlayerFileString((uintptr_t)fileString,
      EiemNativeMeshEntryReadBytes, path, sizeof(path)),
      "file-string-data-unreadable") == 0 && path[0] == 0);
  fileString[0x20] = 3;
  assert(strcmp(EiemReadPlayerFileString((uintptr_t)fileString,
      EiemNativeMeshEntryReadBytes, path, sizeof(path)),
      "file-string-mode-unverified") == 0);
  fileString[0x20] = 0;
  Put(fileString, 0, (uintptr_t)"CAB\ninvalid");
  Put(fileString, 0x10, uint64_t{11});
  assert(strcmp(EiemReadPlayerFileString((uintptr_t)fileString,
      EiemNativeMeshEntryReadBytes, path, sizeof(path)),
      "file-string-text-invalid") == 0 && path[0] == 0);
  Put(fileString, 0x10, uint64_t{0});
  assert(strcmp(EiemReadPlayerFileString((uintptr_t)fileString,
      EiemNativeMeshEntryReadBytes, path, sizeof(path)),
      "file-string-empty") == 0);
  assert(strcmp(EiemReadPlayerFileString(uintptr_t{1},
      EiemNativeMeshEntryReadBytes, path, sizeof(path)),
      "file-string-header-unreadable") == 0);
  puts("PASS: provider file string appears in the real diagnostic log; inline, owned and borrowed spans; exact bounds; invalid/empty/truncated/unreadable strings report explicit reasons without retaining pointers or calling game file services");
  puts("PASS: unsupported player rejected; seven ABI arguments/output/return preserved; exact nested and concurrent association; SEH restores context; segmented positions survive original teardown; synchronized/unreadable/missing records fail explicitly");
}
