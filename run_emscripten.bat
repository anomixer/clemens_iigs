@echo off
setlocal

rem Enable local emsdk
call %~dp0\tools\emsdk\emsdk_env.bat

rem Run the emulator
echo Starting emulator...
call emrun build-em/host/clemens_iigs.html

goto :eof
