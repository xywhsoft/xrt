/**
 * @file main.c
 * @brief Template Sub-templates Example - Includes and modularity
 *        模板子模板示例 - 包含和模块化
 * 
 * This example demonstrates:
 * - Using include dictionary for sub-templates
 * - Template modularity and reuse
 * - Dynamic template composition
 * 
 * 本示例演示：
 * - 使用包含字典进行子模板
 * - 模板模块化和重用
 * - 动态模板组合
 * 
 * Build: tcc main.c -o ../../bin/template_template_sub.exe
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

void test_basic_include(void)
{
	printf("=== Test: Basic Include ===\n");
	printf("=== 测试：基础包含 ===\n");
	
	XTE_LiteObject headerTemplate = xteParse((char*)"<header>{{title}}</header>", 25, "{{}}");
	XTE_LiteObject footerTemplate = xteParse((char*)"<footer>{{copyright}}</footer>", 29, "{{}}");
	
	const char* mainTemplate = 
		"<html>"
		"{% include header %}"
		"<body>{{content}}</body>"
		"{% include footer %}"
		"</html>";
	printf("Main Template: %s\n", mainTemplate);
	printf("主模板: %s\n", mainTemplate);
	
	XTE_LiteObject objTemplate = xteParse((char*)mainTemplate, strlen(mainTemplate), "{{}}");
	if (!objTemplate)
	{
		printf("Failed to parse main template\n");
		xteParseFree(headerTemplate);
		xteParseFree(footerTemplate);
		return;
	}
	
	xdict tblInclude = xrtDictCreate(sizeof(XTE_LiteObject), XRT_OBJMODE_LOCAL);
	int bNew = 0;
	XTE_LiteObject* pSlot = NULL;
	
	pSlot = (XTE_LiteObject*)xrtDictSet(tblInclude, "header", 6, &bNew);
	*pSlot = headerTemplate;
	
	pSlot = (XTE_LiteObject*)xrtDictSet(tblInclude, "footer", 6, &bNew);
	*pSlot = footerTemplate;
	
	xvalue tblVal = xvoTableCreate();
	xvoTableSetString(tblVal, "title", 5, "My Page");
	xvoTableSetString(tblVal, "content", 7, "Welcome to my website!");
	xvoTableSetString(tblVal, "copyright", 9, "2024 XRT Library");
	
	size_t nSize = 0;
	char* result = xteMake(objTemplate, tblVal, xvoNull(), tblInclude, &nSize);
	
	if (result)
	{
		printf("\nResult:\n");
		printf("结果:\n");
		printf("%.*s\n", (int)nSize, result);
	}
	
	xvoUnref(tblVal);
	xteParseFree(objTemplate);
	xrtDictDestroy(tblInclude);
}

void test_nested_includes(void)
{
	printf("\n=== Test: Nested Includes ===\n");
	printf("=== 测试：嵌套包含 ===\n");
	
	XTE_LiteObject navItemTemplate = xteParse((char*)"<li>{{item}}</li>", 16, "{{}}");
	XTE_LiteObject navTemplate = xteParse((char*)"<nav><ul>{% include nav_item %}</ul></nav>", 41, "{{}}");
	
	const char* mainTemplate = 
		"<div>"
		"{% include nav %}"
		"<section>{{body}}</section>"
		"</div>";
	printf("Main Template: %s\n", mainTemplate);
	printf("主模板: %s\n", mainTemplate);
	
	XTE_LiteObject objTemplate = xteParse((char*)mainTemplate, strlen(mainTemplate), "{{}}");
	if (!objTemplate)
	{
		printf("Failed to parse main template\n");
		xteParseFree(navTemplate);
		xteParseFree(navItemTemplate);
		return;
	}
	
	xdict tblInclude = xrtDictCreate(sizeof(XTE_LiteObject), XRT_OBJMODE_LOCAL);
	int bNew = 0;
	XTE_LiteObject* pSlot = NULL;
	
	pSlot = (XTE_LiteObject*)xrtDictSet(tblInclude, "nav", 3, &bNew);
	*pSlot = navTemplate;
	
	pSlot = (XTE_LiteObject*)xrtDictSet(tblInclude, "nav_item", 8, &bNew);
	*pSlot = navItemTemplate;
	
	xvalue tblVal = xvoTableCreate();
	xvoTableSetString(tblVal, "item", 4, "Home");
	xvoTableSetString(tblVal, "body", 4, "Main content goes here");
	
	size_t nSize = 0;
	char* result = xteMake(objTemplate, tblVal, xvoNull(), tblInclude, &nSize);
	
	if (result)
	{
		printf("\nResult:\n");
		printf("结果:\n");
		printf("%.*s\n", (int)nSize, result);
	}
	
	xvoUnref(tblVal);
	xteParseFree(objTemplate);
	xrtDictDestroy(tblInclude);
}

void test_dynamic_includes(void)
{
	printf("\n=== Test: Dynamic Include Selection ===\n");
	printf("=== 测试：动态包含选择 ===\n");
	
	XTE_LiteObject greetingEn = xteParse((char*)"Hello, {{name}}!", 15, "{{}}");
	XTE_LiteObject greetingZh = xteParse((char*)"你好, {{name}}!", 14, "{{}}");
	
	xdict tblInclude = xrtDictCreate(sizeof(XTE_LiteObject), XRT_OBJMODE_LOCAL);
	int bNew = 0;
	XTE_LiteObject* pSlot = NULL;
	
	pSlot = (XTE_LiteObject*)xrtDictSet(tblInclude, "greeting_en", 11, &bNew);
	*pSlot = greetingEn;
	
	pSlot = (XTE_LiteObject*)xrtDictSet(tblInclude, "greeting_zh", 11, &bNew);
	*pSlot = greetingZh;
	
	xvalue tblVal = xvoTableCreate();
	xvoTableSetString(tblVal, "name", 4, "World");
	
	printf("Using greeting_en:\n");
	XTE_LiteObject objTemplate = xteParse((char*)"{% include greeting_en %}", 24, "{{}}");
	size_t nSize = 0;
	char* result = xteMake(objTemplate, tblVal, xvoNull(), tblInclude, &nSize);
	if (result)
	{
		printf("%.*s\n", (int)nSize, result);
	}
	xteParseFree(objTemplate);
	
	printf("\nUsing greeting_zh:\n");
	objTemplate = xteParse((char*)"{% include greeting_zh %}", 24, "{{}}");
	result = xteMake(objTemplate, tblVal, xvoNull(), tblInclude, &nSize);
	if (result)
	{
		printf("%.*s\n", (int)nSize, result);
	}
	xteParseFree(objTemplate);
	
	xvoUnref(tblVal);
	xrtDictDestroy(tblInclude);
}

int main(void)
{
	xrtInit();
	
	printf("========================================\n");
	printf("  Template Sub Example / 模板子模板示例\n");
	printf("========================================\n");
	
	test_basic_include();
	test_nested_includes();
	test_dynamic_includes();
	
	printf("\n=== All tests completed! ===\n");
	printf("=== 所有测试完成！===\n");
	
	xrtUnit();
	return 0;
}
