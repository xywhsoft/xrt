tcc -m32 test.c xrt.c -std=c99 -DDEBUG_TRACE -o release/x86/test.exe

@echo;
@echo off

cd release
cd x86
test.exe

pause
