/**
 * @file main.c
 * @brief Static Stack Example - xstack operations
 *        静态栈示例 - xstack 操作
 * 
 * This example demonstrates:
 * - Creating and destroying static stacks
 * - Pushing and popping data
 * - Accessing top element
 * - Stack with fixed maximum capacity
 * 
 * 本示例演示：
 * - 创建和销毁静态栈
 * - 压栈和出栈数据
 * - 访问栈顶元素
 * - 固定最大容量的栈
 * 
 * Build: tcc main.c -o ../../bin/stack_static_stack.exe
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

void test_basic_stack(void)
{
	printf("=== Test: Basic Static Stack Operations ===\n");
	printf("=== 测试：基本静态栈操作 ===\n");
	
	#define MAX_ITEMS 10
	
	xstack pStack = xrtStackCreate(MAX_ITEMS, sizeof(int));
	
	printf("Pushing values 10, 20, 30, 40, 50...\n");
	printf("压入值 10, 20, 30, 40, 50...\n");
	
	int vals[] = {10, 20, 30, 40, 50};
	for (int i = 0; i < 5; i++)
	{
		xrtStackPushData(pStack, &vals[i]);
		printf("  Pushed: %d, Count: %u/%u\n", vals[i], pStack->Count, pStack->MaxCount);
	}
	
	printf("\nPopping all values:\n");
	printf("\n弹出所有值:\n");
	while (pStack->Count > 0)
	{
		int* pTop = (int*)xrtStackTop(pStack);
		int* pPop = (int*)xrtStackPop(pStack);
		printf("  Top: %d, Popped: %d, Remaining: %u\n", *pTop, *pPop, pStack->Count);
	}
	
	xrtStackDestroy(pStack);
}

void test_stack_getpos(void)
{
	printf("\n=== Test: GetPos (Access Any Position) ===\n");
	printf("=== 测试：GetPos（访问任意位置）===\n");
	
	xstack pStack = xrtStackCreate(10, sizeof(int));
	
	int vals[] = {100, 200, 300, 400, 500};
	for (int i = 0; i < 5; i++)
	{
		xrtStackPushData(pStack, &vals[i]);
	}
	
	printf("Stack contents (from bottom to top):\n");
	printf("栈内容（从底部到顶部）:\n");
	for (uint32 i = 1; i <= pStack->Count; i++)
	{
		int* pVal = (int*)xrtStackGetPos(pStack, i);
		printf("  Position %u: %d\n", i, *pVal);
	}
	
	xrtStackDestroy(pStack);
}

void test_overflow_protection(void)
{
	printf("\n=== Test: Overflow Protection ===\n");
	printf("=== 测试：溢出保护 ===\n");
	
	#define SMALL_MAX 3
	
	xstack pStack = xrtStackCreate(SMALL_MAX, sizeof(int));
	
	printf("Created stack with max capacity: %u\n", SMALL_MAX);
	printf("创建栈，最大容量: %u\n", SMALL_MAX);
	
	for (int i = 1; i <= 5; i++)
	{
		ptr pResult = xrtStackPush(pStack);
		if (pResult)
		{
			*(int*)pResult = i * 10;
			printf("  Pushed %d, Count: %u\n", i * 10, pStack->Count);
		}
		else
		{
			printf("  Push %d failed - stack full! Count: %u\n", i * 10, pStack->Count);
		}
	}
	
	xrtStackDestroy(pStack);
}

int main(void)
{
	xrtInit();
	
	printf("========================================\n");
	printf("  Static Stack Example / 静态栈示例\n");
	printf("========================================\n");
	
	test_basic_stack();
	test_stack_getpos();
	test_overflow_protection();
	
	printf("\n=== All tests completed! ===\n");
	printf("=== 所有测试完成！===\n");
	
	xrtUnit();
	return 0;
}
