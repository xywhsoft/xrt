@echo off
setlocal

del /q /f release\x64\test*.exe 2>nul
del /q /f release\x64\xrt_test* 2>nul
del /q /f release\x64\test\out_*.txt 2>nul
del /q /f release\x64\test\test_ptr.txt 2>nul
del /q /f release\x64\test\test_put.txt 2>nul
del /q /f release\x64\test\test_putall.txt 2>nul
del /q /f release\x64\test\test_copy.txt 2>nul
del /q /f release\x64\test\test_move.txt 2>nul
del /q /f release\x64\test\test_size.txt 2>nul
del /q /f release\x64\test\test_write_u16.txt 2>nul
del /q /f release\x64\test\test_write_u16be.txt 2>nul
del /q /f release\x64\test\test_write_u32.txt 2>nul

echo.
echo build_GCC_CLEAN_TEST_x64.bat: PASS
