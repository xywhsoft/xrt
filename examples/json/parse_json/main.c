/**
 * @file main.c
 * @brief Parse JSON Example - xrtParseJSON
 *        解析JSON示例 - xrtParseJSON
 * 
 * This example demonstrates:
 * - Parsing JSON strings into value objects
 * - Accessing parsed JSON data
 * 
 * 本示例演示：
 * - 将JSON字符串解析为值对象
 * - 访问解析后的JSON数据
 * 
 * Build: tcc main.c -o ../../bin/json_parse_json.exe
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

void test_parse_object(void)
{
	printf("=== Test: Parse JSON Object ===\n");
	printf("=== 测试：解析JSON对象 ===\n");
	
	const char* json = "{\"name\":\"Alice\",\"age\":30,\"active\":true}";
	printf("Input: %s\n", json);
	printf("输入: %s\n", json);
	
	xvalue pRoot = xrtParseJSON((char*)json, strlen(json));
	if (pRoot)
	{
		printf("Parsed successfully!\n");
		printf("解析成功！\n");
		
		xvalue pName = xvoTableGetValue(pRoot, "name", 4);
		if (pName)
		{
			printf("  name: %s\n", pName->vText);
			xvoUnref(pName);
		}
		
		xvalue pAge = xvoTableGetValue(pRoot, "age", 3);
		if (pAge)
		{
			printf("  age: %lld\n", (long long)pAge->vInt);
			xvoUnref(pAge);
		}
		
		xvalue pActive = xvoTableGetValue(pRoot, "active", 6);
		if (pActive)
		{
			printf("  active: %s\n", pActive->vBool ? "true" : "false");
			xvoUnref(pActive);
		}
		
		xvoUnref(pRoot);
	}
	else
	{
		printf("Parse failed!\n");
		printf("解析失败！\n");
	}
}

void test_parse_array(void)
{
	printf("\n=== Test: Parse JSON Array ===\n");
	printf("=== 测试：解析JSON数组 ===\n");
	
	const char* json = "[10, 20, 30, 40, 50]";
	printf("Input: %s\n", json);
	printf("输入: %s\n", json);
	
	xvalue pRoot = xrtParseJSON((char*)json, strlen(json));
	if (pRoot)
	{
		printf("Parsed successfully!\n");
		printf("解析成功！\n");
		printf("  Array count: %u\n", xvoArrayItemCount(pRoot));
		printf("  数组计数: %u\n", xvoArrayItemCount(pRoot));
		
		printf("  Elements:\n");
		printf("  元素:\n");
		for (uint32 i = 0; i < xvoArrayItemCount(pRoot); i++)
		{
			xvalue pItem = xvoArrayGetValue(pRoot, i);
			if (pItem)
			{
				printf("    [%u] = %lld\n", i, (long long)pItem->vInt);
				xvoUnref(pItem);
			}
		}
		
		xvoUnref(pRoot);
	}
}

void test_parse_nested(void)
{
	printf("\n=== Test: Parse Nested JSON ===\n");
	printf("=== 测试：解析嵌套JSON ===\n");
	
	const char* json = "{\"user\":{\"name\":\"Bob\",\"scores\":[85,90,78]}}";
	printf("Input: %s\n", json);
	printf("输入: %s\n", json);
	
	xvalue pRoot = xrtParseJSON((char*)json, strlen(json));
	if (pRoot)
	{
		printf("Parsed successfully!\n");
		printf("解析成功！\n");
		
		xvalue pUser = xvoTableGetValue(pRoot, "user", 4);
		if (pUser)
		{
			xvalue pName = xvoTableGetValue(pUser, "name", 4);
			if (pName)
			{
				printf("  user.name: %s\n", pName->vText);
				xvoUnref(pName);
			}
			
			xvalue pScores = xvoTableGetValue(pUser, "scores", 6);
			if (pScores)
			{
				printf("  user.scores: [");
				for (uint32 i = 0; i < xvoArrayItemCount(pScores); i++)
				{
					xvalue pItem = xvoArrayGetValue(pScores, i);
					if (pItem)
					{
						if (i > 0) printf(", ");
						printf("%lld", (long long)pItem->vInt);
						xvoUnref(pItem);
					}
				}
				printf("]\n");
				xvoUnref(pScores);
			}
			
			xvoUnref(pUser);
		}
		
		xvoUnref(pRoot);
	}
}

int main(void)
{
	xrtInit();
	
	printf("========================================\n");
	printf("  Parse JSON Example / 解析JSON示例\n");
	printf("========================================\n");
	
	test_parse_object();
	test_parse_array();
	test_parse_nested();
	
	printf("\n=== All tests completed! ===\n");
	printf("=== 所有测试完成！===\n");
	
	xrtUnit();
	return 0;
}
