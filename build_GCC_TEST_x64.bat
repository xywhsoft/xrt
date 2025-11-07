gcc -m64 test.c xrt.c -lWs2_32 -lIPHLPAPI -DDEBUG_TRACE -o release/x64/test.exe

@echo;
@echo off

cd release
cd x64
test.exe

pause
