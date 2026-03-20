@echo off
setlocal

set "ARGS=%~1 %~2"
if "%~1"=="" set "ARGS=16 4"

gcc -m64 test.c -I . -lWs2_32 -lIPHLPAPI -O2 -s -ffunction-sections -fdata-sections -Wl,--gc-sections -o release/x64/test_xnet_native_core.exe || goto :eof

release\x64\test_xnet_native_core.exe xnet_native_core %ARGS% || goto :eof

echo.
echo build_GCC_TEST_XNET_NATIVE_CORE_x64.bat: PASS
