/**
 * @file main.c
 * @brief Value Array Example - xvoCreateArray operations
 *        值数组示例 - xvoCreateArray 操作
 * 
 * This example demonstrates:
 * - Creating and using value arrays
 * - Appending, inserting, setting, getting, removing values
 * 
 * 本示例演示：
 * - 创建和使用值数组
 * - 追加、插入、设置、获取、删除值
 * 
 * Build: tcc main.c -o ../../bin/value_value_array.exe
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

void test_array_operations(void)
{
	printf("=== Test: Value Array Operations ===\n");
	printf("=== 测试：值数组操作 ===\n");
	
	xvalue pArr = xvoCreateArray();
	printf("Created array value\n");
	printf("创建数组值\n");
	
	printf("\nAppending values...\n");
	printf("\n追加值...\n");
	
	xvoArrayAppendInt(pArr, 100);
	xvoArrayAppendInt(pArr, 200);
	xvoArrayAppendInt(pArr, 300);
	xvoArrayAppendText(pArr, "Hello", 5, false);
	xvoArrayAppendFloat(pArr, 3.14);
	
	printf("Array count: %u\n", xvoArrayItemCount(pArr));
	printf("数组计数: %u\n", xvoArrayItemCount(pArr));
	
	printf("\nGetting values:\n");
	printf("\n获取值:\n");
	
	for (uint32 i = 0; i < xvoArrayItemCount(pArr); i++)
	{
		xvalue pItem = xvoArrayGetValue(pArr, i);
		if (pItem)
		{
			if (pItem->Type == XVO_DT_INT)
				printf("  [%u] int: %lld\n", i, (long long)pItem->vInt);
			else if (pItem->Type == XVO_DT_FLOAT)
				printf("  [%u] float: %.2f\n", i, pItem->vFloat);
			else if (pItem->Type == XVO_DT_TEXT)
				printf("  [%u] text: %s\n", i, pItem->vText);
			xvoUnref(pItem);
		}
	}
	
	printf("\nInserting at index 1...\n");
	printf("\n在索引1插入...\n");
	xvoArrayInsertInt(pArr, 1, 999);
	
	printf("After insertion:\n");
	printf("插入后:\n");
	for (uint32 i = 0; i < xvoArrayItemCount(pArr); i++)
	{
		xvalue pItem = xvoArrayGetValue(pArr, i);
		if (pItem)
		{
			if (pItem->Type == XVO_DT_INT)
				printf("  [%u] int: %lld\n", i, (long long)pItem->vInt);
			xvoUnref(pItem);
		}
	}
	
	printf("\nRemoving index 0...\n");
	printf("\n删除索引0...\n");
	xvoArrayRemove(pArr, 0, 1);
	printf("Array count after removal: %u\n", xvoArrayItemCount(pArr));
	printf("删除后数组计数: %u\n", xvoArrayItemCount(pArr));
	
	xvoUnref(pArr);
}

int main(void)
{
	xrtInit();
	
	printf("========================================\n");
	printf("  Value Array Example / 值数组示例\n");
	printf("========================================\n");
	
	test_array_operations();
	
	printf("\n=== All tests completed! ===\n");
	printf("=== 所有测试完成！===\n");
	
	xrtUnit();
	return 0;
}
