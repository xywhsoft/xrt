/*
 * 范例：containers/fixed_stack —— 调用方缓冲上的零分配工作栈
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtFixedStackInit  把栈绑定到调用方提供的存储（不再分配）
 *   xrtFixedStackPush  压入元素副本；缓冲满时失败（不越界）
 *   xrtFixedStackPop   弹出栈顶；空栈返回 false 并设置错误
 *   xrtFixedStackUnit  归还句柄（不释放调用方缓冲）
 * 模块宏：XRT_MODULE_STACK
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/containers/fixed_stack/main.c -lws2_32 -liphlpapi
 * 预期输出（LIFO，3 行）：
 *   function=3 position=30
 *   function=2 position=20
 *   function=1 position=10
 *
 * 适用场景：递归下降解析器、遍历回溯等"已知最大深度"的热路径——
 * 全程零堆分配（_noalloc 契约测试保证），比 xrtStack 更快，
 * 代价是深度上限固定：本例 8 帧，压第 9 个会失败。
 */

#include <stdio.h>

#include <xrt.h>



/* 模拟解析器的调用帧：函数编号 + 扫描位置。 */
typedef struct exampleframe {
	int Function;
	int Position;
} exampleframe;



int main(void)
{
	xfixedstack tFrames;
	exampleframe pStorage[8];      /* 栈的物理存储：本地数组 */
	exampleframe pInput[] = {
		{ 1, 10 },
		{ 2, 20 },
		{ 3, 30 }
	};
	exampleframe tFrame;

	/*
	 * 绑定外部存储：参数为 栈句柄/缓冲/容量(字节数)/元素大小。
	 * 之后的 Push/Pop 全部在这块内存上进行，永不分配。
	 */
	if ( !xrtFixedStackInit(&tFrames, pStorage, sizeof(pStorage), sizeof(exampleframe)) ) {
		return 1;
	}

	/* 依次压入 3 帧（容量 8，远未触顶）。 */
	for ( size_t i = 0; i < 3; i++ ) {
		if ( !xrtFixedStackPush(&tFrames, &pInput[i]) ) {
			return 2;
		}
	}

	/* LIFO 弹出；栈空时结束并清掉"空栈"错误记录。 */
	while ( xrtFixedStackPop(&tFrames, &tFrame) ) {
		printf("function=%d position=%d\n", tFrame.Function, tFrame.Position);
	}
	xrtClearError();
	xrtFixedStackUnit(&tFrames);
	return 0;
}
