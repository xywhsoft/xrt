@echo off
setlocal

rem TCC currently does not support XRT's production coroutine backend.
rem Build the DLL with coroutine disabled instead of falling back to an
rem archived backend implementation.
tcc -m64 -shared xrt.c -std=c99 -DBUILD_DLL -DXRT_NO_COROUTINE -lWs2_32 -lIPHLPAPI -lShell32 -o release/x64/xrt.dll
if errorlevel 1 goto :eof

echo.
echo build_TCC_DLL_x64.bat: PASS
pause
