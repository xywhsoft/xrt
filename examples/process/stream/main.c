#include <xrt.h>

#include <stdio.h>
#include <string.h>



/*
 * 范例：process/stream —— Argv 启动 + 双管道流式读写
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtProcessConfigInit      直接执行（不经 shell）的配置入口
 *   Config.Program / Args / ArgCount   程序 + 参数数组（无注入面）
 *   XPROCESS_IO_PIPE          该流接父进程管道
 *   xrtProcessWrite / Read    向 stdin 写 / 从 stdout 读（流式）
 *   xrtProcessClose           关闭指定流端（EOF 语义）
 * 模块宏：XRT_MODULE_PROCESS
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c ${BS}
 *       examples/process/stream/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   streamed through a child process
 *
 * Argv vs Shell：参数逐个传递、不经过解释器——
 *   文件名带空格/引号都安全，不可信输入必走这条路。
 * Close(STDIN) 的时机：写完立即关——子进程（more/cat）
 *   看到 EOF 才会结束并刷出输出；不关会互相等死。
 */


/* 直接连接真实管道，适合需要自行控制流式读取的调用方。 */
int main(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		const cstr pArgs[] = { "/C", "more" };
		cstr sProgram = "cmd.exe";
	#else
		const cstr pArgs[] = { "-c", "cat" };
		cstr sProgram = "/bin/sh";
	#endif
	static const char sInput[] = "streamed through a child process\n";
	xprocessconfig Config;
	xprocess* pProcess;
	char pOutput[128];
	int64 iRead;

	if ( !xrtProcessConfigInit(&Config) ) {
		return 1;
	}
	Config.Program = sProgram;
	Config.Args = pArgs;
	Config.ArgCount = sizeof(pArgs) / sizeof(pArgs[0]);
	Config.Stdin.Mode = XPROCESS_IO_PIPE;
	Config.Stdout.Mode = XPROCESS_IO_PIPE;
	Config.Stderr.Mode = XPROCESS_IO_NULL;
	pProcess = xrtProcessSpawn(&Config);
	if ( pProcess == NULL ) {
		const xerror* pError = xrtGetError();

		fprintf(
			stderr,
			"process spawn failed: %s: %s (system=%d)\n",
			pError != NULL ? xrtErrorOperation(pError) : "spawn",
			pError != NULL ? xrtErrorMessage(pError) : "unknown error",
			pError != NULL ? xrtErrorSystemCode(pError) : 0
		);
		return 2;
	}
	if ( xrtProcessWrite(pProcess, sInput, sizeof(sInput) - 1u) <= 0 ) {
		xrtProcessDestroy(pProcess);
		return 3;
	}
	(void)xrtProcessClose(pProcess, XPROCESS_STDIN);
	while ( (iRead = xrtProcessRead(
		pProcess,
		XPROCESS_STDOUT,
		pOutput,
		sizeof(pOutput)
	)) > 0 ) {
		(void)fwrite(pOutput, 1u, (size_t)iRead, stdout);
	}
	if ( (iRead < 0) || (xrtProcessWait(pProcess) != XWAIT_OK) ) {
		xrtProcessDestroy(pProcess);
		return 4;
	}
	xrtProcessDestroy(pProcess);
	return 0;
}
