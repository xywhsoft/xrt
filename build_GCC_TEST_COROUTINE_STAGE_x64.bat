gcc -m64 dev/test_coroutine_min.c xrt.c -I . -lWs2_32 -lIPHLPAPI -DDEBUG_TRACE -O2 -s -ffunction-sections -fdata-sections -Wl,--gc-sections -o release/x64/test_coroutine_min.exe
gcc -m64 dev/test_coroutine_core.c xrt.c -I . -lWs2_32 -lIPHLPAPI -DDEBUG_TRACE -O2 -s -ffunction-sections -fdata-sections -Wl,--gc-sections -o release/x64/test_coroutine.exe
gcc -m64 dev/test_xnet2_sync_core.c xrt.c -I . -lWs2_32 -lIPHLPAPI -DDEBUG_TRACE -O2 -s -ffunction-sections -fdata-sections -Wl,--gc-sections -o release/x64/test_xnet2_sync_core.exe
gcc -m64 dev/test_xnet2_listener_accept_core.c xrt.c -I . -lWs2_32 -lIPHLPAPI -DDEBUG_TRACE -O2 -s -ffunction-sections -fdata-sections -Wl,--gc-sections -o release/x64/test_listener_accept_core.exe

@echo;
@echo off

cd release
cd x64
test_coroutine_min.exe
test_coroutine.exe
test_xnet2_sync_core.exe
test_listener_accept_core.exe

pause
