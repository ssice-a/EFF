@echo off
setlocal
pushd "%~dp0..\.."
set "cache_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"%cache_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
call tools\build_imgui.cmd static
if errorlevel 1 exit /b 1
if not exist "bin\replacement-cache-tests" mkdir "bin\replacement-cache-tests"
cl /nologo /utf-8 /O2 /MT /EHsc /std:c++17 /bigobj /DEIEM_FORMAT3_RUNTIME=1 /DEIEM_STATIC_RUNTIME /DEIEM_SKIP_LEGACY_DISK_CHECK /Ideps\minhook_lib\include /Fo"bin\replacement-cache-tests\effect_debug_smoke.obj" tools\diagnostics\effect_debug_smoke.cpp deps\minhook_lib\lib\libMinHook.x64.lib /Fe"bin\replacement-cache-tests\effect_debug_smoke.exe" /link /NODEFAULTLIB:MSVCRT bcrypt.lib user32.lib
set "cache_result=%errorlevel%"
popd
exit /b %cache_result%
