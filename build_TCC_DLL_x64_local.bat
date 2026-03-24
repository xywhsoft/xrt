@echo off
setlocal

if not exist release\x64 (
	mkdir release\x64 || goto :eof
)

rem TCC currently does not support XRT's production coroutine backend.
rem Build the local DLL with coroutine disabled instead of touching the
rem tracked release\x64\xrt.dll artifact.
tcc -m64 -shared xrt.c -std=c99 -DBUILD_DLL -DXRT_NO_COROUTINE -lWs2_32 -lIPHLPAPI -lShell32 -o release/x64/xrt_tcc_local.dll
if errorlevel 1 goto :eof

del /f /q release\x64\xrt_tcc_local.def >nul 2>nul

echo.
echo Local TCC DLL build successful: release\x64\xrt_tcc_local.dll
goto :eof
