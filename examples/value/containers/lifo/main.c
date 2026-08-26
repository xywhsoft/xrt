#include <stdio.h>
#include <xrt.h>



/* 释放示例资源并输出可观察的析构顺序。 */
static void exampleResourceDrop(ptr pHandle, ptr pUserData)
{
	(void)pUserData;
	printf("drop resource %d\n", *(int*)pHandle);
	xrtFree(pHandle);
}



/* 创建一个由 Value 独占管理的示例资源。 */
static xvalue* exampleResource(int iValue)
{
	static const xvaluehandleops tOps = {
		NULL,
		exampleResourceDrop,
		NULL,
		NULL
	};
	ptr pHandle = xrtMalloc(sizeof(int));
	xvalue* pValue;

	if ( pHandle == NULL ) {
		return NULL;
	}
	*(int*)pHandle = iValue;
	pValue = xrtValueHandleTake(&pHandle, &tOps, NULL);
	if ( pValue == NULL ) {
		xrtFree(pHandle);
	}
	return pValue;
}



/* 保持字段正向访问，并在对象销毁时按构造逆序释放资源。 */
int main(void)
{
	xvalue* pObject = xrtValueObjectLifo();
	xvalue* pFirst = exampleResource(1);
	xvalue* pSecond = exampleResource(2);
	xstrview Key = { 0 };
	int iResult = 1;

	if ( (pObject == NULL) || (pFirst == NULL) || (pSecond == NULL) ||
		 !xrtValueObjectSetTake(
			pObject,
			XRT_STR_LITERAL("first"),
			&pFirst
		 ) ||
		 !xrtValueObjectSetTake(
			pObject,
			XRT_STR_LITERAL("second"),
			&pSecond
		 ) ||
		 (xrtValueObjectAt(pObject, 0, &Key) == NULL) ) {
		goto cleanup;
	}
	printf("first field: %.*s\n", (int)Key.Size, Key.Data);
	iResult = 0;

cleanup:
	xrtValueRelease(pSecond);
	xrtValueRelease(pFirst);
	xrtValueRelease(pObject);
	return iResult;
}
