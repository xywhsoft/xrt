#include "../test.h"



/* XLON 写出故障注入器记录底层调用、命中点和仍存活的原始块。 */
typedef struct testxlonwriteallocator {
	size_t Calls;
	size_t FailAt;
	size_t Live;
	bool Hit;
} testxlonwriteallocator;



/* 在指定底层分配序号失败，其余请求交给 C 运行库。 */
static ptr testXlonWriteAlloc(ptr pContext, size_t iSize)
{
	testxlonwriteallocator* pState = (testxlonwriteallocator*)pContext;
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
static ptr testXlonWriteRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	testxlonwriteallocator* pState = (testxlonwriteallocator*)pContext;
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
static void testXlonWriteFree(ptr pContext, ptr pMemory)
{
	testxlonwriteallocator* pState = (testxlonwriteallocator*)pContext;

	if ( pMemory == NULL ) {
		return;
	}
	testRequire(pState->Live != 0, "XLON write OOM live counter underflow");
	pState->Live--;
	free(pMemory);
}



/* 深层增量写入器的 sink 立即消费借用字节且不再分配。 */
static bool testXlonWriteSink(xbytesview Data, ptr pUserData)
{
	(void)Data;
	(void)pUserData;
	return true;
}



/* 同时覆盖内存输出、Base64、结果移交和增量容器帧扩容。 */
static bool testXlonWriteAttempt(const xvalue* pValue)
{
	xxlonwriteconfig Config;
	xxlonwriter* pWriter = NULL;
	str sText = NULL;
	size_t iSize = SIZE_MAX;
	bool bComplete = false;

	sText = xrtXlonStringify(pValue, false, &iSize);
	if ( sText == NULL ) {
		testRequire(iSize == SIZE_MAX, "XLON stringify OOM changed result size");
		goto done;
	}
	xrtFree(sText);
	sText = NULL;

	xrtXlonWriteConfigInit(&Config);
	pWriter = xrtXlonWriterCreateSink(&Config, testXlonWriteSink, NULL);
	if ( pWriter == NULL ) {
		goto done;
	}
	for ( size_t i = 0; i < 256u; i++ ) {
		if ( !xrtXlonWriterSet(pWriter) ) {
			goto done;
		}
	}
	if ( !xrtXlonWriterBytes(pWriter, XRT_BYTES_LITERAL("x")) ) {
		goto done;
	}
	for ( size_t i = 0; i < 256u; i++ ) {
		if ( !xrtXlonWriterEnd(pWriter) ) {
			goto done;
		}
	}
	if ( !xrtXlonWriterFinish(pWriter) ) {
		goto done;
	}
	bComplete = true;

done:
	xrtFree(sText);
	xrtXlonWriterFree(pWriter);
	xrtClearError();
	return bComplete;
}



/* 扫描所有稳定底层分配点，要求失败可恢复且没有原始块泄漏。 */
int main(void)
{
	static testxlonwriteallocator State = { 0, SIZE_MAX, 0, false };
	xallocator Allocator;
	uint8 Data[4097];
	xvalue* pValue;
	size_t iBaseline;
	size_t iCalls;

	Allocator.Context = &State;
	Allocator.Alloc = testXlonWriteAlloc;
	Allocator.Realloc = testXlonWriteRealloc;
	Allocator.Free = testXlonWriteFree;
	testRequire(
		xrtSetAllocator(&Allocator),
		"XLON write OOM allocator install failed"
	);
	for ( size_t i = 0; i < sizeof(Data); i++ ) {
		Data[i] = (uint8)((i * 29u) & 0xFFu);
	}
	pValue = xrtValueBytes((xbytesview){ Data, sizeof(Data) });
	testRequire(pValue != NULL, "XLON write OOM fixture create failed");

	/* 预热尺寸类，再以成功路径定义当前稳定扫描区间。 */
	testRequire(testXlonWriteAttempt(pValue), "XLON write OOM warm-up failed");
	xrtValueRelease(pValue);
	testMemoryDebugDrain("XLON write OOM memory debug reset failed");
	iBaseline = State.Live;

	State.FailAt = SIZE_MAX;
	pValue = xrtValueBytes((xbytesview){ Data, sizeof(Data) });
	testRequire(pValue != NULL, "XLON write OOM baseline fixture create failed");
	State.Calls = 0;
	testRequire(testXlonWriteAttempt(pValue), "XLON write OOM baseline failed");
	iCalls = State.Calls;
	testRequire(iCalls != 0, "XLON write OOM fixture reached no allocation");
	xrtValueRelease(pValue);
	testMemoryDebugDrain("XLON write OOM baseline reset failed");
	testRequire(State.Live == iBaseline, "XLON write baseline leaked storage");

	for ( size_t iFail = 1u; iFail <= iCalls; iFail++ ) {
		State.FailAt = SIZE_MAX;
		pValue = xrtValueBytes((xbytesview){ Data, sizeof(Data) });
		testRequire(pValue != NULL, "XLON write OOM fixture recreate failed");
		State.Calls = 0;
		State.FailAt = iFail;
		State.Hit = false;
		testRequire(
			!testXlonWriteAttempt(pValue),
			"XLON write unexpectedly survived injected OOM"
		);
		testRequire(State.Hit, "XLON write OOM target was not reached");
		xrtValueRelease(pValue);
		testMemoryDebugDrain("XLON write OOM memory debug reset failed");
		testRequire(State.Live == iBaseline, "XLON write OOM leaked storage");
	}

	State.FailAt = SIZE_MAX;
	pValue = xrtValueBytes((xbytesview){ Data, sizeof(Data) });
	testRequire(pValue != NULL, "XLON write recovery fixture create failed");
	testRequire(
		testXlonWriteAttempt(pValue),
		"XLON write did not recover after OOM"
	);
	xrtValueRelease(pValue);
	testMemoryDebugDrain("XLON write OOM recovery reset failed");
	testRequire(State.Live == iBaseline, "XLON write recovery leaked storage");
	xrtClearError();
	printf("[PASS] XLON write OOM\n");
	return 0;
}
