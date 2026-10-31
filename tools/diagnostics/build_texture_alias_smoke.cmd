@echo off
setlocal
pushd "%~dp0../.."
set "alias_vswhere=%ProgramFiles(x86)%/Microsoft Visual Studio/Installer/vswhere.exe"
where cl >nul 2>nul
if errorlevel 1 for /f "usebackq tokens=*" %%i in (`"%alias_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i/VC/Auxiliary/Build/vcvars64.bat" >nul
if not exist bin/resource-pack mkdir bin/resource-pack
cl /nologo /utf-8 /O2 /MT /EHsc /std:c++17 /bigobj /Fo"bin/resource-pack/texture_alias_smoke.obj" tools/diagnostics/texture_alias_smoke.cpp /Fe"bin/resource-pack/texture_alias_smoke.exe" /link bcrypt.lib cabinet.lib
set "alias_result=%errorlevel%"
popd
exit /b %alias_result%
