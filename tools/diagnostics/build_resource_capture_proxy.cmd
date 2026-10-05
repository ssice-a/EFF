@echo off
setlocal
pushd "%~dp0..\.."
set "capture_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "capture_vs_install="
if exist "%capture_vswhere%" (
 for /f "usebackq tokens=*" %%i in (`"%capture_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "capture_vs_install=%%i"
)
if defined capture_vs_install call "%capture_vs_install%\VC\Auxiliary\Build\vcvars64.bat" >nul
where cl >nul 2>nul
if errorlevel 1 exit /b 1
if not exist bin\resource-capture-proxy mkdir bin\resource-capture-proxy
set "capture_source=src\proxy_vulkan_full.cpp"
if not "%~1"=="" set "capture_source=%~1"
rem Diagnostic proxy only. The resource-entry plugin and inputs are not rebuilt.
cl /nologo /O2 /MD /LD /EHsc /std:c++17 ^
 /DEIEM_ENABLE_GFXTRACE_CAPTURE ^
 /Fo"bin\resource-capture-proxy\proxy_vulkan_full.obj" ^
 "%capture_source%" /Fe"bin\resource-capture-proxy\vulkan-1.dll" /link /DLL
set "capture_result=%errorlevel%"
popd
exit /b %capture_result%
