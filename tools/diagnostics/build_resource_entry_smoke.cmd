@echo off
setlocal
pushd "%~dp0..\.."
set "entry_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "entry_vs_install="
if exist "%entry_vswhere%" (
 for /f "usebackq tokens=*" %%i in (`"%entry_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "entry_vs_install=%%i"
)
if defined entry_vs_install call "%entry_vs_install%\VC\Auxiliary\Build\vcvars64.bat" >nul
call tools\build_imgui.cmd
if errorlevel 1 exit /b 1
where cl >nul 2>nul
if errorlevel 1 exit /b 1
if not exist bin\resource-directory-entry mkdir bin\resource-directory-entry
cl /nologo /utf-8 /O2 /MD /EHsc /std:c++17 /bigobj /Ideps\minhook_lib\include ^
 /Fo"bin\resource-directory-entry\resource_entry_smoke.obj" ^
 tools\diagnostics\resource_entry_smoke.cpp deps\minhook_lib\lib\libMinHook.x64.lib ^
 /Fe"bin\resource-directory-entry\resource_entry_smoke.exe" /link bcrypt.lib user32.lib
set "entry_result=%errorlevel%"
popd
exit /b %entry_result%
