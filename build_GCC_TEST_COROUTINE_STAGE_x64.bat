@echo off
setlocal

gcc -m64 test.c -I . -lWs2_32 -lIPHLPAPI -DDEBUG_TRACE -O2 -s -ffunction-sections -fdata-sections -Wl,--gc-sections -o release/x64/test_coroutine_stage.exe || goto :eof

release\x64\test_coroutine_stage.exe coroutine_min || goto :eof
release\x64\test_coroutine_stage.exe coroutine || goto :eof
release\x64\test_coroutine_stage.exe xnet2_sync || goto :eof
release\x64\test_coroutine_stage.exe xnet2_listener_accept_core || goto :eof

echo.
echo build_GCC_TEST_COROUTINE_STAGE_x64.bat: PASS
pause
goto :eof
