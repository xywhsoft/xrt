#include <stdio.h>
#include <xrt.h>



/* 释放 Value 独占的整数句柄。 */
static void exampleHandleDrop(ptr pHandle, ptr pUserData)
{
	(void)pUserData;
	xrtFree(pHandle);
}



/* 按句柄指向的整数计算进程内哈希。 */
static uint64 exampleHandleHash(ptr pHandle, ptr pUserData)
{
	(void)pUserData;
	return (uint64)*(int*)pHandle;
}



/* 按句柄指向的整数判断相等。 */
static bool exampleHandleEqual(ptr pLeft, ptr pRight, ptr pUserData)
{
	(void)pUserData;
	return *(int*)pLeft == *(int*)pRight;
}



/* 演示句柄所有权接管、借用读取、哈希和标量相等。 */
int main(void)
{
	static const xvaluehandleops tOps = {
		NULL,
		exampleHandleDrop,
		exampleHandleHash,
		exampleHandleEqual
	};
	ptr pLeftHandle = xrtMalloc(sizeof(int));
	ptr pRightHandle = xrtMalloc(sizeof(int));
	xvalue* pLeft = NULL;
	xvalue* pRight = NULL;
	const xvaluehandleops* pReadOps;
	ptr pReadHandle;
	uint64 iHash;
	int iResult = 0;

	if ( (pLeftHandle == NULL) || (pRightHandle == NULL) ) {
		iResult = 1;
		goto cleanup;
	}
	*(int*)pLeftHandle = 77;
	*(int*)pRightHandle = 77;
	pLeft = xrtValueHandleTake(&pLeftHandle, &tOps, NULL);
	pRight = xrtValueHandleTake(&pRightHandle, &tOps, NULL);
	if ( (pLeft == NULL) || (pRight == NULL) ) {
		iResult = 2;
		goto cleanup;
	}
	if (
		!xrtValueGetHandle(pLeft, &pReadHandle, &pReadOps, NULL) ||
		(pReadOps != &tOps) ||
		(*(int*)pReadHandle != 77) ||
		!xrtValueHash(pLeft, &iHash) ||
		!xrtValueScalarEqual(pLeft, pRight)
	) {
		iResult = 3;
		goto cleanup;
	}
	printf("handle value: %d, hash: %llu\n", *(int*)pReadHandle, (unsigned long long)iHash);

cleanup:
	xrtValueRelease(pRight);
	xrtValueRelease(pLeft);
	xrtFree(pRightHandle);
	xrtFree(pLeftHandle);
	return iResult;
}
