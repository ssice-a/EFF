@echo off
setlocal
pushd "%~dp0.."
set "imgui_runtime=/MD"
set "imgui_folder=bin\imgui"
if /i "%~1"=="static" (
 set "imgui_runtime=/MT"
 set "imgui_folder=bin\imgui-static"
) else if not "%~1"=="" (
 echo [ERROR] Expected optional static runtime argument.
 popd
 exit /b 1
)
set "imgui_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
where cl >nul 2>nul
if errorlevel 1 for /f "usebackq tokens=*" %%i in (`"%imgui_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist "%imgui_folder%" mkdir "%imgui_folder%"
powershell -NoProfile -Command "$lib=Get-Item '%imgui_folder%/eiem_imgui.lib' -ErrorAction SilentlyContinue; if($lib -and !(Get-ChildItem 'deps/imgui' -File | Where-Object LastWriteTimeUtc -gt $lib.LastWriteTimeUtc)){exit 0}; exit 1"
if not errorlevel 1 (popd & exit /b 0)
cl /nologo /utf-8 /c /O2 %imgui_runtime% /EHsc /std:c++17 /bigobj /Fo"%imgui_folder%\\" deps\imgui\imgui.cpp deps\imgui\imgui_draw.cpp deps\imgui\imgui_widgets.cpp deps\imgui\imgui_tables.cpp deps\imgui\imgui_impl_win32.cpp deps\imgui\imgui_impl_dx11.cpp
if errorlevel 1 (popd & exit /b 1)
lib /nologo /out:"%imgui_folder%\eiem_imgui.lib" "%imgui_folder%\imgui.obj" "%imgui_folder%\imgui_draw.obj" "%imgui_folder%\imgui_widgets.obj" "%imgui_folder%\imgui_tables.obj" "%imgui_folder%\imgui_impl_win32.obj" "%imgui_folder%\imgui_impl_dx11.obj"
set "imgui_result=%errorlevel%"
popd
exit /b %imgui_result%
