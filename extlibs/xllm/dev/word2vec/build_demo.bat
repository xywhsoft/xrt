@echo off
setlocal

if not exist build (
    mkdir build
)

echo ========================================
echo   word2vec Demo Builder
echo ========================================
echo.

gcc -std=c11 -Wall -Wextra -O2 ^
    vendor\word2vec\word2vec.c ^
    -o build\word2vec_train.exe ^
    -lm -pthread

if errorlevel 1 (
    echo.
    echo Trainer build failed.
    exit /b 1
)

gcc -std=c11 -Wall -Wextra -O2 ^
    demo_main.c ^
    src\word2vec_demo.c ^
    -o build\word2vec_demo.exe ^
    -lm

if errorlevel 1 (
    echo.
    echo Demo build failed.
    exit /b 1
)

echo.
echo Running word2vec demo...
build\word2vec_demo.exe

endlocal
