@echo off
setlocal

rem ============================================================
rem  ROM Check
rem  The Apple IIgs ROM 3 is required for Emscripten builds.
rem  It is NOT included in this repository (copyright Apple Computer, Inc.)
rem  You must supply your own ROM file.
rem
rem  Default path:  rom\rom.v3  (relative to project root)
rem  Override:      set CLEMENS_ROM_PATH=C:\path\to\your\rom.v3
rem ============================================================
if defined CLEMENS_ROM_PATH (
    set ROM_PATH=%CLEMENS_ROM_PATH%
) else (
    set ROM_PATH=%~dp0rom\rom.v3
)

if not exist "%ROM_PATH%" (
    echo.
    echo [ERROR] Apple IIgs ROM file not found: %ROM_PATH%
    echo.
    echo  The Emscripten build requires an Apple IIgs ROM 3 image.
    echo  This file is NOT included in the repository due to copyright.
    echo.
    echo  To proceed:
    echo    1. Obtain a legitimate Apple IIgs ROM 3 dump  ^(rom.v3^)
    echo    2. Place it at:  %~dp0rom\rom.v3
    echo       -- OR --
    echo    3. Set the environment variable before running this script:
    echo         set CLEMENS_ROM_PATH=C:\path\to\your\rom.v3
    echo.
    exit /b 1
)
echo [Setup] ROM found: %ROM_PATH%

rem Define tools directory
set TOOLS_DIR=%~dp0tools
if not exist "%TOOLS_DIR%" mkdir "%TOOLS_DIR%"

rem --- EMSDK Setup ---
if not exist "%TOOLS_DIR%\emsdk" (
    echo [Setup] Cloning EMSDK...
    git clone https://github.com/emscripten-core/emsdk.git "%TOOLS_DIR%\emsdk"
    if errorlevel 1 goto error
)

echo [Setup] Configuring EMSDK 3.1.51...
pushd "%TOOLS_DIR%\emsdk"
call emsdk.bat install 3.1.51
if errorlevel 1 (
    popd
    goto error
)
call emsdk.bat activate 3.1.51
if errorlevel 1 (
    popd
    goto error
)
call emsdk_env.bat
popd

rem --- Ninja Setup ---
if not exist "%TOOLS_DIR%\ninja.exe" (
    echo [Setup] Downloading Ninja...
    pushd "%TOOLS_DIR%"
    curl -L -o ninja-win.zip https://github.com/ninja-build/ninja/releases/download/v1.12.1/ninja-win.zip
    if errorlevel 1 (
        popd
        goto error
    )
    echo [Setup] Extracting Ninja...
    powershell -command "Expand-Archive -Force ninja-win.zip ."
    if errorlevel 1 (
        popd
        goto error
    )
    del ninja-win.zip
    popd
)

rem Add Ninja to PATH
set PATH=%TOOLS_DIR%;%PATH%

rem --- Build ---
if not exist "build-em" mkdir build-em

rem Clean CMakeCache if switching generators (e.g. from MinGW to Ninja)
if exist "build-em\CMakeCache.txt" (
    echo [Build] Cleaning previous CMake cache...
    del "build-em\CMakeCache.txt"
)

cd build-em
echo [Build] Configuring CMake...
call emcmake cmake .. -G "Ninja" -DEMSCRIPTEN=1 -DCMAKE_BUILD_TYPE=Release -DCLEMENS_ROM_PATH="%ROM_PATH%"
if errorlevel 1 goto error

echo [Build] Compiling...
call cmake --build .
if errorlevel 1 goto error

echo [Build] Host assets (coi-serviceworker.js)...
if not exist "host\coi-serviceworker.js" (
     if not exist "host" mkdir host
     curl -o host/coi-serviceworker.js https://raw.githubusercontent.com/gzuidhof/coi-serviceworker/master/coi-serviceworker.js
)

echo.
echo Build complete.
echo To run the emulator, execute: run_emscripten.bat
goto :eof

:error
echo Build failed.
exit /b 1

