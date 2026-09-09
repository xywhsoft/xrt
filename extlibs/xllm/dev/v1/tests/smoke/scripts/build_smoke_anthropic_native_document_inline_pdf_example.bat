@echo off
setlocal
set XLLM_REPO_ROOT=%~dp0..\..\..
for %%I in ("%XLLM_REPO_ROOT%") do set XLLM_REPO_ROOT=%%~fI
cd /d "%XLLM_REPO_ROOT%"

if not exist build mkdir build

gcc -std=c11 -Wall -Wextra -I. -Ilib -DXRT_IMPLEMENTATION -DXLLM_IMPLEMENTATION ^
    examples\smoke_anthropic_native_document_inline_pdf.c ^
    -o build\smoke_anthropic_native_document_inline_pdf.exe ^
    -lws2_32 -liphlpapi -lshell32

if not "%errorlevel%"=="0" exit /b %errorlevel%

build\smoke_anthropic_native_document_inline_pdf.exe
if not "%errorlevel%"=="0" exit /b %errorlevel%

echo smoke_anthropic_native_document_inline_pdf ok
