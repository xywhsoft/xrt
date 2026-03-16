@echo off
setlocal

call "%~dp0build_GCC_TEST_COROUTINE_STAGE_x64.bat" || goto :eof
call "%~dp0build_GCC_TEST_HTTP_UTIL_FUZZ_CORPUS_x64.bat" || goto :eof
call "%~dp0build_GCC_TEST_XNET2_STAGE_x64.bat" || goto :eof

echo.
echo build_GCC_TEST_MODERN_x64.bat: PASS
