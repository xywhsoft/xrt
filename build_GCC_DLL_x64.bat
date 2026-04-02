@echo off
cd /d "%~dp0"

set GCC_WARN=-Wall -Wextra

if not exist release\x64 (
	mkdir release\x64 || goto :end
)

gcc -m64 -shared xrt.c -O2 -s -fPIC -ffunction-sections -fdata-sections -Wl,--gc-sections %GCC_WARN% -DBUILD_DLL -lWs2_32 -lIPHLPAPI -o release/x64/xrt.dll
if errorlevel 1 goto :end

echo.
echo Build successful: release\x64\xrt.dll

:end
pause
