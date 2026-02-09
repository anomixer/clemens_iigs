@echo off
setlocal

rem Enable local emsdk
call %~dp0\tools\emsdk\emsdk_env.bat

if not exist "build-em" mkdir build-em
cd build-em
call emcmake cmake .. -DEMSCRIPTEN=1 -DCMAKE_BUILD_TYPE=Release -G "MinGW Makefiles"
if errorlevel 1 goto error
call cmake --build .
if errorlevel 1 goto error
echo Build complete. To run, type: emrun host/clemens_iigs.html
goto :eof

:error
echo Build failed. Ensure you have activated the Emscripten SDK environment (emsdk_env.bat).
exit /b 1
