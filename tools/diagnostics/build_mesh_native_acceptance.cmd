@echo off
setlocal
pushd "%~dp0..\.."
set "mesh_native_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
where cl >nul 2>nul
if errorlevel 1 for /f "usebackq tokens=*" %%i in (`"%mesh_native_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
where cl >nul 2>nul
if errorlevel 1 goto mesh_native_failed
if not exist bin\mesh-native-acceptance mkdir bin\mesh-native-acceptance
call tools\build_imgui.cmd static
if errorlevel 1 goto mesh_native_failed
cl /nologo /utf-8 /O2 /Zi /MT /LD /EHsc /std:c++17 /bigobj /W4 /DEIEM_MESH_TARGET_PROBE=1 /DEIEM_MESH_NATIVE_ACCEPTANCE=1 /DEIEM_STATIC_RUNTIME=1 /Ideps\minhook_lib\include /Fo"bin\mesh-native-acceptance\entry.obj" /Fd"bin\mesh-native-acceptance\compile.pdb" src\eiem_resource_entry.cpp deps\minhook_lib\lib\libMinHook.x64.lib /Fe"bin\mesh-native-acceptance\eff.dll" /link /DLL /DEBUG /OPT:REF /OPT:ICF /INCREMENTAL:NO /NODEFAULTLIB:MSVCRT bcrypt.lib user32.lib /PDB:"bin\mesh-native-acceptance\eff.pdb"
set "mesh_native_result=%errorlevel%"
popd
exit /b %mesh_native_result%
:mesh_native_failed
popd
exit /b 1
