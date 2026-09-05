#include <stdio.h>
#include <xrt.h>



/* 读取示例整数值。 */
static int64 exampleValueInt(const xvalue* pValue)
{
	int64 iValue = 0;

	(void)xrtValueGetInt(pValue, &iValue);
	return iValue;
}



/*
 * 范例：value/containers/indexed —— 稀疏 IntMap、规范 Set 与 Take
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtValueIntMap       负数/稀疏整数键映射
 *   xrtValueSet          数值语义规范化集合
 *   xrtValueTake         移出元素并取得所有权
 * 模块宏：XRT_MODULE_VALUE
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/value/containers/indexed/main.c -lws2_32 -liphlpapi
 * 预期输出：（见运行，键含负数与稀疏位）
 *
 * Set 的数值规范化：int 70 与 float 42.0 这类跨类型
 *   数值在集合/相等语义下按值处理（ScalarEqual 口径）；
 *   Take 后原容器让出所有权（详见 ownership 范例）。
 */


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
