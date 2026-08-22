#include "../test.h"



typedef struct testitem {
	int Value;
	xlistnode Link;
} testitem;



/* 从任意位置嵌入的链表节点恢复测试对象。 */
static testitem* testListItem(xlistnode* pNode)
{
	return XRT_CONTAINER_OF(pNode, testitem, Link);
}



/* 验证无分配侵入式链表的基础顺序、移动、弹出和清空契约。 */
int main(void)
{
	xlist List = XRT_LIST_INIT;
	testitem First = { 1, XRT_LIST_NODE_INIT };
	testitem Second = { 2, XRT_LIST_NODE_INIT };
	testitem Third = { 3, XRT_LIST_NODE_INIT };
	testitem Fourth;
	xlistnode* pNode;

	xrtListNodeInit(&Fourth.Link);
	Fourth.Value = 4;
	testRequire(xrtListReady(&List), "static list should be ready");
	testRequire(xrtListEmpty(&List), "new list should be empty");
	testRequire(xrtListValidate(&List), "new list should validate");

	testRequire(xrtListPushBack(&List, &Second.Link), "push second failed");
	testRequire(xrtListPushFront(&List, &First.Link), "push first failed");
	testRequire(
		xrtListInsertAfter(&List, &Second.Link, &Fourth.Link),
		"insert fourth failed"
	);
	testRequire(
		xrtListInsertBefore(&List, &Fourth.Link, &Third.Link),
		"insert third failed"
	);
	testRequire(xrtListCount(&List) == 4u, "list count mismatch");
	testRequire(
		(testListItem(xrtListFirst(&List))->Value == 1) &&
		(testListItem(xrtListNext(&First.Link))->Value == 2) &&
		(testListItem(xrtListPrev(&Fourth.Link))->Value == 3) &&
		(testListItem(xrtListLast(&List))->Value == 4),
		"list order mismatch"
	);
	testRequire(
		xrtListContains(&List, &Third.Link) &&
		xrtListLinked(&Third.Link) &&
		(xrtListOwner(&Third.Link) == &List),
		"node ownership mismatch"
	);

	testRequire(xrtListMoveFront(&List, &Third.Link), "move front failed");
	testRequire(xrtListMoveBack(&List, &First.Link), "move back failed");
	testRequire(
		(testListItem(List.First)->Value == 3) &&
		(testListItem(List.Last)->Value == 1) &&
		xrtListValidate(&List),
		"move order mismatch"
	);

	pNode = xrtListPopFront(&List);
	testRequire(
		(pNode == &Third.Link) && !xrtListLinked(pNode) &&
		(pNode->Prev == NULL) && (pNode->Next == NULL),
		"pop front did not detach node"
	);
	pNode = xrtListPopBack(&List);
	testRequire(
		(pNode == &First.Link) && !xrtListLinked(pNode),
		"pop back did not detach node"
	);
	testRequire(xrtListClear(&List), "list clear failed");
	testRequire(
		xrtListEmpty(&List) &&
		!xrtListLinked(&Second.Link) &&
		!xrtListLinked(&Fourth.Link) &&
		xrtListValidate(&List),
		"list clear did not detach all nodes"
	);
	testRequire(xrtListPopFront(&List) == NULL, "empty pop should be null");
	testRequire(xrtListPopBack(&List) == NULL, "empty reverse pop should be null");
	printf("[PASS] list\n");
	return 0;
}
