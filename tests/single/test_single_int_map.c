#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立提供整数映射的完整生命周期。 */
int main(void)
{
	xintmap tMap;
	int iValue = 42;
	int64 iKey;

	if ( !xrtIntMapInit(&tMap, sizeof(int)) ) {
		return 1;
	}
	if ( !xrtIntMapSet(&tMap, -7, &iValue) ) {
		xrtIntMapUnit(&tMap);
		return 2;
	}
	if ( (*(int*)xrtIntMapGet(&tMap, -7) != 42) || (xrtIntMapCount(&tMap) != 1) ) {
		xrtIntMapUnit(&tMap);
		return 3;
	}
	if ( (xrtIntMapFirst(&tMap, &iKey) == NULL) || (iKey != -7) ) {
		xrtIntMapUnit(&tMap);
		return 4;
	}
	xrtIntMapUnit(&tMap);
	return 0;
}
