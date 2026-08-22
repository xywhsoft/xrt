#include <xruntime.h>

#include <stdio.h>



/* 展示类型树的有序插入、下界查询和规范键借用。 */
int main(void)
{
	xtypedtree* pTree = xrtTypedTreeCreate(
		xrtTypeInt32(), xrtTypeInt64()
	);
	int32 iFirstKey = 30;
	int32 iSecondKey = 10;
	int32 iSearchKey = 20;
	int64 iFirstValue = 300;
	int64 iSecondValue = 100;
	const void* pStoredKey;
	const int64* pValue;

	if ( (pTree == NULL) ||
		 !xrtTypedTreeSet(pTree, &iFirstKey, &iFirstValue) ||
		 !xrtTypedTreeSet(pTree, &iSecondKey, &iSecondValue) ) {
		xrtTypedTreeDestroy(pTree);
		return 1;
	}
	pValue = (const int64*)xrtTypedTreeLowerBound(
		pTree, &iSearchKey, &pStoredKey
	);
	if ( pValue == NULL ) {
		xrtTypedTreeDestroy(pTree);
		return 2;
	}
	printf(
		"key=%d value=%lld count=%zu\n",
		*(const int32*)pStoredKey,
		(long long)*pValue,
		xrtTypedTreeCount(pTree)
	);
	xrtTypedTreeDestroy(pTree);
	return 0;
}
