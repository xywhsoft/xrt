/**
 * @file main.c
 * @brief SAX Parser Example - xrtJsonParseSAX
 *        SAX解析器示例 - xrtJsonParseSAX
 * 
 * This example demonstrates:
 * - Streaming JSON parsing with SAX callbacks
 * - Event-based JSON processing
 * 
 * 本示例演示：
 * - 使用SAX回调进行流式JSON解析
 * - 基于事件的JSON处理
 * 
 * Build: tcc main.c -o ../../bin/json_sax_parser.exe
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

json_sax_ret_t sax_callback(json_sax_parser_t* parser)
{
	static int nCount = 0;
	nCount++;
	
	json_string_t* jkey = &parser->array[parser->index];
	json_type_t type = (json_type_t)jkey->info.type;
	
	if (type == JSON_ARRAY || type == JSON_OBJECT)
	{
		if (parser->value.vcmd == JSON_SAX_START)
			printf("  Event %d: Start %s\n", nCount, type == JSON_ARRAY ? "Array" : "Object");
		else
			printf("  Event %d: End %s\n", nCount, type == JSON_ARRAY ? "Array" : "Object");
	}
	else if (type == JSON_STRING)
	{
		if (jkey->str && jkey->info.len > 0)
		{
			printf("  Event %d: String = %.*s\n", nCount, (int)jkey->info.len, jkey->str);
		}
		else
		{
			printf("  Event %d: String = %.*s\n", nCount, 
				(int)parser->value.vstr.info.len, parser->value.vstr.str);
		}
	}
	else if (type == JSON_INT || type == JSON_LINT)
	{
		printf("  Event %d: Number (int) = %lld\n", nCount, (long long)parser->value.vnum.vlint);
	}
	else if (type == JSON_DOUBLE)
	{
		printf("  Event %d: Number (float) = %.2f\n", nCount, parser->value.vnum.vdbl);
	}
	else if (type == JSON_BOOL)
	{
		printf("  Event %d: Boolean = %s\n", nCount, parser->value.vnum.vbool ? "true" : "false");
	}
	else if (type == JSON_NULL)
	{
		printf("  Event %d: Null\n", nCount);
	}
	
	return JSON_SAX_PARSE_CONTINUE;
}

void test_sax_parser(void)
{
	printf("=== Test: SAX Parser ===\n");
	printf("=== 测试：SAX解析器 ===\n");
	
	const char* json = "{\"name\":\"Alice\",\"age\":30,\"active\":true}";
	printf("Input: %s\n", json);
	printf("输入: %s\n", json);
	
	printf("\nSAX Events:\n");
	printf("SAX事件:\n");
	
	int result = xrtJsonParseSAX((char*)json, strlen(json), sax_callback);
	
	if (result == 0)
	{
		printf("\nParse completed successfully!\n");
		printf("解析成功完成！\n");
	}
	else
	{
		printf("Parse failed with error: %d\n", result);
		printf("解析失败，错误: %d\n", result);
	}
}

void test_sax_array(void)
{
	printf("\n=== Test: SAX Parser with Array ===\n");
	printf("=== 测试：带数组的SAX解析器 ===\n");
	
	const char* json = "[1, 2, 3, \"hello\", true, null]";
	printf("Input: %s\n", json);
	printf("输入: %s\n", json);
	
	printf("\nSAX Events:\n");
	printf("SAX事件:\n");
	
	int result = xrtJsonParseSAX((char*)json, strlen(json), sax_callback);
	
	if (result == 0)
	{
		printf("\nParse completed!\n");
		printf("解析完成！\n");
	}
}

int main(void)
{
	xrtInit();
	
	printf("========================================\n");
	printf("  SAX Parser Example / SAX解析器示例\n");
	printf("========================================\n");
	
	test_sax_parser();
	test_sax_array();
	
	printf("\n=== All tests completed! ===\n");
	printf("=== 所有测试完成！===\n");
	
	xrtUnit();
	return 0;
}
