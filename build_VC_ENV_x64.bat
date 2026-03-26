@echo off

where cl.exe >nul 2>nul
if not errorlevel 1 goto :eof

set "XRT_VSWHERE="
for %%I in ("%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe") do set "XRT_VSWHERE=%%~sI"
if not exist "%XRT_VSWHERE%" (
	echo [ERROR] vswhere.exe not found: %XRT_VSWHERE%
	echo [ERROR] Please install Visual Studio Build Tools or open a Developer Command Prompt.
	exit /b 1
)

set "XRT_VS_ROOT="
for /f "usebackq delims=" %%I in (`""%XRT_VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath"`) do (
	set "XRT_VS_ROOT=%%I"
)

if not defined XRT_VS_ROOT (
	echo [ERROR] Visual C++ Build Tools not found.
	exit /b 1
)

for %%I in ("%XRT_VS_ROOT%") do set "XRT_VS_ROOT=%%~sI"

set "XRT_VSDEVCMD=%XRT_VS_ROOT%\Common7\Tools\VsDevCmd.bat"
if not exist "%XRT_VSDEVCMD%" (
	echo [ERROR] VsDevCmd.bat not found: %XRT_VSDEVCMD%
	exit /b 1
)

call "%XRT_VSDEVCMD%" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 (
	echo [ERROR] Failed to initialize VC build environment.
	exit /b 1
)

where cl.exe >nul 2>nul
if errorlevel 1 (
	echo [ERROR] cl.exe is unavailable after initializing the VC environment.
	exit /b 1
)

exit /b 0
