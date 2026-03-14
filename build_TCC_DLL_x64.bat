tcc -m64 -shared xrt.c -std=c99 -DBUILD_DLL -lWs2_32 -lIPHLPAPI -o release/x64/xrt.dll

@echo;
@echo off

pause
