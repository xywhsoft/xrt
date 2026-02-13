/**
 * @file main.c
 * @brief Basic Template Example - xteParse/xteMake
 *        基础模板示例 - xteParse/xteMake
 * 
 * This example demonstrates:
 * - Template parsing and rendering
 * - Simple variable substitution
 * 
 * 本示例演示：
 * - 模板解析和渲染
 * - 简单变量替换
 * 
 * Build: tcc main.c -o ../../bin/template_basic_template.exe
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

void test_basic_template(void)
{
	printf("=== Test: Basic Template ===\n");
	printf("=== 测试：基础模板 ===\n");
	
	const char* template = "Hello, {{name}}! Welcome to {{place}}.";
	printf("Template: %s\n", template);
	printf("模板: %s\n", template);
	
	XTE_LiteObject objTemplate = xteParse((char*)template, strlen(template), "{{}}");
	if (!objTemplate)
	{
		printf("Failed to parse template\n");
		return;
	}
	
	xvalue tblVal = xvoTableCreate();
	xvoTableSetString(tblVal, "name", 4, "World");
	xvoTableSetString(tblVal, "place", 5, "XRT Library");
	
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

void test_html_template(void)
{
	printf("\n=== Test: HTML Template ===\n");
	printf("=== 测试：HTML模板 ===\n");
	
	const char* template = "<html><body><h1>{{title}}</h1><p>{{content}}</p></body></html>";
	printf("Template: %s\n", template);
	printf("模板: %s\n", template);
	
	XTE_LiteObject objTemplate = xteParse((char*)template, strlen(template), "{{}}");
	if (!objTemplate)
	{
		printf("Failed to parse template\n");
		return;
	}
	
	xvalue tblVal = xvoTableCreate();
	xvoTableSetString(tblVal, "title", 5, "Welcome Page");
	xvoTableSetString(tblVal, "content", 7, "This is a dynamically generated content.");
	
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

void test_multiple_variables(void)
{
	printf("\n=== Test: Multiple Variables ===\n");
	printf("=== 测试：多个变量 ===\n");
	
	const char* template = "{{greeting}}, {{name}}! You have {{count}} messages.";
	printf("Template: %s\n", template);
	printf("模板: %s\n", template);
	
	XTE_LiteObject objTemplate = xteParse((char*)template, strlen(template), "{{}}");
	if (!objTemplate)
	{
		printf("Failed to parse template\n");
		return;
	}
	
	xvalue tblVal = xvoTableCreate();
	xvoTableSetString(tblVal, "greeting", 8, "Good morning");
	xvoTableSetString(tblVal, "name", 4, "Alice");
	xvoTableSetInt(tblVal, "count", 5, 5);
	
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
	printf("  Basic Template Example / 基础模板示例\n");
	printf("========================================\n");
	
	test_basic_template();
	test_html_template();
	test_multiple_variables();
	
	printf("\n=== All tests completed! ===\n");
	printf("=== 所有测试完成！===\n");
	
	xrtUnit();
	return 0;
}
