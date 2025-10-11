gcc -m32 test.c xrt.c -lWs2_32 -lIPHLPAPI -o release/x86/test.exe

@echo;
@echo off

cd release
cd x86
test.exe

pause
