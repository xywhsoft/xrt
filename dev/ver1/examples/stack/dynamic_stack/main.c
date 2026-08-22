/**
 * @file main.c
 * @brief Dynamic Stack Example - xdynstack operations
 *        动态栈示例 - xdynstack 操作
 * 
 * This example demonstrates:
 * - Creating and destroying dynamic stacks
 * - Pushing and popping data
 * - Stack grows automatically (no fixed capacity)
 * 
 * 本示例演示：
 * - 创建和销毁动态栈
 * - 压栈和出栈数据
 * - 栈自动增长（无固定容量限制）
 * 
 * Build: tcc main.c -o ../../bin/stack_dynamic_stack.exe
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

void test_basic_dynamic_stack(void)
{
	printf("=== Test: Basic Dynamic Stack Operations ===\n");
	printf("=== 测试：基本动态栈操作 ===\n");
	
	xdynstack pStack = xrtDynStackCreate(sizeof(int));
	
	printf("Pushing 20 values...\n");
	printf("压入20个值...\n");
	
	for (int i = 1; i <= 20; i++)
	{
		int* pSlot = (int*)xrtDynStackPush(pStack);
		*pSlot = i * 5;
		if (i <= 5 || i > 15)
		{
			printf("  Pushed: %d, Count: %u\n", *pSlot, pStack->Count);
		}
		else if (i == 6)
		{
			printf("  ... (pushing more) ...\n");
		}
	}
	
	printf("\nPopping 10 values:\n");
	printf("\n弹出10个值:\n");
	for (int i = 0; i < 10; i++)
	{
		int* pTop = (int*)xrtDynStackTop(pStack);
		xrtDynStackPop(pStack);
		printf("  Popped: %d, Remaining: %u\n", *pTop, pStack->Count);
	}
	
	printf("\nTop after popping:\n");
	printf("弹出后栈顶:\n");
	int* pTop = (int*)xrtDynStackTop(pStack);
	printf("  Top value: %d\n", *pTop);
	
	xrtDynStackDestroy(pStack);
}

void test_push_data(void)
{
	printf("\n=== Test: PushData Convenience Function ===\n");
	printf("=== 测试：PushData 便捷函数 ===\n");
	
	xdynstack pStack = xrtDynStackCreate(sizeof(double));
	
	double vals[] = {3.14, 2.718, 1.414, 1.732};
	int nCount = sizeof(vals) / sizeof(vals[0]);
	
	for (int i = 0; i < nCount; i++)
	{
		xrtDynStackPushData(pStack, &vals[i]);
	}
	
	printf("Stack contents (doubles):\n");
	printf("栈内容（双精度浮点数）:\n");
	while (pStack->Count > 0)
	{
		double* pTop = (double*)xrtDynStackTop(pStack);
		printf("  %.3f\n", *pTop);
		xrtDynStackPop(pStack);
	}
	
	xrtDynStackDestroy(pStack);
}

void test_getpos(void)
{
	printf("\n=== Test: GetPos (Access Any Position) ===\n");
	printf("=== 测试：GetPos（访问任意位置）===\n");
	
	xdynstack pStack = xrtDynStackCreate(sizeof(int));
	
	for (int i = 1; i <= 5; i++)
	{
		int val = i * 100;
		xrtDynStackPushData(pStack, &val);
	}
	
	printf("Stack contents (position 1=bottom, 5=top):\n");
	printf("栈内容（位置1=底部，5=顶部）:\n");
	for (uint32 i = 1; i <= pStack->Count; i++)
	{
		int* pVal = (int*)xrtDynStackGetPos(pStack, i);
		printf("  Position %u: %d\n", i, *pVal);
	}
	
	xrtDynStackDestroy(pStack);
}

int main(void)
{
	xrtInit();
	
	printf("========================================\n");
	printf("  Dynamic Stack Example / 动态栈示例\n");
	printf("========================================\n");
	
	test_basic_dynamic_stack();
	test_push_data();
	test_getpos();
	
	printf("\n=== All tests completed! ===\n");
	printf("=== 所有测试完成！===\n");
	
	xrtUnit();
	return 0;
}
