#include <stdio.h>

#include <xrt.h>



/*
 * 范例：process/terminal —— 伪终端（PTY）中执行命令
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtProcessTerminalSupported   当前平台是否支持 PTY
 *   Config.Terminal = true        请求分配伪终端
 *   xrtProcessRead                读合并输出（stdout+stderr 同流）
 * 模块宏：XRT_MODULE_PROCESS
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c ${BS}
 *       examples/process/terminal/main.c -lws2_32 -liphlpapi
 * 预期输出：terminal output（前面混有终端初始化转义序列——
 *   ESC[2J 清屏等，这正是 PTY 在工作的证据）
 *
 * 为什么需要 PTY：有些程序检测到"非终端"就改变行为
 *   （颜色关闭、进度条消失、缓冲模式变化）；
 *   分配伪终端让它们以为连着真人终端。
 * 输出含 ANSI 转义序列是正常现象——消费端要么原样转发，
 *   要么接终端模拟器渲染（xws 网页终端即此用法）。
 */


/* 在伪终端中执行命令并流式转发合并输出。 */
int main(void)
{
	xprocessconfig Config;
	xprocess* pProcess;
	char sOutput[256];
	int64 iRead;

	if ( !xrtProcessTerminalSupported() ) {
		return 0;
	}
	#if defined(_WIN32) || defined(_WIN64)
		if ( !xrtProcessShellConfigInit(&Config, "echo terminal output") ) {
			return 1;
		}
	#else
		if ( !xrtProcessShellConfigInit(&Config, "printf 'terminal output\\n'") ) {
			return 1;
		}
	#endif
	Config.Terminal = true;
	pProcess = xrtProcessSpawn(&Config);
	if ( pProcess == NULL ) {
		return 2;
	}
	while ( (iRead = xrtProcessRead(
		pProcess,
		XPROCESS_STDOUT,
		sOutput,
		sizeof(sOutput)
	)) > 0 ) {
		(void)fwrite(sOutput, 1u, (size_t)iRead, stdout);
	}
	if ( (iRead < 0) || (xrtProcessWait(pProcess) != XWAIT_OK) ) {
		xrtProcessDestroy(pProcess);
		return 3;
	}
	xrtProcessDestroy(pProcess);
	return 0;
}
