@echo off
setlocal
pushd "%~dp0..\.."
set "author_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
where cl >nul 2>nul
if errorlevel 1 for /f "usebackq tokens=*" %%i in (`"%author_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist bin\resource-pack mkdir bin\resource-pack
cl /nologo /utf-8 /O2 /MT /EHsc /std:c++17 /bigobj /W4 /Fo"bin\resource-pack\author_builder_origins_smoke.obj" tools\diagnostics\author_builder_origins_smoke.cpp /Fe"bin\resource-pack\author_builder_origins_smoke.exe" /link bcrypt.lib
set "author_result=%errorlevel%"
popd
exit /b %author_result%
