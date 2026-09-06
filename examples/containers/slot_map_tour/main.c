/*
 * 范例：containers/slot_map_tour —— 槽位表补集（代际/堆形态/迭代/改值）
 * ----------------------------------------------------------------
 * 演示 API：
 *   【生命周期】  xrtSlotMapCreate / Destroy / Clear / Reserve
 *   【访问】      xrtSlotMapGet（代际不符返回空）/ Set（原地改值）
 *   【代际】      xrtSlotGeneration（句柄分解）
 *   【迭代器】    xrtSlotMapIterBegin / IterNext / IterEnd
 * 模块宏：XRT_MODULE_SLOT_MAP
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/containers/slot_map_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   slot: gen(index=0 gen=1) create/get/set ok
 *   slot: stale-after-remove=1 clear-get=0 iter pairs=2
 */

#include <stdio.h>
#include <stdint.h>
#include <xrt.h>

int main(void)
{
	xslotmap* pMap = NULL;
	xslotmapiter Iter;
	xslot SlotA;
	xslot SlotB;
	xslot Stale;
	ptr pValue = NULL;
	int iPairs = 0;
	int iResult = 1;

	/* ---- 堆形态 + 插入/读取/改值 ---- */
	pMap = xrtSlotMapCreate();
	if ( (pMap == NULL) ||
		!xrtSlotMapReserve(pMap, 8u) ) {
		goto Cleanup;
	}
	SlotA = xrtSlotMapInsert(pMap, (ptr)1);
	SlotB = xrtSlotMapInsert(pMap, (ptr)2);
	if ( (SlotA == 0u) || (SlotB == 0u) ||
		(xrtSlotMapGet(pMap, SlotA) != (ptr)1) ||
		(xrtSlotMapGet(pMap, SlotB) != (ptr)2) ||
		!xrtSlotMapSet(pMap, SlotA, (ptr)11) ||
		(xrtSlotMapGet(pMap, SlotA) != (ptr)11) ) {
		goto Cleanup;
	}
	/* 代际分解：索引 0/1，代际从 1 起。 */
	if ( (xrtSlotIndex(SlotA) != 0u) ||
		(xrtSlotGeneration(SlotA) != 1u) ||
		(xrtSlotIndex(SlotB) != 1u) ) {
		goto Cleanup;
	}
	printf("slot: gen(index=0 gen=1) create/get/set ok\n");

	/* ---- 删除 → 陈旧句柄 ---- */
	/* Remove 回传被删值，包括最近一次 Set 更新后的值。 */
	if ( !xrtSlotMapRemove(pMap, SlotA, &pValue) ||
		(pValue != (ptr)11) ) {
		goto Cleanup;
	}
	Stale = SlotA;
	if ( xrtSlotMapGet(pMap, Stale) != NULL ||
		xrtSlotMapContains(pMap, Stale) ||
		xrtSlotMapRemove(pMap, Stale, NULL) ) {
		goto Cleanup;
	}
	/* 同索引新句柄：代际已递增。 */
	{
		xslot SlotC = xrtSlotMapInsert(pMap, (ptr)3);

		if ( (SlotC == Stale) ||
			(xrtSlotIndex(SlotC) != xrtSlotIndex(Stale)) ||
			(xrtSlotGeneration(SlotC) <=
				xrtSlotGeneration(Stale)) ||
			(xrtSlotMapGet(pMap, SlotC) != (ptr)3) ) {
			goto Cleanup;
		}
	}
	printf("slot: stale-after-remove=1 ");

	/* ---- Clear + 迭代器 ---- */
	xrtSlotMapClear(pMap);
	if ( xrtSlotMapGet(pMap, SlotB) != NULL ||
		xrtSlotMapContains(pMap, SlotB) ) {
		goto Cleanup;
	}
	printf("clear-get=0 ");
	(void)xrtSlotMapInsert(pMap, (ptr)100);
	(void)xrtSlotMapInsert(pMap, (ptr)200);
	if ( !xrtSlotMapIterBegin(pMap, &Iter) ) {
		goto Cleanup;
	}
	while ( (pValue = xrtSlotMapIterNext(&Iter, &SlotA)) !=
		NULL ) {
		if ( ((uintptr_t)pValue != 100u) &&
			((uintptr_t)pValue != 200u) ) {
			goto Cleanup;
		}
		++iPairs;
	}
	xrtSlotMapIterEnd(&Iter);
	if ( iPairs != 2 ) {
		goto Cleanup;
	}
	printf("iter pairs=%d\n", iPairs);
	iResult = 0;

Cleanup:
	xrtSlotMapDestroy(pMap);
	return iResult;
}
