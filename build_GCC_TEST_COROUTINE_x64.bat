@echo off
setlocal

gcc -m64 dev/test_coroutine_core.c xrt.c -I . -lWs2_32 -lIPHLPAPI -DDEBUG_TRACE -O2 -s -ffunction-sections -fdata-sections -Wl,--gc-sections -o release/x64/test_coroutine.exe || goto :eof

pushd release\x64
test_coroutine.exe || goto :runfail
popd

echo.
echo build_GCC_TEST_COROUTINE_x64.bat: PASS
pause
goto :eof

:runfail
popd
