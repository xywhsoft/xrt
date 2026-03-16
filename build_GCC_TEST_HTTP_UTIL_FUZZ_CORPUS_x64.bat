@echo off
setlocal

call build_GCC_TEST_HTTP_UTIL_FUZZ_x64.bat
if errorlevel 1 exit /b %errorlevel%

for %%f in (dev\fuzz\http_util_corpus\*) do (
	release\x64\xrt_test_http_util_fuzz.exe "%%f"
	if errorlevel 1 exit /b %errorlevel%
)

echo HTTP util fuzz corpus run passed.
exit /b 0
