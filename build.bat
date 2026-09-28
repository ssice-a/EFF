@echo off

setlocal enabledelayedexpansion

echo ==========================================
echo   EIEM Build Script
echo ==========================================
echo.

:: Set up MSVC environment
set "vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "vcvars="

if exist "%vswhere%" (
    for /f "usebackq tokens=*" %%i in (`"%vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
        set "install_path=%%i"
    )
)

if defined install_path (
    set "vcvars=!install_path!\VC\Auxiliary\Build\vcvars64.bat"
)

if defined vcvars if exist "!vcvars!" (
    echo [INFO] Found MSVC at: "!vcvars!"
    call "!vcvars!" >nul
)

where cl >nul 2>nul
if %errorlevel% neq 0 (
    echo [WARN] Automatic detection failed.
    set /p "user_path=Please drag and drop vcvars64.bat here and press Enter: "
    if exist "!user_path!" (
        call "!user_path!" >nul
    )
)

where cl >nul 2>nul
if %errorlevel% equ 0 (
    echo [SUCCESS] MSVC Environment Ready.
) else (
    echo [ERROR] Failed to set up MSVC environment.

    exit /b 1
)

echo [OK] MSVC compiler found
echo.

if not exist bin mkdir bin

:: Build Lua as C++ so protected Lua errors unwind C++ binding locals safely.
if not exist bin\lua mkdir bin\lua
set "lua_sources="
for %%f in (deps\lua\*.c) do (
    if /I not "%%~nxf"=="lua.c" if /I not "%%~nxf"=="luac.c" set "lua_sources=!lua_sources! %%f"
)
cl /nologo /O2 /MD /EHsc /TP /c /Ideps\lua /Fo"bin\lua\\" !lua_sources!
if errorlevel 1 exit /b 1
lib /nologo /out:bin\lua.lib bin\lua\*.obj
if errorlevel 1 exit /b 1

:: Build eiem.dll
echo [1/4] Compiling version resource ...
rc /nologo /fo bin\version.res src\version.rc

echo [2/4] Building eiem.dll ...
cl /nologo /utf-8 /O2 /Zi /MD /LD /EHsc /std:c++17 ^
    /DEIEM_VISIBILITY_CONTROLLER_PROBE_BUILD ^
    /DEIEM_MATERIAL_LIFECYCLE_PROBE_BUILD ^
    /DEIEM_NATIVE_VFX_SLOT_ADAPTER_BUILD ^
    /DEIEM_PRESERVE_NATIVE_VFX_MATERIALS_BUILD ^
    /DEIEM_PERDRAW_END_RESTORE_BUILD ^
    /DEIEM_PERDRAW_VFX_FANOUT_BUILD ^
    /DEIEM_NATIVE_VFX_PASSTHROUGH_TEXTURE_PATCH_BUILD ^
    /DEIEM_DITHER_PROBE_BUILD ^
    /Fd"bin\eiem_compile.pdb" ^
    /Ideps\minhook_lib\include ^
    /Ideps\imgui ^
    /Ideps\lua ^
    src\eiem.cpp ^
    deps\imgui\imgui.cpp ^
    deps\imgui\imgui_draw.cpp ^
    deps\imgui\imgui_tables.cpp ^
    deps\imgui\imgui_widgets.cpp ^
    deps\imgui\imgui_impl_win32.cpp ^
    deps\imgui\imgui_impl_dx11.cpp ^
    bin\version.res ^
    bin\lua.lib ^
    deps\minhook_lib\lib\libMinHook.x64.lib ^
    user32.lib ^
    gdi32.lib ^
    winmm.lib ^
    comdlg32.lib ^
    d3d11.lib ^
    dxgi.lib ^
    dwmapi.lib ^
    ole32.lib ^
    winhttp.lib ^
    /Fe"bin\eiem.dll" ^
    /link /DLL /DEBUG /OPT:REF /OPT:ICF /INCREMENTAL:NO /PDB:"bin\eiem.pdb"

if %errorlevel% neq 0 (
    echo [ERROR] eiem.dll build failed!

    exit /b 1
)
echo [OK] eiem.dll built successfully
echo.

:: Build d3dcompiler_47.dll (proxy loader)
echo [3/4] Building d3dcompiler_47.dll (proxy loader) ...
cl /nologo /O2 /MD /LD /EHsc /std:c++17 ^
    src\proxy_d3dcompiler.cpp ^
    /Fe"bin\d3dcompiler_47.dll" ^
    /link /DLL

if %errorlevel% neq 0 (
    echo [ERROR] d3dcompiler_47.dll build failed!

    exit /b 1
)
echo [OK] d3dcompiler_47.dll built successfully
echo.

:: Build vulkan-1.dll (vulkan proxy loader)
echo [4/4] Building vulkan-1.dll (vulkan proxy loader) ...
cl /nologo /O2 /MD /LD /EHsc /std:c++17 ^
    src\proxy_vulkan_full.cpp ^
    /Fe"bin\vulkan-1.dll" ^
    /link /DLL

if %errorlevel% neq 0 (
    echo [ERROR] vulkan-1.dll build failed!
    exit /b 1
)
echo [OK] vulkan-1.dll built successfully
echo.

:: Distribution template only; never overwrite the installed user's configuration.
copy /y config\eiem.ini bin\eiem.ini >nul
if %errorlevel% neq 0 exit /b 1
copy /y deps\lua\LICENSE bin\LUA-LICENSE.txt >nul
if %errorlevel% neq 0 exit /b 1

:: Clean up intermediate files (CWD .obj + bin\ .exp/.lib)
del /q eiem.obj 2>nul
del /q imgui.obj 2>nul
del /q imgui_draw.obj 2>nul
del /q imgui_tables.obj 2>nul
del /q imgui_widgets.obj 2>nul
del /q imgui_impl_win32.obj 2>nul
del /q imgui_impl_dx11.obj 2>nul
del /q proxy_d3dcompiler.obj 2>nul
del /q proxy_vulkan_full.obj 2>nul
del /q bin\eiem.exp 2>nul
del /q bin\eiem.lib 2>nul
del /q bin\d3dcompiler_47.exp 2>nul
del /q bin\d3dcompiler_47.lib 2>nul
del /q bin\vulkan-1.exp 2>nul
del /q bin\vulkan-1.lib 2>nul

echo ==========================================
echo   Build Complete!
echo ==========================================
echo.
echo Output files in bin\:
echo   - eiem.dll               (EIEM plugin)
echo   - d3dcompiler_47.dll     (DX proxy loader)
echo   - vulkan-1.dll           (Vulkan proxy loader)
echo   - eiem.ini              (global settings template)
echo.

