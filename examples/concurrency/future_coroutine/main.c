#include <stdio.h>
#include <xrt.h>



/* 协程等待示例在同一调度器中消费一个已完成 Future。 */
static ptr awaitFuture(ptr pData)
{
	xfuture* pFuture = (xfuture*)pData;

	if ( xrtFutureAwait(pFuture) != XWAIT_OK ) {
		return NULL;
	}
	return xrtFutureValue(pFuture);
}



/*
 * 范例：concurrency/future_coroutine —— 协程 × Future：等待桥
 * ----------------------------------------------------------------
 * 演示 API：
 *   Future 等待的协程变体（Await——不阻塞原生线程）
 * 模块宏：XRT_MODULE_FUTURE_BRIDGE
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c $BS}
 *       examples/concurrency/future_coroutine/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   await value: 64
 *
 * future_bridge 把"等 Future"翻译成协程挂起：调度器
 *   线程不睡，等 Future 完成后恢复协程——异步世界
 *   的两大模型在此互通。
 */


/* 演示 Future 与协程调度器之间不阻塞原生线程的等待桥。 */
int main(void)
{
	xfuture* pFuture;
	xpromise* pPromise;
	xcosched* pSched;
	xcoro* pCo;
	int iValue = 64;

	pPromise = xrtPromiseCreate(&pFuture, NULL);
	pSched = xrtCoSchedCreate();
	if ( (pPromise == NULL) || (pSched == NULL) ||
		 !xrtPromiseResolve(pPromise, &iValue) ) {
		return 1;
	}
	pCo = xrtCoSpawn(pSched, awaitFuture, pFuture, NULL);
	if ( (pCo == NULL) || !xrtCoSchedRun(pSched) ) {
		return 2;
	}
	printf("await value: %d\n", *(int*)xrtCoResult(pCo));
	xrtCoDestroy(pCo);
	xrtCoSchedDestroy(pSched);
	xrtPromiseDestroy(pPromise);
	xrtFutureDestroy(pFuture);
	return 0;
}
