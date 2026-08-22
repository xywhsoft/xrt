#include "../test.h"
#include <xrt/future_bridge.h>



/* 测试分配器可以在指定底层调用之后持续返回内存不足。 */
typedef struct testfuturebridgeoom {
	size_t Count;
	size_t AllowThrough;
} testfuturebridgeoom;



/* 在允许范围内分配底层存储，超过故障边界后持续失败。 */
static ptr testFutureBridgeOomAlloc(ptr pContext, size_t iSize)
{
	testfuturebridgeoom* pState = (testfuturebridgeoom*)pContext;

	pState->Count++;
	return pState->Count <= pState->AllowThrough ?
		malloc(iSize) : NULL;
}



/* 测试分配器保持标准重分配语义。 */
static ptr testFutureBridgeOomRealloc(ptr pContext, ptr pMemory, size_t iSize)
{
	(void)pContext;
	return realloc(pMemory, iSize);
}



/* 释放测试分配器实际拥有的底层 span。 */
static void testFutureBridgeOomFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* 填充监听只占用目标尺寸类，不参与桥接结果判断。 */
static void testFutureBridgeOomFillCallback(ptr pData)
{
	(void)pData;
}



/* 失败安装的桥接回调在后续取消时绝不能执行。 */
static void testFutureBridgeOomCallback(ptr pData)
{
	int* pCount = (int*)pData;

	(*pCount)++;
}



/* 验证监听安装 OOM 不留下回调，并且装配失败状态可立即观察。 */
int main(void)
{
	static testfuturebridgeoom tState = { 0, SIZE_MAX };
	xallocator tAllocator = {
		&tState,
		testFutureBridgeOomAlloc,
		testFutureBridgeOomRealloc,
		testFutureBridgeOomFree
	};
	xfuturebridge tBridge;
	xcancelwatch* aFill[256];
	xcancel* pCancel;
	xfuture* pFuture;
	size_t iFill = 0;
	int iCallbackCount = 0;

	testRequire(xrtSetAllocator(&tAllocator), "failed to install future bridge OOM allocator");
	pFuture = xrtFutureBridgeCreate(&tBridge, NULL);
	testRequire(pFuture != NULL, "future bridge create failed");
	testRequire(xrtFutureBridgePromise(&tBridge) != NULL,
		"future bridge promise is null");
	pCancel = xrtPromiseCancelToken(xrtFutureBridgePromise(&tBridge));
	testRequire(pCancel != NULL, "future bridge cancel token failed");

	/*
		禁止建立新 span，并持有已有尺寸类中的全部监听块。
		这样桥接安装必然命中监听对象分配，而不依赖平台对象大小。
	*/
	tState.AllowThrough = tState.Count;
	while ( iFill < (sizeof(aFill) / sizeof(aFill[0])) ) {
		aFill[iFill] = xrtCancelWatch(
			pCancel,
			testFutureBridgeOomFillCallback,
			NULL
		);
		if ( aFill[iFill] == NULL ) {
			break;
		}
		iFill++;
	}
	testRequire(iFill < (sizeof(aFill) / sizeof(aFill[0])),
		"future bridge OOM filler did not exhaust watch storage");

	xrtClearError();
	testRequire(
		!xrtFutureBridgeWatch(
			&tBridge,
			testFutureBridgeOomCallback,
			&iCallbackCount
		),
		"future bridge watch succeeded under OOM"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"future bridge watch OOM error mismatch");
	testRequire(xrtFutureBridgeFail(&tBridge),
		"future bridge failure publish failed");
	testRequire(!xrtFutureBridgeWait(&tBridge),
		"failed future bridge remained publishable");

	/* 解除故障和填充监听后，取消 Future 不得命中失败安装的桥接回调。 */
	tState.AllowThrough = SIZE_MAX;
	while ( iFill != 0 ) {
		iFill--;
		xrtCancelUnwatch(aFill[iFill]);
	}
	xrtCancelDestroy(pCancel);
	testRequire(xrtFutureCancel(pFuture), "future bridge cancellation failed");
	testRequire(iCallbackCount == 0, "failed future bridge left a callback behind");

	xrtPromiseDestroy(xrtFutureBridgePromise(&tBridge));
	xrtFutureDestroy(pFuture);

	printf("[PASS] future bridge OOM\n");
	return 0;
}
