@echo off
setlocal
pushd "%~dp0..\.."
set "shape_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"%shape_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist bin\resource-directory-entry mkdir bin\resource-directory-entry
cl /nologo /utf-8 /O2 /MD /EHsc /std:c++17 /W4 /Fo"bin\resource-directory-entry\shape_snapshot_smoke.obj" tools\diagnostics\shape_snapshot_smoke.cpp /Fe"bin\resource-directory-entry\shape_snapshot_smoke.exe"
set "shape_result=%errorlevel%"
popd
exit /b %shape_result%
