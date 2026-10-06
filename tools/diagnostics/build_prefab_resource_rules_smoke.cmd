@echo off
setlocal
pushd "%~dp0..\.."
set "rules_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"%rules_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist bin\resource-rules mkdir bin\resource-rules
cl /nologo /utf-8 /O2 /MD /EHsc /std:c++17 /Fo"bin\resource-rules\rules_smoke.obj" tools\diagnostics\prefab_resource_rules_smoke.cpp /Fe"bin\resource-rules\rules_smoke.exe" /link bcrypt.lib
set "rules_result=%errorlevel%"
popd
exit /b %rules_result%
