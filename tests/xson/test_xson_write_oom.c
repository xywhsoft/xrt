#include "../test.h"



/* XSON 写出故障注入器记录底层调用、命中点和仍存活的原始块。 */
typedef struct testxsonwriteallocator {
	size_t Calls;
	size_t FailAt;
	size_t Live;
	bool Hit;
} testxsonwriteallocator;



/* 在指定底层分配序号失败，其余请求交给 C 运行库。 */
static ptr testXsonWriteAlloc(ptr pContext, size_t iSize)
{
	testxsonwriteallocator* pState = (testxsonwriteallocator*)pContext;
	ptr pMemory;

	pState->Calls++;
	if ( pState->Calls == pState->FailAt ) {
		pState->Hit = true;
		return NULL;
	}
	pMemory = malloc(iSize);
	if ( pMemory != NULL ) {
		pState->Live++;
	}
	return pMemory;
}



/* 重分配失败时保留原块，只在从空指针创建块时增加存活计数。 */
static ptr testXsonWriteRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	testxsonwriteallocator* pState = (testxsonwriteallocator*)pContext;
	ptr pResult;

	pState->Calls++;
	if ( pState->Calls == pState->FailAt ) {
		pState->Hit = true;
		return NULL;
	}
	pResult = realloc(pMemory, iSize);
	if ( (pResult != NULL) && (pMemory == NULL) ) {
		pState->Live++;
	}
	return pResult;
}



/* 释放底层块并维护存活计数。 */
static void testXsonWriteFree(ptr pContext, ptr pMemory)
{
	testxsonwriteallocator* pState = (testxsonwriteallocator*)pContext;

	if ( pMemory == NULL ) {
		return;
	}
	testRequire(pState->Live != 0, "XSON write OOM live counter underflow");
	pState->Live--;
	free(pMemory);
}



/* 深层增量写入器的 sink 立即消费借用字节且不再分配。 */
static bool testXsonWriteSink(xbytesview Data, ptr pUserData)
{
	(void)Data;
	(void)pUserData;
	return true;
}



/* 同时覆盖内存输出、Base64、结果移交和增量容器帧扩容。 */
static bool testXsonWriteAttempt(const xvalue* pValue)
{
	xxsonwriteconfig Config;
	xxsonwriter* pWriter = NULL;
	str sText = NULL;
	size_t iSize = SIZE_MAX;
	bool bComplete = false;

	sText = xrtXsonStringify(pValue, false, &iSize);
	if ( sText == NULL ) {
		testRequire(iSize == SIZE_MAX, "XSON stringify OOM changed result size");
		goto done;
	}
	xrtFree(sText);
	sText = NULL;

	xrtXsonWriteConfigInit(&Config);
	pWriter = xrtXsonWriterCreateSink(&Config, testXsonWriteSink, NULL);
	if ( pWriter == NULL ) {
		goto done;
	}
	for ( size_t i = 0; i < 256u; i++ ) {
		if ( !xrtXsonWriterSet(pWriter) ) {
			goto done;
		}
	}
	if ( !xrtXsonWriterBytes(pWriter, XRT_BYTES_LITERAL("x")) ) {
		goto done;
	}
	for ( size_t i = 0; i < 256u; i++ ) {
		if ( !xrtXsonWriterEnd(pWriter) ) {
			goto done;
		}
	}
	if ( !xrtXsonWriterFinish(pWriter) ) {
		goto done;
	}
	bComplete = true;

done:
	xrtFree(sText);
	xrtXsonWriterFree(pWriter);
	xrtClearError();
	return bComplete;
}



/* 扫描所有稳定底层分配点，要求失败可恢复且没有原始块泄漏。 */
int main(void)
{
	static testxsonwriteallocator State = { 0, SIZE_MAX, 0, false };
	xallocator Allocator;
	uint8 Data[4097];
	xvalue* pValue;
	size_t iBaseline;
	size_t iCalls;

	Allocator.Context = &State;
	Allocator.Alloc = testXsonWriteAlloc;
	Allocator.Realloc = testXsonWriteRealloc;
	Allocator.Free = testXsonWriteFree;
	testRequire(
		xrtSetAllocator(&Allocator),
		"XSON write OOM allocator install failed"
	);
	for ( size_t i = 0; i < sizeof(Data); i++ ) {
		Data[i] = (uint8)((i * 29u) & 0xFFu);
	}
	pValue = xrtValueBytes((xbytesview){ Data, sizeof(Data) });
	testRequire(pValue != NULL, "XSON write OOM fixture create failed");

	/* 预热尺寸类，再以成功路径定义当前稳定扫描区间。 */
	testRequire(testXsonWriteAttempt(pValue), "XSON write OOM warm-up failed");
	xrtValueRelease(pValue);
	testMemoryDebugDrain("XSON write OOM memory debug reset failed");
	iBaseline = State.Live;

	State.FailAt = SIZE_MAX;
	pValue = xrtValueBytes((xbytesview){ Data, sizeof(Data) });
	testRequire(pValue != NULL, "XSON write OOM baseline fixture create failed");
	State.Calls = 0;
	testRequire(testXsonWriteAttempt(pValue), "XSON write OOM baseline failed");
	iCalls = State.Calls;
	testRequire(iCalls != 0, "XSON write OOM fixture reached no allocation");
	xrtValueRelease(pValue);
	testMemoryDebugDrain("XSON write OOM baseline reset failed");
	testRequire(State.Live == iBaseline, "XSON write baseline leaked storage");

	for ( size_t iFail = 1u; iFail <= iCalls; iFail++ ) {
		State.FailAt = SIZE_MAX;
		pValue = xrtValueBytes((xbytesview){ Data, sizeof(Data) });
		testRequire(pValue != NULL, "XSON write OOM fixture recreate failed");
		State.Calls = 0;
		State.FailAt = iFail;
		State.Hit = false;
		testRequire(
			!testXsonWriteAttempt(pValue),
			"XSON write unexpectedly survived injected OOM"
		);
		testRequire(State.Hit, "XSON write OOM target was not reached");
		xrtValueRelease(pValue);
		testMemoryDebugDrain("XSON write OOM memory debug reset failed");
		testRequire(State.Live == iBaseline, "XSON write OOM leaked storage");
	}

	State.FailAt = SIZE_MAX;
	pValue = xrtValueBytes((xbytesview){ Data, sizeof(Data) });
	testRequire(pValue != NULL, "XSON write recovery fixture create failed");
	testRequire(
		testXsonWriteAttempt(pValue),
		"XSON write did not recover after OOM"
	);
	xrtValueRelease(pValue);
	testMemoryDebugDrain("XSON write OOM recovery reset failed");
	testRequire(State.Live == iBaseline, "XSON write recovery leaked storage");
	xrtClearError();
	printf("[PASS] XSON write OOM\n");
	return 0;
}
