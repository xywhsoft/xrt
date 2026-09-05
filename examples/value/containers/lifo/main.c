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



/*
 * 范例：value/containers/lifo —— LIFO 对象：构造序访问、逆序析构
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtValueObjectLifo     保持首次插入顺序、逆序释放拥有值
 *   xrtValueHandleTake     把示例资源包成句柄值（带 Drop 回调）
 *   xrtValueObjectSetTake  字段写入（移交值所有权）
 * 模块宏：XRT_MODULE_VALUE
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/value/containers/lifo/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   first field: first
 *   drop resource 2
 *   drop resource 1
 *
 * "字段正向访问 + 资源逆序释放"的组合正是栈式资源管理：
 *   后构造的资源可能引用先构造的（如连接引用池），
 *   逆序拆除保证被引用者后死。HandleTake 的 Drop 回调
 *   让析构顺序可打印观察——这是 containers/graph 的
 *   DAG 身份与 map 插入序之外的第三种顺序语义。
 */


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
