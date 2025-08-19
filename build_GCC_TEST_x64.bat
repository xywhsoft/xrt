gcc -m64 test.c xrt.c -o release/x64/test.exe

@echo;
@echo off

cd release
cd x64
test.exe

pause
