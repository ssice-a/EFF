@echo off
setlocal
pushd "%~dp0"
rem The installed DLL accepts Format 3 author directories only.
rem Usage: build.bat [capture|load-perf|effects-debug].
set "entry_folder=bin\resource-directory-entry-format3"
set "entry_capture="
set "entry_effects="
set "entry_option=%~1"
if not "%entry_option%"=="" if /i not "%entry_option%"=="capture" if /i not "%entry_option%"=="load-perf" if /i not "%entry_option%"=="effects-debug" (
 echo [ERROR] Expected capture, load-perf or effects-debug.
 goto entry_failed
)
if /i "%entry_option%"=="capture" (
 set "entry_capture=/DEIEM_DIAGNOSTICS /DEIEM_SHAPE_CAPTURE_GFXTRACE"
) else if /i "%entry_option%"=="load-perf" (
 set "entry_capture=/DEIEM_LOAD_PERF"
 set "entry_folder=%entry_folder%-load-perf"
) else if /i "%entry_option%"=="effects-debug" (
 set "entry_effects=/DEIEM_EFFECTS_DEBUG"
 set "entry_folder=%entry_folder%-effects-debug"
)
where cl >nul 2>nul
if errorlevel 1 (
 set "entry_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
 call :entry_find_msvc
)
where cl >nul 2>nul
if errorlevel 1 (
 echo [ERROR] MSVC x64 tools unavailable. Install Visual Studio C++ Build Tools.
 goto entry_failed
)
if not exist "%entry_folder%" mkdir "%entry_folder%"
if errorlevel 1 goto entry_failed
echo [1/5] Building Format 3-only development eff.dll ...
rem Game installs may preload older app-local MSVC runtimes. Keep the plugin
rem and its UI implementation self-contained instead of sharing that runtime.
call tools\build_imgui.cmd static
if errorlevel 1 goto entry_failed
rc /nologo /fo "%entry_folder%\version.res" src\version.rc
if errorlevel 1 goto entry_failed
cl /nologo /utf-8 /O2 /Zi /MT /LD /EHsc /std:c++17 /bigobj /W4 ^
 /DEIEM_FORMAT3_RUNTIME=1 %entry_capture% %entry_effects% /DEIEM_STATIC_RUNTIME=1 /Ideps\minhook_lib\include ^
 /Fo"%entry_folder%\eff_resource_entry.obj" ^
 /Fd"%entry_folder%\eff_compile.pdb" ^
 src\eiem_resource_entry.cpp "%entry_folder%\version.res" ^
 deps\minhook_lib\lib\libMinHook.x64.lib ^
 /Fe"%entry_folder%\eff.dll" ^
 /link /DLL /DEBUG /OPT:REF /OPT:ICF /INCREMENTAL:NO /NODEFAULTLIB:MSVCRT bcrypt.lib user32.lib ^
 /PDB:"%entry_folder%\eff.pdb"
if errorlevel 1 goto entry_failed
python tools\diagnostics\verify_demand_loading_probe.py "%entry_folder%\eff.dll" --format3-runtime
if errorlevel 1 goto entry_failed
echo [2/5] Building DirectX proxy loader ...
cl /nologo /O2 /MT /LD /EHsc /std:c++17 ^
 /Fo"%entry_folder%\proxy_d3dcompiler.obj" src\proxy_d3dcompiler.cpp ^
 /Fe"%entry_folder%\d3dcompiler_47.dll" /link /DLL
if errorlevel 1 goto entry_failed
echo [3/5] Building Vulkan proxy loader ...
cl /nologo /O2 /MT /LD /EHsc /std:c++17 ^
 /Fo"%entry_folder%\proxy_vulkan_full.obj" src\proxy_vulkan_full.cpp ^
 /Fe"%entry_folder%\vulkan-1.dll" /link /DLL
if errorlevel 1 goto entry_failed
echo [4/5] Writing DLL data structure declaration ...
powershell -NoProfile -ExecutionPolicy Bypass -File tools\write_package_protocol.ps1 -Output "%entry_folder%\package-protocol.json"
if errorlevel 1 goto entry_failed
echo [5/5] Copying distribution configuration ...
copy /y config\eff.ini "%entry_folder%\eff.ini" >nul
if errorlevel 1 goto entry_failed
echo [OK] %entry_folder% contains the development DLL, proxies and settings. No author tools are distributed.
echo [INFO] Build output only; installed DLL and configuration were not changed.
popd
exit /b 0

:entry_find_msvc
if not exist "%entry_vswhere%" exit /b 1
for /f "usebackq tokens=*" %%i in (`"%entry_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
exit /b 0

:entry_failed
popd
exit /b 1
