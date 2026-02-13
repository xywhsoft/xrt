/**
 * @file main.c
 * @brief Generate JSON Example - xrtStringifyJSON
 *        生成JSON示例 - xrtStringifyJSON
 * 
 * This example demonstrates:
 * - Creating value objects and converting to JSON
 * - Formatted vs compact JSON output
 * 
 * 本示例演示：
 * - 创建值对象并转换为JSON
 * - 格式化与紧凑JSON输出
 * 
 * Build: tcc main.c -o ../../bin/json_generate_json.exe
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

void test_stringify_object(void)
{
	printf("=== Test: Stringify JSON Object ===\n");
	printf("=== 测试：生成JSON对象 ===\n");
	
	xvalue pTable = xvoCreateTable();
	xvoTableSetText(pTable, "name", 4, "Alice", 5, false);
	xvoTableSetInt(pTable, "age", 3, 30);
	xvoTableSetBool(pTable, "active", 6, true);
	xvoTableSetFloat(pTable, "score", 5, 95.5);
	
	printf("Compact output:\n");
	printf("紧凑输出:\n");
	size_t size = 0;
	str pJson = xrtStringifyJSON(pTable, 0, &size);
	if (pJson)
	{
		printf("  %s\n", pJson);
		xrtFree(pJson);
	}
	
	printf("\nFormatted output:\n");
	printf("格式化输出:\n");
	pJson = xrtStringifyJSON(pTable, 1, &size);
	if (pJson)
	{
		printf("  %s\n", pJson);
		xrtFree(pJson);
	}
	
	xvoUnref(pTable);
}

void test_stringify_array(void)
{
	printf("\n=== Test: Stringify JSON Array ===\n");
	printf("=== 测试：生成JSON数组 ===\n");
	
	xvalue pArr = xvoCreateArray();
	xvoArrayAppendInt(pArr, 10);
	xvoArrayAppendInt(pArr, 20);
	xvoArrayAppendInt(pArr, 30);
	xvoArrayAppendText(pArr, "hello", 5, false);
	xvoArrayAppendBool(pArr, true);
	
	printf("Array JSON:\n");
	printf("数组JSON:\n");
	size_t size = 0;
	str pJson = xrtStringifyJSON(pArr, 1, &size);
	if (pJson)
	{
		printf("%s\n", pJson);
		xrtFree(pJson);
	}
	
	xvoUnref(pArr);
}

void test_stringify_nested(void)
{
	printf("\n=== Test: Stringify Nested Structure ===\n");
	printf("=== 测试：生成嵌套结构 ===\n");
	
	xvalue pRoot = xvoCreateTable();
	
	xvalue pUser = xvoCreateTable();
	xvoTableSetText(pUser, "name", 4, "Bob", 3, false);
	xvoTableSetInt(pUser, "id", 2, 12345);
	xvoTableSetValue(pRoot, "user", 4, pUser, true);
	xvoUnref(pUser);
	
	xvalue pTags = xvoCreateArray();
	xvoArrayAppendText(pTags, "admin", 5, false);
	xvoArrayAppendText(pTags, "user", 4, false);
	xvoTableSetValue(pRoot, "tags", 4, pTags, true);
	xvoUnref(pTags);
	
	printf("Nested JSON:\n");
	printf("嵌套JSON:\n");
	size_t size = 0;
	str pJson = xrtStringifyJSON(pRoot, 1, &size);
	if (pJson)
	{
		printf("%s\n", pJson);
		xrtFree(pJson);
	}
	
	xvoUnref(pRoot);
}

int main(void)
{
	xrtInit();
	
	printf("========================================\n");
	printf("  Generate JSON Example / 生成JSON示例\n");
	printf("========================================\n");
	
	test_stringify_object();
	test_stringify_array();
	test_stringify_nested();
	
	printf("\n=== All tests completed! ===\n");
	printf("=== 所有测试完成！===\n");
	
	xrtUnit();
	return 0;
}
