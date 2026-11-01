@echo off
setlocal
pushd "%~dp0..\.."
set "eff_demand_test_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
where cl >nul 2>nul
if errorlevel 1 for /f "usebackq tokens=*" %%i in (`"%eff_demand_test_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
where cl >nul 2>nul
if errorlevel 1 goto failed
set "eff_demand_test_output=bin\demand-resource-regressions"
if not exist "%eff_demand_test_output%" mkdir "%eff_demand_test_output%"
if errorlevel 1 goto failed
cl /nologo /utf-8 /O2 /MT /EHsc /std:c++17 /W4 /I src /Fo"%eff_demand_test_output%\demand_shared_graph_smoke.obj" tools\diagnostics\demand_shared_graph_smoke.cpp /Fe"%eff_demand_test_output%\demand_shared_graph_smoke.exe" /link /NODEFAULTLIB:MSVCRT bcrypt.lib
if errorlevel 1 goto failed
"%eff_demand_test_output%\demand_shared_graph_smoke.exe"
if errorlevel 1 goto failed
cl /nologo /utf-8 /O2 /MT /EHsc /std:c++17 /W4 /I src /Fo"%eff_demand_test_output%\author_material_edits_smoke.obj" tools\diagnostics\author_material_edits_smoke.cpp /Fe"%eff_demand_test_output%\author_material_edits_smoke.exe" /link /NODEFAULTLIB:MSVCRT bcrypt.lib
if errorlevel 1 goto failed
"%eff_demand_test_output%\author_material_edits_smoke.exe" --removal-only
if errorlevel 1 goto failed
popd
exit /b 0
:failed
popd
exit /b 1
