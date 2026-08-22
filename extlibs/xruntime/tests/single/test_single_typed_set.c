#define XRUNTIME_IMPLEMENTATION
#include "../../single/xruntime.h"



/* 验证单头文件中的类型集合唯一值和集合代数路径。 */
int main(void)
{
	xtypedset Left;
	xtypedset Right;
	xtypedset* pUnion;
	int64 iOne = 1;
	int64 iTwo = 2;
	int64 iOutput = 0;
	int iResult;

	if ( !xrtTypedSetInit(&Left, xrtTypeInt64()) ||
		 !xrtTypedSetInit(&Right, xrtTypeInt64()) ) {
		return 1;
	}
	if ( !xrtTypedSetAdd(&Left, &iOne) ||
		 !xrtTypedSetAdd(&Left, &iOne) ||
		 !xrtTypedSetAdd(&Right, &iTwo) ) {
		xrtTypedSetUnit(&Right);
		xrtTypedSetUnit(&Left);
		return 2;
	}
	pUnion = xrtTypedSetUnion(&Left, &Right);
	iResult = ((pUnion == NULL) ||
		(xrtTypedSetCount(pUnion) != 2u) ||
		!xrtTypedSetTake(pUnion, &iTwo, &iOutput) ||
		(iOutput != 2)) ? 3 : 0;
	xrtTypedSetDestroy(pUnion);
	xrtTypedSetUnit(&Right);
	xrtTypedSetUnit(&Left);
	return iResult;
}
