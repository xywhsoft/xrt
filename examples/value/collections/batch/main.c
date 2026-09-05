#include <stdio.h>
#include <xrt.h>



/*
 * 范例：value/collections/batch —— 批量组合：数组连接与映射合并
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtValueArrayExtend     就地并入另一数组（源保留）
 *   xrtValueArrayConcat     连接为新数组（双方都不动）
 *   xrtValueIntMapSetNew    整数键写入（接管值）
 *   xrtValueIntMapMerge + XVALUE_MERGE_REPLACE   冲突覆盖合并
 * 模块宏：XRT_MODULE_VALUE
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/value/collections/batch/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   array=3 map=2 timeout=5
 *
 * Extend 后 Left=[1,2]（Count=2），Concat 再拼 Right 得 3；
 *   合并后 overrides 的 timeout=5 覆盖 defaults 的 30——
 *   与 collections 范例的对象版对称的整数键批量路径。
 */


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
