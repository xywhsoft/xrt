@echo off
setlocal

if not exist bin (
	mkdir bin || exit /b 1
)

tcc -m64 main.c -I ..\..\singlehead -lws2_32 -liphlpapi -o bin\xspider.exe || exit /b 1

echo.
echo build_TCC_x64.bat: PASS
exit /b 0
