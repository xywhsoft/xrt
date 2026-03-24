@echo off
setlocal

del /f /q release\x64\test.exe >nul 2>nul

rem TCC currently does not support XRT's production coroutine backend.
tcc -m64 test.c -I . -std=c99 -DDEBUG_TRACE -DXRT_NO_COROUTINE -lWs2_32 -lIPHLPAPI -lShell32 -o release/x64/test.exe
if errorlevel 1 goto :eof

pushd release\x64
test.exe
popd

echo.
echo build_TCC_TEST_x64.bat: PASS
pause
