#include <stdio.h>
#include <xrt.h>

/*
 * 范例：value/handle —— 句柄值：外部资源的可哈希一等封装
 * ----------------------------------------------------------------
 * 演示 API：
 *   xvaluehandleops            四回调：Drop（析构）/Hash/Equal/Clone
 *   xrtValueHandleTake         接管句柄指针为值（来源槽被清空）
 *   xrtValueGetHandle          借用读取：句柄 + ops 表 + 用户数据
 *   xrtValueHash / ScalarEqual 句柄也参与哈希与相等（走 ops 回调）
 * 模块宏：XRT_MODULE_VALUE
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c  *       examples/value/handle/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   handle value: 77, hash: 3966019471203017175
 *
 * 句柄值的价值：把 FILE 指针、fd、数据库连接等 C 资源放进 value 体系——
 *   可进容器、可比较、可哈希；Drop 回调保证值释放时资源不泄漏。
 * 两个 77 的句柄经 ops.Equal 判相等、同哈希——
 *   句柄值语义完全由 ops 表定义（本例按指向的整数）。
 */





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
