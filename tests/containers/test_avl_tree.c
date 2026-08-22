#include "../test.h"



/* 拥有型树对象包含由释放回调接管的独立字符串。 */
typedef struct testtreeitem {
	int Key;
	int Value;
	str Name;
} testtreeitem;



/* 过对齐对象用于验证池槽和对象偏移共同满足对齐。 */
typedef struct testalignedtreeitem {
	int Key;
	unsigned char Padding[60];
} testalignedtreeitem;



/* 大对象用于验证拥有型树不会按固定 256 槽放大首个池页。 */
typedef struct testlargetreeitem {
	int Key;
	unsigned char Padding[8192];
} testlargetreeitem;



/* 回调状态同时记录释放和访问数量。 */
typedef struct testtreestate {
	size_t DropCount;
	size_t VisitCount;
	int Previous;
	xavltree* ReenterTree;
	int ReenterKey;
	bool TryDestroy;
	bool TryRemove;
	bool ReenterDone;
	bool ReenterBlocked;
} testtreestate;



/* 访问重入状态分别记录只读与修改路径的执行结果。 */
typedef struct testtreevisitstate {
	xavltree* Tree;
	bool ReadAllowed;
	bool RemoveBlocked;
	bool DestroyBlocked;
} testtreevisitstate;



/* 按整数键比较查找键与拥有型对象。 */
static int testAVLTreeCompare(const void* pKey, const void* pItem, ptr pUserData)
{
	int iKey = *(const int*)pKey;
	int iItemKey = ((const testtreeitem*)pItem)->Key;

	(void)pUserData;
	return (iKey > iItemKey) - (iKey < iItemKey);
}



/* 按整数键比较过对齐对象。 */
static int testAVLTreeAlignedCompare(const void* pKey, const void* pItem, ptr pUserData)
{
	int iKey = *(const int*)pKey;
	int iItemKey = ((const testalignedtreeitem*)pItem)->Key;

	(void)pUserData;
	return (iKey > iItemKey) - (iKey < iItemKey);
}



/* 按整数键比较大对象。 */
static int testAVLTreeLargeCompare(const void* pKey, const void* pItem, ptr pUserData)
{
	int iKey = *(const int*)pKey;
	int iItemKey = ((const testlargetreeitem*)pItem)->Key;

	(void)pUserData;
	return (iKey > iItemKey) - (iKey < iItemKey);
}



/* 释放对象浅拷贝后明确移交给树的字符串。 */
static void testAVLTreeDrop(ptr pItem, ptr pUserData)
{
	testtreeitem* pValue = (testtreeitem*)pItem;
	testtreestate* pState = (testtreestate*)pUserData;

	xrtFree(pValue->Name);
	pState->DropCount++;

	/* 释放回调内的同树操作必须被拒绝，不能破坏外层删除或清空。 */
	if ( (pState->ReenterTree != NULL) && !pState->ReenterDone ) {
		pState->ReenterDone = true;
		xrtClearError();
		if ( pState->TryDestroy ) {
			xrtAVLTreeDestroy(pState->ReenterTree);
			pState->ReenterBlocked =
				xrtErrorKind(xrtGetError()) == XERR_STATE;
		} else if ( pState->TryRemove ) {
			pState->ReenterBlocked =
				!xrtAVLTreeRemove(
					pState->ReenterTree,
					&pState->ReenterKey
				) &&
				(xrtErrorKind(xrtGetError()) == XERR_STATE);
		}
	}
}



/* 访问器验证拥有型对象按键升序输出。 */
static bool testAVLTreeVisitor(ptr pItem, ptr pUserData)
{
	testtreeitem* pValue = (testtreeitem*)pItem;
	testtreestate* pState = (testtreestate*)pUserData;

	if ( pState->VisitCount != 0 ) {
		testRequire(pValue->Key > pState->Previous, "owned AVL visit order mismatch");
	}
	pState->Previous = pValue->Key;
	pState->VisitCount++;
	return pState->VisitCount < 11;
}



/* 访问器允许查询同一棵树，但拒绝修改或销毁它。 */
static bool testAVLTreeReentryVisitor(ptr pItem, ptr pUserData)
{
	testtreeitem* pValue = (testtreeitem*)pItem;
	testtreevisitstate* pState = (testtreevisitstate*)pUserData;

	pValue->Value++;
	pState->ReadAllowed =
		(xrtAVLTreeCount(pState->Tree) == 1) &&
		(xrtAVLTreeFind(pState->Tree, &pValue->Key) == pItem);

	xrtClearError();
	pState->RemoveBlocked =
		!xrtAVLTreeRemove(pState->Tree, &pValue->Key) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE);

	xrtClearError();
	xrtAVLTreeDestroy(pState->Tree);
	pState->DestroyBlocked =
		xrtErrorKind(xrtGetError()) == XERR_STATE;
	return false;
}



/* 创建由树负责释放名称的测试对象。 */
static testtreeitem testAVLTreeItem(int iKey)
{
	static const char sName[] = "owned-item";
	testtreeitem tItem;

	tItem.Key = iKey;
	tItem.Value = iKey * 10;
	tItem.Name = (str)xrtMalloc(sizeof(sName));
	testRequire(tItem.Name != NULL, "owned AVL item name allocation failed");
	memcpy(tItem.Name, sName, sizeof(sName));
	return tItem;
}



/* 验证拥有型树复制、查找、边界、稳定地址和重复键合同。 */
static void testAVLTreeBasic(void)
{
	testtreestate tState = { 0 };
	xavltree tTree;
	testtreeitem tItem;
	testtreeitem tDuplicate;
	testtreeitem* pStable;
	bool bNew;
	int iKey;

	testRequire(
		xrtAVLTreeInit(&tTree, sizeof(testtreeitem), testAVLTreeCompare, &tState),
		"owned AVL init failed"
	);
	testRequire(xrtAVLTreeSetDrop(&tTree, testAVLTreeDrop), "owned AVL drop setup failed");
	for ( int i = 0; i < 600; i++ ) {
		tItem = testAVLTreeItem((i * 173) % 600);
		testRequire(
			xrtAVLTreeAdd(&tTree, &tItem.Key, &tItem, &bNew) != NULL,
			"owned AVL add failed"
		);
		testRequire(bNew, "owned AVL unique item reported duplicate");
	}
	testRequire(xrtAVLTreeCount(&tTree) == 600, "owned AVL count mismatch");

	iKey = 0;
	pStable = (testtreeitem*)xrtAVLTreeFind(&tTree, &iKey);
	testRequire((pStable != NULL) && (pStable->Value == 0), "owned AVL find mismatch");
	for ( int i = 601; i < 900; i++ ) {
		tItem = testAVLTreeItem(i);
		testRequire(xrtAVLTreeAdd(&tTree, &tItem.Key, &tItem, NULL) != NULL, "owned AVL growth add failed");
	}
	testRequire(xrtAVLTreeFind(&tTree, &iKey) == pStable, "owned AVL item address was not stable");
	iKey = 1000;
	xrtClearError();
	testRequire(
		xrtAVLTreeAdd(&tTree, &iKey, pStable, NULL) == NULL,
		"owned AVL add should reject a pool-owned source"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "owned AVL add alias error mismatch");
	testRequire(xrtAVLTreeCount(&tTree) == 899, "owned AVL add alias changed count");

	/* 查找键和对象内排序键不等价时必须在复制和挂树前失败。 */
	tItem = testAVLTreeItem(901);
	iKey = 902;
	xrtClearError();
	testRequire(
		xrtAVLTreeAdd(&tTree, &iKey, &tItem, NULL) == NULL,
		"owned AVL mismatched object key should fail"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "owned AVL key mismatch error");
	testRequire(xrtAVLTreeCount(&tTree) == 899, "owned AVL key mismatch changed count");
	xrtFree(tItem.Name);
	iKey = 1000;
	xrtClearError();
	testRequire(
		xrtAVLTreeAdd(&tTree, &iKey, &tTree, NULL) == NULL,
		"owned AVL metadata source should fail"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "owned AVL metadata alias error");

	/* 重复对象仍归调用方，树既不覆盖旧对象也不接管新资源。 */
	tDuplicate = testAVLTreeItem(0);
	tDuplicate.Value = -1;
	testRequire(
		xrtAVLTreeAdd(&tTree, &tDuplicate.Key, &tDuplicate, &bNew) == pStable,
		"owned AVL duplicate did not return existing item"
	);
	testRequire(!bNew && (pStable->Value == 0), "owned AVL duplicate overwrote item");
	xrtFree(tDuplicate.Name);

	iKey = 600;
	testRequire(((testtreeitem*)xrtAVLTreeLowerBound(&tTree, &iKey))->Key == 601, "owned AVL lower bound mismatch");
	iKey = 899;
	testRequire(xrtAVLTreeUpperBound(&tTree, &iKey) == NULL, "owned AVL upper bound end mismatch");
	testRequire(((testtreeitem*)xrtAVLTreeFirst(&tTree))->Key == 0, "owned AVL first mismatch");
	testRequire(((testtreeitem*)xrtAVLTreeLast(&tTree))->Key == 899, "owned AVL last mismatch");

	iKey = 173;
	testRequire(xrtAVLTreeRemove(&tTree, &iKey), "owned AVL remove failed");
	testRequire(tState.DropCount == 1, "owned AVL remove did not drop item");
	testRequire(!xrtAVLTreeHas(&tTree, &iKey), "owned AVL removed key remains present");
	xrtAVLTreeUnit(&tTree);
	testRequire(tState.DropCount == 899, "owned AVL unit drop count mismatch");
}



/* 验证 take、访问、正反迭代以及修改失效。 */
static void testAVLTreeIteration(void)
{
	testtreestate tState = { 0 };
	xavltree tTree;
	xavltreeiter tForward;
	xavltreeiter tSecond;
	xavltreeiter tReverse;
	testtreeitem tItem;
	testtreeitem tTaken;
	int iExpected;

	testRequire(
		xrtAVLTreeInit(&tTree, sizeof(testtreeitem), testAVLTreeCompare, &tState),
		"owned AVL iterator init failed"
	);
	testRequire(xrtAVLTreeSetDrop(&tTree, testAVLTreeDrop), "owned AVL iterator drop setup failed");
	for ( int i = 0; i < 32; i++ ) {
		tItem = testAVLTreeItem((i * 13) % 32);
		testRequire(xrtAVLTreeAdd(&tTree, &tItem.Key, &tItem, NULL) != NULL, "owned AVL iterator add failed");
	}

	testRequire(xrtAVLTreeIterBegin(&tTree, &tForward), "owned AVL forward begin failed");
	testRequire(xrtAVLTreeIterBegin(&tTree, &tSecond), "owned AVL second begin failed");
	for ( iExpected = 0; iExpected < 32; iExpected++ ) {
		testtreeitem* pValue = (testtreeitem*)xrtAVLTreeIterNext(&tForward);

		testRequire((pValue != NULL) && (pValue->Key == iExpected), "owned AVL forward order mismatch");
	}
	testRequire(xrtAVLTreeIterNext(&tForward) == NULL, "owned AVL forward iterator did not end");
	testRequire(((testtreeitem*)xrtAVLTreeIterNext(&tSecond))->Key == 0, "owned AVL iterator states collided");
	xrtAVLTreeIterEnd(&tSecond);

	testRequire(xrtAVLTreeIterRBegin(&tTree, &tReverse), "owned AVL reverse begin failed");
	for ( iExpected = 31; iExpected >= 0; iExpected-- ) {
		testtreeitem* pValue = (testtreeitem*)xrtAVLTreeIterNext(&tReverse);

		testRequire((pValue != NULL) && (pValue->Key == iExpected), "owned AVL reverse order mismatch");
	}
	testRequire(xrtAVLTreeVisit(&tTree, testAVLTreeVisitor, &tState) == 11, "owned AVL visitor stop mismatch");

	/* 拥有型范围迭代直接复用侵入式包含边界路径。 */
	iExpected = 12;
	testRequire(
		xrtAVLTreeIterFrom(&tTree, &iExpected, &tForward),
		"owned AVL iterator from begin failed"
	);
	testRequire(
		((testtreeitem*)xrtAVLTreeIterNext(&tForward))->Key == 12,
		"owned AVL iterator from first mismatch"
	);
	testRequire(
		((testtreeitem*)xrtAVLTreeIterNext(&tForward))->Key == 13,
		"owned AVL iterator from next mismatch"
	);
	xrtAVLTreeIterEnd(&tForward);
	testRequire(
		xrtAVLTreeIterRFrom(&tTree, &iExpected, &tReverse),
		"owned AVL reverse iterator from begin failed"
	);
	testRequire(
		((testtreeitem*)xrtAVLTreeIterNext(&tReverse))->Key == 12,
		"owned AVL reverse iterator from first mismatch"
	);
	xrtAVLTreeIterEnd(&tReverse);

	iExpected = 7;
	{
		int iOther = 8;
		testtreeitem* pOther = (testtreeitem*)xrtAVLTreeFind(&tTree, &iOther);

		xrtClearError();
		testRequire(
			!xrtAVLTreeTake(&tTree, &iExpected, pOther),
			"owned AVL take should reject pool alias output"
		);
		testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "owned AVL take alias error mismatch");
		testRequire(
			(((testtreeitem*)xrtAVLTreeFind(&tTree, &iExpected))->Key == 7) &&
			(((testtreeitem*)xrtAVLTreeFind(&tTree, &iOther))->Key == 8),
			"owned AVL take alias changed tree"
		);
	}
	xrtClearError();
	testRequire(
		!xrtAVLTreeTake(&tTree, &iExpected, &tTree.Base.Count),
		"owned AVL take should reject tree metadata output"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"owned AVL metadata output error mismatch"
	);
	testRequire(xrtAVLTreeHas(&tTree, &iExpected), "owned AVL metadata output changed tree");
	testRequire(xrtAVLTreeTake(&tTree, &iExpected, &tTaken), "owned AVL take failed");
	testRequire((tTaken.Key == 7) && (tState.DropCount == 0), "owned AVL take called drop");
	xrtFree(tTaken.Name);

	testRequire(xrtAVLTreeIterBegin(&tTree, &tForward), "owned AVL mutation begin failed");
	testRequire(xrtAVLTreeIterNext(&tForward) != NULL, "owned AVL mutation first item missing");
	tItem = testAVLTreeItem(100);
	testRequire(xrtAVLTreeAdd(&tTree, &tItem.Key, &tItem, NULL) != NULL, "owned AVL mutation add failed");
	xrtClearError();
	testRequire(xrtAVLTreeIterNext(&tForward) == NULL, "owned AVL mutated iterator should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE, "owned AVL mutation error mismatch");

	xrtAVLTreeClear(&tTree);
	testRequire((xrtAVLTreeCount(&tTree) == 0) && (tState.DropCount == 32), "owned AVL clear mismatch");
	xrtAVLTreeUnit(&tTree);
}



/* 验证释放回调不能重入删除或销毁同一棵树。 */
static void testAVLTreeDropReentry(void)
{
	testtreestate tClearState = { 0 };
	testtreestate tDestroyState = { 0 };
	xavltree tTree;
	xavltree* pTree;
	testtreeitem tItem;
	int iKey;

	testRequire(
		xrtAVLTreeInit(
			&tTree,
			sizeof(testtreeitem),
			testAVLTreeCompare,
			&tClearState
		),
		"owned AVL reentry clear init failed"
	);
	testRequire(xrtAVLTreeSetDrop(&tTree, testAVLTreeDrop), "owned AVL reentry clear drop failed");
	for ( iKey = 1; iKey <= 2; iKey++ ) {
		tItem = testAVLTreeItem(iKey);
		testRequire(
			xrtAVLTreeAdd(&tTree, &tItem.Key, &tItem, NULL) != NULL,
			"owned AVL reentry clear add failed"
		);
	}
	tClearState.ReenterTree = &tTree;
	tClearState.ReenterKey = 2;
	tClearState.TryRemove = true;
	xrtAVLTreeClear(&tTree);
	testRequire(tClearState.ReenterDone, "owned AVL clear did not exercise reentry");
	testRequire(tClearState.ReenterBlocked, "owned AVL clear allowed remove reentry");
	testRequire(
		(xrtAVLTreeCount(&tTree) == 0) && (tClearState.DropCount == 2),
		"owned AVL clear reentry changed final state"
	);
	xrtAVLTreeUnit(&tTree);

	pTree = xrtAVLTreeCreate(
		sizeof(testtreeitem),
		testAVLTreeCompare,
		&tDestroyState
	);
	testRequire(pTree != NULL, "owned AVL reentry destroy create failed");
	testRequire(xrtAVLTreeSetDrop(pTree, testAVLTreeDrop), "owned AVL reentry destroy drop failed");
	tItem = testAVLTreeItem(9);
	testRequire(
		xrtAVLTreeAdd(pTree, &tItem.Key, &tItem, NULL) != NULL,
		"owned AVL reentry destroy add failed"
	);
	tDestroyState.ReenterTree = pTree;
	tDestroyState.TryDestroy = true;
	iKey = 9;
	testRequire(xrtAVLTreeRemove(pTree, &iKey), "owned AVL outer remove failed");
	testRequire(tDestroyState.ReenterDone, "owned AVL remove did not exercise destroy reentry");
	testRequire(tDestroyState.ReenterBlocked, "owned AVL remove allowed destroy reentry");
	testRequire(xrtAVLTreeCount(pTree) == 0, "owned AVL destroy reentry damaged tree");
	xrtClearError();
	xrtAVLTreeDestroy(pTree);
}



/* 验证访问回调只允许查询和原地修改非排序字段。 */
static void testAVLTreeVisitReentry(void)
{
	testtreestate tDropState = { 0 };
	testtreevisitstate tVisitState = { 0 };
	xavltree* pTree;
	testtreeitem tItem;
	testtreeitem* pStored;
	int iKey = 7;

	pTree = xrtAVLTreeCreate(
		sizeof(testtreeitem),
		testAVLTreeCompare,
		&tDropState
	);
	testRequire(pTree != NULL, "owned AVL visit reentry create failed");
	testRequire(xrtAVLTreeSetDrop(pTree, testAVLTreeDrop), "owned AVL visit reentry drop failed");
	tItem = testAVLTreeItem(iKey);
	tItem.Value = 10;
	pStored = (testtreeitem*)xrtAVLTreeAdd(
		pTree,
		&tItem.Key,
		&tItem,
		NULL
	);
	testRequire(pStored != NULL, "owned AVL visit reentry add failed");

	tVisitState.Tree = pTree;
	testRequire(
		xrtAVLTreeVisit(
			pTree,
			testAVLTreeReentryVisitor,
			&tVisitState
		) == 1,
		"owned AVL visit reentry count mismatch"
	);
	testRequire(tVisitState.ReadAllowed, "owned AVL visitor query was rejected");
	testRequire(tVisitState.RemoveBlocked, "owned AVL visitor removed current item");
	testRequire(tVisitState.DestroyBlocked, "owned AVL visitor destroyed current tree");
	testRequire(
		(xrtAVLTreeCount(pTree) == 1) && (pStored->Value == 11),
		"owned AVL visitor changed structure or lost value mutation"
	);

	xrtClearError();
	xrtAVLTreeDestroy(pTree);
	testRequire(tDropState.DropCount == 1, "owned AVL visit reentry final drop mismatch");
}



/* 验证显式过对齐和堆创建入口。 */
static void testAVLTreeAlignmentAndCreate(void)
{
	xavltree tAligned;
	xavltree tLarge;
	xavltree* pCreated;
	testalignedtreeitem tItem;
	testalignedtreeitem* pStored;
	testlargetreeitem tLargeItem;

	memset(&tItem, 0, sizeof(tItem));
	tItem.Key = 9;
	testRequire(
		xrtAVLTreeInitAligned(
			&tAligned,
				sizeof(testalignedtreeitem),
				64,
				testAVLTreeAlignedCompare,
				NULL
			),
		"aligned AVL init failed"
	);
	pStored = (testalignedtreeitem*)xrtAVLTreeAdd(&tAligned, &tItem.Key, &tItem, NULL);
	testRequire((pStored != NULL) && (((uintptr_t)pStored & 63u) == 0), "owned AVL item alignment mismatch");
	xrtAVLTreeUnit(&tAligned);

	/* 节点大于 8 KiB 时，首个池页仍以约 64 KiB 为目标。 */
	memset(&tLargeItem, 0, sizeof(tLargeItem));
	tLargeItem.Key = 11;
	testRequire(
		xrtAVLTreeInit(
			&tLarge,
			sizeof(testlargetreeitem),
			testAVLTreeLargeCompare,
			NULL
		),
		"large-object AVL init failed"
	);
	testRequire(
		xrtAVLTreeAdd(
			&tLarge,
			&tLargeItem.Key,
			&tLargeItem,
			NULL
		) != NULL,
		"large-object AVL add failed"
	);
	testRequire(
		(tLarge.Pool.PageCapacity < XRT_POOL_PAGE_CAPACITY) &&
		(tLarge.Pool.Pages != NULL) &&
		(tLarge.Pool.Pages->MemorySize <= XRT_POOL_PAGE_BYTES_DEFAULT),
		"large-object AVL pool page amplification"
	);
	xrtAVLTreeUnit(&tLarge);

	testRequire(
		!xrtAVLTreeInitAligned(&tAligned, sizeof(testalignedtreeitem), 24, testAVLTreeAlignedCompare, NULL),
		"non-power-of-two AVL alignment should fail"
	);
	pCreated = xrtAVLTreeCreate(sizeof(testtreeitem), testAVLTreeCompare, NULL);
	testRequire(pCreated != NULL, "owned AVL create failed");
	xrtAVLTreeDestroy(pCreated);
}



/* 运行拥有型 AVL 完整合同测试。 */
int main(void)
{
	testAVLTreeBasic();
	testAVLTreeIteration();
	testAVLTreeDropReentry();
	testAVLTreeVisitReentry();
	testAVLTreeAlignmentAndCreate();
	printf("[PASS] avl_tree\n");
	return 0;
}
