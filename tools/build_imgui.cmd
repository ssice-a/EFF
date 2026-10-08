@echo off
setlocal
pushd "%~dp0.."
set "imgui_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
where cl >nul 2>nul
if errorlevel 1 for /f "usebackq tokens=*" %%i in (`"%imgui_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist bin\imgui mkdir bin\imgui
powershell -NoProfile -Command "$lib=Get-Item 'bin/imgui/eiem_imgui.lib' -ErrorAction SilentlyContinue; if($lib -and !(Get-ChildItem 'deps/imgui' -File | Where-Object LastWriteTimeUtc -gt $lib.LastWriteTimeUtc)){exit 0}; exit 1"
if not errorlevel 1 (popd & exit /b 0)
cl /nologo /utf-8 /c /O2 /MD /EHsc /std:c++17 /bigobj /Fo"bin\imgui\\" deps\imgui\imgui.cpp deps\imgui\imgui_draw.cpp deps\imgui\imgui_widgets.cpp deps\imgui\imgui_tables.cpp deps\imgui\imgui_impl_win32.cpp deps\imgui\imgui_impl_dx11.cpp
if errorlevel 1 (popd & exit /b 1)
lib /nologo /out:bin\imgui\eiem_imgui.lib bin\imgui\imgui.obj bin\imgui\imgui_draw.obj bin\imgui\imgui_widgets.obj bin\imgui\imgui_tables.obj bin\imgui\imgui_impl_win32.obj bin\imgui\imgui_impl_dx11.obj
set "imgui_result=%errorlevel%"
popd
exit /b %imgui_result%
