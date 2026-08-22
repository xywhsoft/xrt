#include "../test.h"



#define TEST_MEMORY_DEBUG_BATCH_COUNT	65536u



static ptr __testMemoryDebugBatch[TEST_MEMORY_DEBUG_BATCH_COUNT];



/* 统计访问器看到的事件数量并验证顺序。 */
typedef struct test_event_state {
	uint64 LastSequence;
	size_t Count;
} test_event_state;



/* 验证事件序号严格递增。 */
static bool testVisitEvent(const xmemdebugevent* pEvent, ptr pUserData)
{
	test_event_state* pState = (test_event_state*)pUserData;

	testRequire(pEvent->Sequence > pState->LastSequence, "event sequence must increase");
	pState->LastSequence = pEvent->Sequence;
	pState->Count++;
	return true;
}



/* 验证活动分配访问器可以找到指定地址。 */
static bool testVisitAllocation(const xmemdebugallocation* pAllocation, ptr pUserData)
{
	ptr* ppExpected = (ptr*)pUserData;

	if ( pAllocation->Address == *ppExpected ) {
		*ppExpected = NULL;
	}
	return true;
}



/* 统计活动分配快照中的全部记录。 */
static bool testCountAllocation(const xmemdebugallocation* pAllocation, ptr pUserData)
{
	size_t* pCount = (size_t*)pUserData;

	testRequire(pAllocation->Address != NULL, "batch visitor received null allocation");
	(*pCount)++;
	return true;
}



/* 覆盖旧版已经压实的越界、重复释放、释放后写入和泄漏边界。 */
int main(void)
{
	xmemdebugsnapshot tSnapshot;
	test_event_state tEvents;
	unsigned char* pOverflow;
	unsigned char* pUnderflow;
	unsigned char* pDouble;
	unsigned char* pUseAfterFree;
	unsigned char* pReuse;
	unsigned char* pLarge;
	unsigned char* pLive;
	unsigned char* pForeign;
	ptr pExpected;
	size_t iBatchVisited;

	testRequire(xrtMemDebugEnabled(), "memory debug should start enabled");
	testRequire(xrtMemDebugReset(), "initial memory debug reset failed");

	/* 大批量释放必须保持近似线性，不能按活动分配总数逐块扫描。 */
	for ( size_t i = 0; i < TEST_MEMORY_DEBUG_BATCH_COUNT; i++ ) {
		__testMemoryDebugBatch[i] = xrtMalloc(24);
		testRequire(
			__testMemoryDebugBatch[i] != NULL,
			"batch allocation failed"
		);
	}
	xrtMemDebugSnapshot(&tSnapshot);
	testRequire(
		tSnapshot.LiveCount == TEST_MEMORY_DEBUG_BATCH_COUNT,
		"batch live count mismatch"
	);
	iBatchVisited = 0;
	testRequire(
		xrtMemDebugVisitLive(testCountAllocation, &iBatchVisited) ==
			TEST_MEMORY_DEBUG_BATCH_COUNT,
		"batch live visitor count mismatch"
	);
	testRequire(
		iBatchVisited == TEST_MEMORY_DEBUG_BATCH_COUNT,
		"batch live visitor state mismatch"
	);
	for ( size_t i = 0; i < TEST_MEMORY_DEBUG_BATCH_COUNT; i++ ) {
		xrtFree(__testMemoryDebugBatch[i]);
		__testMemoryDebugBatch[i] = NULL;
	}
	xrtMemDebugSnapshot(&tSnapshot);
	testRequire(tSnapshot.LiveCount == 0, "batch release retained allocations");
	testRequire(xrtMemDebugReset(), "batch memory debug reset failed");

	/* 逻辑分配故障不能被小对象池缓存绕过，并且只触发一次。 */
	testRequire(
		xrtMemDebugFailAfter(0),
		"immediate allocation fault setup failed"
	);
	pLive = (unsigned char*)xrtMalloc(16);
	testRequire(
		(pLive == NULL) &&
		xrtMemDebugFailTriggered() &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"immediate logical allocation fault mismatch"
	);
	xrtClearError();
	pLive = (unsigned char*)xrtMalloc(16);
	testRequire(
		pLive != NULL,
		"one-shot allocation fault remained armed"
	);
	xrtFree(pLive);
	testRequire(
		xrtMemDebugFailAfter(1),
		"delayed allocation fault setup failed"
	);
	pLive = (unsigned char*)xrtMalloc(16);
	testRequire(
		pLive != NULL,
		"delayed allocation fault rejected the allowed allocation"
	);
	pReuse = (unsigned char*)xrtMalloc(16);
	testRequire(
		(pReuse == NULL) && xrtMemDebugFailTriggered(),
		"delayed logical allocation fault mismatch"
	);
	xrtClearError();
	xrtFree(pLive);
	xrtMemDebugFailClear();

	pLive = (unsigned char*)xrtMalloc(48);
	testRequire(pLive != NULL, "live allocation failed");
	xrtMemDebugSnapshot(&tSnapshot);
	testRequire((tSnapshot.LiveCount == 1) && (tSnapshot.LiveBytes == 48), "live snapshot mismatch");
	pExpected = pLive;
	testRequire(xrtMemDebugVisitLive(testVisitAllocation, &pExpected) == 1, "live visitor count mismatch");
	testRequire(pExpected == NULL, "live visitor did not expose allocation");
	testRequire(!xrtMemDebugEnable(false), "debug mode must not change with a live allocation");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE, "live mode change must report state error");
	xrtClearError();
	xrtFree(pLive);

	pForeign = (unsigned char*)malloc(32);
	testRequire(pForeign != NULL, "foreign allocation failed");
	xrtFree(pForeign);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "foreign free must report argument error");
	xrtClearError();
	testRequire(
		xrtRealloc(pForeign, 64) == NULL,
		"foreign realloc should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"foreign realloc must report argument error"
	);
	xrtClearError();
	free(pForeign);

	pOverflow = (unsigned char*)xrtMalloc(16);
	testRequire(pOverflow != NULL, "overflow allocation failed");
	pOverflow[16] = 0x7A;
	xrtFree(pOverflow);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE, "overflow free must report state error");
	xrtClearError();

	pUnderflow = (unsigned char*)xrtMalloc(16);
	testRequire(pUnderflow != NULL, "underflow allocation failed");
	pUnderflow[-1] ^= 0x7A;
	xrtFree(pUnderflow);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE, "underflow free must report state error");
	xrtClearError();

	pDouble = (unsigned char*)xrtMalloc(24);
	testRequire(pDouble != NULL, "double-free allocation failed");
	xrtFree(pDouble);
	xrtFree(pDouble);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE, "double free must report state error");
	xrtClearError();

	pUseAfterFree = (unsigned char*)xrtMalloc(32);
	testRequire(pUseAfterFree != NULL, "use-after-free allocation failed");
	xrtFree(pUseAfterFree);
	pUseAfterFree[sizeof(ptr) + 1] = 0x11;
	pReuse = (unsigned char*)xrtMalloc(32);
	testRequire(pReuse == pUseAfterFree, "pool should reuse the most recently released block");
	xrtFree(pReuse);

	pLarge = (unsigned char*)xrtMalloc(2048);
	testRequire(pLarge != NULL, "quarantine allocation failed");
	xrtFree(pLarge);
	xrtFree(pLarge);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE, "quarantine double free must report state error");
	xrtClearError();

	xrtMemDebugSnapshot(&tSnapshot);
	testRequire(tSnapshot.LiveCount == 0, "all tracked allocations should be released");
	testRequire(tSnapshot.OverflowCount >= 1, "overflow was not detected");
	testRequire(tSnapshot.UnderflowCount >= 1, "underflow was not detected");
	testRequire(tSnapshot.DoubleFreeCount >= 2, "double frees were not detected");
	testRequire(tSnapshot.InvalidFreeCount >= 1, "invalid free was not detected");
	testRequire(tSnapshot.UseAfterFreeCount >= 1, "use-after-free was not detected");
	testRequire(tSnapshot.QuarantineCount >= 1, "large release was not quarantined");

	memset(&tEvents, 0, sizeof(tEvents));
	testRequire(xrtMemDebugVisit(testVisitEvent, &tEvents) == tSnapshot.EventCount, "event visitor count mismatch");
	testRequire(tEvents.Count == tSnapshot.EventCount, "event visitor state mismatch");
	testRequire(xrtMemDebugReset(), "final memory debug reset failed");
	xrtMemDebugSnapshot(&tSnapshot);
	testRequire((tSnapshot.EventCount == 0) && (tSnapshot.QuarantineCount == 0), "reset did not clear debug state");

	testRequire(xrtMemDebugEnable(false), "disabling memory debug failed");
	pLive = (unsigned char*)xrtMalloc(32);
	testRequire(pLive != NULL, "disabled allocation failed");
	xrtFree(pLive);
	xrtMemDebugSnapshot(&tSnapshot);
	testRequire((tSnapshot.AllocCount == 0) && (tSnapshot.EventCount == 0), "disabled debug mode recorded events");
	testRequire(xrtMemDebugEnable(true), "enabling memory debug failed");

	for ( size_t i = 0; i < 300; i++ ) {
		pLive = (unsigned char*)xrtMalloc(16);
		testRequire(pLive != NULL, "event capacity allocation failed");
		xrtFree(pLive);
	}
	xrtMemDebugSnapshot(&tSnapshot);
	testRequire(tSnapshot.EventCount == 512, "event history must remain bounded");
	testRequire(xrtMemDebugReset(), "capacity test reset failed");
	printf("[PASS] memory_debug\n");
	return 0;
}
