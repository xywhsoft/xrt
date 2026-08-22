#include "../test.h"



/* 可切换分配器用于验证集合插入和合并的失败原子性。 */
typedef struct testsetoomstate {
	bool Fail;
	bool Limited;
	size_t Remaining;
} testsetoomstate;



/* 在允许状态下转发到底层分配器。 */
static ptr testSetOomAlloc(ptr pContext, size_t iSize)
{
	testsetoomstate* pState = (testsetoomstate*)pContext;

	if ( pState->Fail || (pState->Limited && (pState->Remaining == 0)) ) {
		return NULL;
	}
	if ( pState->Limited ) {
		pState->Remaining--;
	}
	return malloc(iSize);
}



/* 在允许状态下转发到底层重分配器。 */
static ptr testSetOomRealloc(ptr pContext, ptr pMemory, size_t iSize)
{
	testsetoomstate* pState = (testsetoomstate*)pContext;

	if ( pState->Fail || (pState->Limited && (pState->Remaining == 0)) ) {
		return NULL;
	}
	if ( pState->Limited ) {
		pState->Remaining--;
	}
	return realloc(pMemory, iSize);
}



/* 释放测试分配器取得的内存。 */
static void testSetOomFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* 验证桶、条目、待提交扩容和合并 OOM 均不改变可见集合。 */
int main(void)
{
	testsetoomstate tState = { false, false, 0 };
	xallocator tAllocator;
	xset tSet;
	xset tSource;
	unsigned char arrItem[2048];
	unsigned char arrStable[2048];
	const void* pStable;
	bool bNew;

	memset(arrItem, 0, sizeof(arrItem));
	tAllocator.Context = &tState;
	tAllocator.Alloc = testSetOomAlloc;
	tAllocator.Realloc = testSetOomRealloc;
	tAllocator.Free = testSetOomFree;
	testRequire(xrtSetAllocator(&tAllocator), "failed to install set OOM allocator");
	testRequire(xrtSetInit(&tSet, sizeof(arrItem)), "OOM set init failed");

	/* 首次插入必须先取得桶数组，失败时集合仍完全为空。 */
	tState.Fail = true;
	xrtClearError();
	testRequire(xrtSetGetOrAdd(&tSet, arrItem, &bNew) == NULL, "set first bucket allocation should fail");
	testRequire(!bNew && (xrtSetCount(&tSet) == 0) && (tSet.Buckets == NULL), "set bucket OOM changed state");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY, "set bucket OOM error mismatch");

	/* 预留桶后强制大条目分配失败。 */
	tState.Fail = false;
	testRequire(xrtSetReserve(&tSet, 12), "set OOM reserve setup failed");
	tState.Fail = true;
	xrtClearError();
	testRequire(xrtSetGetOrAdd(&tSet, arrItem, &bNew) == NULL, "set entry allocation should fail");
	testRequire(!bNew && (xrtSetCount(&tSet) == 0) && (xrtSetCapacity(&tSet) == 12), "set entry OOM changed state");

	/* 填满负载阈值后，仅允许新桶成功，条目失败时不得提交新桶。 */
	tState.Fail = false;
	for ( int i = 0; i < 12; i++ ) {
		memcpy(arrItem, &i, sizeof(i));
		testRequire(xrtSetAdd(&tSet, arrItem), "set OOM threshold fill failed");
	}
	memcpy(arrStable, arrItem, sizeof(arrStable));
	pStable = xrtSetGet(&tSet, arrStable);
	testRequire(pStable != NULL, "set OOM stable item setup failed");
	tState.Limited = true;
	tState.Remaining = 1;
	memset(arrItem, 0xA5, sizeof(arrItem));
	xrtClearError();
	testRequire(xrtSetGetOrAdd(&tSet, arrItem, &bNew) == NULL, "set pending growth entry should fail");
	testRequire(
		!bNew &&
		(xrtSetCount(&tSet) == 12) &&
		(xrtSetCapacity(&tSet) == 12) &&
		!xrtSetHas(&tSet, arrItem),
		"set pending growth failure committed state"
	);

	/* 合并通过事务暂存提交，任意分配失败都必须保留目标和已有条目地址。 */
	tState.Limited = false;
	tState.Fail = false;
	testRequire(xrtSetInit(&tSource, sizeof(arrItem)), "set OOM source init failed");
	memset(arrItem, 0x5A, sizeof(arrItem));
	testRequire(xrtSetAdd(&tSource, arrItem), "set OOM source add failed");
	tState.Fail = true;
	xrtClearError();
	testRequire(!xrtSetMerge(&tSet, &tSource), "set merge should fail under OOM");
	testRequire(
		(xrtSetCount(&tSet) == 12) &&
		!xrtSetHas(&tSet, arrItem) &&
		(xrtSetGet(&tSet, arrStable) == pStable),
		"failed set merge changed target or moved old item"
	);
	testRequire(xrtSetCreate(sizeof(int)) == NULL, "set create should fail under OOM");

	tState.Fail = false;
	xrtSetUnit(&tSource);
	xrtSetUnit(&tSet);
	printf("[PASS] set OOM\n");
	return 0;
}
