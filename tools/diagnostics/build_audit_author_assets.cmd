@echo off
setlocal
pushd "%~dp0..\.."
set "audit_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
where cl >nul 2>nul
if errorlevel 1 for /f "usebackq tokens=*" %%i in (`"%audit_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist bin\author-assets mkdir bin\author-assets
cl /nologo /utf-8 /O2 /MD /EHsc /std:c++17 /bigobj /W4 /Fo"bin\author-assets\audit.obj" tools\diagnostics\audit_author_assets.cpp /Fe"bin\author-assets\audit.exe" /link bcrypt.lib
set "audit_result=%errorlevel%"
popd
exit /b %audit_result%
