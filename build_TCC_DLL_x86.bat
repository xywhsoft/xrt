tcc -m32 -shared xrt.c -std=c99 -DBUILD_DLL -o release/x86/xrt.dll

@echo;
@echo off

pause
