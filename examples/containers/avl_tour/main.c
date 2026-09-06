/*
 * 范例：containers/avl_tour —— AVL 双形态全接口（侵入式 + 拥有式）
 * ----------------------------------------------------------------
 * 演示 API：
 *   【侵入式 xavl】  xrtAVLClear / Find / LowerBound / UpperBound /
 *                    First / Last / Remove /
 *                    Visit（访问器可提前停止）/
 *                    IterRBegin（降序）/ IterFrom（从键起升序）/
 *                    IterRFrom（不大于键起降序）/ IterEnd
 *   【拥有式 xavltree】 InitAligned / Create / CreateAligned / Destroy /
 *                    SetDrop（对象资源释放器）/ Clear / Count /
 *                    ConstFind / Has / Take（移出所有权）/
 *                    Remove / First / Last / LowerBound / UpperBound /
 *                    Visit / IterBegin / IterRBegin / IterFrom /
 *                    IterRFrom / IterEnd
 * 模块宏：XRT_MODULE_AVL
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/containers/avl_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   avl: intrusive find/bounds/visit/iter = ok (count=4)
 *   avl: owning tree 22 APIs = ok (drop=1) after-clear drop=2
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>

/* 业务结构：键内嵌在节点容器里。 */
typedef struct exampleitem {
	int Key;
	xavlnode Node;
} exampleitem;

/* 侵入式比较器：key 是 int*，节点经 userData 无需还原（Key 放在
 * 节点前部，直接用偏移取回容器）。 */
static int exampleCompareIntrusive(const void* pKey,
	const xavlnode* pNode, ptr pUserData)
{
	const exampleitem* pItem;

	(void)pUserData;
	pItem = (const exampleitem*)((const char*)pNode -
		offsetof(exampleitem, Node));
	return *(const int*)pKey - pItem->Key;
}

/* 访问器：数到第三个就停（验证提前停止语义）。 */
static bool exampleVisitCount(xavlnode* pNode, ptr pUserData)
{
	size_t* pSeen = (size_t*)pUserData;
	const exampleitem* pItem = (const exampleitem*)pNode;

	(void)pItem;
	*pSeen = *pSeen + 1u;
	return *pSeen < 3u;  /* 第三个返回 false 终止 */
}

/* 拥有式比较器：key 与存储对象都是 int。 */
static int exampleCompareOwned(const void* pKey, const void* pItem,
	ptr pUserData)
{
	(void)pUserData;
	return *(const int*)pKey - *(const int*)pItem;
}

/* Drop 计数器：Clear 时逐对象回收。 */
static void exampleDrop(ptr pItem, ptr pUserData)
{
	size_t* pDropped = (size_t*)pUserData;

	(void)pItem;
	*pDropped = *pDropped + 1u;
}

static bool exampleVisitOwned(ptr pItem, ptr pUserData)
{
	size_t* pSeen = (size_t*)pUserData;

	(void)pItem;
	*pSeen = *pSeen + 1u;
	return true;
}

int main(void)
{
	exampleitem Items[6];
	int Keys[6];
	int Probe;
	size_t iSeen;
	size_t iDropped;
	int i;

	/* ---- 侵入式形态：插入 5,2,8,1,9（乱序制造旋转） ---- */
	{
		xavl Tree;
		xavliter Iter;
		xavlnode* pNode;

		Keys[0] = 5;
		Keys[1] = 2;
		Keys[2] = 8;
		Keys[3] = 1;
		Keys[4] = 9;
		(void)xrtAVLInit(&Tree);
		for ( i = 0; i < 5; ++i ) {
			xrtAVLNodeInit(&Items[i].Node);
			Items[i].Key = Keys[i];
			if ( xrtAVLInsert(&Tree, &Items[i].Node, &Keys[i],
				exampleCompareIntrusive, NULL, NULL) ==
				NULL ) {
				return 1;
			}
		}
		/* Find / LowerBound / UpperBound。 */
		Probe = 5;
		if ( (xrtAVLFind(&Tree, &Probe,
				exampleCompareIntrusive, NULL) !=
				&Items[0].Node) ) {
			return 2;
		}
		Probe = 4;  /* 不存在：LowerBound → 5，UpperBound → 5。 */
		if ( (xrtAVLLowerBound(&Tree, &Probe,
				exampleCompareIntrusive, NULL) !=
				&Items[0].Node) ||
			(xrtAVLUpperBound(&Tree, &Probe,
				exampleCompareIntrusive, NULL) !=
				&Items[0].Node) ) {
			return 3;
		}
		Probe = 5;  /* 存在：UpperBound → 8（严格大于）。 */
		if ( xrtAVLUpperBound(&Tree, &Probe,
				exampleCompareIntrusive, NULL) !=
			&Items[2].Node ) {
			return 4;
		}
		/* First / Last：升序首 1 末 9。 */
		if ( (xrtAVLFirst(&Tree) != &Items[3].Node) ||
			(xrtAVLLast(&Tree) != &Items[4].Node) ) {
			return 5;
		}
		/* Visit：第三个返回 false 提前停止 → 恰好访问 3 个。 */
		iSeen = 0;
		if ( (xrtAVLVisit(&Tree, exampleVisitCount, &iSeen) !=
			3u) || (iSeen != 3u) ) {
			return 6;
		}
		/* IterRBegin：降序 9,8,...；IterEnd 提前结束。 */
		if ( !xrtAVLIterRBegin(&Tree, &Iter) ||
			((pNode = xrtAVLIterNext(&Iter)) == NULL) ||
			(((exampleitem*)((char*)pNode -
				offsetof(exampleitem, Node)))->Key != 9) ) {
			return 7;
		}
		xrtAVLIterEnd(&Iter);
		/* IterFrom：从第一个不小于 5 开始 → 5,8,9。 */
		Probe = 5;
		iSeen = 0;
		if ( !xrtAVLIterFrom(&Tree, &Probe,
				exampleCompareIntrusive, NULL, &Iter) ) {
			return 8;
		}
		while ( (pNode = xrtAVLIterNext(&Iter)) != NULL ) {
			++iSeen;
		}
		xrtAVLIterEnd(&Iter);
		if ( iSeen != 3u ) {
			return 9;
		}
		/* IterRFrom：从第一个不大于 8 开始降序 → 8,5,2,1。 */
		Probe = 8;
		iSeen = 0;
		if ( !xrtAVLIterRFrom(&Tree, &Probe,
				exampleCompareIntrusive, NULL, &Iter) ) {
			return 10;
		}
		while ( (pNode = xrtAVLIterNext(&Iter)) != NULL ) {
			++iSeen;
		}
		xrtAVLIterEnd(&Iter);
		if ( iSeen != 4u ) {
			return 11;
		}
		/* Remove：删 5 后 Find 不再命中；Clear 清空计数。 */
		Probe = 5;
		if ( xrtAVLRemove(&Tree, &Probe, exampleCompareIntrusive,
			NULL) != &Items[0].Node ) {
			return 12;
		}
		if ( (xrtAVLFind(&Tree, &Probe, exampleCompareIntrusive,
			NULL) != NULL) || (Tree.Count != 4u) ) {
			return 13;
		}
		printf("avl: intrusive find/bounds/visit/iter = ok"
			" (count=%zu)\n", Tree.Count);
		xrtAVLClear(&Tree);
		if ( Tree.Count != 0u ) {
			return 14;
		}
	}

	/* ---- 拥有式形态：堆树 + Drop + 全接口 ---- */
	{
		/* Drop 回调的用户数据来自树级 userData（Create 时传入）。 */
		xavltree* pTree;
		xavltreeiter Iter;
		ptr pItem;
		int Value;

		iDropped = 0;
		pTree = xrtAVLTreeCreate(sizeof(int),
			exampleCompareOwned, &iDropped);
		if ( pTree == NULL ) {
			return 15;
		}
		if ( !xrtAVLTreeSetDrop(pTree, exampleDrop) ) {
			xrtAVLTreeDestroy(pTree);
			return 16;
		}
		Keys[0] = 10;
		Keys[1] = 30;
		Keys[2] = 20;
		for ( i = 0; i < 3; ++i ) {
			if ( xrtAVLTreeAdd(pTree, &Keys[i], &Keys[i], NULL) ==
				NULL ) {
				xrtAVLTreeDestroy(pTree);
				return 17;
			}
		}
		if ( (xrtAVLTreeCount(pTree) != 3u) ) {
			xrtAVLTreeDestroy(pTree);
			return 18;
		}
		/* Find / ConstFind / Has。 */
		Probe = 20;
		if ( (xrtAVLTreeFind(pTree, &Probe) == NULL) ||
			(*(const int*)xrtAVLTreeConstFind(pTree,
				&Probe) != 20) ||
			!xrtAVLTreeHas(pTree, &Probe) ) {
			xrtAVLTreeDestroy(pTree);
			return 19;
		}
		/* 边界族：First=10 Last=30 LowerBound(20)=20
		 * UpperBound(20)=30。 */
		if ( (*(int*)xrtAVLTreeFirst(pTree) != 10) ||
			(*(int*)xrtAVLTreeLast(pTree) != 30) ||
			(*(int*)xrtAVLTreeLowerBound(pTree, &Probe) != 20) ||
			(*(int*)xrtAVLTreeUpperBound(pTree, &Probe) != 30) ) {
			xrtAVLTreeDestroy(pTree);
			return 20;
		}
		/* Visit 全量。 */
		iSeen = 0;
		if ( (xrtAVLTreeVisit(pTree, exampleVisitOwned, &iSeen) !=
			3u) || (iSeen != 3u) ) {
			xrtAVLTreeDestroy(pTree);
			return 21;
		}
		/* 迭代族：Begin 升序 10,20,30；RFrom(20) 降序 20,10。 */
		iSeen = 0;
		if ( !xrtAVLTreeIterBegin(pTree, &Iter) ) {
			xrtAVLTreeDestroy(pTree);
			return 22;
		}
		while ( xrtAVLTreeIterNext(&Iter) != NULL ) {
			++iSeen;
		}
		xrtAVLTreeIterEnd(&Iter);
		if ( iSeen != 3u ) {
			xrtAVLTreeDestroy(pTree);
			return 23;
		}
		Probe = 20;
		iSeen = 0;
		if ( !xrtAVLTreeIterRFrom(pTree, &Probe, &Iter) ) {
			xrtAVLTreeDestroy(pTree);
			return 24;
		}
		while ( (pItem = xrtAVLTreeIterNext(&Iter)) != NULL ) {
			++iSeen;
		}
		xrtAVLTreeIterEnd(&Iter);
		if ( iSeen != 2u ) {
			xrtAVLTreeDestroy(pTree);
			return 25;
		}
		/* IterRBegin 空转一遍覆盖接口。 */
		if ( xrtAVLTreeIterRBegin(pTree, &Iter) ) {
			xrtAVLTreeIterEnd(&Iter);
		}
		/* Take：移出 20（所有权转移给调用方）。 */
		Probe = 20;
		if ( !xrtAVLTreeTake(pTree, &Probe, &Value) ||
			(Value != 20) || xrtAVLTreeHas(pTree, &Probe) ) {
			xrtAVLTreeDestroy(pTree);
			return 26;
		}
		/* Remove：删 10。 */
		Probe = 10;
		if ( !xrtAVLTreeRemove(pTree, &Probe) ||
			(xrtAVLTreeCount(pTree) != 1u) ) {
			xrtAVLTreeDestroy(pTree);
			return 27;
		}
		printf("avl: owning tree 22 APIs = ok (drop=%zu)",
			iDropped);
		/* Clear 触发剩余对象的 Drop → 累计 2（Take 的不计）。 */
		xrtAVLTreeClear(pTree);
		printf(" after-clear drop=%zu", iDropped);
		if ( (iDropped != 2u) || (xrtAVLTreeCount(pTree) != 0u) ) {
			xrtAVLTreeDestroy(pTree);
			return 28;
		}
		printf("\n");
		/* Destroy 在 Drop 后再释放树本体（Drop 不再触发）。 */
		xrtAVLTreeDestroy(pTree);
		if ( iDropped != 2u ) {
			return 29;
		}
	}

	/* ---- 内嵌 + 对齐变体：InitAligned / Unit ---- */
	{
		xavltree Embedded;

		if ( !xrtAVLTreeInitAligned(&Embedded, sizeof(int),
			sizeof(int), exampleCompareOwned, NULL) ) {
			return 30;
		}
		Probe = 7;
		if ( xrtAVLTreeAdd(&Embedded, &Probe, &Probe, NULL) ==
			NULL ) {
			return 30;
		}
		/* CreateAligned 堆变体（对齐必须整除元素大小）。 */
		{
			xavltree* pAligned = xrtAVLTreeCreateAligned(
				sizeof(int), sizeof(int),
				exampleCompareOwned, NULL);

			if ( pAligned == NULL ) {
				return 31;
			}
			xrtAVLTreeDestroy(pAligned);
		}
		xrtAVLTreeUnit(&Embedded);
	}
	return 0;
}
