tcc -m64 test.c xrt.c -DDEBUG_TRACE -o release/x64/test.exe

@echo;
@echo off

cd release
cd x64
test.exe

pause
