// Load the real production DLL with the host's C++ runtimes in a child process.
// /MT keeps this harness independent of the runtimes under test. No game runs.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>

LONG WINAPI Crash(EXCEPTION_POINTERS* error) {
  HMODULE module=nullptr;wchar_t name[MAX_PATH]{};
  auto address=error->ExceptionRecord->ExceptionAddress;
  GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                    reinterpret_cast<LPCWSTR>(address),&module);
  if(module)GetModuleFileNameW(module,name,MAX_PATH);
  fwprintf(stderr,L"FAIL exception=%08lX module=%ls offset=%llX\n",
           error->ExceptionRecord->ExceptionCode,name,
           (unsigned long long)((ULONG_PTR)address-(ULONG_PTR)module));
  fflush(stderr);ExitProcess(2);
}

int wmain(int argc,wchar_t** argv) {
  if(argc!=3){fwprintf(stderr,L"usage: production_dll_load_smoke <eff.dll> <host-runtime-directory>\n");return 1;}
  SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
  SetUnhandledExceptionFilter(Crash);
  wchar_t executable[32768]{};GetModuleFileNameW(nullptr,executable,32768);
  auto slash=wcsrchr(executable,L'\\');if(!slash)return 1;*slash=0;
  wchar_t logPath[32768];swprintf_s(logPath,L"%ls\\plugin\\eff_log.txt",executable);
  // Clear only this harness's log, so a previous run cannot mask worker failure.
  auto cleanLog=CreateFileW(logPath,GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
                           nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
  if(cleanLog==INVALID_HANDLE_VALUE)return 1;
  CloseHandle(cleanLog);
  // Resolve and preload exactly the host copies, avoiding ambient PATH lookup.
  wchar_t directory[32768]{};
  if(!GetFullPathNameW(argv[2],32768,directory,nullptr))return 1;
  SetDllDirectoryW(directory);
  const wchar_t* runtimeNames[]={L"vcruntime140.dll",L"vcruntime140_1.dll",L"msvcp140.dll"};
  for(auto name:runtimeNames) {
    wchar_t path[32768];swprintf_s(path,L"%ls\\%ls",directory,name);
    if(GetFileAttributesW(path)==INVALID_FILE_ATTRIBUTES)continue;
    if(!LoadLibraryW(path)){fwprintf(stderr,L"FAIL runtime=%ls error=%lu\n",path,GetLastError());return 1;}
  }
  auto module=LoadLibraryW(argv[1]);
  if(!module){fwprintf(stderr,L"FAIL LoadLibrary error=%lu\n",GetLastError());return 1;}
  auto enable=reinterpret_cast<bool(*)()>(GetProcAddress(module,"AP_PluginEnable"));
  auto disable=reinterpret_cast<bool(*)()>(GetProcAddress(module,"AP_PluginDisable"));
  if(!enable||!disable)return 1;
  for(unsigned i=0;i<16;++i)if(!enable()||!disable())return 1;
  Sleep(500); // The startup worker must also survive its first log mutex lock.
  auto log=CreateFileW(logPath,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
                      nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
  if(log==INVALID_HANDLE_VALUE){fwprintf(stderr,L"FAIL startup log read error=%lu\n",GetLastError());return 1;}
  char data[16384]{};DWORD read=0;
  bool started=ReadFile(log,data,sizeof(data)-1,&read,nullptr)&&strstr(data,"[BUILD] resource-directory-entry");
  CloseHandle(log);if(!started){fwprintf(stderr,L"FAIL startup log missing\n");return 1;}
  fwprintf(stdout,L"PASS production DLL attach, startup logging and exported control locks; host=%ls\n",directory);
  fflush(stdout);
  // Background startup waits for GameAssembly, which this harness never loads.
  ExitProcess(0);
}
