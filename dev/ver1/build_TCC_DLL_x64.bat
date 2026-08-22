@echo off
cd /d "%~dp0"

if not exist release\x64 (
	mkdir release\x64 || goto :end
)

tcc -m64 -shared xrt.c -std=c99 -DBUILD_DLL -lWs2_32 -lIPHLPAPI -lShell32 -o release/x64/xrt.dll
if errorlevel 1 goto :end

echo.
echo Build successful: release\x64\xrt.dll

:end
pause
