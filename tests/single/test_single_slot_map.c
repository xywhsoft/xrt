#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立提供代际槽句柄的插入、复用和陈旧检查。 */
int main(void)
{
	xslotmap tMap;
	int iFirst = 1;
	int iSecond = 2;
	xslot First;
	xslot Second;

	if ( !xrtSlotMapInit(&tMap) ) {
		return 1;
	}
	First = xrtSlotMapInsert(&tMap, &iFirst);
	if ( First == XRT_SLOT_INVALID ) {
		xrtSlotMapUnit(&tMap);
		return 2;
	}
	if ( !xrtSlotMapRemove(&tMap, First, NULL) ) {
		xrtSlotMapUnit(&tMap);
		return 3;
	}
	Second = xrtSlotMapInsert(&tMap, &iSecond);
	if (
		(Second == XRT_SLOT_INVALID) ||
		(Second == First) ||
		xrtSlotMapContains(&tMap, First) ||
		(xrtSlotMapGet(&tMap, Second) != &iSecond)
	) {
		xrtSlotMapUnit(&tMap);
		return 4;
	}
	xrtSlotMapUnit(&tMap);
	return 0;
}
