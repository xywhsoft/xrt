/**
 * @file main.c
 * @brief Template Expressions Example - xteExprParse/xteExprEval
 *        模板表达式示例 - xteExprParse/xteExprEval
 * 
 * This example demonstrates:
 * - Expression parsing and evaluation
 * - Arithmetic and comparison operators
 * - Logical operators
 * 
 * 本示例演示：
 * - 表达式解析和求值
 * - 算术和比较运算符
 * - 逻辑运算符
 * 
 * Build: tcc main.c -o ../../bin/template_template_expr.exe
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

void test_arithmetic_expr(void)
{
	printf("=== Test: Arithmetic Expressions ===\n");
	printf("=== 测试：算术表达式 ===\n");
	
	xvalue tblVal = xvoTableCreate();
	xvoTableSetInt(tblVal, "a", 1, 10);
	xvoTableSetInt(tblVal, "b", 1, 20);
	
	const char* expr1 = "a + b";
	printf("Expression: %s\n", expr1);
	
	XTE_ExprResult result = xteExprParse(expr1, strlen(expr1));
	if (result->Success)
	{
		xvalue val = xteExprEval(result->Root, tblVal, tblVal, xvoNull());
		printf("Result: %lld\n", (long long)val.vInt);
		xvoUnref(val);
	}
	else
	{
		printf("Parse error: %s at position %zu\n", result->ErrorDesc, result->ErrorPos);
	}
	xteExprFree(result);
	
	const char* expr2 = "a * 2 + b";
	printf("\nExpression: %s\n", expr2);
	
	result = xteExprParse(expr2, strlen(expr2));
	if (result->Success)
	{
		xvalue val = xteExprEval(result->Root, tblVal, tblVal, xvoNull());
		printf("Result: %lld\n", (long long)val.vInt);
		xvoUnref(val);
	}
	xteExprFree(result);
	
	xvoUnref(tblVal);
}

void test_comparison_expr(void)
{
	printf("\n=== Test: Comparison Expressions ===\n");
	printf("=== 测试：比较表达式 ===\n");
	
	xvalue tblVal = xvoTableCreate();
	xvoTableSetInt(tblVal, "age", 3, 25);
	xvoTableSetInt(tblVal, "score", 5, 85);
	
	const char* expr1 = "age >= 18";
	printf("Expression: %s (age=25)\n", expr1);
	
	XTE_ExprResult result = xteExprParse(expr1, strlen(expr1));
	if (result->Success)
	{
		xvalue val = xteExprEval(result->Root, tblVal, tblVal, xvoNull());
		printf("Result: %s\n", val.vBool ? "true" : "false");
		xvoUnref(val);
	}
	xteExprFree(result);
	
	const char* expr2 = "score > 90";
	printf("\nExpression: %s (score=85)\n", expr2);
	
	result = xteExprParse(expr2, strlen(expr2));
	if (result->Success)
	{
		xvalue val = xteExprEval(result->Root, tblVal, tblVal, xvoNull());
		printf("Result: %s\n", val.vBool ? "true" : "false");
		xvoUnref(val);
	}
	xteExprFree(result);
	
	xvoUnref(tblVal);
}

void test_logical_expr(void)
{
	printf("\n=== Test: Logical Expressions ===\n");
	printf("=== 测试：逻辑表达式 ===\n");
	
	xvalue tblVal = xvoTableCreate();
	xvoTableSetBool(tblVal, "is_admin", 8, true);
	xvoTableSetBool(tblVal, "is_active", 9, true);
	xvoTableSetInt(tblVal, "level", 5, 5);
	
	const char* expr1 = "is_admin and is_active";
	printf("Expression: %s\n", expr1);
	
	XTE_ExprResult result = xteExprParse(expr1, strlen(expr1));
	if (result->Success)
	{
		xvalue val = xteExprEval(result->Root, tblVal, tblVal, xvoNull());
		printf("Result: %s\n", val.vBool ? "true" : "false");
		xvoUnref(val);
	}
	xteExprFree(result);
	
	const char* expr2 = "is_admin or level > 10";
	printf("\nExpression: %s (level=5)\n", expr2);
	
	result = xteExprParse(expr2, strlen(expr2));
	if (result->Success)
	{
		xvalue val = xteExprEval(result->Root, tblVal, tblVal, xvoNull());
		printf("Result: %s\n", val.vBool ? "true" : "false");
		xvoUnref(val);
	}
	xteExprFree(result);
	
	const char* expr3 = "not is_admin";
	printf("\nExpression: %s\n", expr3);
	
	result = xteExprParse(expr3, strlen(expr3));
	if (result->Success)
	{
		xvalue val = xteExprEval(result->Root, tblVal, tblVal, xvoNull());
		printf("Result: %s\n", val.vBool ? "true" : "false");
		xvoUnref(val);
	}
	xteExprFree(result);
	
	xvoUnref(tblVal);
}

void test_convenience_function(void)
{
	printf("\n=== Test: Convenience Function ===\n");
	printf("=== 测试：便捷函数 ===\n");
	
	xvalue tblVal = xvoTableCreate();
	xvoTableSetInt(tblVal, "value", 5, 100);
	
	const char* expr = "value > 50";
	printf("Expression: %s\n", expr);
	
	int result = xteExprEvalBool(expr, strlen(expr), tblVal, tblVal, xvoNull());
	printf("Boolean result: %s\n", result ? "true" : "false");
	
	xvoUnref(tblVal);
}

int main(void)
{
	xrtInit();
	
	printf("========================================\n");
	printf("  Template Expressions Example / 模板表达式示例\n");
	printf("========================================\n");
	
	test_arithmetic_expr();
	test_comparison_expr();
	test_logical_expr();
	test_convenience_function();
	
	printf("\n=== All tests completed! ===\n");
	printf("=== 所有测试完成！===\n");
	
	xrtUnit();
	return 0;
}
