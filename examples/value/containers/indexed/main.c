#include <stdio.h>
#include <xrt.h>



/* 读取示例整数值。 */
static int64 exampleValueInt(const xvalue* pValue)
{
	int64 iValue = 0;

	(void)xrtValueGetInt(pValue, &iValue);
	return iValue;
}



/* 演示稀疏 IntMap、规范 Set 和 Take 所有权。 */
int main(void)
{
	xvalue* pMap = xrtValueIntMap();
	xvalue* pSet = xrtValueSet();
	xvalue* pTaken;
	xvalue* pQuery = xrtValueFloat(42.0);
	int iResult = 1;

	if ( (pMap == NULL) || (pSet == NULL) || (pQuery == NULL) ||
		 !xrtValueIntMapSetNew(pMap, -7, xrtValueInt(70)) ||
		 !xrtValueIntMapSetNew(pMap, 9, xrtValueInt(90)) ||
		 !xrtValueSetAddNew(pSet, xrtValueInt(42)) ||
		 !xrtValueSetAdd(pSet, pQuery) ) {
		goto cleanup;
	}

	pTaken = xrtValueIntMapTake(pMap, -7);
	if ( (pTaken == NULL) || (exampleValueInt(pTaken) != 70) ||
		 (xrtValueCount(pSet) != 1) ||
		 !xrtValueSetHas(pSet, pQuery) ) {
		xrtValueRelease(pTaken);
		goto cleanup;
	}
	printf(
		"taken=%lld, set-count=%zu\n",
		(long long)exampleValueInt(pTaken),
		xrtValueCount(pSet)
	);
	xrtValueRelease(pTaken);
	iResult = 0;

cleanup:
	xrtValueRelease(pQuery);
	xrtValueRelease(pSet);
	xrtValueRelease(pMap);
	return iResult;
}
