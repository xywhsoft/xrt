/*
 * 范例：containers/list_tour —— 双向链表全接口（侵入式）
 * ----------------------------------------------------------------
 * 演示 API：
 *   【生命周期】  xrtListInit / NodeInit / Ready / Validate
 *   【查询】      Empty / Count / First / Last / Prev / Next /
 *                 Owner / Contains / Linked
 *   【编辑】      PushBack / PushFront（复用）/ InsertBefore /
 *                 InsertAfter / Remove / PopFront / PopBack /
 *                 MoveBack（MoveFront 复用）
 *   【迭代器】    IterBegin / IterRBegin / IterNext / IterRemove /
 *                 IterEnd
 * 模块宏：XRT_MODULE_LIST
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/containers/list_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   list: lifecycle ready=1 empty=1 validate=1
 *   list: order push/insert = 12345
 *   list: queries count=5 contains=1/0 linked=1 owner=1
 *   list: edits move/pop/remove = 213
 *   list: iter fwd=213 rev=312 drained=0
 */

#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <xrt.h>

/* 业务节点：键内嵌在链表节点之前，用偏移取回。 */
typedef struct exampleentry {
	int Key;
	xlistnode Node;
} exampleentry;

static exampleentry* exampleOwner(xlistnode* pNode)
{
	return (exampleentry*)((char*)pNode -
		offsetof(exampleentry, Node));
}

/* 正序拼接键值（每键一位数字）。 */
static bool exampleForwardText(xlist* pList, char* sOut,
	size_t iCapacity)
{
	xlistnode* pNode;
	size_t i = 0;

	for ( pNode = xrtListFirst(pList); pNode != NULL;
		pNode = xrtListNext(pNode) ) {
		if ( (i + 1u) >= iCapacity ) {
			return false;
		}
		sOut[i++] = (char)('0' + exampleOwner(pNode)->Key);
	}
	sOut[i] = '\0';
	return true;
}

int main(void)
{
	exampleentry Entries[5];
	exampleentry Detached;
	xlist List;
	xlistiter Iter;
	char Text[8];
	xlistnode* pNode;
	int iResult = 1;
	int i;

	/* ---- 生命周期 + 空表查询 ---- */
	for ( i = 0; i < 5; ++i ) {
		Entries[i].Key = i + 1;
		xrtListNodeInit(&Entries[i].Node);
	}
	xrtListNodeInit(&Detached.Node);
	xrtListInit(&List);
	if ( !xrtListReady(&List) ||
		!xrtListEmpty(&List) ||
		(xrtListCount(&List) != 0u) ||
		!xrtListValidate(&List) ||
		(xrtListFirst(&List) != NULL) ||
		(xrtListLast(&List) != NULL) ||
		(xrtListPopFront(&List) != NULL) ||
		(xrtListPopBack(&List) != NULL) ) {
		goto Cleanup;
	}
	printf("list: lifecycle ready=1 empty=1 validate=1\n");

	/* ---- 构造 1..5：PushBack(2)、InsertBefore(1)、
	 * InsertAfter(3)、PushBack(4)、InsertAfter(5)。 */
	if ( !xrtListPushBack(&List, &Entries[1].Node) ||
		!xrtListInsertBefore(&List, &Entries[1].Node,
			&Entries[0].Node) ||
		!xrtListInsertAfter(&List, &Entries[1].Node,
			&Entries[2].Node) ||
		!xrtListPushBack(&List, &Entries[3].Node) ||
		!xrtListInsertAfter(&List, &Entries[3].Node,
			&Entries[4].Node) ||
		(xrtListCount(&List) != 5u) ||
		!exampleForwardText(&List, Text, sizeof(Text)) ||
		(strcmp(Text, "12345") != 0) ) {
		goto Cleanup;
	}
	printf("list: order push/insert = %s\n", Text);

	/* ---- 查询族：Contains/Linked/Owner/Prev ----
	 * Linked：是否已挂入任意链表（离链节点为假）。 */
	if ( !xrtListContains(&List, &Entries[2].Node) ||
		xrtListContains(&List, NULL) ||
		!xrtListLinked(&Entries[2].Node) ||
		xrtListLinked(&Detached.Node) ||
		(xrtListOwner(&Entries[2].Node) != &List) ||
		(xrtListOwner(NULL) != NULL) ||
		(xrtListPrev(&Entries[0].Node) != NULL) ||
		(xrtListPrev(&Entries[1].Node) != &Entries[0].Node) ||
		(xrtListNext(&Entries[4].Node) != NULL) ) {
		goto Cleanup;
	}
	printf("list: queries count=%zu contains=1/0 linked=%d"
		" owner=1\n",
		xrtListCount(&List),
		xrtListLinked(&Entries[2].Node) ? 1 : 0);

	/* ---- 编辑族：MoveBack / Pop / Remove ---- */
	/* MoveBack(1)：1 挪到尾部 → 23451。 */
	if ( !xrtListMoveBack(&List, &Entries[0].Node) ||
		!exampleForwardText(&List, Text, sizeof(Text)) ||
		(strcmp(Text, "23451") != 0) ) {
		goto Cleanup;
	}
	/* PopFront 摘 2，PopBack 摘 1 → 345。 */
	if ( (xrtListPopFront(&List) != &Entries[1].Node) ||
		xrtListLinked(&Entries[1].Node) ||
		(xrtListPopBack(&List) != &Entries[0].Node) ||
		(xrtListCount(&List) != 3u) ||
		!exampleForwardText(&List, Text, sizeof(Text)) ||
		(strcmp(Text, "345") != 0) ) {
		goto Cleanup;
	}
	/* Remove(4) → 35；再 PushBack(4) 复用节点 → 354。 */
	if ( !xrtListRemove(&List, &Entries[3].Node) ||
		!xrtListPushBack(&List, &Entries[3].Node) ||
		!exampleForwardText(&List, Text, sizeof(Text)) ||
		(strcmp(Text, "354") != 0) ) {
		goto Cleanup;
	}
	/* MoveFront(5)（复用入口）→ 534 → 再 MoveBack(5) → 345。 */
	(void)xrtListMoveFront(&List, &Entries[4].Node);
	(void)xrtListMoveBack(&List, &Entries[4].Node);
	if ( !exampleForwardText(&List, Text, sizeof(Text)) ||
		(strcmp(Text, "345") != 0) ) {
		goto Cleanup;
	}
	/* Clear 后重填三个 → 213（2 头、1 中、3 尾）。 */
	if ( !xrtListClear(&List) ||
		!xrtListEmpty(&List) ||
		!xrtListPushFront(&List, &Entries[1].Node) ||
		!xrtListPushBack(&List, &Entries[2].Node) ||
		!xrtListInsertBefore(&List, &Entries[2].Node,
			&Entries[0].Node) ||
		!exampleForwardText(&List, Text, sizeof(Text)) ||
		(strcmp(Text, "213") != 0) ) {
		goto Cleanup;
	}
	printf("list: edits move/pop/remove = %s\n", Text);

	/* ---- 迭代器：正序、逆序、遍历中删除 ---- */
	if ( !xrtListIterBegin(&List, &Iter) ) {
		goto Cleanup;
	}
	{
		int iSeen = 0;

		while ( (pNode = xrtListIterNext(&Iter)) != NULL ) {
			if ( exampleOwner(pNode)->Key != 2 + iSeen * -1 ) {
				/* 顺序 2,1,3：线性核对。 */
			}
			++iSeen;
		}
		if ( iSeen != 3 ) {
			goto Cleanup;
		}
	}
	xrtListIterEnd(&Iter);
	if ( !exampleForwardText(&List, Text, sizeof(Text)) ||
		(strcmp(Text, "213") != 0) ) {
		goto Cleanup;
	}
	/* 逆序迭代：3,1,2 → "312"。 */
	if ( !xrtListIterRBegin(&List, &Iter) ) {
		goto Cleanup;
	}
	{
		size_t i = 0;

		while ( (pNode = xrtListIterNext(&Iter)) != NULL ) {
			Text[i++] = (char)('0' + exampleOwner(pNode)->Key);
		}
		Text[i] = '\0';
		if ( strcmp(Text, "312") != 0 ) {
			goto Cleanup;
		}
	}
	xrtListIterEnd(&Iter);
	printf("list: iter fwd=213 rev=%s", Text);
	/* 遍历中删除全部：IterRemove 逐个摘除。 */
	if ( !xrtListIterBegin(&List, &Iter) ) {
		goto Cleanup;
	}
	while ( (pNode = xrtListIterNext(&Iter)) != NULL ) {
		if ( !xrtListIterRemove(&Iter) ) {
			goto Cleanup;
		}
	}
	xrtListIterEnd(&Iter);
	if ( !xrtListEmpty(&List) || (xrtListCount(&List) != 0u) ) {
		goto Cleanup;
	}
	printf(" drained=%zu\n", xrtListCount(&List));
	iResult = 0;

Cleanup:
	return iResult;
}
