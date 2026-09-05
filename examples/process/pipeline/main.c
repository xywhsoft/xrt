#include <stdio.h>

#include <xrt.h>



/*
 * 范例：process/pipeline —— 多阶段管道：stdout 自动接下一段 stdin
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtProcessPipeline         按阶段数组生成并串联全部子进程
 *   Result.Stdout / Size       末段输出捕获（拥有式）
 *   xrtProcessPipelineSuccess  全段退出状态判定
 * 模块宏：XRT_MODULE_PROCESS
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c ${BS}
 *       examples/process/pipeline/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   pipeline output
 *
 * 等价 shell 的 a | b：阶段间真实 OS 管道连接，
 *   数据流不经过父进程；Windows 段用 findstr 过滤、
 *   POSIX 段用 tr 变大写（平台各取等价工具）。
 * 与多进程 Spawn 手工连管相比：泄漏、句柄继承、
 *   死锁（双端都满）这些坑全部由实现接管。
 */


/* 用两个 Shell 阶段演示真实管道连接与末段捕获。 */
int main(void)
{
	xprocesspipelineresult Result;
	xprocessconfig Stages[2];
	bool bOk;

	#if defined(_WIN32) || defined(_WIN64)
		if ( !xrtProcessShellConfigInit(&Stages[0], "echo pipeline output") ||
			!xrtProcessShellConfigInit(&Stages[1], "findstr pipeline") ) {
			return 1;
		}
	#else
		if ( !xrtProcessShellConfigInit(&Stages[0], "printf 'pipeline output\n'") ||
			!xrtProcessShellConfigInit(&Stages[1], "tr a-z A-Z") ) {
			return 1;
		}
	#endif
	bOk = xrtProcessPipeline(Stages, 2u, NULL, &Result);
	if ( !bOk ) {
		const xerror* pError = xrtGetError();

		fprintf(
			stderr,
			"pipeline failed: %s\n",
			pError != NULL ? xrtErrorMessage(pError) : "unknown error"
		);
		return 2;
	}
	fwrite(Result.Stdout, 1u, Result.StdoutSize, stdout);
	bOk = xrtProcessPipelineSuccess(&Result);
	xrtProcessPipelineResultUnit(&Result);
	return bOk ? 0 : 3;
}
