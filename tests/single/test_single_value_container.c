#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立提供 COW、负索引解析与稳定迭代。 */
int main(void)
{
	xvalue* pArray = xrtValueArray();
	xvalue* pObject = xrtValueObject();
	xvalue* pCopy = NULL;
	xvalueiter tIterator;
	xvaluekey Key;
	xvalue* pItem;
	size_t iLast;
	int64 iValue;
	int iResult = 1;

	if ( (pArray == NULL) || (pObject == NULL) ||
		 !xrtValueArrayAppendNew(pArray, xrtValueInt(10)) ||
		 !xrtValueArrayAppendNew(pArray, xrtValueInt(20)) ||
		 !xrtValueArrayResolve(pArray, -1, &iLast) ||
		 (iLast != 1) ||
		 !xrtValueObjectSetNew(
			pObject,
			XRT_STR_LITERAL("code"),
			xrtValueInt(200)
		 ) ) {
		goto cleanup;
	}
	pCopy = xrtValueClone(pObject);
	if ( (pCopy == NULL) ||
		 !xrtValueObjectSetNew(
			pCopy,
			XRT_STR_LITERAL("ok"),
			xrtValueBool(true)
		 ) ||
		 xrtValueObjectHas(pObject, XRT_STR_LITERAL("ok")) ) {
		goto cleanup;
	}
	if ( !xrtValueIterBegin(pObject, &tIterator) ) {
		goto cleanup;
	}
	pItem = xrtValueIterNext(&tIterator, &Key);
	if ( (pItem != NULL) && xrtValueGetInt(pItem, &iValue) &&
		 (iValue == 200) && (Key.String.Size == 4) ) {
		iResult = 0;
	}
	xrtValueIterEnd(&tIterator);

cleanup:
	xrtValueRelease(pCopy);
	xrtValueRelease(pObject);
	xrtValueRelease(pArray);
	return iResult;
}
