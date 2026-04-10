/**
 * @file main.c
 * @brief Reference Count Example - xvoAddRef/xvoUnref
 *        引用计数示例 - xvoAddRef/xvoUnref
 * 
 * This example demonstrates:
 * - Reference counting mechanism
 * - Adding and releasing references
 * 
 * 本示例演示：
 * - 引用计数机制
 * - 添加和释放引用
 * 
 * Build: tcc main.c -o ../../bin/value_refcount.exe
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

void test_refcount(void)
{
	printf("=== Test: Reference Counting ===\n");
	printf("=== 测试：引用计数 ===\n");
	
	xvalue pVal = xvoCreateInt(100);
	printf("Created value with initial refcount\n");
	printf("创建值，初始引用计数\n");
	printf("  Value: %lld\n", (long long)pVal->vInt);
	
	printf("\nAdding reference...\n");
	printf("\n添加引用...\n");
	xvoAddRef(pVal);
	printf("  RefCount increased\n");
	printf("  引用计数增加\n");
	
	printf("\nReleasing first reference (Unref)...\n");
	printf("\n释放第一个引用（Unref）...\n");
	xvoUnref(pVal);
	printf("  RefCount decreased, but value still valid\n");
	printf("  引用计数减少，但值仍然有效\n");
	
	printf("\nReleasing second reference (Unref)...\n");
	printf("\n释放第二个引用（Unref）...\n");
	printf("  Value will be freed\n");
	printf("  值将被释放\n");
	xvoUnref(pVal);
	
	printf("  Value freed successfully\n");
	printf("  值成功释放\n");
}

void test_shared_values(void)
{
	printf("\n=== Test: Shared Values ===\n");
	printf("=== 测试：共享值 ===\n");
	
	xvalue pShared = xvoCreateText("Shared string", 13, false);
	printf("Created shared string value\n");
	printf("创建共享字符串值\n");
	
	xvalue pArr = xvoCreateArray();
	printf("Created array\n");
	printf("创建数组\n");
	
	xvoAddRef(pShared);
	xvoArrayAppendValue(pArr, pShared, true);
	printf("Added shared value to array (refcount increased)\n");
	printf("将共享值添加到数组（引用计数增加）\n");
	
	xvoUnref(pShared);
	printf("Released original reference\n");
	printf("释放原始引用\n");
	
	xvoUnref(pArr);
	printf("Released array (shared value freed with array)\n");
	printf("释放数组（共享值随数组释放）\n");
}

int main(void)
{
	xrtInit();
	
	printf("========================================\n");
	printf("  Reference Count Example / 引用计数示例\n");
	printf("========================================\n");
	
	test_refcount();
	test_shared_values();
	
	printf("\n=== All tests completed! ===\n");
	printf("=== 所有测试完成！===\n");
	
	xrtUnit();
	return 0;
}
