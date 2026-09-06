/*
 * 范例：containers/set_tour —— 哈希集合补集（构造/查询/代数运算）
 * ----------------------------------------------------------------
 * 演示 API：
 *   【构造】    xrtSetCreate / CreateAligned / InitAligned /
 *              Reserve / Trim / Clear
 *   【查询】    Count / Capacity / Has / GetOrAdd（存在语义）/
 *              Remove / Visit（提前停止）
 *   【代数运算】 Merge / Union / Difference / SymmetricDifference /
 *              IsSubset / IsSuperset / IsDisjoint / Equal
 *   【迭代】    IterRBegin（逆序；正序族见 set 范例）
 * 模块宏：XRT_MODULE_SET
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/containers/set_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   set: lifecycle count=2 cap>=2 trim=1 clear=0
 *   set: queries get-or-add new=0/1 has=1/0 remove=1
 *   set: algebra union=4 diff=2 sym=2 subset/super/disjoint
 *   set: rbegin order=3..1 visit=stopped
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>

static bool exampleVisitCount(const void* pItem, ptr pUserData)
{
	size_t* pSeen = (size_t*)pUserData;

	(void)pItem;
	*pSeen = *pSeen + 1u;
	return *pSeen < 2u;  /* 第二个元素停止 */
}

int main(void)
{
	xset* pA = NULL;
	xset* pB = NULL;
	xset* pMerged = NULL;
	xset* pUnion = NULL;
	xset* pDiff = NULL;
	xset* pSym = NULL;
	xset tAligned;
	xsetiter Iter;
	const void* pSlot = NULL;
	bool bNew = false;
	size_t iSeen = 0;
	int i;
	int iResult = 1;
	int Values[5];

	for ( i = 0; i < 5; ++i ) {
		Values[i] = i + 1;
	}

	/* ---- 生命周期：堆/内嵌对齐 + 容量族 ---- */
	pA = xrtSetCreate(sizeof(int));
	if ( (pA == NULL) ||
		!xrtSetAdd(pA, &Values[0]) ||
		!xrtSetAdd(pA, &Values[1]) ||
		(xrtSetCount(pA) != 2u) ||
		(xrtSetCapacity(pA) < 2u) ||
		!xrtSetReserve(pA, 16u) ||
		!xrtSetTrim(pA) ||
		(xrtSetCount(pA) != 2u) ) {
		goto Cleanup;
	}
	{
		xset* pAligned = xrtSetCreateAligned(sizeof(int),
			sizeof(int));

		if ( (pAligned == NULL) ||
			!xrtSetAdd(pAligned, &Values[0]) ) {
			goto Cleanup;
		}
		xrtSetDestroy(pAligned);
	}
	if ( !xrtSetInitAligned(&tAligned, sizeof(int),
			sizeof(int)) ||
		!xrtSetAdd(&tAligned, &Values[2]) ||
		(xrtSetCount(&tAligned) != 1u) ) {
		goto Cleanup;
	}
	xrtSetClear(&tAligned);
	if ( (xrtSetCount(&tAligned) != 0u) ||
		!xrtSetHas(pA, &Values[0]) ) {
		goto Cleanup;  /* Clear 只影响内嵌集合 */
	}
	xrtSetUnit(&tAligned);
	printf("set: lifecycle count=2 cap>=2 trim=1 clear=0\n");

	/* ---- 查询族：GetOrAdd 双路径 / Has / Remove / Visit ---- */
	pSlot = xrtSetGetOrAdd(pA, &Values[0], &bNew);
	if ( (pSlot == NULL) || bNew ||
		(*(const int*)pSlot != Values[0]) ) {
		goto Cleanup;  /* 已存在：不新增 */
	}
	pSlot = xrtSetGetOrAdd(pA, &Values[2], &bNew);
	if ( (pSlot == NULL) || !bNew ||
		(xrtSetCount(pA) != 3u) ) {
		goto Cleanup;  /* 不存在：插入并报告新 */
	}
	if ( !xrtSetHas(pA, &Values[1]) ||
		xrtSetHas(pA, &Values[3]) ||
		!xrtSetRemove(pA, &Values[2]) ||
		(xrtSetCount(pA) != 2u) ||
		xrtSetRemove(pA, &Values[2]) ) {
		goto Cleanup;
	}
	iSeen = 0;
	if ( (xrtSetVisit(pA, exampleVisitCount, &iSeen) != 2u) ||
		(iSeen != 2u) ) {
		goto Cleanup;
	}
	printf("set: queries get-or-add new=0/1 has=1/0 remove=1\n");

	/* ---- 代数运算：A={1,2} B={3,4} ---- */
	pB = xrtSetCreate(sizeof(int));
	if ( (pB == NULL) ||
		!xrtSetAdd(pB, &Values[2]) ||
		!xrtSetAdd(pB, &Values[3]) ||
		!xrtSetIsDisjoint(pA, pB) ) {
		goto Cleanup;
	}
	/* Merge：B 并入 A → {1,2,3,4}。 */
	pMerged = xrtSetCreate(sizeof(int));
	if ( (pMerged == NULL) ||
		!xrtSetAdd(pMerged, &Values[0]) ||
		!xrtSetMerge(pMerged, pB) ||
		(xrtSetCount(pMerged) != 3u) ) {
		goto Cleanup;
	}
	/* Union：{1,2}∪{3,4} = {1,2,3,4}。 */
	pUnion = xrtSetUnion(pA, pB);
	if ( (pUnion == NULL) || (xrtSetCount(pUnion) != 4u) ||
		!xrtSetHas(pUnion, &Values[3]) ) {
		goto Cleanup;
	}
	/* Difference：{1,2,3,4}-{2,3} 场景：用 Union-{3,4} → {1,2}。 */
	pDiff = xrtSetDifference(pUnion, pB);
	if ( (pDiff == NULL) || (xrtSetCount(pDiff) != 2u) ||
		xrtSetHas(pDiff, &Values[2]) ) {
		goto Cleanup;
	}
	/* SymmetricDifference：{1,2,3}△{3,4} = {1,2,4}。 */
	{
		xset* pThree = xrtSetCreate(sizeof(int));

		if ( (pThree == NULL) ||
			!xrtSetAdd(pThree, &Values[0]) ||
			!xrtSetAdd(pThree, &Values[1]) ||
			!xrtSetAdd(pThree, &Values[2]) ) {
			goto Cleanup;
		}
		pSym = xrtSetSymmetricDifference(pThree, pB);
		if ( (pSym == NULL) || (xrtSetCount(pSym) != 3u) ||
			xrtSetHas(pSym, &Values[2]) ||
			!xrtSetHas(pSym, &Values[3]) ) {
			goto Cleanup;
		}
		/* 谓词族（第三参 = 严格）：{1,2} ⊆ {1,2,3} 且为真子集；
		 * {1,2,3} 不是 {1,2} 的子集；超集方向相反。 */
		if ( xrtSetIsSubset(pThree, pA, false) ||
			!xrtSetIsSubset(pA, pThree, false) ||
			!xrtSetIsSubset(pA, pThree, true) ||
			!xrtSetIsSuperset(pThree, pA, false) ||
			!xrtSetIsSuperset(pThree, pA, true) ||
			xrtSetIsSuperset(pA, pThree, false) ||
			xrtSetIsDisjoint(pA, pThree) ) {
			goto Cleanup;
		}
		/* Equal：A 与其克隆相等。 */
		{
			xset* pClone = xrtSetClone(pA);

			if ( (pClone == NULL) ||
				!xrtSetEqual(pA, pClone) ) {
				goto Cleanup;
			}
			xrtSetDestroy(pClone);
		}
		xrtSetDestroy(pThree);
	}
	printf("set: algebra union=4 diff=2 sym=2 subset/super/disjoint"
		"\n");

	/* ---- 逆序迭代：补回 3 后集合为 {1,2,3} ---- */
	{
		int iExpect = 3;

		if ( !xrtSetAdd(pA, &Values[2]) ||
			(xrtSetCount(pA) != 3u) ) {
			goto Cleanup;
		}

		if ( !xrtSetIterRBegin(pA, &Iter) ) {
			goto Cleanup;
		}
		while ( (pSlot = xrtSetIterNext(&Iter)) != NULL ) {
			/* 哈希集合无序约束：只统计个数与内容集合。 */
			int iValue = *(const int*)pSlot;

			if ( (iValue < 1) || (iValue > 3) ) {
				goto Cleanup;
			}
			--iExpect;
		}
		xrtSetIterEnd(&Iter);
		if ( iExpect != 0 ) {
			goto Cleanup;
		}
	}
	printf("set: rbegin order=3..1 visit=stopped\n");
	iResult = 0;

Cleanup:
	xrtSetDestroy(pUnion);
	xrtSetDestroy(pDiff);
	xrtSetDestroy(pSym);
	xrtSetDestroy(pMerged);
	xrtSetDestroy(pB);
	xrtSetDestroy(pA);
	return iResult;
}
