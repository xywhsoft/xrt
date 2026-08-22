@echo off
setlocal

set XSSH_DIR=%~dp0..\..
for %%I in ("%XSSH_DIR%") do set XSSH_DIR=%%~fI

if "%XUI2_DIR%"=="" (
	set XUI2_DIR=D:\git\xge\dev\xui2
)
for %%I in ("%XUI2_DIR%") do set XUI2_DIR=%%~fI

set OUT_DIR=%XSSH_DIR%\bin
set OUT=%OUT_DIR%\xssh_terminal_xui2.exe
set XGE_BUILD=%XUI2_DIR%\build
set XGE_LIB=%XGE_BUILD%\xge.lib
set XGE_DLL=%XGE_BUILD%\xge.dll
set SRC="%~dp0main.c" "%XUI2_DIR%\src\xui_core.c" "%XUI2_DIR%\src\xui_widget.c" "%XUI2_DIR%\src\xui_input.c" "%XUI2_DIR%\src\xui_text.c" "%XUI2_DIR%\src\xui_label.c" "%XUI2_DIR%\src\xui_button.c" "%XUI2_DIR%\src\xui_scroll_model.c" "%XUI2_DIR%\src\xui_scrollbar.c" "%XUI2_DIR%\src\xui_scroll_frame.c" "%XUI2_DIR%\src\xui_scroll_view.c" "%XUI2_DIR%\src\xui_popup.c" "%XUI2_DIR%\src\xui_menu.c" "%XUI2_DIR%\src\xui_terminal.c" "%XUI2_DIR%\src\xui_proxy_xge.c"
set INC=-I"%XUI2_DIR%"
set FLAGS=-O2 -Wall -Wextra -Wno-unused-parameter -Wno-unused-function -Wno-cast-function-type -DXGE_DLL -DXGE_DEBUGMODE=0
set LIBS="%XGE_LIB%" -lm -lws2_32 -liphlpapi -lgdi32 -luser32 -lshell32 -lopengl32 -lole32 -lwinmm -lavrt

where gcc >nul 2>nul
if %errorlevel% neq 0 (
	echo [ERROR] gcc not found in PATH
	exit /b 1
)

if not exist "%XUI2_DIR%\xui.h" (
	echo [ERROR] XUI2_DIR is invalid: %XUI2_DIR%
	exit /b 1
)

if not exist "%OUT_DIR%" (
	mkdir "%OUT_DIR%" || exit /b 1
)

if not exist "%XGE_LIB%" (
	pushd "%XUI2_DIR%" || exit /b 1
	call build_dll.bat
	set BUILD_RET=%errorlevel%
	popd
	if not "%BUILD_RET%"=="0" exit /b %BUILD_RET%
)

if not exist "%XGE_DLL%" (
	echo [ERROR] missing xge.dll: %XGE_DLL%
	exit /b 1
)

echo [xssh] Building xssh_terminal_xui2...
gcc %FLAGS% %INC% -o "%OUT%" %SRC% %LIBS%
if %errorlevel% neq 0 (
	echo [xssh] Build failed
	exit /b 1
)

copy /Y "%XGE_DLL%" "%OUT_DIR%\xge.dll" >nul
if %errorlevel% neq 0 exit /b %errorlevel%

echo [xssh] Build successful: %OUT%
endlocal
