@echo off
setlocal

if not exist release\x64 (
	mkdir release\x64 || goto :eof
)

gcc -m64 -shared xrt.c -O2 -s -fPIC -ffunction-sections -fdata-sections -Wl,--gc-sections -DBUILD_DLL -lWs2_32 -lIPHLPAPI -o release/x64/xrt.dll || goto :eof

echo.
echo Release GCC DLL build successful: release\x64\xrt.dll
goto :eof
