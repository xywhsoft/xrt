#include "../test.h"
#include "../../src/internal/xrt_slot_map.h"



/* 白盒验证代际耗尽后槽位退役，不会回绕并重新命中旧句柄。 */
static void testSlotGenerationRetirement(void)
{
	xslotmap tMap;
	xslotentry* pEntry;
	int iFirst = 1;
	int iSecond = 2;
	xslot Initial;
	xslot Maximum;
	xslot Next;

	testRequire(xrtSlotMapInit(&tMap), "retired slot map init failed");
	Initial = xrtSlotMapInsert(&tMap, &iFirst);
	testRequire(Initial != XRT_SLOT_INVALID, "retired slot setup failed");
	pEntry = (xslotentry*)tMap.Storage.Data;
	pEntry->Generation = XRT_SLOT_GENERATION_MAX;
	Maximum = (
		((xslot)XRT_SLOT_GENERATION_MAX << 32u) |
		((xslot)xrtSlotIndex(Initial) + 1u)
	);

	testRequire(
		xrtSlotMapRemove(&tMap, Maximum, NULL),
		"maximum generation slot remove failed"
	);
	testRequire(
		(pEntry->Generation == 0) &&
		(tMap.FreeSlot == XRT_SLOT_INDEX_INVALID),
		"maximum generation slot was not retired"
	);
	Next = xrtSlotMapInsert(&tMap, &iSecond);
	testRequire(Next != XRT_SLOT_INVALID, "insert after retired slot failed");
	testRequire(xrtSlotIndex(Next) == 1, "retired slot index was reused");
	testRequire(!xrtSlotMapContains(&tMap, Maximum), "retired maximum handle became valid");
	testRequire(!xrtSlotMapContains(&tMap, Initial), "retired initial handle became valid");
	xrtSlotMapUnit(&tMap);
}



/* 验证代际句柄、常数时间槽复用和外置迭代器合同。 */
int main(void)
{
	xslotmap tMap;
	xslotmap* pCreated;
	xslotmapiter tIterator;
	int pValues[] = { 10, 20, 30, 40, 50, 60, 70, 80 };
	xslot SlotA;
	xslot SlotB;
	xslot SlotC;
	xslot SlotD;
	xslot SlotE;
	xslot SlotF;
	xslot IterSlot;
	ptr pRemoved = NULL;
	ptr pValue;
	size_t iSpan;

	testRequire(xrtSlotIndex(XRT_SLOT_INVALID) == XRT_SLOT_INDEX_INVALID, "invalid slot index mismatch");
	testRequire(xrtSlotGeneration(XRT_SLOT_INVALID) == 0, "invalid slot generation mismatch");
	testRequire(xrtSlotMapInit(&tMap), "slot map init failed");
	testRequire(xrtSlotMapReserve(&tMap, 4), "slot map reserve failed");
	testRequire((tMap.Count == 0) && (tMap.Storage.Count == 0), "slot map initial state mismatch");

	SlotA = xrtSlotMapInsert(&tMap, &pValues[0]);
	SlotB = xrtSlotMapInsert(&tMap, &pValues[1]);
	SlotC = xrtSlotMapInsert(&tMap, &pValues[2]);
	testRequire(
		(SlotA != XRT_SLOT_INVALID) &&
		(SlotB != XRT_SLOT_INVALID) &&
		(SlotC != XRT_SLOT_INVALID),
		"slot map insert failed"
	);
	testRequire(xrtSlotIndex(SlotA) == 0, "first slot index mismatch");
	testRequire(xrtSlotIndex(SlotB) == 1, "second slot index mismatch");
	testRequire(xrtSlotGeneration(SlotB) == 1, "initial slot generation mismatch");
	testRequire(xrtSlotMapContains(&tMap, SlotB), "slot map contains failed");
	testRequire(xrtSlotMapGet(&tMap, SlotC) == &pValues[2], "slot map get mismatch");

	testRequire(xrtSlotMapSet(&tMap, SlotB, &pValues[3]), "slot map set failed");
	testRequire(xrtSlotMapGet(&tMap, SlotB) == &pValues[3], "slot map set value mismatch");
	testRequire(xrtSlotMapRemove(&tMap, SlotB, &pRemoved), "slot map remove failed");
	testRequire(pRemoved == &pValues[3], "slot map removed value mismatch");
	testRequire(!xrtSlotMapContains(&tMap, SlotB), "removed slot remained valid");
	xrtClearError();
	testRequire(xrtSlotMapGet(&tMap, SlotB) == NULL, "stale slot get should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_RANGE, "stale slot error mismatch");

	SlotD = xrtSlotMapInsert(&tMap, &pValues[4]);
	testRequire(xrtSlotIndex(SlotD) == xrtSlotIndex(SlotB), "released slot was not reused");
	testRequire(SlotD != SlotB, "reused slot retained stale handle");
	testRequire(
		xrtSlotGeneration(SlotD) == (xrtSlotGeneration(SlotB) + 1u),
		"reused slot generation mismatch"
	);
	testRequire(!xrtSlotMapContains(&tMap, SlotB), "stale handle matched reused slot");

	/* 替换指针不改变槽结构，因此不会使正在进行的迭代失效。 */
	testRequire(xrtSlotMapIterBegin(&tMap, &tIterator), "slot iterator begin failed");
	testRequire(xrtSlotMapSet(&tMap, SlotD, &pValues[5]), "slot set during iteration failed");
	pValue = xrtSlotMapIterNext(&tIterator, &IterSlot);
	testRequire((pValue == &pValues[0]) && (IterSlot == SlotA), "slot iterator first mismatch");
	pValue = xrtSlotMapIterNext(&tIterator, &IterSlot);
	testRequire((pValue == &pValues[5]) && (IterSlot == SlotD), "slot iterator second mismatch");
	pValue = xrtSlotMapIterNext(&tIterator, &IterSlot);
	testRequire((pValue == &pValues[2]) && (IterSlot == SlotC), "slot iterator third mismatch");
	testRequire(xrtSlotMapIterNext(&tIterator, &IterSlot) == NULL, "slot iterator end mismatch");
	testRequire(IterSlot == XRT_SLOT_INVALID, "ended iterator returned a slot");

	/* 插入、删除和清空会推进版本并拒绝继续使用旧迭代器。 */
	testRequire(xrtSlotMapIterBegin(&tMap, &tIterator), "mutation iterator begin failed");
	testRequire(xrtSlotMapIterNext(&tIterator, NULL) == &pValues[0], "mutation iterator first mismatch");
	SlotE = xrtSlotMapInsert(&tMap, &pValues[6]);
	testRequire(SlotE != XRT_SLOT_INVALID, "mutation slot insert failed");
	xrtClearError();
	testRequire(xrtSlotMapIterNext(&tIterator, NULL) == NULL, "mutated iterator should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE, "mutated iterator error mismatch");

	/* 输出不能覆盖槽表自身的内部记录。 */
	xrtClearError();
	testRequire(
		!xrtSlotMapRemove(&tMap, SlotA, (ptr*)tMap.Storage.Data),
		"internal remove output alias should fail"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "remove alias error mismatch");
	testRequire(xrtSlotMapContains(&tMap, SlotA), "remove alias changed slot state");
	testRequire(xrtSlotMapRemove(&tMap, SlotA, NULL), "slot remove without output failed");

	/* 空值没有活动槽语义，避免 Get 的空值与陈旧句柄产生歧义。 */
	xrtClearError();
	testRequire(
		xrtSlotMapInsert(&tMap, NULL) == XRT_SLOT_INVALID,
		"NULL slot insert should fail"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "NULL insert error mismatch");
	xrtClearError();
	testRequire(!xrtSlotMapSet(&tMap, SlotD, NULL), "NULL slot set should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "NULL set error mismatch");
	xrtClearError();
	testRequire(!xrtSlotMapContains(&tMap, XRT_SLOT_INVALID), "invalid slot should not exist");
	testRequire(xrtGetError() == NULL, "contains reported invalid handle as an error");

	iSpan = tMap.Storage.Count;
	xrtSlotMapClear(&tMap);
	testRequire((tMap.Count == 0) && (tMap.Storage.Count == iSpan), "slot clear state mismatch");
	testRequire(!xrtSlotMapContains(&tMap, SlotC), "clear retained an old handle");
	testRequire(!xrtSlotMapContains(&tMap, SlotD), "clear retained a reused handle");
	testRequire(!xrtSlotMapContains(&tMap, SlotE), "clear retained a late handle");
	SlotF = xrtSlotMapInsert(&tMap, &pValues[7]);
	testRequire(SlotF != XRT_SLOT_INVALID, "insert after clear failed");
	testRequire(xrtSlotIndex(SlotF) == 0, "clear did not rebuild the lowest reusable slot");
	testRequire(!xrtSlotMapContains(&tMap, SlotA), "old handle matched after clear");

	#if SIZE_MAX > UINT32_MAX
		xrtClearError();
		testRequire(
			!xrtSlotMapReserve(&tMap, (size_t)UINT32_MAX + 1u),
			"oversized slot reserve should fail"
		);
		testRequire(
			xrtErrorKind(xrtGetError()) == XERR_RANGE,
			"oversized slot reserve error mismatch"
		);
	#endif

	xrtSlotMapUnit(&tMap);
	testRequire(
		(tMap.Storage.ItemSize == 0) &&
		(tMap.Count == 0) &&
		(tMap.Version == 0),
		"slot map unit state mismatch"
	);

	pCreated = xrtSlotMapCreate();
	testRequire(pCreated != NULL, "slot map create failed");
	testRequire(
		xrtSlotMapInsert(pCreated, &pValues[0]) != XRT_SLOT_INVALID,
		"created slot map insert failed"
	);
	xrtSlotMapDestroy(pCreated);
	testSlotGenerationRetirement();
	printf("[PASS] slot_map\n");
	return 0;
}
