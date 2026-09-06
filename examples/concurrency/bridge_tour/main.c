/*
 * 范例：concurrency/bridge_tour —— Future/Promise 装配桥
 * ----------------------------------------------------------------
 * 演示 API：
 *   【创建】      xrtFutureBridgeCreate（Future+桥一体）
 *                 xrtFutureBridgeInit（挂到已有 Promise）
 *                 xrtFutureBridgePromise（借用底层 Promise）
 *   【装配】      xrtFutureBridgeReady（成功——允许写终态）
 *                 xrtFutureBridgeFail（失败——只回收结果）
 *                 xrtFutureBridgeWait（装配窗口查询）
 *   【取消转发】  xrtFutureBridgeWatch / Unwatch
 * 模块宏：XRT_MODULE_FUTURE_BRIDGE
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/concurrency/bridge_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   bridge: create + promise borrow ok
 *   bridge: watch + ready -> resolve -> future value ok
 *   bridge: init on own promise + fail -> wait false ok
 *
 * 桥解决"异步操作先返回 Future、结果稍后才到"的装配竞态：
 *   Ready 之前的终态写入被挂起，Ready/Fail 决定放行或回收。
 */

#include <stdio.h>
#include <stdbool.h>
#include <xrt.h>

/* 取消回调：记录被转发的事实。 */
static void exampleCancel(ptr pData)
{
	bool* pCancelled = (bool*)pData;

	*pCancelled = true;
}

int main(void)
{
	xfuturebridge Bridge;
	xfuturebridge Bridge2;
	xfuture* pFuture = NULL;
	xfuture* pFuture2 = NULL;
	xpromise* pPromise2 = NULL;
	xpromise* pBorrowed = NULL;
	bool bCancelled = false;
	int iResult = 1;

	/* ---- Create：Future 与桥一体创建，Promise 可借用。 ---- */
	pFuture = xrtFutureBridgeCreate(&Bridge, NULL);
	if ( (pFuture == NULL) ||
		((pBorrowed = xrtFutureBridgePromise(&Bridge)) == NULL) ) {
		goto Cleanup;
	}
	printf("bridge: create + promise borrow ok\n");

	/* ---- Watch 取消转发 + Ready 放行 + Promise 写终态。
	 * Wait 由"异步操作线程"调用——阻塞等装配方发布 Ready/Fail；
	 * 单线程演示中先 Ready 再 Wait 立即返回。 ---- */
	if ( !xrtFutureBridgeWatch(&Bridge, exampleCancel,
			(ptr)&bCancelled) ||
		!xrtFutureBridgeReady(&Bridge) ||
		!xrtFutureBridgeWait(&Bridge) ||
		!xrtPromiseResolve(pBorrowed, (ptr)42) ||
		(xrtFutureWaitFor(pFuture, 1000000ull) != XWAIT_OK) ||
		(xrtFutureValue(pFuture) != (ptr)42) ) {
		goto Cleanup;
	}
	xrtFutureBridgeUnwatch(&Bridge);
	printf("bridge: watch + ready -> resolve -> future value ok\n");

	/* ---- Init 挂到自有 Promise + Fail 回收：Wait 为假。 ---- */
	pPromise2 = xrtPromiseCreate(&pFuture2, NULL);
	if ( (pPromise2 == NULL) ||
		(pFuture2 == NULL) ||
		!xrtFutureBridgeInit(&Bridge2, pPromise2) ||
		(xrtFutureBridgePromise(&Bridge2) != pPromise2) ||
		!xrtFutureBridgeFail(&Bridge2) ||
		xrtFutureBridgeWait(&Bridge2) ) {
		goto Cleanup;
	}
	/* Fail 只约束"底层完成回调回收结果"——Promise 归调用方
	 * 所有，终态写入仍然合法（此处用 Resolve 收口 Future）。 */
	if ( !xrtPromiseResolve(pPromise2, (ptr)1) ||
		(xrtFutureWaitFor(pFuture2, 1000000ull) != XWAIT_OK) ) {
		goto Cleanup;
	}
	printf("bridge: init on own promise + fail -> wait false ok\n");
	iResult = 0;

Cleanup:
	xrtFutureBridgeUnwatch(&Bridge2);
	xrtPromiseDestroy(pPromise2);
	xrtFutureDestroy(pFuture2);
	xrtFutureBridgeUnwatch(&Bridge);
	xrtFutureDestroy(pFuture);
	return iResult;
}
