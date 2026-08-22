#include "../test.h"



#define TEST_FUTURE_CONTINUE_OOM_LIMIT 4096u



/* 失败分配器在尺寸类耗尽后拒绝新的 backing span。 */
typedef struct testfuturecontinueoom {
	size_t Calls;
	bool Fail;
	int Freed;
} testfuturecontinueoom;



/* 失败阶段拒绝全部 backing 分配，其余请求转发系统堆。 */
static ptr testFutureContinueOomAlloc(ptr pData, size_t iSize)
{
	testfuturecontinueoom* pState = (testfuturecontinueoom*)pData;

	pState->Calls++;
	return pState->Fail ? NULL : malloc(iSize);
}



/* 延续模块不依赖重分配，仍保持完整分配器接口。 */
static ptr testFutureContinueOomRealloc(
	ptr pData,
	ptr pMemory,
	size_t iSize
)
{
	testfuturecontinueoom* pState = (testfuturecontinueoom*)pData;

	pState->Calls++;
	return pState->Fail ? NULL : realloc(pMemory, iSize);
}



/* 释放成功创建阶段产生的内存块。 */
static void testFutureContinueOomFree(ptr pData, ptr pMemory)
{
	(void)pData;
	free(pMemory);
}



/* Owned 数据释放过程验证失败返回不转移所有权。 */
static void testFutureContinueOomDestroy(ptr pValue, ptr pData)
{
	testfuturecontinueoom* pState = (testfuturecontinueoom*)pValue;

	(void)pData;
	pState->Freed++;
}



/* 成功延续把源值直接写入输出 Promise。 */
static void testFutureContinueOomProc(
	const xfutureresult* pInput,
	xpromise* pOutput,
	ptr pData
)
{
	(void)pData;
	testRequire(xrtPromiseResolve(pOutput, pInput->Value),
		"future continuation OOM callback resolve failed");
}



/* 验证每个构造分配点都完整回滚，且成功受理后才接管数据。 */
int main(void)
{
	testfuturecontinueoom tState = { 0 };
	xallocator tAllocator = {
		&tState,
		testFutureContinueOomAlloc,
		testFutureContinueOomRealloc,
		testFutureContinueOomFree
	};
	xfuture* pSource;
	xfuture* pNext;
	xfuture* arrNext[TEST_FUTURE_CONTINUE_OOM_LIMIT];
	xpromise* pPromise;
	size_t iAccepted = 0;
	int iValue = 19;

	testRequire(xrtSetAllocator(&tAllocator),
		"failed to install future continuation OOM allocator");
	pPromise = xrtPromiseCreate(&pSource, NULL);
	testRequire(pPromise != NULL,
		"future continuation OOM source create failed");
	tState.Fail = true;
	while ( iAccepted < TEST_FUTURE_CONTINUE_OOM_LIMIT ) {
		pNext = xrtFutureThenOwned(
			pSource,
			testFutureContinueOomProc,
			&tState,
			testFutureContinueOomDestroy,
			NULL
		);
		if ( pNext == NULL ) {
			break;
		}
		arrNext[iAccepted++] = pNext;
	}
	testRequire(iAccepted < TEST_FUTURE_CONTINUE_OOM_LIMIT,
		"future continuation did not reach backing OOM");
	testRequire((tState.Freed == 0) &&
		(xrtFutureState(pSource) == XFUTURE_PENDING),
		"future continuation OOM changed ownership or source state");

	tState.Fail = false;
	testRequire(xrtPromiseResolve(pPromise, &iValue),
		"future continuation OOM source resolve failed");
	testRequire(tState.Freed == (int)iAccepted,
		"future continuation OOM accepted data lifetime mismatch");
	for ( size_t i = 0; i < iAccepted; i++ ) {
		testRequire(xrtFutureValue(arrNext[i]) == &iValue,
			"future continuation OOM accepted result mismatch");
		xrtFutureDestroy(arrNext[i]);
	}

	pNext = xrtFutureThenOwned(
		pSource,
		testFutureContinueOomProc,
		&tState,
		testFutureContinueOomDestroy,
		NULL
	);
	testRequire((pNext != NULL) && (xrtFutureValue(pNext) == &iValue) &&
		(tState.Freed == (int)iAccepted + 1),
		"future continuation did not recover after OOM");

	xrtFutureDestroy(pNext);
	xrtPromiseDestroy(pPromise);
	xrtFutureDestroy(pSource);
	printf("[PASS] future continuation OOM\n");
	return 0;
}
