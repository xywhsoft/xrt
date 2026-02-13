/**
 * @file main.c
 * @brief Template Control Flow Example - Conditionals and loops
 *        模板控制流示例 - 条件和循环
 * 
 * This example demonstrates:
 * - Conditional rendering with if/else
 * - Loop iteration
 * - Template control structures
 * 
 * 本示例演示：
 * - 使用if/else进行条件渲染
 * - 循环迭代
 * - 模板控制结构
 * 
 * Build: tcc main.c -o ../../bin/template_template_control.exe
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

void test_conditional(void)
{
	printf("=== Test: Conditional Rendering ===\n");
	printf("=== 测试：条件渲染 ===\n");
	
	const char* template = 
		"{% if show_message %}Message: {{message}}{% endif %}"
		"{% if is_admin %}Welcome Admin!{% else %}Welcome User!{% endif %}";
	printf("Template: %s\n", template);
	printf("模板: %s\n", template);
	
	XTE_LiteObject objTemplate = xteParse((char*)template, strlen(template), "{{}}");
	if (!objTemplate)
	{
		printf("Failed to parse template\n");
		return;
	}
	
	xvalue tblVal = xvoTableCreate();
	xvoTableSetBool(tblVal, "show_message", 12, true);
	xvoTableSetString(tblVal, "message", 7, "Hello from XRT!");
	xvoTableSetBool(tblVal, "is_admin", 8, true);
	
	size_t nSize = 0;
	char* result = xteMake(objTemplate, tblVal, xvoNull(), NULL, &nSize);
	
	if (result)
	{
		printf("\nResult (admin=true):\n");
		printf("结果 (admin=true):\n");
		printf("%.*s\n", (int)nSize, result);
	}
	
	xvoTableSetBool(tblVal, "is_admin", 8, false);
	result = xteMake(objTemplate, tblVal, xvoNull(), NULL, &nSize);
	
	if (result)
	{
		printf("\nResult (admin=false):\n");
		printf("结果 (admin=false):\n");
		printf("%.*s\n", (int)nSize, result);
	}
	
	xvoUnref(tblVal);
	xteParseFree(objTemplate);
}

void test_loop(void)
{
	printf("\n=== Test: Loop Iteration ===\n");
	printf("=== 测试：循环迭代 ===\n");
	
	const char* template = 
		"Items:\n"
		"{% for item in items %}"
		"  - {{item}}\n"
		"{% endfor %}";
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
	
	xvoArrayAppendString(arrItems, "First Item");
	xvoArrayAppendString(arrItems, "Second Item");
	xvoArrayAppendString(arrItems, "Third Item");
	
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

void test_nested_control(void)
{
	printf("\n=== Test: Nested Control ===\n");
	printf("=== 测试：嵌套控制 ===\n");
	
	const char* template = 
		"{% if has_items %}"
		"Items:\n"
		"{% for item in items %}"
		"  {{item.name}}: {{item.value}}\n"
		"{% endfor %}"
		"{% else %}"
		"No items available.\n"
		"{% endif %}";
	printf("Template: %s\n", template);
	printf("模板: %s\n", template);
	
	XTE_LiteObject objTemplate = xteParse((char*)template, strlen(template), "{{}}");
	if (!objTemplate)
	{
		printf("Failed to parse template\n");
		return;
	}
	
	xvalue tblVal = xvoTableCreate();
	xvoTableSetBool(tblVal, "has_items", 9, true);
	
	xvalue arrItems = xvoArrayCreate();
	
	xvalue item1 = xvoTableCreate();
	xvoTableSetString(item1, "name", 4, "Item A");
	xvoTableSetInt(item1, "value", 5, 100);
	xvoArrayAppendTable(arrItems, item1);
	
	xvalue item2 = xvoTableCreate();
	xvoTableSetString(item2, "name", 4, "Item B");
	xvoTableSetInt(item2, "value", 5, 200);
	xvoArrayAppendTable(arrItems, item2);
	
	xvoTableSetArray(tblVal, "items", 5, arrItems);
	
	size_t nSize = 0;
	char* result = xteMake(objTemplate, tblVal, xvoNull(), NULL, &nSize);
	
	if (result)
	{
		printf("\nResult (with items):\n");
		printf("结果 (有项目):\n");
		printf("%.*s\n", (int)nSize, result);
	}
	
	xvoTableSetBool(tblVal, "has_items", 9, false);
	result = xteMake(objTemplate, tblVal, xvoNull(), NULL, &nSize);
	
	if (result)
	{
		printf("\nResult (no items):\n");
		printf("结果 (无项目):\n");
		printf("%.*s\n", (int)nSize, result);
	}
	
	xvoUnref(tblVal);
	xteParseFree(objTemplate);
}

int main(void)
{
	xrtInit();
	
	printf("========================================\n");
	printf("  Template Control Example / 模板控制示例\n");
	printf("========================================\n");
	
	test_conditional();
	test_loop();
	test_nested_control();
	
	printf("\n=== All tests completed! ===\n");
	printf("=== 所有测试完成！===\n");
	
	xrtUnit();
	return 0;
}
