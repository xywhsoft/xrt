/*
 * 范例：string/split —— 零分配分隔迭代器（空段保留语义）
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtStrSplitInit  初始化迭代器（借用原串 + 分隔符视图）
 *   xrtStrSplitNext  逐段取值（视图直接指向原串内部）
 * 模块宏：XRT_MODULE_STRING
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/string/split/main.c -lws2_32 -liphlpapi
 * 预期输出（注意第三行是空段）：
 *   alpha
 *   beta
 *
 *   gamma
 *
 * 空段保留："alpha,beta,,gamma" 的连续逗号产生空段——
 *   CSV/固定格式解析常需要感知"这一列为空"而不是跳过；
 *   需要丢弃空段时在循环里判 Item.Size == 0 即可。
 * 全程零分配：所有段都是原串的切片视图；
 *   要收集成列表用 xrtStrSplitList（分配型便捷层）。
 */

#include <stdio.h>

#include <xrt.h>



int main(void)
{
	xstrsplit tSplit;
	xstrview Item;

	if ( !xrtStrSplitInit(&tSplit, XRT_STR_LITERAL("alpha,beta,,gamma"), XRT_STR_LITERAL(",")) ) {
		return 1;
	}
	while ( xrtStrSplitNext(&tSplit, &Item) ) {
		/* 空段 Data 为 NULL、Size 为 0：打印空行即为证据。 */
		printf("%.*s\n", (int)Item.Size, Item.Data != NULL ? Item.Data : "");
	}
	return 0;
}
