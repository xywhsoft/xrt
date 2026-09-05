/*
 * 范例：containers/ptr_stack —— 指针栈：按 LIFO 顺序回滚已获取的资源
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtPtrStackInit  初始化（元素固定为 ptr）
 *   xrtPtrStackPush  压入一个对象指针
 *   xrtPtrStackPop   弹出栈顶指针；空栈返回 false 并设置错误
 *   xrtClearError    清除"栈已空"的预期错误记录
 * 模块宏：XRT_MODULE_STACK
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/containers/ptr_stack/main.c -lws2_32 -liphlpapi
 * 预期输出（LIFO：30 20 10）：
 *   30
 *   20
 *   10
 *
 * 典型用途——错误路径统一回滚：
 *   每成功获取一个资源就 Push；中途失败时不断 Pop 逐个释放，
 *   释放顺序天然与获取顺序相反（后获取的先释放），
 *   这正是依赖资源的安全拆除顺序。
 * 栈只搬运指针、不拥有对象：目标的生命周期完全由调用方决定。
 */

#include <stdio.h>
#include <xrt.h>



int main(void)
{
	xptrstack tResources;   /* "已获取资源"回滚栈 */
	int pValues[] = { 10, 20, 30 };
	ptr pValue;

	if ( !xrtPtrStackInit(&tResources) ) {
		return 1;
	}

	/* 模拟依次获取三个资源（此处用栈变量代替真实句柄）。 */
	for ( size_t i = 0; i < 3; i++ ) {
		if ( !xrtPtrStackPush(&tResources, &pValues[i]) ) {
			xrtPtrStackUnit(&tResources);
			return 2;
		}
	}

	/*
	 * LIFO 弹出即回滚：30（最后获取）最先释放。
	 * 真实代码里这里通常是 xrtFree / xrtXxxFree 而不是 printf。
	 * 栈空时 Pop 置错误，循环后清一次。
	 */
	while ( xrtPtrStackPop(&tResources, &pValue) ) {
		printf("%d\n", *(int*)pValue);
	}
	xrtClearError();
	xrtPtrStackUnit(&tResources);
	return 0;
}
