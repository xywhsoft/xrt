#include "../test.h"



/* 可按分配序号失败并统计存活块的图层测试分配器。 */
typedef struct testvaluegraphoom {
	size_t Calls;
	size_t FailAt;
	size_t Live;
	bool Tracking;
} testvaluegraphoom;



/* 在指定调用处失败，其余分配交给系统堆。 */
static ptr testValueGraphOomAlloc(ptr pContext, size_t iSize)
{
	testvaluegraphoom* pState = (testvaluegraphoom*)pContext;
	ptr pMemory;

	pState->Calls++;
	if ( (pState->FailAt != 0) && (pState->Calls == pState->FailAt) ) {
		return NULL;
	}
	pMemory = malloc(iSize);
	if ( (pMemory != NULL) && pState->Tracking ) {
		pState->Live++;
	}
	return pMemory;
}



/* 在指定调用处保留旧块并失败，其余重分配交给系统堆。 */
static ptr testValueGraphOomRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	testvaluegraphoom* pState = (testvaluegraphoom*)pContext;
	ptr pResult;

	pState->Calls++;
	if ( (pState->FailAt != 0) && (pState->Calls == pState->FailAt) ) {
		return NULL;
	}
	pResult = realloc(pMemory, iSize);
	if ( (pResult != NULL) && (pMemory == NULL) && pState->Tracking ) {
		pState->Live++;
	}
	return pResult;
}



/* 释放测试块并维护存活计数。 */
static void testValueGraphOomFree(ptr pContext, ptr pMemory)
{
	testvaluegraphoom* pState = (testvaluegraphoom*)pContext;

	if ( pMemory == NULL ) {
		return;
	}
	if ( pState->Tracking ) {
		testRequire(
			pState->Live != 0,
			"graph OOM live counter underflow"
		);
		pState->Live--;
	}
	free(pMemory);
}



/* 读取可区分逻辑活动块与全局堆缓存的存活快照。 */
static void testValueGraphOomLive(
	const testvaluegraphoom* pState,
	size_t* pCount,
	size_t* pBytes
)
{
	#if defined(XRT_FEATURE_MEMORY_DEBUG)
		xmemdebugsnapshot tSnapshot;

		(void)pState;
		xrtMemDebugSnapshot(&tSnapshot);
		*pCount = tSnapshot.LiveCount;
		*pBytes = tSnapshot.LiveBytes;
	#else
		*pCount = pState->Live;
		*pBytes = 0;
	#endif
}



/* 创建包含重复子对象、四种容器和大量独立节点的无环图。 */
static xvalue* testValueGraphOomFixture(void)
{
	xvalue* pRoot = xrtValueArray();
	xvalue* pObject = xrtValueObject();
	xvalue* pMap = xrtValueIntMap();
	xvalue* pSet = xrtValueSet();

	if ( (pRoot == NULL) || (pObject == NULL) ||
		 (pMap == NULL) || (pSet == NULL) ) {
		goto fail;
	}
	if ( !xrtValueObjectSetNew(
		pObject,
		XRT_STR_LITERAL("name"),
		xrtValueString(XRT_STR_LITERAL("child"))
	) ) {
		goto fail;
	}
	if ( !xrtValueIntMapSetNew(pMap, -7, xrtValueInt(7)) ||
		 !xrtValueSetAddNew(pSet, xrtValueFloat(9.0)) ) {
		goto fail;
	}
	if ( !xrtValueArrayAppend(pRoot, pObject) ||
		 !xrtValueArrayAppend(pRoot, pObject) ||
		 !xrtValueArrayAppendTake(pRoot, &pMap) ||
		 !xrtValueArrayAppendTake(pRoot, &pSet) ) {
		goto fail;
	}
	for ( int64 i = 0; i < 128; i++ ) {
		xvalue* pChild = xrtValueObject();

		if ( (pChild == NULL) || !xrtValueObjectSetNew(
			pChild,
			XRT_STR_LITERAL("index"),
			xrtValueInt(i)
		) || !xrtValueArrayAppendTake(pRoot, &pChild) ) {
			xrtValueRelease(pChild);
			goto fail;
		}
	}
	xrtValueRelease(pObject);
	return pRoot;

fail:
	xrtValueRelease(pSet);
	xrtValueRelease(pMap);
	xrtValueRelease(pObject);
	xrtValueRelease(pRoot);
	return NULL;
}



/* 穷举深拷贝中的每个前置分配失败点并验证无泄漏、无部分状态。 */
int main(void)
{
	static testvaluegraphoom tState = { 0, 0, 0, true };
	xallocator tAllocator = {
		&tState,
		testValueGraphOomAlloc,
		testValueGraphOomRealloc,
		testValueGraphOomFree
	};
	xvalue* pSource;
	xvalue* pScalar;
	xvalue* pScalarCopy;
	xvalue* pWarm;
	size_t iInitialCount;
	size_t iInitialBytes;
	size_t iBaselineCount;
	size_t iBaselineBytes;
	size_t iFailures = 0;
	size_t iSuccesses = 0;

	testRequire(
		xrtSetAllocator(&tAllocator),
		"failed to install graph OOM allocator"
	);
	testValueGraphOomLive(&tState, &iInitialCount, &iInitialBytes);
	pSource = testValueGraphOomFixture();
	pScalar = xrtValueBool(true);
	testRequire(
		(pSource != NULL) && (pScalar != NULL),
		"graph OOM fixture failed"
	);
	tState.Calls = 0;
	tState.FailAt = 1;
	pScalarCopy = xrtValueDeepClone(pScalar);
	testRequire(
		(pScalarCopy == pScalar) && (tState.Calls == 0),
		"scalar deep clone allocated"
	);
	xrtValueRelease(pScalarCopy);
	tState.FailAt = 0;
	pWarm = xrtValueDeepClone(pSource);
	testRequire(
		(pWarm != NULL) && xrtValueEqual(pSource, pWarm),
		"graph OOM warm-up failed"
	);
	xrtValueRelease(pWarm);
	testValueGraphOomLive(
		&tState,
		&iBaselineCount,
		&iBaselineBytes
	);
	for ( size_t iFailAt = 1; iFailAt <= 96; iFailAt++ ) {
		xvalue* pCopy;

		tState.Calls = 0;
		tState.FailAt = iFailAt;
		xrtClearError();
		pCopy = xrtValueDeepClone(pSource);
		if ( pCopy == NULL ) {
			iFailures++;
			testRequire(
				xrtErrorKind(xrtGetError()) == XERR_MEMORY,
				"graph allocation failure error mismatch"
			);
		} else {
			/* 克隆故障域结束后，验证阶段不得继续消费同一故障点。 */
			tState.FailAt = 0;
			iSuccesses++;
			testRequire(
				xrtValueEqual(pSource, pCopy),
				"graph OOM successful clone mismatch"
			);
			testRequire(
				xrtValueArrayGet(pCopy, 0) == xrtValueArrayGet(pCopy, 1),
				"graph OOM clone lost shared identity"
			);
			xrtValueRelease(pCopy);
		}
		{
			size_t iCurrentCount;
			size_t iCurrentBytes;

			testValueGraphOomLive(
				&tState,
				&iCurrentCount,
				&iCurrentBytes
			);
			testRequire(
				(iCurrentCount == iBaselineCount) &&
				(iCurrentBytes == iBaselineBytes),
				"graph OOM attempt leaked partial state"
			);
		}
		if ( (iFailures != 0) && (iSuccesses >= 8) ) {
			break;
		}
	}
	testRequire(
		(iFailures != 0) && (iSuccesses >= 8),
		"graph OOM sweep did not cover failure and success"
	);
	tState.FailAt = 0;
	xrtValueRelease(pScalar);
	xrtValueRelease(pSource);
	xrtClearError();
	#if defined(XRT_FEATURE_MEMORY_DEBUG)
		{
			size_t iFinalCount;
			size_t iFinalBytes;

			testValueGraphOomLive(
				&tState,
				&iFinalCount,
				&iFinalBytes
			);
			testRequire(
				(iFinalCount == iInitialCount) &&
				(iFinalBytes == iInitialBytes),
				"graph OOM fixture retained logical allocations"
			);
		}
	#else
		testRequire(
			tState.Live <= iBaselineCount,
			"graph OOM fixture retained direct allocations"
		);
	#endif
	tState.Tracking = false;
	printf("[PASS] value graph OOM\n");
	return 0;
}
