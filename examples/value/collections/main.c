#include <stdio.h>
#include <xrt.h>

/*
 * 范例：value/collections —— 对象合并与集合并集：配置层两大件
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtValueObjectMerge + XVALUE_MERGE_REPLACE
 *                        对象覆盖合并（后者覆盖前者同键）
 *   xrtValueSetUnion     两集合并为新集合
 *   xrtValueSetMerge     并集就地版（并入目标）
 *   xrtValueSetIsDisjoint / SetEqual   不相交 / 相等判断
 *   xrtValueClone        浅克隆（共享元素，见 ownership 范例对比）
 *   xrtValueCount        元素计数
 * 模块宏：XRT_MODULE_VALUE
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c  *       examples/value/collections/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   options=1 permissions=2
 *
 * 场景对应：默认配置 + 用户覆盖（timeout 30 → 5）、
 *   权限并集（read ∪ write）。合并策略除 REPLACE 外还有
 *   保留既有/报错冲突等（见 value.h XVALUE_MERGE_*）。
 * 验证矩阵：合并后 count=1（覆盖不增键）；
 *   Clone+Merge 的就地并集 与 Union 产物 SetEqual。
 */





/* 演示对象覆盖合并和保持稳定顺序的 Set 并集。 */
int main(void)
{
	xvalue* pDefaults = xrtValueObject();
	xvalue* pOptions = xrtValueObject();
	xvalue* pLeft = xrtValueSet();
	xvalue* pRight = xrtValueSet();
	xvalue* pUnion = NULL;
	xvalue* pMerged = NULL;
	int iResult = 1;

	if ( (pDefaults == NULL) || (pOptions == NULL) ||
		 (pLeft == NULL) || (pRight == NULL) ||
		 !xrtValueObjectSetNew(
			pDefaults,
			XRT_STR_LITERAL("timeout"),
			xrtValueInt(30)
		 ) ||
		 !xrtValueObjectSetNew(
			pOptions,
			XRT_STR_LITERAL("timeout"),
			xrtValueInt(5)
		 ) ||
		 !xrtValueObjectMerge(
			pDefaults,
			pOptions,
			XVALUE_MERGE_REPLACE
		 ) ||
		 !xrtValueSetAddNew(pLeft, xrtValueString(XRT_STR_LITERAL("read"))) ||
		 !xrtValueSetAddNew(pRight, xrtValueString(XRT_STR_LITERAL("write"))) ||
		 !xrtValueSetIsDisjoint(pLeft, pRight) ) {
		goto cleanup;
	}
	pUnion = xrtValueSetUnion(pLeft, pRight);
	pMerged = xrtValueClone(pLeft);
	if ( (pUnion == NULL) || (pMerged == NULL) ||
		 !xrtValueSetMerge(pMerged, pRight) ||
		 !xrtValueSetEqual(pMerged, pUnion) ) {
		goto cleanup;
	}
	printf(
		"options=%zu permissions=%zu\n",
		xrtValueCount(pDefaults),
		xrtValueCount(pUnion)
	);
	iResult = 0;

cleanup:
	xrtValueRelease(pMerged);
	xrtValueRelease(pUnion);
	xrtValueRelease(pRight);
	xrtValueRelease(pLeft);
	xrtValueRelease(pOptions);
	xrtValueRelease(pDefaults);
	return iResult;
}
