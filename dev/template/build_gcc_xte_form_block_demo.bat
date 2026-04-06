@echo off
setlocal
cd /d "%~dp0"

if not exist ..\..\release\x64 (
	mkdir ..\..\release\x64 || exit /b 1
)

gcc -m64 xte_form_block_demo.c -I ..\.. -lWs2_32 -lIPHLPAPI -O2 -Wall -Wextra -ffunction-sections -fdata-sections -Wl,--gc-sections -o ..\..\release\x64\xte_form_block_demo.exe || exit /b 1

..\..\release\x64\xte_form_block_demo.exe || exit /b 1

echo.
echo build_gcc_xte_form_block_demo.bat: PASS
exit /b 0
