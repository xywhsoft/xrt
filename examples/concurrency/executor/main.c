#include <stdio.h>
#include <xrt.h>



/*
 * 范例：concurrency/executor —— 执行器：提交-完成的最小闭环
 * ----------------------------------------------------------------
 * 演示 API：
 *   执行器提交入口（无返回值任务）
 *   完成等待
 * 模块宏：XRT_MODULE_EXECUTOR
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c $BS}
 *       examples/concurrency/executor/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   completed: 1000
 *
 * executor 是"执行资源"抽象：任务提交给它而不是直接
 *   开线程——池化复用、work-stealing 都在这层实现，
 *   业务只见提交与完成（completed: 1000 即工作产物）。
 */


/* 示例工作不需要返回值或 Future。 */
static void executorExampleRun(ptr pData)
{
	xatomic32* pCount = (xatomic32*)pData;

	(void)xrtAtomic32FetchAdd(pCount, 1, XMEMORY_RELAXED);
}



int main(void)
{
	xexecutor* pExecutor = xrtExecutorCreate(NULL);
	xatomic32 Count;

	xrtAtomic32Init(&Count, 0);
	if ( pExecutor == NULL ) {
		return 1;
	}
	for ( size_t i = 0; i < 1000; i++ ) {
		if ( !xrtExecutorSubmit(
			pExecutor,
			executorExampleRun,
			&Count,
			NULL,
			NULL
		) ) {
			(void)xrtExecutorCancel(pExecutor);
			(void)xrtExecutorDestroy(pExecutor);
			return 2;
		}
	}
	if ( !xrtExecutorDestroy(pExecutor) ) {
		return 3;
	}
	printf("completed: %u\n", xrtAtomic32Load(&Count, XMEMORY_ACQUIRE));
	return 0;
}
