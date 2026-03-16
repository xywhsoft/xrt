@echo off
setlocal

gcc -m64 dev/test_coroutine_min.c xrt.c -I . -lWs2_32 -lIPHLPAPI -DDEBUG_TRACE -O2 -s -ffunction-sections -fdata-sections -Wl,--gc-sections -o release/x64/test_coroutine_min.exe || goto :eof
gcc -m64 dev/test_coroutine_core.c xrt.c -I . -lWs2_32 -lIPHLPAPI -DDEBUG_TRACE -O2 -s -ffunction-sections -fdata-sections -Wl,--gc-sections -o release/x64/test_coroutine.exe || goto :eof
gcc -m64 dev/test_xnet2_sync_core.c xrt.c -I . -lWs2_32 -lIPHLPAPI -DDEBUG_TRACE -O2 -s -ffunction-sections -fdata-sections -Wl,--gc-sections -o release/x64/test_xnet2_sync_core.exe || goto :eof
gcc -m64 dev/test_xnet2_listener_accept_core.c xrt.c -I . -lWs2_32 -lIPHLPAPI -DDEBUG_TRACE -O2 -s -ffunction-sections -fdata-sections -Wl,--gc-sections -o release/x64/test_listener_accept_core.exe || goto :eof

pushd release\x64
test_coroutine_min.exe || goto :runfail
test_coroutine.exe || goto :runfail
test_xnet2_sync_core.exe || goto :runfail
test_listener_accept_core.exe || goto :runfail
popd

echo.
echo build_GCC_TEST_COROUTINE_STAGE_x64.bat: PASS
pause
goto :eof

:runfail
popd
