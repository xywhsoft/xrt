@echo off
setlocal

if not exist release\x64 (
	mkdir release\x64 || goto :eof
)

rem TCC currently does not support XRT's production coroutine backend.
rem Build the tracked DLL with coroutine disabled instead of falling back to
rem an archived backend implementation.
tcc -m64 -shared xrt.c -std=c99 -DBUILD_DLL -DXRT_NO_COROUTINE -lWs2_32 -lIPHLPAPI -lShell32 -o release/x64/xrt.dll
if errorlevel 1 goto :eof

echo.
echo Release TCC DLL build successful: release\x64\xrt.dll
goto :eof
