#include <stdio.h>
#include <xrt.h>



/* 展示深克隆保留 DAG 身份，同时与来源图完全隔离。 */
int main(void)
{
	xvalue* pRoot = xrtValueArray();
	xvalue* pChild = xrtValueObject();
	xvalue* pCopy;
	xvalue* pCopyChild;
	int64 iSource = 0;
	int64 iCopy = 0;

	if ( (pRoot == NULL) || (pChild == NULL) ||
		 !xrtValueObjectSetNew(
			pChild,
			XRT_STR_LITERAL("count"),
			xrtValueInt(1)
		 ) ||
		 !xrtValueArrayAppend(pRoot, pChild) ||
		 !xrtValueArrayAppend(pRoot, pChild) ) {
		xrtValueRelease(pChild);
		xrtValueRelease(pRoot);
		return 1;
	}
	pCopy = xrtValueDeepClone(pRoot);
	if ( (pCopy == NULL) ||
		 !xrtValueEqual(pRoot, pCopy) ||
		 (xrtValueArrayGet(pCopy, 0) !=
		  xrtValueArrayGet(pCopy, 1)) ) {
		xrtValueRelease(pCopy);
		xrtValueRelease(pChild);
		xrtValueRelease(pRoot);
		return 2;
	}
	pCopyChild = xrtValueArrayEdit(pCopy, 0);
	if ( (pCopyChild == NULL) ||
		 !xrtValueObjectSetNew(
			pCopyChild,
			XRT_STR_LITERAL("count"),
			xrtValueInt(2)
		 ) ||
		 !xrtValueGetInt(
			xrtValueObjectGet(
				pChild,
				XRT_STR_LITERAL("count")
			),
			&iSource
		 ) ||
		 !xrtValueGetInt(
			xrtValueObjectGet(
				pCopyChild,
				XRT_STR_LITERAL("count")
			),
			&iCopy
		 ) ) {
		xrtValueRelease(pCopy);
		xrtValueRelease(pChild);
		xrtValueRelease(pRoot);
		return 3;
	}
	printf(
		"equal=%d shared=%d source=%lld copy=%lld\n",
		xrtValueEqual(pRoot, pCopy) ? 1 : 0,
		xrtValueArrayGet(pCopy, 0) ==
			xrtValueArrayGet(pCopy, 1) ? 1 : 0,
		(long long)iSource,
		(long long)iCopy
	);
	xrtValueRelease(pCopy);
	xrtValueRelease(pChild);
	xrtValueRelease(pRoot);
	return 0;
}
