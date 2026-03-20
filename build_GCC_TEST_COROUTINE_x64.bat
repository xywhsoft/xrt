@echo off
setlocal

gcc -m64 test.c -I . -lWs2_32 -lIPHLPAPI -DDEBUG_TRACE -O2 -s -ffunction-sections -fdata-sections -Wl,--gc-sections -o release/x64/test_coroutine.exe || goto :eof

release\x64\test_coroutine.exe coroutine || goto :eof

echo.
echo build_GCC_TEST_COROUTINE_x64.bat: PASS
pause
goto :eof
