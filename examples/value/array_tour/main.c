/*
 * 范例：value/array_tour —— 数组全接口巡礼：插入/弹出/交换/移除/接管
 * ----------------------------------------------------------------
 * 演示 API：
 *   【追加/插入】 AppendTake / Insert / InsertNew / InsertTake
 *   【替换/移除】 Set / SetTake / Remove / Swap
 *   【取出】     At（带负索引）/ Take / Pop
 *   【容量】     xrtValueReserve / xrtValueTrim / xrtValueCapacity / xrtValueClear
 * 模块宏：XRT_MODULE_VALUE
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c \
 *       examples/value/array_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   after-insert: a x b c
 *   after-swap: a x c b
 *   after-remove: a c b
 *   at(-1)=b pop=b take(0)=a final-count=1
 *
 * 所有权三件套约定（全库一致）：
 *   Insert/Set   增加引用（源仍归调用方）；
 *   InsertNew/SetNew 接管"新建值"（函数名里的 New）；
 *   InsertTake/SetTake/AppendTake 移交"已有变量"（源槽清零）。
 *   At 支持负索引（-1 = 末元素）——与 Resolve 同一规则。
 */

#include <stdio.h>
#include <xrt.h>

#define SV(x) XRT_STR_LITERAL(x)

static void dump(const char* pTag, xvalue* pArray)
{
	printf("%s:", pTag);
	for ( size_t i = 0; i < xrtValueCount(pArray); i++ ) {
		xstrview Text;

		(void)xrtValueGetString(xrtValueArrayGet(pArray, i), &Text);
		printf(" %.*s", (int)Text.Size, Text.Data);
	}
	printf("\n");
}

int main(void)
{
	xvalue* pArray = xrtValueArray();
	char Buffer[8];
	xstrview Text;
	xvalue* pTaken;

	/* 基础三元素：a b c（New 版接管新建值）。 */
	(void)xrtValueArrayAppendNew(pArray, xrtValueString(SV("a")));
	(void)xrtValueArrayAppendNew(pArray, xrtValueString(SV("b")));
	(void)xrtValueArrayAppendNew(pArray, xrtValueString(SV("c")));

	/* Insert 族：位置 1 插入 x（借引用 / 接管双形态各演示）。 */
	{
		xvalue* pOwned = xrtValueString(SV("x"));

		(void)xrtValueArrayInsert(pArray, 1u, pOwned);   /* 借用 */
		xrtValueRelease(pOwned);
	}
	dump("after-insert", pArray);

	/* Swap 交换位置 2、3；Set/SetTake 替换位置 1。 */
	(void)xrtValueArraySwap(pArray, 2u, 3u);
	dump("after-swap", pArray);
	{
		xvalue* pOwned = xrtValueString(SV("x2"));

		(void)xrtValueArraySet(pArray, 1u, pOwned);      /* 借用替换 */
		xrtValueRelease(pOwned);
		pOwned = xrtValueString(SV("x2"));
		(void)xrtValueArraySetTake(pArray, 1u, &pOwned); /* 移交替换 */
	}
	(void)xrtValueArrayRemove(pArray, 1u, 1u);          /* 删掉 x2 */
	dump("after-remove", pArray);

	/* At 负索引 / Pop 弹出末元素 / Take 按下标移交。 */
	(void)xrtValueGetString(xrtValueArrayAt(pArray, -1), &Text);
	snprintf(Buffer, sizeof(Buffer), "%.*s", (int)Text.Size, Text.Data);
	printf("at(-1)=%s", Buffer);
	pTaken = xrtValueArrayPop(pArray);
	(void)xrtValueGetString(pTaken, &Text);
	snprintf(Buffer, sizeof(Buffer), "%.*s", (int)Text.Size, Text.Data);
	printf(" pop=%s", Buffer);
	xrtValueRelease(pTaken);
	pTaken = xrtValueArrayTake(pArray, 0u);
	(void)xrtValueGetString(pTaken, &Text);
	snprintf(Buffer, sizeof(Buffer), "%.*s", (int)Text.Size, Text.Data);
	printf(" take(0)=%s", Buffer);
	xrtValueRelease(pTaken);
	printf(" final-count=%zu\n", xrtValueCount(pArray));

	/* 容量三件套 + Clear：Reserve 预留、Capacity 查询、Trim 收缩。 */
	(void)xrtValueReserve(pArray, 100u);
	printf("reserved-cap>=%zu", xrtValueCapacity(pArray));
	(void)xrtValueTrim(pArray);
	printf(" trimmed-cap=%zu", xrtValueCapacity(pArray));
	xrtValueClear(pArray);
	printf(" cleared-count=%zu\n", xrtValueCount(pArray));

	/* InsertNew / InsertTake / AppendTake 三个接管形态收尾演示。 */
	{
		xvalue* pA = xrtValueInt(1);
		xvalue* pB = xrtValueInt(2);

		(void)xrtValueArrayInsertNew(pArray, 0u, xrtValueInt(0));
		(void)xrtValueArrayInsertTake(pArray, 1u, &pA);
		(void)xrtValueArrayAppendTake(pArray, &pB);
		printf("take-forms-count=%zu\n", xrtValueCount(pArray));
	}
	xrtValueRelease(pArray);
	return 0;
}
