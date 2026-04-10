@echo off
setlocal

if not exist bin (
	mkdir bin || exit /b 1
)

gcc -m64 main.c -I ..\..\singlehead -lWs2_32 -lIPHLPAPI -O2 -s -ffunction-sections -fdata-sections -Wl,--gc-sections -o bin\xspider.exe || exit /b 1

echo.
echo build_GCC_x64.bat: PASS
exit /b 0
