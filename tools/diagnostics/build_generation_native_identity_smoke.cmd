@echo off
setlocal
pushd "%~dp0..\.."
set "eff_identity_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
where cl >nul 2>nul
if errorlevel 1 for /f "usebackq tokens=*" %%i in (`"%eff_identity_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
where cl >nul 2>nul
if errorlevel 1 goto failed
if not exist bin\generation-native-identity mkdir bin\generation-native-identity
cl /nologo /utf-8 /O2 /MT /EHsc /std:c++17 /bigobj /W4 /Fo"bin\generation-native-identity\smoke.obj" tools\diagnostics\generation_native_identity_smoke.cpp /Fe"bin\generation-native-identity\smoke.exe" /link /NODEFAULTLIB:MSVCRT bcrypt.lib
if errorlevel 1 goto failed
bin\generation-native-identity\smoke.exe
set "eff_identity_result=%errorlevel%"
popd
exit /b %eff_identity_result%
:failed
popd
exit /b 1
