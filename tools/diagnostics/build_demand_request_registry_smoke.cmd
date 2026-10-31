@echo off
setlocal
pushd "%~dp0..\.."
set "eff_demand_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
where cl >nul 2>nul
if errorlevel 1 for /f "usebackq tokens=*" %%i in (`"%eff_demand_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
where cl >nul 2>nul
if errorlevel 1 goto failed
if not exist bin\demand-loading-probe mkdir bin\demand-loading-probe
cl /nologo /utf-8 /O2 /MT /EHsc /std:c++17 /W4 /Fo"bin\demand-loading-probe\registry.obj" tools\diagnostics\demand_request_registry_smoke.cpp /Fe"bin\demand-loading-probe\registry.exe" /link /NODEFAULTLIB:MSVCRT bcrypt.lib
if errorlevel 1 goto failed
bin\demand-loading-probe\registry.exe
if errorlevel 1 goto failed
cl /nologo /utf-8 /O2 /MT /EHsc /std:c++17 /W4 /Fo"bin\demand-loading-probe\shared-graph.obj" tools\diagnostics\demand_shared_graph_smoke.cpp /Fe"bin\demand-loading-probe\shared-graph.exe" /link /NODEFAULTLIB:MSVCRT bcrypt.lib
if errorlevel 1 goto failed
bin\demand-loading-probe\shared-graph.exe
set "eff_demand_result=%errorlevel%"
popd
exit /b %eff_demand_result%
:failed
popd
exit /b 1
