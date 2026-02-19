@echo off
setlocal

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
call emcmake cmake .. -G "Ninja" -DEMSCRIPTEN=1 -DCMAKE_BUILD_TYPE=Release
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
