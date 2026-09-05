/*
 * 范例：value/set_tour —— 集合全接口巡礼：运算四件套 + 包含判定 + 移交
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtValueSetAddTake      移交方式添加元素
 *   xrtValueSetRemove       删除等价值
 *   xrtValueSetTake         移交集合中的规范值
 *   xrtValueSetIntersection 交集（保持左集合顺序）
 *   xrtValueSetDifference   差集（左相对右）
 *   xrtValueSetSymmetricDifference 对称差集（左独有 + 右独有）
 *   xrtValueSetIsSubset / IsSuperset   子集/超集判定（bProper 真子集）
 * 模块宏：XRT_MODULE_VALUE
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c \
 *       examples/value/set_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   A={1,2} B={2,3}
 *   intersection={2} difference={1} symdiff={1,3}
 *   subset={1,2}<={1,2,3}:1 proper:1
 *   superset={1,2,3}>={1}:1
 *   take=2 removed=1
 *
 * 运算返回新集合（堆分配，Release 释放）；判定的 bProper
 *   参数区分"子集"与"真子集"——相等集合是子集但不是真子集。
 */

#include <stdio.h>
#include <xrt.h>

static void dumpSet(cstr pTag, const xvalue* pSet)
{
	xvalueiter Iter;
	xvaluekey Key;
	xvalue* pItem;
	size_t i = 0;

	printf("%s={", pTag);
	(void)xrtValueIterBegin(pSet, &Iter);
	while ( (pItem = xrtValueIterNext(&Iter, &Key)) != NULL ) {
		int64 iValue = 0;

		(void)xrtValueGetInt(pItem, &iValue);
		printf("%s%lld", i ? "," : "", (long long)iValue);
		i++;
	}
	xrtValueIterEnd(&Iter);
	printf("} ");
}

int main(void)
{
	xvalue* pA = xrtValueSet();
	xvalue* pB = xrtValueSet();
	xvalue* pBig = xrtValueSet();
	xvalue* pSmall = xrtValueSet();
	xvalue* pResult;
	xvalue* pTaken;

	/* AddTake：移交添加（源槽清零）。 */
	{
		xvalue* pOwned = xrtValueInt(1);

		(void)xrtValueSetAddTake(pA, &pOwned);
		pOwned = xrtValueInt(2);
		(void)xrtValueSetAddTake(pA, &pOwned);
	}
	(void)xrtValueSetAddNew(pB, xrtValueInt(2));
	(void)xrtValueSetAddNew(pB, xrtValueInt(3));
	dumpSet("A", pA);
	dumpSet("B", pB);
	printf("\n");

	/* 运算三件套：交集 / 差集 / 对称差集。 */
	pResult = xrtValueSetIntersection(pA, pB);
	dumpSet("intersection", pResult);
	xrtValueRelease(pResult);
	pResult = xrtValueSetDifference(pA, pB);
	dumpSet("difference", pResult);
	xrtValueRelease(pResult);
	pResult = xrtValueSetSymmetricDifference(pA, pB);
	dumpSet("symdiff", pResult);
	xrtValueRelease(pResult);
	printf("\n");

	/* 包含判定：{1,2} ⊆ {1,2,3}，真子集也成立。 */
	(void)xrtValueSetAddNew(pBig, xrtValueInt(1));
	(void)xrtValueSetAddNew(pBig, xrtValueInt(2));
	(void)xrtValueSetAddNew(pBig, xrtValueInt(3));
	(void)xrtValueSetAddNew(pSmall, xrtValueInt(1));
	printf("subset={1,2}<={1,2,3}:%d proper:%d\n",
		xrtValueSetIsSubset(pA, pBig, false) ? 1 : 0,
		xrtValueSetIsSubset(pA, pBig, true) ? 1 : 0);
	printf("superset={1,2,3}>={1}:%d\n",
		xrtValueSetIsSuperset(pBig, pSmall, false) ? 1 : 0);

	/* Take：移交集合中的规范值；Remove：删除等价值。 */
	pTaken = xrtValueSetTake(pA, xrtValueInt(2));
	if ( pTaken != NULL ) {
		int64 iValue = 0;

		(void)xrtValueGetInt(pTaken, &iValue);
		printf("take=%lld ", (long long)iValue);
		xrtValueRelease(pTaken);
	}
	printf("removed=%d\n", xrtValueSetRemove(pB, xrtValueInt(3)) ? 1 : 0);

	xrtValueRelease(pSmall);
	xrtValueRelease(pBig);
	xrtValueRelease(pB);
	xrtValueRelease(pA);
	return 0;
}
