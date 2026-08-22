#include "../test.h"



/* 测试节点故意把侵入式链接放在业务字段之后。 */
typedef struct testavlnode {
	int Key;
	int Value;
	xavlnode Link;
} testavlnode;



/* 访问统计用于验证有序访问和提前停止。 */
typedef struct testvisitstate {
	int Previous;
	size_t Limit;
	size_t Count;
} testvisitstate;



/* 从嵌入链接恢复测试业务节点。 */
static testavlnode* testAVLNode(xavlnode* pNode)
{
	return (testavlnode*)((bytes)pNode - offsetof(testavlnode, Link));
}



/* 从只读嵌入链接恢复测试业务节点。 */
static const testavlnode* testAVLConstNode(const xavlnode* pNode)
{
	return (const testavlnode*)((cbytes)pNode - offsetof(testavlnode, Link));
}



/* 按整数键比较查找键与侵入式节点。 */
static int testAVLCompare(const void* pKey, const xavlnode* pNode, ptr pUserData)
{
	int iKey = *(const int*)pKey;
	int iNodeKey = testAVLConstNode(pNode)->Key;

	(void)pUserData;
	return (iKey > iNodeKey) - (iKey < iNodeKey);
}



/* 递归验证顺序、高度和平衡约束，并统计真实节点数量。 */
static uint8 testAVLValidateBranch(
	const xavlnode* pNode,
	bool bHasMinimum,
	int iMinimum,
	bool bHasMaximum,
	int iMaximum,
	size_t* pCount
)
{
	const testavlnode* pItem;
	uint8 iLeft;
	uint8 iRight;
	uint8 iHeight;

	if ( pNode == NULL ) {
		return 0;
	}
	pItem = testAVLConstNode(pNode);
	testRequire(!bHasMinimum || (pItem->Key > iMinimum), "AVL minimum order violated");
	testRequire(!bHasMaximum || (pItem->Key < iMaximum), "AVL maximum order violated");

	iLeft = testAVLValidateBranch(
		pNode->Left,
		bHasMinimum,
		iMinimum,
		true,
		pItem->Key,
		pCount
	);
	iRight = testAVLValidateBranch(
		pNode->Right,
		true,
		pItem->Key,
		bHasMaximum,
		iMaximum,
		pCount
	);
	iHeight = (uint8)((iLeft > iRight ? iLeft : iRight) + 1u);
	testRequire(pNode->Height == iHeight, "AVL stored height mismatch");
	testRequire(
		((int)iLeft - (int)iRight >= -1) && ((int)iLeft - (int)iRight <= 1),
		"AVL balance factor violated"
	);
	(*pCount)++;
	return iHeight;
}



/* 验证整棵树的公开摘要与真实结构一致。 */
static void testAVLValidate(const xavl* pTree)
{
	size_t iCount = 0;

	(void)testAVLValidateBranch(pTree->Root, false, 0, false, 0, &iCount);
	testRequire(iCount == pTree->Count, "AVL count mismatch");
	testRequire((iCount == 0) == (pTree->Root == NULL), "AVL root summary mismatch");
}



/* 按升序访问并在达到限制后停止。 */
static bool testAVLVisitor(xavlnode* pNode, ptr pUserData)
{
	testvisitstate* pState = (testvisitstate*)pUserData;
	int iKey = testAVLNode(pNode)->Key;

	if ( pState->Count != 0 ) {
		testRequire(iKey > pState->Previous, "AVL visitor order mismatch");
	}
	pState->Previous = iKey;
	pState->Count++;
	return pState->Count < pState->Limit;
}



/* 验证基础插入、重复键、查找、边界和删除合同。 */
static void testAVLBasic(void)
{
	xavl tTree;
	testavlnode pNodes[9];
	testavlnode tDuplicate;
	int pKeys[] = { 40, 20, 60, 10, 30, 50, 70, 25, 35 };
	int iKey;
	bool bNew;

	memset(pNodes, 0, sizeof(pNodes));
	memset(&tDuplicate, 0, sizeof(tDuplicate));
	testRequire(xrtAVLInit(&tTree), "AVL init failed");
	for ( size_t i = 0; i < 9; i++ ) {
		pNodes[i].Key = pKeys[i];
		pNodes[i].Value = pKeys[i] * 10;
		xrtAVLNodeInit(&pNodes[i].Link);
		testRequire(
			xrtAVLInsert(
				&tTree,
				&pNodes[i].Link,
				&pNodes[i].Key,
				testAVLCompare,
				NULL,
				&bNew
			) == &pNodes[i].Link,
			"AVL insert failed"
		);
		testRequire(bNew, "AVL insert did not report new node");
		testAVLValidate(&tTree);
	}

	/* 重复键不修改候选节点，也不分配任何隐藏状态。 */
	tDuplicate.Key = 30;
	tDuplicate.Link.Left = (xavlnode*)(uintptr_t)1;
	tDuplicate.Link.Right = (xavlnode*)(uintptr_t)2;
	tDuplicate.Link.Height = 77;
	testRequire(
		xrtAVLInsert(
			&tTree,
			&tDuplicate.Link,
			&tDuplicate.Key,
			testAVLCompare,
			NULL,
			&bNew
		) == &pNodes[4].Link,
		"AVL duplicate did not return existing node"
	);
	testRequire(!bNew, "AVL duplicate reported new node");
	testRequire(
		(tDuplicate.Link.Left == (xavlnode*)(uintptr_t)1) &&
		(tDuplicate.Link.Right == (xavlnode*)(uintptr_t)2) &&
		(tDuplicate.Link.Height == 77),
		"AVL duplicate modified candidate node"
	);
	tDuplicate.Key = 31;
	xrtClearError();
	testRequire(
		xrtAVLInsert(
			&tTree,
			&tDuplicate.Link,
			&tDuplicate.Key,
			testAVLCompare,
			NULL,
			NULL
		) == NULL,
		"AVL linked-state candidate should fail"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE, "AVL candidate state error mismatch");
	testRequire(tTree.Count == 9, "AVL invalid candidate changed tree count");

	iKey = 25;
	testRequire(testAVLNode(xrtAVLFind(&tTree, &iKey, testAVLCompare, NULL))->Value == 250, "AVL find mismatch");
	iKey = 26;
	testRequire(testAVLNode(xrtAVLLowerBound(&tTree, &iKey, testAVLCompare, NULL))->Key == 30, "AVL lower bound mismatch");
	iKey = 30;
	testRequire(testAVLNode(xrtAVLUpperBound(&tTree, &iKey, testAVLCompare, NULL))->Key == 35, "AVL upper bound mismatch");
	testRequire(testAVLNode(xrtAVLFirst(&tTree))->Key == 10, "AVL first mismatch");
	testRequire(testAVLNode(xrtAVLLast(&tTree))->Key == 70, "AVL last mismatch");

	/* 删除双子节点必须返回原节点，而不是被搬移的前驱节点。 */
	iKey = 40;
	testRequire(xrtAVLRemove(&tTree, &iKey, testAVLCompare, NULL) == &pNodes[0].Link, "AVL root remove mismatch");
	testRequire(
		(pNodes[0].Link.Left == NULL) &&
		(pNodes[0].Link.Right == NULL) &&
		(pNodes[0].Link.Height == 0),
		"AVL removed node was not reset"
	);
	testAVLValidate(&tTree);
	iKey = 999;
	testRequire(xrtAVLRemove(&tTree, &iKey, testAVLCompare, NULL) == NULL, "AVL missing remove should be empty");
}



/* 验证多迭代器、正反遍历、访问提前停止和修改失效。 */
static void testAVLIteration(void)
{
	xavl tTree;
	testavlnode pNodes[17];
	testavlnode tExtra;
	xavliter tForward;
	xavliter tSecond;
	xavliter tReverse;
	testvisitstate tVisit = { 0, 5, 0 };
	bool bNew;
	int iExpected = 0;
	int iBoundary;

	memset(pNodes, 0, sizeof(pNodes));
	memset(&tExtra, 0, sizeof(tExtra));
	testRequire(xrtAVLInit(&tTree), "AVL iterator tree init failed");
	for ( size_t i = 0; i < 17; i++ ) {
		pNodes[i].Key = (int)((i * 7u) % 17u);
		xrtAVLNodeInit(&pNodes[i].Link);
		testRequire(
			xrtAVLInsert(&tTree, &pNodes[i].Link, &pNodes[i].Key, testAVLCompare, NULL, NULL) != NULL,
			"AVL iterator setup failed"
		);
	}

	testRequire(xrtAVLIterBegin(&tTree, &tForward), "AVL forward iterator begin failed");
	testRequire(xrtAVLIterBegin(&tTree, &tSecond), "AVL second iterator begin failed");
	while ( true ) {
		xavlnode* pNode = xrtAVLIterNext(&tForward);

		if ( pNode == NULL ) {
			break;
		}
		testRequire(testAVLNode(pNode)->Key == iExpected, "AVL forward iterator order mismatch");
		iExpected++;
	}
	testRequire(iExpected == 17, "AVL forward iterator count mismatch");
	testRequire(testAVLNode(xrtAVLIterNext(&tSecond))->Key == 0, "AVL parallel iterator state collided");
	xrtAVLIterEnd(&tSecond);

	testRequire(xrtAVLIterRBegin(&tTree, &tReverse), "AVL reverse iterator begin failed");
	iExpected = 16;
	while ( true ) {
		xavlnode* pNode = xrtAVLIterNext(&tReverse);

		if ( pNode == NULL ) {
			break;
		}
		testRequire(testAVLNode(pNode)->Key == iExpected, "AVL reverse iterator order mismatch");
		iExpected--;
	}
	testRequire(iExpected == -1, "AVL reverse iterator count mismatch");
	testRequire(xrtAVLVisit(&tTree, testAVLVisitor, &tVisit) == 5, "AVL visitor stop count mismatch");

	/* 包含边界迭代必须直接定位到 ceil 或 floor，而不是从树端扫描。 */
	iBoundary = 5;
	testRequire(
		xrtAVLIterFrom(
			&tTree,
			&iBoundary,
			testAVLCompare,
			NULL,
			&tForward
		),
		"AVL iterator from begin failed"
	);
	testRequire(testAVLNode(xrtAVLIterNext(&tForward))->Key == 5, "AVL iterator from first mismatch");
	testRequire(testAVLNode(xrtAVLIterNext(&tForward))->Key == 6, "AVL iterator from next mismatch");
	xrtAVLIterEnd(&tForward);
	iBoundary = 5;
	testRequire(
		xrtAVLIterRFrom(
			&tTree,
			&iBoundary,
			testAVLCompare,
			NULL,
			&tReverse
		),
		"AVL reverse iterator from begin failed"
	);
	testRequire(testAVLNode(xrtAVLIterNext(&tReverse))->Key == 5, "AVL reverse iterator from first mismatch");
	testRequire(testAVLNode(xrtAVLIterNext(&tReverse))->Key == 4, "AVL reverse iterator from next mismatch");
	xrtAVLIterEnd(&tReverse);

	testRequire(xrtAVLIterBegin(&tTree, &tForward), "AVL mutation iterator begin failed");
	testRequire(xrtAVLIterNext(&tForward) != NULL, "AVL mutation iterator first item missing");
	tExtra.Key = 99;
	xrtAVLNodeInit(&tExtra.Link);
	testRequire(
		xrtAVLInsert(&tTree, &tExtra.Link, &tExtra.Key, testAVLCompare, NULL, &bNew) != NULL,
		"AVL mutation insert failed"
	);
	xrtClearError();
	testRequire(xrtAVLIterNext(&tForward) == NULL, "AVL mutated iterator should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE, "AVL iterator mutation error mismatch");
}



/* 以稳定排列反复插入和删除，覆盖全部旋转及前驱替换路径。 */
static void testAVLStress(void)
{
	xavl tTree;
	testavlnode pNodes[257];
	bool pRemoved[257];

	memset(pNodes, 0, sizeof(pNodes));
	memset(pRemoved, 0, sizeof(pRemoved));
	testRequire(xrtAVLInit(&tTree), "AVL stress init failed");
	for ( size_t i = 0; i < 257; i++ ) {
		size_t iKey = (i * 73u) % 257u;

		pNodes[iKey].Key = (int)iKey;
		xrtAVLNodeInit(&pNodes[iKey].Link);
		testRequire(
			xrtAVLInsert(
				&tTree,
				&pNodes[iKey].Link,
				&pNodes[iKey].Key,
				testAVLCompare,
				NULL,
				NULL
			) != NULL,
			"AVL stress insert failed"
		);
		testAVLValidate(&tTree);
	}

	for ( size_t i = 0; i < 257; i++ ) {
		size_t iKey = (i * 151u) % 257u;
		int iSearch = (int)iKey;

		testRequire(!pRemoved[iKey], "AVL stress removal permutation repeated");
		pRemoved[iKey] = true;
		testRequire(
			xrtAVLRemove(&tTree, &iSearch, testAVLCompare, NULL) == &pNodes[iKey].Link,
			"AVL stress remove returned wrong node"
		);
		testAVLValidate(&tTree);
	}
	testRequire((tTree.Root == NULL) && (tTree.Count == 0), "AVL stress final state mismatch");
}



/* 运行侵入式 AVL 完整合同测试。 */
int main(void)
{
	testAVLBasic();
	testAVLIteration();
	testAVLStress();
	printf("[PASS] avl\n");
	return 0;
}
