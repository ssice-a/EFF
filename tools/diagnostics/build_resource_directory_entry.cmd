@echo off
setlocal
pushd "%~dp0..\.."
set "entry_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "entry_vs_install="
if exist "%entry_vswhere%" (
 for /f "usebackq tokens=*" %%i in (`"%entry_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "entry_vs_install=%%i"
)
if defined entry_vs_install call "%entry_vs_install%\VC\Auxiliary\Build\vcvars64.bat" >nul
where cl >nul 2>nul
if errorlevel 1 exit /b 1
if not exist bin\resource-directory-entry mkdir bin\resource-directory-entry
rem One translation unit: loading inputs plus optional public shape snapshots.
rem No retired base replacement headers, private memory writers or draw hooks.
set "entry_capture_flags="
if "%~1"=="capture" set "entry_capture_flags=/DEIEM_SHAPE_CAPTURE_GFXTRACE"
cl /nologo /utf-8 /O2 /Zi /MD /LD /EHsc /std:c++17 /W4 ^
 %entry_capture_flags% ^
 /Ideps\minhook_lib\include ^
 /Fo"bin\resource-directory-entry\eff_resource_entry.obj" ^
 /Fd"bin\resource-directory-entry\eff_compile.pdb" ^
 src\eiem_resource_entry.cpp deps\minhook_lib\lib\libMinHook.x64.lib ^
 /Fe"bin\resource-directory-entry\eff.dll" ^
 /link /DLL /DEBUG /OPT:REF /OPT:ICF /INCREMENTAL:NO bcrypt.lib user32.lib ^
 /PDB:"bin\resource-directory-entry\eff.pdb"
if errorlevel 1 goto entry_failed
cl /nologo /O2 /MD /LD /EHsc /std:c++17 ^
 /Fo"bin\resource-directory-entry\proxy_vulkan_full.obj" ^
 src\proxy_vulkan_full.cpp /Fe"bin\resource-directory-entry\vulkan-1.dll" /link /DLL
set "entry_result=%errorlevel%"
popd
exit /b %entry_result%
:entry_failed
popd
exit /b 1
