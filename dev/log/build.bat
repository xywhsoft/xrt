@echo off

cd /d "%~dp0"

echo ========================================
echo XRT 日志系统编译脚本
echo ========================================
echo.

REM 检查 TCC 是否可用
where tcc >nul 2>&1
if errorlevel 1 (
    echo 错误: 找不到 TCC 编译器！
    echo 请确保 TCC 已安装并添加到 PATH 环境变量中。
    echo.
    pause
    exit /b 1
)

echo TCC 版本:
tcc -v
echo.

echo ========================================
echo 开始编译...
echo ========================================
echo.

REM 编译测试程序
REM test_xrt_log.c 定义 XRT_IMPLEMENTATION
REM xrt_log.c 不定义 XRT_IMPLEMENTATION（通过 include xrt.h 使用 XRT）
tcc xrt_log.c test_xrt_log.c -o test.exe

if errorlevel 1 (
    echo.
    echo ========================================
    echo 编译失败！
    echo ========================================
    echo.
    pause
    exit /b 1
)

echo.
echo ========================================
echo 编译成功！
echo ========================================
echo.
echo 运行程序:
echo.

test.exe

echo.
echo ========================================
echo 程序执行完毕
echo ========================================
echo.
echo 日志文件: app.log

pause
