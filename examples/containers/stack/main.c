/*
 * 范例：containers/stack —— 深度未知的通用值栈（自动扩容）
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtStackInit   按元素大小初始化（内部容量按需增长）
 *   xrtStackPush   压入一个元素的副本
 *   xrtStackPop    弹出栈顶到调用方变量；空栈返回 false 并设置错误
 *   xrtClearError  清除"栈已空"的预期错误（见下方说明）
 *   xrtStackUnit   归还缓冲
 * 模块宏：XRT_MODULE_STACK
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/containers/stack/main.c -lws2_32 -liphlpapi
 * 预期输出（LIFO，20 行）：
 *   200
 *   190
 *   ...（依次递减 10）
 *   10
 *
 * Pop 的错误约定：弹出失败（栈空）会设置线程错误 XERR_STATE；
 * "循环弹到空"是合法用法，循环结束后清一次错误即可。
 * 同族：fixed_stack（零分配固定缓冲）、block_stack（地址稳定）
 *      与 ptr_stack / ptr_fixed_stack（指针语义）。
 */

#include <stdio.h>

#include <xrt.h>



int main(void)
{
	xstack tValues;
	int iValue;

	/* 只声明元素大小；首块缓冲在首次 Push 时按需分配。 */
	if ( !xrtStackInit(&tValues, sizeof(int)) ) {
		return 1;
	}

	/* 压入 10..200：栈自动扩容，调用方不感知容量。 */
	for ( int i = 1; i <= 20; i++ ) {
		iValue = i * 10;
		if ( !xrtStackPush(&tValues, &iValue) ) {
			xrtStackUnit(&tValues);
			return 2;
		}
	}

	/*
	 * LIFO 弹出：每成功一次写入 iValue 并返回 true；
	 * 栈空时返回 false 结束循环，同时在线程错误槽留下
	 * "空栈"记录——用完清掉，避免污染后续错误检查。
	 */
	while ( xrtStackPop(&tValues, &iValue) ) {
		printf("%d\n", iValue);
	}
	xrtClearError();
	xrtStackUnit(&tValues);
	return 0;
}
