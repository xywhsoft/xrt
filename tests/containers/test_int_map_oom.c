#include "../test.h"



/* 可切换分配器用于验证整数映射的失败原子性。 */
typedef struct testoomstate {
	bool Fail;
} testoomstate;



/* 在允许状态下转发到底层分配器。 */
static ptr testOomAlloc(ptr pContext, size_t iSize)
{
	testoomstate* pState = (testoomstate*)pContext;

	return pState->Fail ? NULL : malloc(iSize);
}



/* 在允许状态下转发到底层重分配器。 */
static ptr testOomRealloc(ptr pContext, ptr pMemory, size_t iSize)
{
	testoomstate* pState = (testoomstate*)pContext;

	return pState->Fail ? NULL : realloc(pMemory, iSize);
}



/* 释放测试分配器取得的内存。 */
static void testOomFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* 验证新键 OOM 不改变映射，重复键和替换不执行分配。 */
int main(void)
{
	testoomstate tState = { false };
	xallocator tAllocator;
	xintmap tMap;
	int iValue = 10;
	int iReplacement = 20;
	int* pStored;
	bool bNew;

	tAllocator.Context = &tState;
	tAllocator.Alloc = testOomAlloc;
	tAllocator.Realloc = testOomRealloc;
	tAllocator.Free = testOomFree;
	testRequire(xrtSetAllocator(&tAllocator), "failed to install int map OOM allocator");
	testRequire(xrtIntMapInit(&tMap, sizeof(int)), "OOM int map init failed");

	tState.Fail = true;
	xrtClearError();
	testRequire(
		xrtIntMapGetOrAdd(&tMap, 1, &bNew) == NULL,
		"int map first allocation should fail"
	);
	testRequire(!bNew, "failed int map get-or-add reported new value");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY, "int map OOM error mismatch");
	testRequire(
		(xrtIntMapCount(&tMap) == 0) &&
		(tMap.Tree.Base.Root == NULL) &&
		(tMap.Tree.Pool.LiveCount == 0),
		"int map OOM changed visible state"
	);

	tState.Fail = false;
	testRequire(xrtIntMapSet(&tMap, 1, &iValue), "int map OOM recovery set failed");
	pStored = (int*)xrtIntMapGet(&tMap, 1);
	testRequire((pStored != NULL) && (*pStored == 10), "int map OOM recovery value mismatch");

	tState.Fail = true;
	xrtClearError();
	testRequire(
		xrtIntMapGetOrAdd(&tMap, 1, &bNew) == pStored,
		"int map duplicate should not allocate"
	);
	testRequire(!bNew && (*pStored == 10), "int map duplicate changed value");
	testRequire(xrtGetError() == NULL, "int map duplicate reported stale failure");
	testRequire(xrtIntMapSet(&tMap, 1, &iReplacement), "int map replacement should not allocate");
	testRequire(*pStored == 20, "int map replacement under OOM mismatch");

	/* 用完首个池页后，下一个键才必须申请新页。 */
	tState.Fail = false;
	for ( int64 i = 2; i <= (int64)XRT_POOL_PAGE_CAPACITY; i++ ) {
		testRequire(xrtIntMapSet(&tMap, i, &iValue), "int map OOM page fill failed");
	}
	tState.Fail = true;
	xrtClearError();
	testRequire(
		!xrtIntMapSet(&tMap, XRT_POOL_PAGE_CAPACITY + 1, &iValue),
		"int map new page should fail under OOM"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY, "int map set OOM error mismatch");
	testRequire(
		(xrtIntMapCount(&tMap) == XRT_POOL_PAGE_CAPACITY) &&
		!xrtIntMapHas(&tMap, XRT_POOL_PAGE_CAPACITY + 1),
		"int map failed set changed keys"
	);
	testRequire(xrtIntMapCreate(sizeof(int)) == NULL, "int map create should fail under OOM");

	tState.Fail = false;
	xrtIntMapUnit(&tMap);
	printf("[PASS] int_map OOM\n");
	return 0;
}
