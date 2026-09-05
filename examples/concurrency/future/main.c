#include <stdio.h>
#include <xrt.h>



/*
 * 范例：concurrency/future —— Future/Promise：完成与借用值读取
 * ----------------------------------------------------------------
 * 演示 API：
 *   Promise 完成入口（发布值）
 *   Future 等待 + 借用值读取
 * 模块宏：XRT_MODULE_FUTURE
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c $BS}
 *       examples/concurrency/future/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   future value: 42
 *
 * 一对角色：Promise 是"写端"（生产者完成它），
 *   Future 是"读端"（消费者等待它）——值 42 经
 *   发布→等待→借用读取完成一次异步交付。
 */


/* Future/Promise 基础示例直接完成并读取一个借用值。 */
int main(void)
{
	xfuture* pFuture;
	xpromise* pPromise;
	int iValue = 42;

	pPromise = xrtPromiseCreate(&pFuture, NULL);
	if ( (pPromise == NULL) || !xrtPromiseResolve(pPromise, &iValue) ) {
		return 1;
	}
	if ( xrtFutureWait(pFuture) != XWAIT_OK ) {
		return 2;
	}
	printf("future value: %d\n", *(int*)xrtFutureValue(pFuture));
	xrtPromiseDestroy(pPromise);
	xrtFutureDestroy(pFuture);
	return 0;
}
