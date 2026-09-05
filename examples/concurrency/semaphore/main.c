#include <stdio.h>
#include <xrt.h>



/*
 * 范例：concurrency/semaphore —— 信号量：工作许可交付
 * ----------------------------------------------------------------
 * 演示 API：
 *   xsem 等待族（xwaitresult 统一语义）
 * 模块宏：XRT_MODULE_SEMAPHORE
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/concurrency/semaphore/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   semaphore wait result: 0
 *
 * 计数信号量 = N 个许可：连接池限流、生产者配速的
 *   原语。等待结果 0 即 XWAIT_OK——与其他等待类
 *   （线程/条件/Future）共享同一结果枚举。
 */


/* 用计数信号量交付一个工作许可。 */
int main(void)
{
	xsem Semaphore;
	xwaitresult Result;

	if ( !xrtSemInit(&Semaphore, 0, 1) ||
		!xrtSemPost(&Semaphore) ) {
		return 1;
	}
	Result = xrtSemWaitUntil(
		&Semaphore,
		xrtDeadlineAfter(UINT64_C(1000000))
	);
	printf("semaphore wait result: %d\n", (int)Result);
	return xrtSemUnit(&Semaphore) &&
		(Result == XWAIT_OK) ? 0 : 2;
}
