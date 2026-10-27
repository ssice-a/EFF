@echo off
setlocal
pushd "%~dp0..\.."
set "catalog_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
where cl >nul 2>nul
if errorlevel 1 for /f "usebackq tokens=*" %%i in (`"%catalog_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
where cl >nul 2>nul
if errorlevel 1 goto catalog_failed
if not exist bin\catalog-survey mkdir bin\catalog-survey
call tools\build_imgui.cmd static
if errorlevel 1 goto catalog_failed
cl /nologo /utf-8 /O2 /Zi /MT /LD /EHsc /std:c++17 /bigobj /W4 /DEIEM_CATALOG_SURVEY=1 /DEIEM_STATIC_RUNTIME=1 /Ideps\minhook_lib\include /Fo"bin\catalog-survey\entry.obj" /Fd"bin\catalog-survey\compile.pdb" src\eiem_resource_entry.cpp deps\minhook_lib\lib\libMinHook.x64.lib /Fe"bin\catalog-survey\eff.dll" /link /DLL /DEBUG /OPT:REF /OPT:ICF /INCREMENTAL:NO /NODEFAULTLIB:MSVCRT bcrypt.lib user32.lib /PDB:"bin\catalog-survey\eff.pdb"
set "catalog_result=%errorlevel%"
popd
exit /b %catalog_result%
:catalog_failed
popd
exit /b 1
