#include "../test.h"



/* 可切换分配器用于验证拥有型树的失败原子性。 */
typedef struct testoomstate {
	bool Fail;
} testoomstate;



/* OOM 测试对象只保存内联数据，排除外部资源干扰。 */
typedef struct testoomitem {
	int Key;
	int Value;
} testoomitem;



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



/* 按整数键比较 OOM 测试对象。 */
static int testOomCompare(const void* pKey, const void* pItem, ptr pUserData)
{
	int iKey = *(const int*)pKey;
	int iItemKey = ((const testoomitem*)pItem)->Key;

	(void)pUserData;
	return (iKey > iItemKey) - (iKey < iItemKey);
}



/* 验证首次分配失败和重复键命中均不破坏树与池计数。 */
int main(void)
{
	testoomstate tState = { false };
	xallocator tAllocator;
	xavltree tTree;
	testoomitem tItem = { 10, 100 };
	testoomitem tDuplicate = { 10, -1 };
	testoomitem* pStored;
	bool bNew;

	tAllocator.Context = &tState;
	tAllocator.Alloc = testOomAlloc;
	tAllocator.Realloc = testOomRealloc;
	tAllocator.Free = testOomFree;
	testRequire(xrtSetAllocator(&tAllocator), "failed to install AVL OOM allocator");
	testRequire(
		xrtAVLTreeInit(&tTree, sizeof(testoomitem), testOomCompare, NULL),
		"OOM AVL init failed"
	);

	tState.Fail = true;
	xrtClearError();
	testRequire(
		xrtAVLTreeAdd(&tTree, &tItem.Key, &tItem, &bNew) == NULL,
		"owned AVL first allocation should fail"
	);
	testRequire(!bNew, "owned AVL failed add reported new item");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY, "owned AVL OOM error mismatch");
	testRequire(
		(xrtAVLTreeCount(&tTree) == 0) &&
		(tTree.Base.Root == NULL) &&
		(tTree.Pool.LiveCount == 0),
		"owned AVL OOM changed visible state"
	);

	tState.Fail = false;
	pStored = (testoomitem*)xrtAVLTreeAdd(&tTree, &tItem.Key, &tItem, &bNew);
	testRequire((pStored != NULL) && bNew, "owned AVL recovery add failed");
	tState.Fail = true;
	xrtClearError();
	testRequire(
		xrtAVLTreeAdd(&tTree, &tDuplicate.Key, &tDuplicate, &bNew) == pStored,
		"owned AVL duplicate should not allocate"
	);
	testRequire(!bNew && (pStored->Value == 100), "owned AVL duplicate changed stored item");
	testRequire(xrtGetError() == NULL, "owned AVL duplicate reported stale failure");

	tState.Fail = false;
	xrtAVLTreeUnit(&tTree);
	printf("[PASS] avl_tree OOM\n");
	return 0;
}
