#include <stdio.h>
#include <xrt.h>



/* 演示对象覆盖合并和保持稳定顺序的 Set 并集。 */
int main(void)
{
	xvalue* pDefaults = xrtValueObject();
	xvalue* pOptions = xrtValueObject();
	xvalue* pLeft = xrtValueSet();
	xvalue* pRight = xrtValueSet();
	xvalue* pUnion = NULL;
	xvalue* pMerged = NULL;
	int iResult = 1;

	if ( (pDefaults == NULL) || (pOptions == NULL) ||
		 (pLeft == NULL) || (pRight == NULL) ||
		 !xrtValueObjectSetNew(
			pDefaults,
			XRT_STR_LITERAL("timeout"),
			xrtValueInt(30)
		 ) ||
		 !xrtValueObjectSetNew(
			pOptions,
			XRT_STR_LITERAL("timeout"),
			xrtValueInt(5)
		 ) ||
		 !xrtValueObjectMerge(
			pDefaults,
			pOptions,
			XVALUE_MERGE_REPLACE
		 ) ||
		 !xrtValueSetAddNew(pLeft, xrtValueString(XRT_STR_LITERAL("read"))) ||
		 !xrtValueSetAddNew(pRight, xrtValueString(XRT_STR_LITERAL("write"))) ||
		 !xrtValueSetIsDisjoint(pLeft, pRight) ) {
		goto cleanup;
	}
	pUnion = xrtValueSetUnion(pLeft, pRight);
	pMerged = xrtValueClone(pLeft);
	if ( (pUnion == NULL) || (pMerged == NULL) ||
		 !xrtValueSetMerge(pMerged, pRight) ||
		 !xrtValueSetEqual(pMerged, pUnion) ) {
		goto cleanup;
	}
	printf(
		"options=%zu permissions=%zu\n",
		xrtValueCount(pDefaults),
		xrtValueCount(pUnion)
	);
	iResult = 0;

cleanup:
	xrtValueRelease(pMerged);
	xrtValueRelease(pUnion);
	xrtValueRelease(pRight);
	xrtValueRelease(pLeft);
	xrtValueRelease(pOptions);
	xrtValueRelease(pDefaults);
	return iResult;
}
