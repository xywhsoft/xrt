/**
 * @file main.c
 * @brief Pointer Stack Example - xrtStackPushPtr/PopPtr/TopPtr
 *        指针栈示例 - xrtStackPushPtr/PopPtr/TopPtr
 * 
 * This example demonstrates:
 * - Using static stack for pointer storage
 * - Push/Pop/Top for pointers
 * - Using dynamic stack for pointer storage
 * 
 * 本示例演示：
 * - 使用静态栈存储指针
 * - 指针的压栈/出栈/获取栈顶
 * - 使用动态栈存储指针
 * 
 * Build: tcc main.c -o ../../bin/stack_ptr_stack.exe
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

void test_static_ptr_stack(void)
{
	printf("=== Test: Static Stack - Pointer Operations ===\n");
	printf("=== 测试：静态栈 - 指针操作 ===\n");
	
	#define MAX_PTRS 10
	
	xstack pStack = xrtStackCreate(MAX_PTRS, sizeof(ptr));
	
	int a = 100, b = 200, c = 300, d = 400;
	
	printf("Pushing pointers to static stack...\n");
	printf("向静态栈压入指针...\n");
	
	xrtStackPushPtr(pStack, &a);
	xrtStackPushPtr(pStack, &b);
	xrtStackPushPtr(pStack, &c);
	xrtStackPushPtr(pStack, &d);
	
	printf("Pushed %u pointers\n", pStack->Count);
	
	printf("\nPopping pointers:\n");
	printf("\n弹出指针:\n");
	while (pStack->Count > 0)
	{
		int* pPtr = (int*)xrtStackPopPtr(pStack);
		printf("  Popped pointer to value: %d\n", *pPtr);
	}
	
	xrtStackDestroy(pStack);
}

void test_dynamic_ptr_stack(void)
{
	printf("\n=== Test: Dynamic Stack - Pointer Operations ===\n");
	printf("=== 测试：动态栈 - 指针操作 ===\n");
	
	xdynstack pStack = xrtDynStackCreate(sizeof(ptr));
	
	char* strs[] = {"First", "Second", "Third", "Fourth", "Fifth"};
	int nCount = sizeof(strs) / sizeof(strs[0]);
	
	printf("Pushing string pointers to dynamic stack...\n");
	printf("向动态栈压入字符串指针...\n");
	
	for (int i = 0; i < nCount; i++)
	{
		xrtDynStackPushPtr(pStack, strs[i]);
	}
	
	printf("Pushed %u string pointers\n", pStack->Count);
	
	printf("\nPopping string pointers:\n");
	printf("\n弹出字符串指针:\n");
	while (pStack->Count > 0)
	{
		char* pStr = (char*)xrtDynStackPopPtr(pStack);
		printf("  Popped: %s\n", pStr);
	}
	
	xrtDynStackDestroy(pStack);
}

void test_getpos_ptr(void)
{
	printf("\n=== Test: GetPosPtr (Access Any Position) ===\n");
	printf("=== 测试：GetPosPtr（访问任意位置）===\n");
	
	xdynstack pStack = xrtDynStackCreate(sizeof(ptr));
	
	int vals[] = {10, 20, 30, 40, 50};
	int nCount = sizeof(vals) / sizeof(vals[0]);
	
	for (int i = 0; i < nCount; i++)
	{
		xrtDynStackPushPtr(pStack, &vals[i]);
	}
	
	printf("Stack pointer contents (position 1=bottom):\n");
	printf("栈指针内容（位置1=底部）:\n");
	for (uint32 i = 1; i <= pStack->Count; i++)
	{
		int* pVal = (int*)xrtDynStackGetPosPtr(pStack, i);
		printf("  Position %u: *ptr = %d\n", i, *pVal);
	}
	
	printf("\nTop pointer:\n");
	printf("栈顶指针:\n");
	int* pTop = (int*)xrtDynStackTopPtr(pStack);
	printf("  *top = %d\n", *pTop);
	
	xrtDynStackDestroy(pStack);
}

int main(void)
{
	xrtInit();
	
	printf("========================================\n");
	printf("  Pointer Stack Example / 指针栈示例\n");
	printf("========================================\n");
	
	test_static_ptr_stack();
	test_dynamic_ptr_stack();
	test_getpos_ptr();
	
	printf("\n=== All tests completed! ===\n");
	printf("=== 所有测试完成！===\n");
	
	xrtUnit();
	return 0;
}
