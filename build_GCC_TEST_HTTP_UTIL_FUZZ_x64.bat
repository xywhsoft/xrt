@echo off
setlocal

if not exist release\x64 mkdir release\x64

gcc -m64 dev\fuzz_http_util_core.c xrt.c -I . -O2 -s -ffunction-sections -fdata-sections -Wl,--gc-sections -lws2_32 -liphlpapi -o release\x64\xrt_test_http_util_fuzz.exe
if errorlevel 1 exit /b %errorlevel%

release\x64\xrt_test_http_util_fuzz.exe
exit /b %errorlevel%
