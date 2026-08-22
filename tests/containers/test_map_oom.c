#include "../test.h"



/* 可切换分配器用于验证映射扩容和条目分配的失败原子性。 */
typedef struct testmapoomstate {
	bool Fail;
	bool Limited;
	size_t Remaining;
} testmapoomstate;



/* 在允许状态下转发到底层分配器。 */
static ptr testMapOomAlloc(ptr pContext, size_t iSize)
{
	testmapoomstate* pState = (testmapoomstate*)pContext;

	if ( pState->Fail || (pState->Limited && (pState->Remaining == 0)) ) {
		return NULL;
	}
	if ( pState->Limited ) {
		pState->Remaining--;
	}
	return malloc(iSize);
}



/* 在允许状态下转发到底层重分配器。 */
static ptr testMapOomRealloc(ptr pContext, ptr pMemory, size_t iSize)
{
	testmapoomstate* pState = (testmapoomstate*)pContext;

	if ( pState->Fail || (pState->Limited && (pState->Remaining == 0)) ) {
		return NULL;
	}
	if ( pState->Limited ) {
		pState->Remaining--;
	}
	return realloc(pMemory, iSize);
}



/* 释放测试分配器取得的内存。 */
static void testMapOomFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* 验证桶和条目 OOM 均不改变键集合，重复键与替换不分配。 */
int main(void)
{
	static unsigned char arrLargeKey[2048];
	testmapoomstate tState = { false, false, 0 };
	xallocator tAllocator;
	xmap tMap;
	int iValue = 10;
	int iReplacement = 20;
	int* pStored;
	bool bNew;

	memset(arrLargeKey, 'K', sizeof(arrLargeKey));
	tAllocator.Context = &tState;
	tAllocator.Alloc = testMapOomAlloc;
	tAllocator.Realloc = testMapOomRealloc;
	tAllocator.Free = testMapOomFree;
	testRequire(xrtSetAllocator(&tAllocator), "failed to install map OOM allocator");
	testRequire(xrtMapInit(&tMap, sizeof(int)), "OOM map init failed");

	/* 空映射的首次插入必须先取得桶数组。 */
	tState.Fail = true;
	xrtClearError();
	testRequire(
		xrtMapGetOrAdd(&tMap, XRT_BYTES_LITERAL("first"), &bNew) == NULL,
		"map first bucket allocation should fail"
	);
	testRequire(!bNew, "failed map get-or-add reported new value");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY, "map bucket OOM error mismatch");
	testRequire(
		(xrtMapCount(&tMap) == 0) &&
		(tMap.Buckets == NULL) &&
		(tMap.BucketCount == 0),
		"map bucket OOM changed visible state"
	);

	/* 预留桶后使用大键强制条目走独立底层分配。 */
	tState.Fail = false;
	testRequire(xrtMapReserve(&tMap, 12), "map OOM reserve setup failed");
	tState.Fail = true;
	xrtClearError();
	testRequire(
		xrtMapGetOrAdd(
			&tMap,
			(xbytesview){ arrLargeKey, sizeof(arrLargeKey) },
			&bNew
		) == NULL,
		"map entry allocation should fail"
	);
	testRequire(!bNew, "failed large-key map insert reported new value");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY, "map entry OOM error mismatch");
	testRequire((xrtMapCount(&tMap) == 0) && (xrtMapCapacity(&tMap) == 12), "map entry OOM changed state");

	tState.Fail = false;
	testRequire(xrtMapSet(&tMap, XRT_BYTES_LITERAL("key-0"), &iValue), "map OOM recovery set failed");
	pStored = (int*)xrtMapGet(&tMap, XRT_BYTES_LITERAL("key-0"));
	testRequire((pStored != NULL) && (*pStored == 10), "map OOM recovery value mismatch");

	/* 已有键查询和替换不依赖任何新分配。 */
	tState.Fail = true;
	xrtClearError();
	testRequire(
		xrtMapGetOrAdd(&tMap, XRT_BYTES_LITERAL("key-0"), &bNew) == pStored,
		"map duplicate should not allocate"
	);
	testRequire(!bNew && (*pStored == 10), "map duplicate changed value");
	testRequire(xrtGetError() == NULL, "map duplicate reported stale failure");
	testRequire(
		xrtMapSet(&tMap, XRT_BYTES_LITERAL("key-0"), &iReplacement),
		"map replacement should not allocate"
	);
	testRequire(*pStored == 20, "map replacement under OOM mismatch");

	/* 填满 16 桶的负载阈值后，下一键必须失败在新桶提交之前。 */
	tState.Fail = false;
	for ( int i = 1; i < 12; i++ ) {
		char arrKey[16];
		int iLength = snprintf(arrKey, sizeof(arrKey), "key-%d", i);

		testRequire(
			xrtMapSet(&tMap, (xbytesview){ (cbytes)arrKey, (size_t)iLength }, &iValue),
			"map OOM threshold fill failed"
		);
	}
	testRequire((xrtMapCount(&tMap) == 12) && (xrtMapCapacity(&tMap) == 12), "map threshold setup mismatch");
	tState.Fail = true;
	xrtClearError();
	testRequire(
		!xrtMapSet(&tMap, XRT_BYTES_LITERAL("overflow"), &iValue),
		"map growth bucket allocation should fail"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY, "map growth OOM error mismatch");
	testRequire(
		(xrtMapCount(&tMap) == 12) &&
		(xrtMapCapacity(&tMap) == 12) &&
		!xrtMapHas(&tMap, XRT_BYTES_LITERAL("overflow")),
		"map failed growth changed keys or capacity"
	);
	testRequire(xrtMapCreate(sizeof(int)) == NULL, "map create should fail under OOM");

	/* 放行新桶的底层分配，再让大键条目失败，验证待提交桶被回滚。 */
	tState.Fail = false;
	tState.Limited = true;
	tState.Remaining = 1;
	xrtClearError();
	testRequire(
		xrtMapGetOrAdd(
			&tMap,
			(xbytesview){ arrLargeKey, sizeof(arrLargeKey) },
			&bNew
		) == NULL,
		"map pending growth entry allocation should fail"
	);
	testRequire(!bNew, "failed pending-growth insert reported new value");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY, "map pending growth OOM error mismatch");
	testRequire(
		(xrtMapCount(&tMap) == 12) &&
		(xrtMapCapacity(&tMap) == 12) &&
		!xrtMapHas(&tMap, (xbytesview){ arrLargeKey, sizeof(arrLargeKey) }),
		"map pending growth failure committed buckets or key"
	);

	tState.Limited = false;
	xrtMapUnit(&tMap);
	printf("[PASS] map OOM\n");
	return 0;
}
