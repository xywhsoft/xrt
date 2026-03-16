@echo off
setlocal

rem TCC currently does not support XRT's production coroutine backend.
tcc -m64 -c xrt.c -std=c99 -DXRT_NO_COROUTINE -o release/x64/xrt.o
if errorlevel 1 goto :eof

echo.
echo build_TCC_OBJ_x64.bat: PASS
pause
