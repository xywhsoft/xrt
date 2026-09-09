@echo off
setlocal
set XLLM_REPO_ROOT=%~dp0..\..\..
for %%I in ("%XLLM_REPO_ROOT%") do set XLLM_REPO_ROOT=%%~fI
cd /d "%XLLM_REPO_ROOT%"
pushd examples\openai\azure_openai
if not "%errorlevel%"=="0" exit /b %errorlevel%
call build.bat
set ERR=%errorlevel%
popd
exit /b %ERR%
