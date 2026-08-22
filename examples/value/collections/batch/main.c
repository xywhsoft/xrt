#include <stdio.h>
#include <xrt.h>



/* 演示数组批量连接和整数键映射冲突策略。 */
int main(void)
{
	xvalue* pLeft = xrtValueArray();
	xvalue* pRight = xrtValueArray();
	xvalue* pJoined = NULL;
	xvalue* pDefaults = xrtValueIntMap();
	xvalue* pOverrides = xrtValueIntMap();
	int64 iValue = 0;
	int iResult = 1;

	if ( (pLeft == NULL) || (pRight == NULL) ||
		 (pDefaults == NULL) || (pOverrides == NULL) ||
		 !xrtValueArrayAppendNew(pLeft, xrtValueInt(1)) ||
		 !xrtValueArrayAppendNew(pRight, xrtValueInt(2)) ||
		 !xrtValueArrayExtend(pLeft, pRight) ) {
		goto cleanup;
	}
	pJoined = xrtValueArrayConcat(pLeft, pRight);
	if ( (pJoined == NULL) || (xrtValueCount(pJoined) != 3) ||
		 !xrtValueIntMapSetNew(pDefaults, 1, xrtValueInt(30)) ||
		 !xrtValueIntMapSetNew(pOverrides, 1, xrtValueInt(5)) ||
		 !xrtValueIntMapSetNew(pOverrides, 2, xrtValueInt(8)) ||
		 !xrtValueIntMapMerge(
			pDefaults,
			pOverrides,
			XVALUE_MERGE_REPLACE
		 ) ||
		 !xrtValueGetInt(xrtValueIntMapGet(pDefaults, 1), &iValue) ) {
		goto cleanup;
	}
	printf(
		"array=%zu map=%zu timeout=%lld\n",
		xrtValueCount(pJoined),
		xrtValueCount(pDefaults),
		(long long)iValue
	);
	iResult = 0;

cleanup:
	xrtValueRelease(pOverrides);
	xrtValueRelease(pDefaults);
	xrtValueRelease(pJoined);
	xrtValueRelease(pRight);
	xrtValueRelease(pLeft);
	return iResult;
}
