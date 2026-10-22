@echo off
setlocal
pushd "%~dp0..\.."
set "effect_perf="
set "effect_output=bin\effects-debug-tests"
if /i "%~1"=="load-perf" (
 set "effect_perf=/DEIEM_LOAD_PERF"
 set "effect_output=bin\effects-debug-perf-tests"
) else if not "%~1"=="" (
 echo Expected optional load-perf argument.
 exit /b 1
)
set "effect_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"%effect_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
call tools\build_imgui.cmd static
if errorlevel 1 exit /b 1
if not exist "%effect_output%" mkdir "%effect_output%"
cl /nologo /utf-8 /O2 /MT /EHsc /std:c++17 /bigobj /DEIEM_STATIC_RUNTIME /DEFF_OFFLINE_PACKAGE_V35 %effect_perf% /Ideps\minhook_lib\include /Fo"%effect_output%\effect_debug_smoke.obj" tools\diagnostics\effect_debug_smoke.cpp deps\minhook_lib\lib\libMinHook.x64.lib /Fe"%effect_output%\effect_debug_smoke.exe" /link /NODEFAULTLIB:MSVCRT bcrypt.lib user32.lib
set "effect_result=%errorlevel%"
popd
exit /b %effect_result%
