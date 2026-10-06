@echo off
setlocal
pushd "%~dp0../.."
set "cache_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"%cache_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist "bin\offline-cache-smoke" mkdir "bin\offline-cache-smoke"
cl /nologo /utf-8 /O2 /MD /EHsc /std:c++17 /W4 /Fo"bin\offline-cache-smoke\cache.obj" tools\diagnostics\offline_cache_smoke.cpp /Fe"bin\offline-cache-smoke\cache.exe" /link bcrypt.lib
if errorlevel 1 exit /b 1
bin\offline-cache-smoke\cache.exe
set "cache_result=%errorlevel%"
popd
exit /b %cache_result%
