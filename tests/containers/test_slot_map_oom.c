#include "../test.h"



/* 可切换分配器用于注入槽表增长失败。 */
typedef struct testslotoom {
	bool Fail;
} testslotoom;



/* 在允许状态下转发到底层 C 分配器。 */
static ptr testSlotAlloc(ptr pContext, size_t iSize)
{
	testslotoom* pState = (testslotoom*)pContext;

	return pState->Fail ? NULL : malloc(iSize);
}



/* 在允许状态下转发到底层 C 重分配器。 */
static ptr testSlotRealloc(ptr pContext, ptr pMemory, size_t iSize)
{
	testslotoom* pState = (testslotoom*)pContext;

	return pState->Fail ? NULL : realloc(pMemory, iSize);
}



/* 释放测试分配器取得的内存。 */
static void testSlotFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* 验证增长失败回滚和空闲槽复用不依赖后续分配。 */
int main(void)
{
	testslotoom tState = { false };
	xallocator tAllocator;
	xslotmap tMap;
	xslot pSlots[32];
	int pValues[32];
	bytes pData;
	size_t iCapacity;
	size_t iCount;
	size_t iSpan;
	uint64 iVersion;
	xslot Reused;

	tAllocator.Context = &tState;
	tAllocator.Alloc = testSlotAlloc;
	tAllocator.Realloc = testSlotRealloc;
	tAllocator.Free = testSlotFree;
	testRequire(xrtSetAllocator(&tAllocator), "failed to install slot OOM allocator");
	testRequire(xrtSlotMapInit(&tMap), "slot OOM map init failed");

	for ( size_t i = 0; i < 32; i++ ) {
		pValues[i] = (int)i;
	}
	pSlots[0] = xrtSlotMapInsert(&tMap, &pValues[0]);
	testRequire(pSlots[0] != XRT_SLOT_INVALID, "slot OOM first insert failed");
	iCapacity = tMap.Storage.Capacity;
	testRequire(iCapacity <= 32, "slot OOM test capacity is unexpectedly large");
	for ( size_t i = 1; i < iCapacity; i++ ) {
		pSlots[i] = xrtSlotMapInsert(&tMap, &pValues[i]);
		testRequire(pSlots[i] != XRT_SLOT_INVALID, "slot OOM fill failed");
	}
	testRequire(
		tMap.Storage.Count == tMap.Storage.Capacity,
		"slot OOM map was not filled to capacity"
	);

	pData = tMap.Storage.Data;
	iCount = tMap.Count;
	iSpan = tMap.Storage.Count;
	iVersion = tMap.Version;
	tState.Fail = true;
	xrtClearError();
	testRequire(
		xrtSlotMapInsert(&tMap, &pValues[31]) == XRT_SLOT_INVALID,
		"slot growth should fail under OOM"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY, "slot OOM error kind mismatch");
	testRequire(
		(tMap.Storage.Data == pData) &&
		(tMap.Count == iCount) &&
		(tMap.Storage.Count == iSpan) &&
		(tMap.Storage.Capacity == iCapacity) &&
		(tMap.Version == iVersion),
		"slot OOM changed visible state"
	);

	/* 删除和复用现有空槽都不应触发分配。 */
	testRequire(xrtSlotMapRemove(&tMap, pSlots[1], NULL), "slot OOM remove failed");
	Reused = xrtSlotMapInsert(&tMap, &pValues[31]);
	testRequire(Reused != XRT_SLOT_INVALID, "free slot reuse allocated under OOM");
	testRequire(
		(xrtSlotIndex(Reused) == xrtSlotIndex(pSlots[1])) &&
		(Reused != pSlots[1]),
		"slot OOM reuse generation mismatch"
	);
	testRequire(!xrtSlotMapContains(&tMap, pSlots[1]), "stale OOM slot became valid");
	testRequire(xrtSlotMapGet(&tMap, Reused) == &pValues[31], "reused OOM slot value mismatch");

	xrtSlotMapUnit(&tMap);
	tState.Fail = false;
	printf("[PASS] slot_map OOM\n");
	return 0;
}
