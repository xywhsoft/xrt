@echo off
setlocal

gcc -m64 test.c -I . -lWs2_32 -lIPHLPAPI -O2 -s -ffunction-sections -fdata-sections -Wl,--gc-sections -o release/x64/test.exe || goto :eof

release\x64\test.exe coroutine_min || goto :eof
release\x64\test.exe coroutine || goto :eof
release\x64\test.exe xurl_core || goto :eof
release\x64\test.exe preset:xnet2_stage || goto :eof

echo.
echo build_GCC_TEST_x64.bat: PASS
goto :eof
