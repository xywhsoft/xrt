@echo off
setlocal

del /f /q release\x86\test.exe >nul 2>nul

rem TCC currently does not support XRT's production coroutine backend.
tcc -m32 test.c -I . -std=c99 -DDEBUG_TRACE -DXRT_NO_COROUTINE -lWs2_32 -lIPHLPAPI -o release/x86/test.exe
if errorlevel 1 goto :eof

pushd release\x86
test.exe
popd

echo.
echo build_TCC_TEST_x86.bat: PASS
pause
