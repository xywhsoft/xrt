/*
 * XRT Example - Run Program
 * XRT 范例 - 运行程序
 *
 * Description / 说明:
 *   EN: Demonstrates xrtRun, xrtStart, xrtChain for executing external programs.
 *   CN: 演示 xrtRun、xrtStart、xrtChain 执行外部程序。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/os_run_program.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/os_run_program              (Linux, GCC)
 *
 * Note / 注意:
 *   - xrtRun: executes and waits, returns output string
 *   - xrtStart: starts without waiting, returns process handle
 *   - xrtChain: replaces current process (exec)
 *   - Size=0 means auto-detect string length
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

// Test xrtRun - execute and capture output
// 测试 xrtRun - 执行并捕获输出
void TestRun()
{
	printf("=== xrtRun (Execute and Capture) ===\n");
	printf("=== xrtRun (执行并捕获) ===\n");
	
	#if defined(_WIN32) || defined(_WIN64)
		// Windows: use cmd /c echo
		// Windows: 使用 cmd /c echo
		str sCmd = "cmd /c echo Hello from XRT!";
	#else
		// Linux: use echo
		// Linux: 使用 echo
		str sCmd = "echo 'Hello from XRT!'";
	#endif
	
	printf("Command: %s\n", sCmd);
	printf("命令: %s\n", sCmd);
	
	str sOutput = (str)xrtRun(sCmd, 0);
	
	if ( sOutput && sOutput != xCore.sNull ) {
		printf("Output: %s", sOutput);
		printf("输出: %s", sOutput);
		xrtFree(sOutput);
	} else {
		printf("No output or execution failed.\n");
		printf("无输出或执行失败。\n");
	}
	printf("\n");
}

// Test xrtRun with different commands
// 测试不同命令的 xrtRun
void TestRunCommands()
{
	printf("=== Run Different Commands ===\n");
	printf("=== 运行不同命令 ===\n");
	
	#if defined(_WIN32) || defined(_WIN64)
		char* psCommands[] = {
			"cmd /c ver",          // Windows version
			"cmd /c hostname",     // Computer name
			"cmd /c whoami",       // Current user
		};
		char* psDesc[] = {
			"Windows version",
			"Computer name", 
			"Current user",
		};
	#else
		char* psCommands[] = {
			"uname -a",            // System info
			"hostname",            // Computer name
			"whoami",              // Current user
		};
		char* psDesc[] = {
			"System info",
			"Computer name",
			"Current user",
		};
	#endif
	
	int iNumCmds = 3;
	int i;
	for ( i = 0; i < iNumCmds; i++ ) {
		printf("%s (%s):\n", psDesc[i], psDesc[i]);
		str sOutput = (str)xrtRun(psCommands[i], 0);
		if ( sOutput && sOutput != xCore.sNull ) {
			printf("  %s", sOutput);
			xrtFree(sOutput);
		} else {
			printf("  (failed)\n");
		}
	}
	printf("\n");
}

// Test xrtStart - start without waiting
// 测试 xrtStart - 启动不等待
void TestStart()
{
	printf("=== xrtStart (Start Without Wait) ===\n");
	printf("=== xrtStart (启动不等待) ===\n");
	
	#if defined(_WIN32) || defined(_WIN64)
		// Windows: start notepad
		// Windows: 启动记事本
		str sCmd = "notepad";
	#else
		// Linux: start xeyes (if available)
		// Linux: 启动 xeyes (如果可用)
		str sCmd = "xeyes";
	#endif
	
	printf("Starting: %s\n", sCmd);
	printf("启动: %s\n", sCmd);
	printf("(Process starts independently, this program continues)\n");
	printf("(进程独立启动，本程序继续运行)\n");
	
	ptr pHandle = xrtStart(sCmd, 0);
	
	if ( pHandle ) {
		printf("Process started successfully.\n");
		printf("进程启动成功。\n");
		// Note: pHandle is platform-specific
		// 注意: pHandle 是平台相关的
	} else {
		printf("Failed to start process.\n");
		printf("进程启动失败。\n");
	}
	printf("\n");
}

// Test command with arguments
// 测试带参数的命令
void TestRunWithArgs()
{
	printf("=== Command with Arguments ===\n");
	printf("=== 带参数的命令 ===\n");
	
	#if defined(_WIN32) || defined(_WIN64)
		// Windows: dir with arguments
		// Windows: 带参数的 dir
		str sCmd = "cmd /c dir /b .";
	#else
		// Linux: ls with arguments
		// Linux: 带参数的 ls
		str sCmd = "ls -la .";
	#endif
	
	printf("Command: %s\n", sCmd);
	printf("命令: %s\n", sCmd);
	
	str sOutput = (str)xrtRun(sCmd, 0);
	
	if ( sOutput && sOutput != xCore.sNull ) {
		printf("Output:\n%s", sOutput);
		printf("输出:\n%s", sOutput);
		xrtFree(sOutput);
	} else {
		printf("No output.\n");
		printf("无输出。\n");
	}
	printf("\n");
}

// Demonstrate chain (note: this replaces current process)
// 演示 chain (注意: 这会替换当前进程)
void TestChainInfo()
{
	printf("=== xrtChain (Process Replacement) ===\n");
	printf("=== xrtChain (进程替换) ===\n");
	
	printf("xrtChain replaces current process with new one.\n");
	printf("xrtChain 用新进程替换当前进程。\n");
	printf("It does NOT return - the new program takes over.\n");
	printf("它不会返回 - 新程序接管控制。\n");
	printf("\nExample usage (not executed here):\n");
	printf("示例用法 (此处不执行):\n");
	
	#if defined(_WIN32) || defined(_WIN64)
		printf("  xrtChain(\"notepad.exe\", 0);\n");
	#else
		printf("  xrtChain(\"/bin/ls\", 0);\n");
	#endif
	
	printf("\nAfter this call, the current program is replaced.\n");
	printf("调用此函数后，当前程序被替换。\n");
	printf("Use xrtChain for process replacement (exec-style).\n");
	printf("使用 xrtChain 进行进程替换 (exec 风格)。\n");
	printf("\n");
}

// Test practical use cases
// 测试实际用例
void TestPracticalUseCases()
{
	printf("=== Practical Use Cases ===\n");
	printf("=== 实际用例 ===\n");
	
	// Get system information
	// 获取系统信息
	printf("System information:\n");
	printf("系统信息:\n");
	
	#if defined(_WIN32) || defined(_WIN64)
		str sOutput = (str)xrtRun("cmd /c echo %OS%", 0);
		printf("  OS: %s", sOutput ? sOutput : "(unknown)");
		if ( sOutput ) xrtFree(sOutput);
		
		sOutput = (str)xrtRun("cmd /c echo %PROCESSOR_ARCHITECTURE%", 0);
		printf("  Arch: %s", sOutput ? sOutput : "(unknown)");
		if ( sOutput ) xrtFree(sOutput);
	#else
		str sOutput = (str)xrtRun("uname -s", 0);
		printf("  OS: %s", sOutput ? sOutput : "(unknown)");
		if ( sOutput ) xrtFree(sOutput);
		
		sOutput = (str)xrtRun("uname -m", 0);
		printf("  Arch: %s", sOutput ? sOutput : "(unknown)");
		if ( sOutput ) xrtFree(sOutput);
	#endif
	
	printf("\n");
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT OS - Run Program Demo\n");
	printf("XRT 操作系统 - 运行程序演示\n");
	printf("==========================\n\n");
	
	TestRun();
	TestRunCommands();
	TestStart();
	TestRunWithArgs();
	TestChainInfo();
	TestPracticalUseCases();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	return 0;
}
