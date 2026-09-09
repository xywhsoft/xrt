@echo off
setlocal

cd /d "%~dp0"

set "BUILD_DIR=%CD%\build"
set "DOWNLOAD_DIR=%BUILD_DIR%\downloads"
set "RUNTIME_VERSION=1.24.4"
set "RUNTIME_BASENAME=onnxruntime-win-x64-%RUNTIME_VERSION%"
set "RUNTIME_ZIP=%DOWNLOAD_DIR%\%RUNTIME_BASENAME%.zip"
set "RUNTIME_ROOT=%BUILD_DIR%\%RUNTIME_BASENAME%"
set "RUNTIME_DLL=%RUNTIME_ROOT%\lib\onnxruntime.dll"
set "INCLUDE_DIR=%RUNTIME_ROOT%\include"
set "OUTPUT_EXE=%BUILD_DIR%\onnxdemo_demo.exe"

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
if not exist "%DOWNLOAD_DIR%" mkdir "%DOWNLOAD_DIR%"

echo [1/4] preparing minimal ONNX assets
powershell -NoProfile -ExecutionPolicy Bypass -File "%CD%\..\minionnx\prepare_demo_minimal_assets.ps1"
if errorlevel 1 exit /b 1

echo [2/4] fetching prebuilt ONNX Runtime
if not exist "%RUNTIME_DLL%" (
    powershell -NoProfile -ExecutionPolicy Bypass -Command "Invoke-WebRequest -Uri 'https://github.com/microsoft/onnxruntime/releases/download/v%RUNTIME_VERSION%/%RUNTIME_BASENAME%.zip' -OutFile '%RUNTIME_ZIP%'"
    if errorlevel 1 exit /b 1
    powershell -NoProfile -ExecutionPolicy Bypass -Command "Expand-Archive -LiteralPath '%RUNTIME_ZIP%' -DestinationPath '%BUILD_DIR%' -Force"
    if errorlevel 1 exit /b 1
)

echo [3/4] building C demo
gcc -std=c11 -Wall -Wextra -I"%INCLUDE_DIR%" demo_main.c src\onnx_demo.c -o "%OUTPUT_EXE%"
if errorlevel 1 exit /b 1

copy /Y "%RUNTIME_DLL%" "%BUILD_DIR%\onnxruntime.dll" >NUL

echo [4/4] running demo
"%OUTPUT_EXE%"
