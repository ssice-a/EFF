@echo off
setlocal
pushd "%~dp0..\.."
set "discovery_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
where cl >nul 2>nul
if errorlevel 1 for /f "usebackq tokens=*" %%i in (`"%discovery_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist bin\mod-discovery-tests mkdir bin\mod-discovery-tests
cl /nologo /utf-8 /O2 /MT /EHsc /std:c++17 /bigobj /W4 /Fobin\mod-discovery-tests\mod_discovery_smoke.obj tools\diagnostics\mod_discovery_smoke.cpp /Febin\mod-discovery-tests\mod_discovery_smoke.exe /link bcrypt.lib
set "discovery_result=%errorlevel%"
popd
exit /b %discovery_result%
