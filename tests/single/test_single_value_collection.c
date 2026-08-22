#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立提供 Value 批量操作和 Set 代数。 */
int main(void)
{
	xvalue* pLeft = xrtValueSet();
	xvalue* pRight = xrtValueSet();
	xvalue* pUnion = NULL;
	xvalue* pMerged = NULL;
	int iResult = 1;

	if ( (pLeft == NULL) || (pRight == NULL) ||
		 !xrtValueSetAddNew(pLeft, xrtValueInt(1)) ||
		 !xrtValueSetAddNew(pRight, xrtValueInt(2)) ||
		 !xrtValueSetIsDisjoint(pLeft, pRight) ) {
		goto cleanup;
	}
	pUnion = xrtValueSetUnion(pLeft, pRight);
	pMerged = xrtValueClone(pLeft);
	if ( (pUnion == NULL) || (pMerged == NULL) ||
		 !xrtValueSetMerge(pMerged, pRight) ||
		 !xrtValueSetEqual(pMerged, pUnion) ||
		 (xrtValueCount(pUnion) != 2) ) {
		goto cleanup;
	}
	iResult = 0;

cleanup:
	xrtValueRelease(pMerged);
	xrtValueRelease(pUnion);
	xrtValueRelease(pRight);
	xrtValueRelease(pLeft);
	return iResult;
}
