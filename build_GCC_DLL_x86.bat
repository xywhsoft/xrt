gcc -m32 -shared xrt.c -o2 -s -ffunction-sections -fdata-sections -Wl,--gc-sections -DBUILD_DLL -lWs2_32 -lIPHLPAPI -o release/x86/xrt.dll

@echo;
@echo off

pause
