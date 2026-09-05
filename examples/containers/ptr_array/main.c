/*
 * 范例：containers/ptr_array —— 指针数组：只管引用、不拥有对象
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtPtrArrayInit    初始化（元素固定为 ptr，无需给元素大小）
 *   xrtPtrArrayPush    尾部追加一个指针
 *   xrtPtrArrayInsert  在指定下标插入（后续元素整体后移）
 *   xrtPtrArrayGet     按下标取指针（越界返回 NULL）
 *   tArray.Count       公开字段：当前指针数
 * 模块宏：XRT_MODULE_ARRAY
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/containers/ptr_array/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   [0] 10
 *   [1] 30
 *   [2] 20
 *
 * 指针语义与值语义（array/main.c）的取舍：
 *   - ptr_array 存引用：数组释放不影响目标对象，
 *     适合"多态对象集合 / 同一对象出现在多个集合"；
 *   - 对象生命周期完全归调用方——本例目标是栈变量，不能 Free。
 */

#include <stdio.h>
#include <xrt.h>



int main(void)
{
	xptrarray tArray;
	int iFirst = 10;
	int iSecond = 20;
	int iThird = 30;

	if ( !xrtPtrArrayInit(&tArray) ) {
		return 1;
	}

	/*
	 * 追加 10、20 后在下标 1 插入 30：
	 * 最终顺序 [10, 30, 20]——Insert 的"挤开"语义。
	 */
	if (
		!xrtPtrArrayPush(&tArray, &iFirst) ||
		!xrtPtrArrayPush(&tArray, &iSecond) ||
		!xrtPtrArrayInsert(&tArray, 1, &iThird)
	) {
		xrtPtrArrayUnit(&tArray);
		return 2;
	}

	/* 解引用打印：Get 返回 ptr，转回 int* 后取值。 */
	for ( size_t i = 0; i < tArray.Count; i++ ) {
		printf("[%zu] %d\n", i, *(int*)xrtPtrArrayGet(&tArray, i));
	}

	/* 释放的是指针槽位本身；栈上的目标对象不受影响。 */
	xrtPtrArrayUnit(&tArray);
	return 0;
}
