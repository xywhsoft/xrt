#include <stdio.h>
#include <xrt.h>



/* 演示 Array、Object、负索引、嵌套 COW 和快照迭代。 */
int main(void)
{
	xvalue* pResponse = xrtValueObject();
	xvalue* pTags = xrtValueArray();
	xvalue* pCopy = NULL;
	xvalue* pMutableTags;
	xvalueiter tIterator;
	xvaluekey Key;
	xvalue* pItem;
	size_t iLast;
	int iResult = 1;

	if ( (pResponse == NULL) || (pTags == NULL) ||
		 !xrtValueArrayAppendNew(
			pTags,
			xrtValueString(XRT_STR_LITERAL("xrt"))
		 ) ||
		 !xrtValueArrayAppendNew(
			pTags,
			xrtValueString(XRT_STR_LITERAL("network"))
		 ) ||
		 !xrtValueObjectSetNew(
			pResponse,
			XRT_STR_LITERAL("code"),
			xrtValueInt(200)
		 ) ||
		 !xrtValueObjectSetTake(
			pResponse,
			XRT_STR_LITERAL("tags"),
			&pTags
		 ) ) {
		goto cleanup;
	}

	pCopy = xrtValueClone(pResponse);
	if ( pCopy == NULL ) {
		goto cleanup;
	}
	pMutableTags = xrtValueObjectEdit(
		pCopy,
		XRT_STR_LITERAL("tags")
	);
	if ( (pMutableTags == NULL) ||
		 !xrtValueArrayResolve(pMutableTags, -1, &iLast) ||
		 !xrtValueArraySetNew(
			pMutableTags,
			iLast,
			xrtValueString(XRT_STR_LITERAL("http"))
		 ) ) {
		goto cleanup;
	}

	if ( !xrtValueIterBegin(pCopy, &tIterator) ) {
		goto cleanup;
	}
	while ( (pItem = xrtValueIterNext(&tIterator, &Key)) != NULL ) {
		printf(
			"%.*s: %s\n",
			(int)Key.String.Size,
			Key.String.Data,
			xrtValueTypeName(xrtValueType(pItem))
		);
	}
	xrtValueIterEnd(&tIterator);
	iResult = 0;

cleanup:
	xrtValueRelease(pCopy);
	xrtValueRelease(pTags);
	xrtValueRelease(pResponse);
	return iResult;
}
