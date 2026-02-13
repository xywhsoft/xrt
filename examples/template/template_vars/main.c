/**
 * @file main.c
 * @brief Template Variables Example - Nested paths and arrays
 *        模板变量示例 - 嵌套路径和数组
 * 
 * This example demonstrates:
 * - Nested object access with dot notation
 * - Array index access
 * - Complex data structures in templates
 * 
 * 本示例演示：
 * - 使用点号访问嵌套对象
 * - 数组索引访问
 * - 模板中的复杂数据结构
 * 
 * Build: tcc main.c -o ../../bin/template_template_vars.exe
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

void test_nested_vars(void)
{
	printf("=== Test: Nested Variables ===\n");
	printf("=== 测试：嵌套变量 ===\n");
	
	const char* template = "User: {{user.name}}, Email: {{user.contact.email}}";
	printf("Template: %s\n", template);
	printf("模板: %s\n", template);
	
	XTE_LiteObject objTemplate = xteParse((char*)template, strlen(template), "{{}}");
	if (!objTemplate)
	{
		printf("Failed to parse template\n");
		return;
	}
	
	xvalue tblVal = xvoTableCreate();
	
	xvalue userTable = xvoTableCreate();
	xvoTableSetString(userTable, "name", 4, "John Doe");
	
	xvalue contactTable = xvoTableCreate();
	xvoTableSetString(contactTable, "email", 5, "john@example.com");
	xvoTableSetString(contactTable, "phone", 5, "123-456-7890");
	
	xvoTableSetTable(userTable, "contact", 7, contactTable);
	xvoTableSetTable(tblVal, "user", 4, userTable);
	
	size_t nSize = 0;
	char* result = xteMake(objTemplate, tblVal, xvoNull(), NULL, &nSize);
	
	if (result)
	{
		printf("\nResult:\n");
		printf("结果:\n");
		printf("%.*s\n", (int)nSize, result);
	}
	
	xvoUnref(tblVal);
	xteParseFree(objTemplate);
}

void test_array_access(void)
{
	printf("\n=== Test: Array Access ===\n");
	printf("=== 测试：数组访问 ===\n");
	
	const char* template = "First: {{items[0]}}, Second: {{items[1]}}, Third: {{items[2]}}";
	printf("Template: %s\n", template);
	printf("模板: %s\n", template);
	
	XTE_LiteObject objTemplate = xteParse((char*)template, strlen(template), "{{}}");
	if (!objTemplate)
	{
		printf("Failed to parse template\n");
		return;
	}
	
	xvalue tblVal = xvoTableCreate();
	xvalue arrItems = xvoArrayCreate();
	
	xvoArrayAppendString(arrItems, "Apple");
	xvoArrayAppendString(arrItems, "Banana");
	xvoArrayAppendString(arrItems, "Cherry");
	
	xvoTableSetArray(tblVal, "items", 5, arrItems);
	
	size_t nSize = 0;
	char* result = xteMake(objTemplate, tblVal, xvoNull(), NULL, &nSize);
	
	if (result)
	{
		printf("\nResult:\n");
		printf("结果:\n");
		printf("%.*s\n", (int)nSize, result);
	}
	
	xvoUnref(tblVal);
	xteParseFree(objTemplate);
}

void test_mixed_data(void)
{
	printf("\n=== Test: Mixed Data Types ===\n");
	printf("=== 测试：混合数据类型 ===\n");
	
	const char* template = "Product: {{product.name}}, Price: ${{product.price}}, In Stock: {{product.available}}";
	printf("Template: %s\n", template);
	printf("模板: %s\n", template);
	
	XTE_LiteObject objTemplate = xteParse((char*)template, strlen(template), "{{}}");
	if (!objTemplate)
	{
		printf("Failed to parse template\n");
		return;
	}
	
	xvalue tblVal = xvoTableCreate();
	xvalue productTable = xvoTableCreate();
	
	xvoTableSetString(productTable, "name", 4, "XRT Library");
	xvoTableSetFloat(productTable, "price", 5, 99.99);
	xvoTableSetBool(productTable, "available", 9, true);
	
	xvoTableSetTable(tblVal, "product", 7, productTable);
	
	size_t nSize = 0;
	char* result = xteMake(objTemplate, tblVal, xvoNull(), NULL, &nSize);
	
	if (result)
	{
		printf("\nResult:\n");
		printf("结果:\n");
		printf("%.*s\n", (int)nSize, result);
	}
	
	xvoUnref(tblVal);
	xteParseFree(objTemplate);
}

int main(void)
{
	xrtInit();
	
	printf("========================================\n");
	printf("  Template Variables Example / 模板变量示例\n");
	printf("========================================\n");
	
	test_nested_vars();
	test_array_access();
	test_mixed_data();
	
	printf("\n=== All tests completed! ===\n");
	printf("=== 所有测试完成！===\n");
	
	xrtUnit();
	return 0;
}
