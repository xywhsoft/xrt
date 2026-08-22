/**
 * @file main.c
 * @brief Value List Example - xvoCreateList operations
 *        值列表示例 - xvoCreateList 操作
 * 
 * This example demonstrates:
 * - Creating and using value lists
 * - Setting, getting values by index
 * 
 * 本示例演示：
 * - 创建和使用值列表
 * - 按索引设置、获取值
 * 
 * Build: tcc main.c -o ../../bin/value_value_list.exe
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

void test_list_operations(void)
{
	printf("=== Test: Value List Operations ===\n");
	printf("=== 测试：值列表操作 ===\n");
	
	xvalue pList = xvoCreateList();
	printf("Created list value\n");
	printf("创建列表值\n");
	
	printf("\nSetting values by index...\n");
	printf("\n按索引设置值...\n");
	
	xvoListSetInt(pList, 0, 10);
	xvoListSetInt(pList, 1, 20);
	xvoListSetInt(pList, 2, 30);
	xvoListSetText(pList, 3, "item", 4, false);
	xvoListSetFloat(pList, 4, 1.5);
	
	printf("List count: %u\n", xvoListItemCount(pList));
	printf("列表计数: %u\n", xvoListItemCount(pList));
	
	printf("\nGetting values:\n");
	printf("\n获取值:\n");
	
	for (uint32 i = 0; i < xvoListItemCount(pList); i++)
	{
		xvalue pItem = xvoListGetValue(pList, i);
		if (pItem)
		{
			if (pItem->Type == XVO_DT_INT)
				printf("  [%u] int: %lld\n", i, (long long)pItem->vInt);
			else if (pItem->Type == XVO_DT_FLOAT)
				printf("  [%u] float: %.1f\n", i, pItem->vFloat);
			else if (pItem->Type == XVO_DT_TEXT)
				printf("  [%u] text: %s\n", i, pItem->vText);
			xvoUnref(pItem);
		}
	}
	
	printf("\nUpdating index 1...\n");
	printf("\n更新索引1...\n");
	xvoListSetInt(pList, 1, 999);
	
	xvalue pVal = xvoListGetValue(pList, 1);
	if (pVal)
	{
		printf("  New value at index 1: %lld\n", (long long)pVal->vInt);
		xvoUnref(pVal);
	}
	
	xvoUnref(pList);
}

int main(void)
{
	xrtInit();
	
	printf("========================================\n");
	printf("  Value List Example / 值列表示例\n");
	printf("========================================\n");
	
	test_list_operations();
	
	printf("\n=== All tests completed! ===\n");
	printf("=== 所有测试完成！===\n");
	
	xrtUnit();
	return 0;
}
