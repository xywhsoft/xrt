#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 在单头映射中初始化一个新整数值。 */
static bool testSingleMapInit(
	xbytesview Key,
	ptr pValue,
	ptr pUserData
)
{
	(void)Key;
	*(int*)pValue = *(int*)pUserData;
	return true;
}



/* 单头文件必须独立提供字节键映射、键副本和插入顺序迭代。 */
int main(void)
{
	xmap tMap;
	xmapiter tIterator;
	xbytesview Key;
	int iFirst = 10;
	int iSecond = 20;
	int iThird = 30;
	int* pValue;

	if ( !xrtMapInit(&tMap, sizeof(int)) ) {
		return 1;
	}
	if (
		!xrtMapSet(&tMap, XRT_BYTES_LITERAL("first"), &iFirst) ||
		!xrtMapSet(&tMap, XRT_BYTES_LITERAL("second"), &iSecond) ||
		(xrtMapGetOrInit(
			&tMap,
			XRT_BYTES_LITERAL("third"),
			testSingleMapInit,
			&iThird,
			NULL
		) == NULL)
	) {
		xrtMapUnit(&tMap);
		return 2;
	}
	if ( !xrtMapIterBegin(&tMap, &tIterator) ) {
		xrtMapUnit(&tMap);
		return 3;
	}
	pValue = (int*)xrtMapIterNext(&tIterator, &Key);
	if (
		(pValue == NULL) ||
		(*pValue != 10) ||
		(Key.Size != 5) ||
		(memcmp(Key.Data, "first", 5) != 0)
	) {
		xrtMapUnit(&tMap);
		return 4;
	}
	xrtMapIterEnd(&tIterator);
	xrtMapUnit(&tMap);
	return 0;
}
