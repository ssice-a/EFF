@echo off
setlocal
pushd "%~dp0..\.."
set "bone_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"%bone_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist bin\resource-directory-entry mkdir bin\resource-directory-entry
cl /nologo /utf-8 /O2 /MD /EHsc /std:c++17 /Isrc /Fo"bin\resource-directory-entry\inspect_reload_bone_paths.obj" tools\diagnostics\inspect_reload_bone_paths.cpp /Fe"bin\resource-directory-entry\inspect_reload_bone_paths.exe" /link bcrypt.lib
set "bone_result=%errorlevel%"
popd
exit /b %bone_result%
