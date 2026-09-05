/*
 * 范例：containers/block_stack —— 地址稳定、按块增长的工作栈
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtBlockStackInitLayout  按元素大小/对齐/每块元素数初始化
 *   xrtBlockStackAdd         压入一个"零初始化"元素并返回其地址
 *   xrtBlockStackPush        压入元素副本
 *   xrtBlockStackGet         按下标取元素地址
 *   tFrames.Blocks.Count     公开字段：已分配块数
 * 模块宏：XRT_MODULE_STACK
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/containers/block_stack/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   count=21 blocks=6 root=100 stable=yes
 *
 * 为什么需要块栈：普通动态数组扩容会搬家，元素地址全部失效；
 * 块栈按固定大小（本例每块 4 个 int）追加新块，老块原地不动——
 * 压栈过程中拿到的指针（如 AST 节点、回溯帧）始终有效。
 * 这是解释器/解析器保存可变引用图的常用底座。
 */

#include <stdio.h>

#include <xrt.h>



int main(void)
{
	xblockstack tFrames;
	int* pRoot;

	/*
	 * 布局初始化：元素大小 4、对齐 4、每块 4 个元素。
	 * 21 个元素 → ceil(21/4) = 6 块。
	 */
	if ( !xrtBlockStackInitLayout(&tFrames, sizeof(int), sizeof(int), 4) ) {
		return 1;
	}

	/*
	 * Add 与 Push 的区别：Add 只分配槽位（零初始化）并返回地址，
	 * 适合"先占位再填字段"的大结构；填完写 100。
	 * 这个地址就是后续要验证稳定性的根元素。
	 */
	pRoot = (int*)xrtBlockStackAdd(&tFrames);
	if ( pRoot == NULL ) {
		xrtBlockStackUnit(&tFrames);
		return 2;
	}
	*pRoot = 100;

	/* 连续压 20 个元素，触发多次跨块增长（4 块 → 6 块）。 */
	for ( int i = 1; i <= 20; i++ ) {
		if ( !xrtBlockStackPush(&tFrames, &i) ) {
			xrtBlockStackUnit(&tFrames);
			return 3;
		}
	}

	/*
	 * 三项验证：
	 *   count  = 21（1 个 Add + 20 个 Push）；
	 *   blocks = 6（每块 4 元素）；
	 *   stable = 根元素地址与压栈前完全一致——跨块增长没有搬家。
	 */
	printf(
		"count=%zu blocks=%zu root=%d stable=%s\n",
		tFrames.Count,
		tFrames.Blocks.Count,
		*pRoot,
		xrtBlockStackGet(&tFrames, 0) == pRoot ? "yes" : "no"
	);

	xrtBlockStackUnit(&tFrames);
	return 0;
}
