#include <stdio.h>
#include <xruntime.h>



/* 展示类型数组的值复制、查询和自动生命周期。 */
int main(void)
{
	xtypedarray Values;
	xtypedarray* pCopy;
	xtypedarray* pJoined;
	int64 arrInput[] = { 7, 11, 13 };
	int64 iNeedle = 11;

	if ( !xrtTypedArrayInit(&Values, xrtTypeInt64()) ) {
		return 1;
	}
	for ( size_t i = 0; i < sizeof(arrInput) / sizeof(arrInput[0]); i++ ) {
		if ( !xrtTypedArrayPush(&Values, &arrInput[i]) ) {
			xrtTypedArrayUnit(&Values);
			return 2;
		}
	}
	pCopy = xrtTypedArrayClone(&Values);
	pJoined = pCopy != NULL ? xrtTypedArrayConcat(&Values, pCopy) : NULL;
	if ( (pCopy == NULL) || (pJoined == NULL) ||
		 !xrtTypedArrayEquals(&Values, pCopy) ) {
		xrtTypedArrayDestroy(pJoined);
		xrtTypedArrayDestroy(pCopy);
		xrtTypedArrayUnit(&Values);
		return 3;
	}
	printf("count=%zu joined=%zu index=%zu\n",
		xrtTypedArrayCount(&Values),
		xrtTypedArrayCount(pJoined),
		xrtTypedArrayFind(&Values, &iNeedle));
	xrtTypedArrayDestroy(pJoined);
	xrtTypedArrayDestroy(pCopy);
	xrtTypedArrayUnit(&Values);
	return 0;
}
