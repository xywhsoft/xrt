/**
 * @file main.c
 * @brief SAX Printer Example - xrtJsonPrintStart/Value/Finish
 *        SAX打印机示例 - xrtJsonPrintStart/Value/Finish
 * 
 * This example demonstrates:
 * - Building JSON using SAX printer API
 * - Incremental JSON construction
 * 
 * 本示例演示：
 * - 使用SAX打印机API构建JSON
 * - 增量JSON构造
 * 
 * Build: tcc main.c -o ../../bin/json_sax_printer.exe
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

void test_sax_printer_object(void)
{
	printf("=== Test: SAX Printer - Object ===\n");
	printf("=== 测试：SAX打印机 - 对象 ===\n");
	
	json_print_ptr_t ptr = {0};
	json_sax_print_hd handle = json_sax_print_format_start(256, &ptr);
	if (!handle)
	{
		printf("Failed to start printer\n");
		return;
	}
	
	xrtJsonPrintObjectStart(handle, NULL);
	
	json_string_t key = {0};
	json_string_t val = {0};
	
	key.str = "name";
	key.info.len = 4;
	val.str = "Alice";
	val.info.len = 5;
	xrtJsonPrintString(handle, &key, &val);
	
	key.str = "age";
	key.info.len = 3;
	xrtJsonPrintInt(handle, &key, 30);
	
	key.str = "active";
	key.info.len = 6;
	xrtJsonPrintBool(handle, &key, true);
	
	key.str = "score";
	key.info.len = 5;
	xrtJsonPrintDouble(handle, &key, 95.5);
	
	xrtJsonPrintObjectFinish(handle);
	
	size_t length = 0;
	char* result = xrtJsonPrintFinish(handle, &length, &ptr);
	
	if (result)
	{
		printf("Generated JSON:\n");
		printf("生成的JSON:\n");
		printf("%.*s\n", (int)length, result);
	}
}

void test_sax_printer_array(void)
{
	printf("\n=== Test: SAX Printer - Array ===\n");
	printf("=== 测试：SAX打印机 - 数组 ===\n");
	
	json_print_ptr_t ptr = {0};
	json_sax_print_hd handle = json_sax_print_format_start(256, &ptr);
	if (!handle)
	{
		printf("Failed to start printer\n");
		return;
	}
	
	xrtJsonPrintArrayStart(handle, NULL);
	
	xrtJsonPrintInt(handle, NULL, 10);
	xrtJsonPrintInt(handle, NULL, 20);
	xrtJsonPrintInt(handle, NULL, 30);
	
	json_string_t str = {0};
	str.str = "hello";
	str.info.len = 5;
	xrtJsonPrintString(handle, NULL, &str);
	
	xrtJsonPrintBool(handle, NULL, true);
	xrtJsonPrintNull(handle, NULL);
	
	xrtJsonPrintArrayFinish(handle);
	
	size_t length = 0;
	char* result = xrtJsonPrintFinish(handle, &length, &ptr);
	
	if (result)
	{
		printf("Generated JSON:\n");
		printf("生成的JSON:\n");
		printf("%.*s\n", (int)length, result);
	}
}

void test_sax_printer_nested(void)
{
	printf("\n=== Test: SAX Printer - Nested ===\n");
	printf("=== 测试：SAX打印机 - 嵌套 ===\n");
	
	json_print_ptr_t ptr = {0};
	json_sax_print_hd handle = json_sax_print_format_start(512, &ptr);
	if (!handle)
	{
		printf("Failed to start printer\n");
		return;
	}
	
	json_string_t key = {0};
	json_string_t val = {0};
	
	xrtJsonPrintObjectStart(handle, NULL);
	
	key.str = "user";
	key.info.len = 4;
	xrtJsonPrintObjectStart(handle, &key);
	
	key.str = "name";
	key.info.len = 4;
	val.str = "Bob";
	val.info.len = 3;
	xrtJsonPrintString(handle, &key, &val);
	
	key.str = "id";
	key.info.len = 2;
	xrtJsonPrintInt(handle, &key, 12345);
	
	xrtJsonPrintObjectFinish(handle);
	
	key.str = "tags";
	key.info.len = 4;
	xrtJsonPrintArrayStart(handle, &key);
	
	json_string_t tag = {0};
	tag.str = "admin";
	tag.info.len = 5;
	xrtJsonPrintString(handle, NULL, &tag);
	
	tag.str = "user";
	tag.info.len = 4;
	xrtJsonPrintString(handle, NULL, &tag);
	
	xrtJsonPrintArrayFinish(handle);
	
	xrtJsonPrintObjectFinish(handle);
	
	size_t length = 0;
	char* result = xrtJsonPrintFinish(handle, &length, &ptr);
	
	if (result)
	{
		printf("Generated JSON:\n");
		printf("生成的JSON:\n");
		printf("%.*s\n", (int)length, result);
	}
}

void test_sax_printer_compact(void)
{
	printf("\n=== Test: SAX Printer - Compact ===\n");
	printf("=== 测试：SAX打印机 - 紧凑格式 ===\n");
	
	json_print_ptr_t ptr = {0};
	json_sax_print_hd handle = json_sax_print_unformat_start(256, &ptr);
	if (!handle)
	{
		printf("Failed to start printer\n");
		return;
	}
	
	xrtJsonPrintObjectStart(handle, NULL);
	
	json_string_t key = {0};
	json_string_t val = {0};
	
	key.str = "compact";
	key.info.len = 7;
	val.str = "true";
	val.info.len = 4;
	xrtJsonPrintString(handle, &key, &val);
	
	xrtJsonPrintObjectFinish(handle);
	
	size_t length = 0;
	char* result = xrtJsonPrintFinish(handle, &length, &ptr);
	
	if (result)
	{
		printf("Compact JSON:\n");
		printf("紧凑JSON:\n");
		printf("%.*s\n", (int)length, result);
	}
}

int main(void)
{
	xrtInit();
	
	printf("========================================\n");
	printf("  SAX Printer Example / SAX打印机示例\n");
	printf("========================================\n");
	
	test_sax_printer_object();
	test_sax_printer_array();
	test_sax_printer_nested();
	test_sax_printer_compact();
	
	printf("\n=== All tests completed! ===\n");
	printf("=== 所有测试完成！===\n");
	
	xrtUnit();
	return 0;
}
