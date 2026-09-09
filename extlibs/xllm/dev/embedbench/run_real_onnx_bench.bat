@echo off
setlocal

set SCRIPT_DIR=%~dp0
set BUILD_DIR=%SCRIPT_DIR%build\real_onnx_env

if not exist "%BUILD_DIR%\Scripts\python.exe" (
  echo [real-onnx] creating local venv...
  python -m venv "%BUILD_DIR%"
)

echo [real-onnx] installing dependencies...
"%BUILD_DIR%\Scripts\python.exe" -m pip install --upgrade pip >nul
"%BUILD_DIR%\Scripts\python.exe" -m pip install -r "%SCRIPT_DIR%requirements-real-onnx.txt" || exit /b 1

echo [real-onnx] running benchmark...
"%BUILD_DIR%\Scripts\python.exe" "%SCRIPT_DIR%run_real_onnx_bench.py" %*
exit /b %errorlevel%
