@echo off
setlocal
pushd "%~dp0..\.."
set "contract_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
where cl >nul 2>nul
if errorlevel 1 for /f "usebackq tokens=*" %%i in (`"%contract_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
where cl >nul 2>nul
if errorlevel 1 exit /b 1
call tools\build_imgui.cmd
if errorlevel 1 exit /b 1
set "contract_folder=bin\entry-contract-current"
if not exist "%contract_folder%" mkdir "%contract_folder%"
cl /nologo /utf-8 /O2 /MD /EHsc /std:c++17 /bigobj /Ideps\minhook_lib\include /Fo"%contract_folder%\entry_contract_smoke.obj" tools\diagnostics\entry_contract_smoke.cpp deps\minhook_lib\lib\libMinHook.x64.lib /Fe"%contract_folder%\entry_contract_smoke.exe" /link bcrypt.lib user32.lib
set "contract_result=%errorlevel%"
popd
exit /b %contract_result%
