gcc -m64 dev/test_xnet2_stage.c xrt.c -I . -lWs2_32 -lIPHLPAPI -DDEBUG_TRACE -O2 -s -ffunction-sections -fdata-sections -Wl,--gc-sections -o release/x64/test_xnet2_stage.exe

@echo;
@echo off

cd release
cd x64
test_xnet2_stage.exe

pause
