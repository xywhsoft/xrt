#define XRUNTIME_IMPLEMENTATION
#include "../../single/xruntime.h"



/* 验证单头文件中的类型数组生命周期路径。 */
int main(void)
{
	xtypedarray Array;
	xtypedarray* pClone;
	xtypedarray* pConcat;
	int64 iValue = 41;
	int64 iOutput = 0;
	int iResult;

	if ( !xrtTypedArrayInit(&Array, xrtTypeInt64()) ) {
		return 1;
	}
	pClone = NULL;
	pConcat = NULL;
	iResult = (!xrtTypedArrayPush(&Array, &iValue) ||
		((pClone = xrtTypedArrayClone(&Array)) == NULL) ||
		((pConcat = xrtTypedArrayConcat(&Array, pClone)) == NULL) ||
		!xrtTypedArrayEquals(&Array, pClone) ||
		(xrtTypedArrayCount(pConcat) != 2u) ||
		!xrtTypedArrayPop(&Array, &iOutput) ||
		(iOutput != 41)) ? 2 : 0;
	xrtTypedArrayDestroy(pConcat);
	xrtTypedArrayDestroy(pClone);
	xrtTypedArrayUnit(&Array);
	return iResult;
}
