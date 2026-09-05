#include <stdio.h>

#include <xrt.h>



/*
 * 范例：process/future —— 异步等待子进程：Future 承载退出状态
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtProcessWaitAsync   异步等待：立即返回 Future
 *   xrtFutureWait / Value / Destroy   等待完成 / 取值 / 释放
 *   xprocessstatus        退出状态结构（Code / 信号等）
 * 模块宏：XRT_MODULE_PROCESS（依赖 FUTURE）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c ${BS}
 *       examples/process/future/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   exit code: 0
 *
 * 异步等待的价值：同时跑 N 个子进程（编译矩阵、批量转换）时，
 *   Wait 会串行阻塞；WaitAsync 的 Future 可以入 any/all 组合、
 *   挂到事件循环——并发编排的门票。
 * Future 拥有退出状态的所有权：Value 返回借用指针，
 *   Destroy 前有效。
 */


/* 异步等待一个外部命令，并读取 Future 拥有的退出状态。 */
int main(void)
{
	const xprocessstatus* pStatus;
	xprocessconfig Config;
	xprocess* pProcess;
	xfuture* pFuture;
	bool bSuccess;

	#if defined(_WIN32) || defined(_WIN64)
		if ( !xrtProcessShellConfigInit(&Config, "exit /B 0") ) {
			return 1;
		}
	#else
		if ( !xrtProcessShellConfigInit(&Config, "exit 0") ) {
			return 1;
		}
	#endif
	Config.Stdin.Mode = XPROCESS_IO_NULL;
	Config.Stdout.Mode = XPROCESS_IO_NULL;
	Config.Stderr.Mode = XPROCESS_IO_NULL;
	pProcess = xrtProcessSpawn(&Config);
	if ( pProcess == NULL ) {
		return 2;
	}
	pFuture = xrtProcessWaitAsync(pProcess);
	xrtProcessDestroy(pProcess);
	if ( (pFuture == NULL) || (xrtFutureWait(pFuture) != XWAIT_OK) ) {
		xrtFutureDestroy(pFuture);
		return 3;
	}
	pStatus = (const xprocessstatus*)xrtFutureValue(pFuture);
	printf("exit code: %d\n", pStatus != NULL ? pStatus->Code : -1);
	bSuccess = (pStatus != NULL) && (pStatus->Code == 0);
	xrtFutureDestroy(pFuture);
	return bSuccess ? 0 : 4;
}
