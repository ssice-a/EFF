@echo off
setlocal
pushd "%~dp0..\.."
set "trace_extra_defines="
if "%~1"=="prefab-resources" set "trace_extra_defines=/DEIEM_PREFAB_RESOURCE_OBSERVATION_BUILD"
set "trace_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "trace_vs_install="
if exist "%trace_vswhere%" (
    for /f "usebackq tokens=*" %%i in (`"%trace_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "trace_vs_install=%%i"
)
if defined trace_vs_install call "%trace_vs_install%\VC\Auxiliary\Build\vcvars64.bat" >nul
where cl >nul 2>nul
if errorlevel 1 exit /b 1
if not exist bin\lua.lib (
    echo Missing bin\lua.lib. Build the project dependencies first.
    exit /b 1
)
if not exist bin\native-mesh-entry-trace mkdir bin\native-mesh-entry-trace
rc /nologo /fo bin\native-mesh-entry-trace\version.res src\version.rc
if errorlevel 1 exit /b 1
rem Source observation only. Do not add CLONE_BUILD or ENTRY_REPLACE_BUILD.
cl /nologo /utf-8 /O2 /Zi /MD /LD /EHsc /std:c++17 ^
    /DEIEM_NATIVE_MESH_ENTRY_TRACE_BUILD ^
    %trace_extra_defines% ^
    /Fo"bin\native-mesh-entry-trace\\" ^
    /Fd"bin\native-mesh-entry-trace\eff_compile.pdb" ^
    /Ideps\minhook_lib\include /Ideps\imgui /Ideps\lua ^
    src\eiem.cpp ^
    deps\imgui\imgui.cpp deps\imgui\imgui_draw.cpp ^
    deps\imgui\imgui_tables.cpp deps\imgui\imgui_widgets.cpp ^
    deps\imgui\imgui_impl_win32.cpp deps\imgui\imgui_impl_dx11.cpp ^
    bin\native-mesh-entry-trace\version.res bin\lua.lib ^
    deps\minhook_lib\lib\libMinHook.x64.lib ^
    user32.lib gdi32.lib winmm.lib comdlg32.lib d3d11.lib dxgi.lib ^
    dwmapi.lib ole32.lib winhttp.lib ^
    /Fe"bin\native-mesh-entry-trace\eff.dll" ^
    /link /DLL /DEBUG /OPT:REF /OPT:ICF /INCREMENTAL:NO ^
    /PDB:"bin\native-mesh-entry-trace\eff.pdb"
set "trace_result=%errorlevel%"
if not "%trace_result%"=="0" goto trace_done
rem Build the matching normal proxy without the GPU-capture opt-in macro.
rem Install this file in the game root, never in plugin (which loads *.dll).
cl /nologo /O2 /MD /LD /EHsc /std:c++17 ^
    /Fo"bin\native-mesh-entry-trace\proxy_vulkan_full.obj" ^
    src\proxy_vulkan_full.cpp ^
    /Fe"bin\native-mesh-entry-trace\vulkan-1.dll" /link /DLL
set "trace_result=%errorlevel%"
:trace_done
popd
exit /b %trace_result%
