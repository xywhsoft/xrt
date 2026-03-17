@echo off
setlocal

gcc -m64 dev/test_xnet2_stage.c -I . -lWs2_32 -lIPHLPAPI -DDEBUG_TRACE -O2 -s -ffunction-sections -fdata-sections -Wl,--gc-sections -o release/x64/test_xnet2_stage.exe || goto :eof

release\x64\test_xnet2_stage.exe || goto :eof

echo.
echo build_GCC_TEST_XNET2_STAGE_x64.bat: PASS
pause
