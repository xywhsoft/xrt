/*
 * 范例：containers/ptr_fixed_stack —— 零分配固定容量资源回滚栈
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtPtrFixedStackInit   绑定调用方槽位数组（容量固定）
 *   xrtPtrFixedStackPush   压入指针；容量满返回 false
 *   xrtPtrFixedStackPop    弹出指针；空栈返回 false 并设置错误
 *   xrtPtrFixedStackUnit   归还句柄（不动调用方存储）
 * 模块宏：XRT_MODULE_STACK
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/containers/ptr_fixed_stack/main.c -lws2_32 -liphlpapi
 * 预期输出（LIFO）：
 *   30
 *   20
 *   10
 *
 * 与 ptr_stack 的取舍：容量已知（本例 4）时优先固定栈——
 *   槽位在栈帧/结构体里，Push/Pop 全程零堆分配
 *  （_noalloc 契约测试保证），函数级清理路径用它最稳。
 */

#include <stdio.h>
#include <xrt.h>



int main(void)
{
	xptrfixedstack tCleanup;
	ptr pStorage[4];        /* 物理槽位：本地数组 */
	int pResources[] = { 10, 20, 30 };
	ptr pResource;

	/* 绑定外部存储：之后零分配。容量 4 = 最多同时记 4 个待回滚资源。 */
	if ( !xrtPtrFixedStackInit(&tCleanup, pStorage, 4) ) {
		return 1;
	}

	/* 记录三个"已获取"资源（真实代码里是各资源的释放句柄）。 */
	for ( size_t i = 0; i < 3; i++ ) {
		if ( !xrtPtrFixedStackPush(&tCleanup, &pResources[i]) ) {
			xrtPtrFixedStackUnit(&tCleanup);
			return 2;
		}
	}

	/* LIFO 回滚：最后获取的最先释放；栈空置错后清一次。 */
	while ( xrtPtrFixedStackPop(&tCleanup, &pResource) ) {
		printf("%d\n", *(int*)pResource);
	}
	xrtClearError();
	xrtPtrFixedStackUnit(&tCleanup);
	return 0;
}
