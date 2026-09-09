@echo off
cd /d "%~dp0"

if not exist "singlehead" mkdir "singlehead"

if exist "singlehead\single_head_maker.exe" del /f /q "singlehead\single_head_maker.exe"

gcc -std=c11 -O2 -s singlehead\single_head_maker.c -o singlehead\single_head_maker.exe
if errorlevel 1 exit /b %errorlevel%

singlehead\single_head_maker.exe
if errorlevel 1 exit /b %errorlevel%

echo.
echo Single header generated: singlehead\xwork.h
