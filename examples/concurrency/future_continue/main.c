#include <stdio.h>
#include <xrt.h>



/*
 * 范例：concurrency/future_continue —— 延续：then 式结果变换
 * ----------------------------------------------------------------
 * 演示 API：
 *   延续注册（读取源结果 → 输出 Promise 发布新值）
 *   串联后等待最终结果
 * 模块宏：XRT_MODULE_FUTURE
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c $BS}
 *       examples/concurrency/future_continue/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   result: 105
 *
 * then 链的 C 形态：延续回调拿到源 Future 的值，
 *   通过自己的输出 Promise 发布变换结果——
 *   100 → 105 的箭头就是一次异步组合。
 */


/* 延续读取源结果，并通过输出 Promise 发布转换后的值。 */
static void addFive(
	const xfutureresult* pInput,
	xpromise* pOutput,
	ptr pData
)
{
	int* pValue = (int*)pData;

	*pValue = *(int*)pInput->Value + 5;
	(void)xrtPromiseResolve(pOutput, pValue);
}



/* 串联一个短成功延续并读取最终结果。 */
int main(void)
{
	xfuture* pSource;
	xfuture* pNext;
	xpromise* pPromise = xrtPromiseCreate(&pSource, NULL);
	int iSource = 100;
	int iResult = 0;

	if ( pPromise == NULL ) {
		return 1;
	}
	pNext = xrtFutureThen(pSource, addFive, &iResult);
	if ( pNext == NULL ) {
		xrtPromiseDestroy(pPromise);
		xrtFutureDestroy(pSource);
		return 2;
	}
	(void)xrtPromiseResolve(pPromise, &iSource);
	printf("result: %d\n", *(int*)xrtFutureValue(pNext));

	xrtFutureDestroy(pNext);
	xrtPromiseDestroy(pPromise);
	xrtFutureDestroy(pSource);
	return 0;
}
