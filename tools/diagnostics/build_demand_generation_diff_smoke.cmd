@echo off
setlocal
set "smoke_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
where cl >nul 2>nul
if errorlevel 1 (
  if not exist "%smoke_vswhere%" (
    echo [ERROR] MSVC x64 tools unavailable. Install Visual Studio C++ Build Tools.
    exit /b 1
  )
  for /f "usebackq tokens=*" %%i in (`"%smoke_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
)
where cl >nul 2>nul
if errorlevel 1 exit /b 1
if not exist bin\resource-directory-entry mkdir bin\resource-directory-entry
cl /nologo /utf-8 /O2 /EHsc /std:c++17 /Isrc /Fo"bin\resource-directory-entry\demand_generation_diff_smoke.obj" tools\diagnostics\demand_generation_diff_smoke.cpp /Fe"bin\resource-directory-entry\demand_generation_diff_smoke.exe"
if errorlevel 1 exit /b %errorlevel%
bin\resource-directory-entry\demand_generation_diff_smoke.exe
exit /b %errorlevel%
