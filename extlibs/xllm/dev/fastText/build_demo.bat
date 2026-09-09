@echo off
setlocal EnableDelayedExpansion

if not exist build (
    mkdir build
)

set SOURCES=
for %%F in (vendor\fastText\src\*.cc) do (
    if /I not "%%~nxF"=="main.cc" (
        set SOURCES=!SOURCES! %%F
    )
)

echo ========================================
echo   fastText Demo Builder
echo ========================================
echo.

g++ -std=c++17 -Wall -Wextra -O2 -I. -Ivendor\fastText\src ^
    demo_main.c ^
    src\fasttext_demo.cpp ^
    !SOURCES! ^
    -o build\fasttext_demo.exe

if errorlevel 1 (
    echo.
    echo Build failed.
    exit /b 1
)

echo.
echo Running fastText demo...
build\fasttext_demo.exe

endlocal
