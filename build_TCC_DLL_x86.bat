@echo off
setlocal

rem TCC currently does not support XRT's production coroutine backend.
tcc -m32 -shared xrt.c -std=c99 -DBUILD_DLL -DXRT_NO_COROUTINE -lWs2_32 -lIPHLPAPI -o release/x86/xrt.dll
if errorlevel 1 goto :eof

echo.
echo build_TCC_DLL_x86.bat: PASS
pause
