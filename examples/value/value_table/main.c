/**
 * @file main.c
 * @brief Value Table Example - xvoCreateTable operations
 *        值表示例 - xvoCreateTable 操作
 * 
 * This example demonstrates:
 * - Creating and using value tables (dictionaries)
 * - Setting, getting, removing values by key
 * 
 * 本示例演示：
 * - 创建和使用值表（字典）
 * - 按键设置、获取、删除值
 * 
 * Build: tcc main.c -o ../../bin/value_value_table.exe
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

void test_table_operations(void)
{
	printf("=== Test: Value Table Operations ===\n");
	printf("=== 测试：值表操作 ===\n");
	
	xvalue pTable = xvoCreateTable();
	printf("Created table value\n");
	printf("创建表值\n");
	
	printf("\nSetting values...\n");
	printf("\n设置值...\n");
	
	xvoTableSetInt(pTable, "age", 3, 25);
	xvoTableSetText(pTable, "name", 4, "Alice", 5, false);
	xvoTableSetFloat(pTable, "score", 5, 95.5);
	xvoTableSetBool(pTable, "active", 6, true);
	
	printf("Table count: %u\n", xvoTableItemCount(pTable));
	printf("表计数: %u\n", xvoTableItemCount(pTable));
	
	printf("\nGetting values:\n");
	printf("\n获取值:\n");
	
	xvalue pVal = xvoTableGetValue(pTable, "name", 4);
	if (pVal)
	{
		printf("  name: %s\n", pVal->vText);
		xvoUnref(pVal);
	}
	
	pVal = xvoTableGetValue(pTable, "age", 3);
	if (pVal)
	{
		printf("  age: %lld\n", (long long)pVal->vInt);
		xvoUnref(pVal);
	}
	
	pVal = xvoTableGetValue(pTable, "score", 5);
	if (pVal)
	{
		printf("  score: %.1f\n", pVal->vFloat);
		xvoUnref(pVal);
	}
	
	printf("\nChecking key existence:\n");
	printf("\n检查键是否存在:\n");
	printf("  'name' exists: %s\n", xvoTableExists(pTable, "name", 4) ? "yes" : "no");
	printf("  'unknown' exists: %s\n", xvoTableExists(pTable, "unknown", 7) ? "yes" : "no");
	
	printf("\nRemoving 'age'...\n");
	printf("\n删除 'age'...\n");
	xvoTableRemove(pTable, "age", 3);
	printf("  'age' exists after removal: %s\n", xvoTableExists(pTable, "age", 3) ? "yes" : "no");
	printf("  Table count: %u\n", xvoTableItemCount(pTable));
	
	xvoUnref(pTable);
}

int main(void)
{
	xrtInit();
	
	printf("========================================\n");
	printf("  Value Table Example / 值表示例\n");
	printf("========================================\n");
	
	test_table_operations();
	
	printf("\n=== All tests completed! ===\n");
	printf("=== 所有测试完成！===\n");
	
	xrtUnit();
	return 0;
}
