@echo off
setlocal
pushd "%~dp0..\.."
set "mesh_probe_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
where cl >nul 2>nul
if errorlevel 1 for /f "usebackq tokens=*" %%i in (`"%mesh_probe_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
where cl >nul 2>nul
if errorlevel 1 goto mesh_probe_failed
if not exist bin\mesh-target-probe mkdir bin\mesh-target-probe
call tools\build_imgui.cmd static
if errorlevel 1 goto mesh_probe_failed
cl /nologo /utf-8 /O2 /Zi /MT /LD /EHsc /std:c++17 /bigobj /W4 /DEIEM_MESH_TARGET_PROBE=1 /DEIEM_STATIC_RUNTIME=1 /Ideps\minhook_lib\include /Fo"bin\mesh-target-probe\entry.obj" /Fd"bin\mesh-target-probe\compile.pdb" src\eiem_resource_entry.cpp deps\minhook_lib\lib\libMinHook.x64.lib /Fe"bin\mesh-target-probe\eff.dll" /link /DLL /DEBUG /OPT:REF /OPT:ICF /INCREMENTAL:NO /NODEFAULTLIB:MSVCRT bcrypt.lib user32.lib /PDB:"bin\mesh-target-probe\eff.pdb"
set "mesh_probe_result=%errorlevel%"
popd
exit /b %mesh_probe_result%
:mesh_probe_failed
popd
exit /b 1
