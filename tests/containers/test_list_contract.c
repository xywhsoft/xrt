#include "../test.h"



typedef struct testnode {
	xlistnode Link;
	int Value;
} testnode;



/* 要求当前错误属于链表域并具有指定类别和代码。 */
static void testListError(xerrkind Kind, xlisterror Code, cstr sMessage)
{
	const xerror* pError = xrtGetError();

	testRequire(pError != NULL, sMessage);
	testRequire(xrtErrorKind(pError) == Kind, sMessage);
	testRequire(strcmp(xrtErrorDomain(pError), "xrt.list") == 0, sMessage);
	testRequire(xrtErrorCode(pError) == (int32)Code, sMessage);
	xrtClearError();
}



/* 验证所有权、损坏检测、计数上限和迭代失效边界。 */
int main(void)
{
	xlist First = XRT_LIST_INIT;
	xlist Second = XRT_LIST_INIT;
	xlist Unready = { 0 };
	testnode Nodes[5] = {
		{ XRT_LIST_NODE_INIT, 0 },
		{ XRT_LIST_NODE_INIT, 1 },
		{ XRT_LIST_NODE_INIT, 2 },
		{ XRT_LIST_NODE_INIT, 3 },
		{ XRT_LIST_NODE_INIT, 4 }
	};
	xlistiter Iterator;
	xlistnode* pNode;
	size_t iSeen = 0;

	testRequire(!xrtListPushBack(&Unready, &Nodes[0].Link),
		"unready list should reject insertion");
	testListError(XERR_STATE, XLIST_ERROR_STATE, "unready list error mismatch");
	testRequire(!xrtListPushBack(NULL, &Nodes[0].Link),
		"null list should reject insertion");
	testListError(XERR_ARGUMENT, XLIST_ERROR_ARGUMENT, "null list error mismatch");
	testRequire(!xrtListPushBack(&First, NULL), "null node should fail");
	testListError(XERR_ARGUMENT, XLIST_ERROR_ARGUMENT, "null node error mismatch");

	testRequire(xrtListPushBack(&First, &Nodes[0].Link), "first insert failed");
	testRequire(!xrtListPushBack(&First, &Nodes[0].Link),
		"duplicate insert should fail");
	testListError(XERR_STATE, XLIST_ERROR_STATE, "duplicate insert error mismatch");
	testRequire(!xrtListPushBack(&Second, &Nodes[0].Link),
		"cross-list insert should fail");
	testListError(XERR_STATE, XLIST_ERROR_STATE, "cross-list insert error mismatch");
	testRequire(!xrtListRemove(&Second, &Nodes[0].Link),
		"foreign remove should fail");
	testListError(XERR_STATE, XLIST_ERROR_STATE, "foreign remove error mismatch");
	testRequire(!xrtListInsertAfter(&Second, &Nodes[0].Link, &Nodes[1].Link),
		"foreign position should fail");
	testListError(XERR_STATE, XLIST_ERROR_STATE, "foreign position error mismatch");

	First.Count = SIZE_MAX;
	testRequire(!xrtListPushBack(&First, &Nodes[1].Link),
		"overflowing insert should fail");
	testListError(XERR_RANGE, XLIST_ERROR_RANGE, "list count overflow error mismatch");
	First.Count = 1u;
	testRequire(xrtListPushBack(&First, &Nodes[1].Link), "second insert failed");
	testRequire(xrtListPushBack(&First, &Nodes[2].Link), "third insert failed");
	testRequire(xrtListPushBack(&First, &Nodes[3].Link), "fourth insert failed");

	testRequire(xrtListIterBegin(&First, &Iterator), "iterator begin failed");
	while ( (pNode = xrtListIterNext(&Iterator)) != NULL ) {
		testnode* pItem = XRT_CONTAINER_OF(pNode, testnode, Link);

		iSeen++;
		if ( (pItem->Value & 1) != 0 ) {
			testRequire(xrtListIterRemove(&Iterator), "iterator remove failed");
		}
	}
	testRequire(iSeen == 4u, "iterator visit count mismatch");
	testRequire(
		(First.Count == 2u) &&
		(Nodes[0].Link.Owner == &First) &&
		(Nodes[2].Link.Owner == &First) &&
		(Nodes[1].Link.Owner == NULL) &&
		(Nodes[3].Link.Owner == NULL),
		"iterator remove ownership mismatch"
	);
	testRequire(!xrtListIterRemove(&Iterator),
		"ended iterator should reject remove");
	testListError(XERR_STATE, XLIST_ERROR_STATE, "ended iterator error mismatch");

	testRequire(xrtListIterRBegin(&First, &Iterator), "reverse iterator begin failed");
	testRequire(xrtListIterNext(&Iterator) == &Nodes[2].Link,
		"reverse iterator first mismatch");
	testRequire(xrtListPushBack(&First, &Nodes[4].Link), "external iterator insert failed");
	testRequire(xrtListIterNext(&Iterator) == NULL,
		"modified iterator should stop");
	testListError(XERR_STATE, XLIST_ERROR_MODIFIED, "modified iterator error mismatch");

	Nodes[2].Link.Prev = NULL;
	testRequire(!xrtListValidate(&First), "corrupt list should fail validation");
	testListError(XERR_STATE, XLIST_ERROR_STATE, "corrupt validation error mismatch");
	testRequire(!xrtListClear(&First), "corrupt list should reject clear");
	testListError(XERR_STATE, XLIST_ERROR_STATE, "corrupt clear error mismatch");
	testRequire((First.Count == 3u) && (Nodes[0].Link.Owner == &First),
		"failed clear should preserve ownership");
	Nodes[2].Link.Prev = &Nodes[0].Link;
	testRequire(xrtListValidate(&First), "restored list should validate");
	testRequire(xrtListClear(&First), "restored list clear failed");
	testRequire(xrtListClear(&Second), "empty second list clear failed");
	printf("[PASS] list-contract\n");
	return 0;
}
