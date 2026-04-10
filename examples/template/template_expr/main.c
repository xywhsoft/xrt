/*
 * XRT Example - Template Expressions
 * XRT 范例 - 模板表达式
 *
 * Description / 说明:
 *   EN: Demonstrates current expression support through if/elseif and inline bool output.
 *   CN: 通过 if/elseif 和内联布尔输出演示当前模板表达式能力。
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"


xvalue procMakeExprData(void)
{
	xvalue pRoot = xvoCreateTable();
	xvalue pUser = xvoCreateTable();

	xvoTableSetInt(pRoot, "count", 0, 1234567);
	xvoTableSetBool(pRoot, "active", 0, TRUE);
	xvoTableSetInt(pRoot, "score", 0, 88);
	xvoTableSetText(pUser, "name", 0, "Alice", 0, FALSE);
	xvoTableSetValue(pRoot, "user", 0, pUser, TRUE);
	return pRoot;
}


void procTestIfExpression(void)
{
	xvalue pData = procMakeExprData();
	xtetemplate hTemplate = xteParse(
		"{#if:(count >= 1000000) and active}big{#else}small{#end}|"
		"{#if:(score >= 90) or active}pass{#else}fail{#end}|"
		"{#if:not false}yes{#else}no{#end}",
		0,
		NULL);
	str sResult = NULL;
	size_t iRetSize = 0;

	sResult = xteMake(hTemplate, pData, NULL, NULL, &iRetSize);
	printf("if expr: %s\n", sResult);
	printf("if 表达式: %s\n\n", sResult);

	xrtFree(sResult);
	xteDestroyTemplate(hTemplate);
	xvoUnref(pData);
}


void procTestInlineBool(void)
{
	xvalue pData = procMakeExprData();
	xtetemplate hTemplate = xteParse(
		"{?user.name = 'Alice':match:miss}|"
		"{?(count < 10) or (user.name = 'Alice'):hit:skip}",
		0,
		NULL);
	str sResult = NULL;
	size_t iRetSize = 0;

	sResult = xteMake(hTemplate, pData, NULL, NULL, &iRetSize);
	printf("inline bool: %s\n", sResult);
	printf("内联布尔: %s\n\n", sResult);

	xrtFree(sResult);
	xteDestroyTemplate(hTemplate);
	xvoUnref(pData);
}


void procTestResolvePath(void)
{
	xvalue pData = procMakeExprData();
	xvalue pName = xteResolvePath("user.name", 0, pData, pData, NULL, NULL);
	xvalue pMissing = xteResolvePath("user.missing", 0, pData, pData, NULL, NULL);

	printf("resolve user.name   = %s\n", xvoGetText(pName));
	printf("resolve user.missing type = %u\n", (unsigned)pMissing->Type);
	printf("路径解析 user.name = %s\n\n", xvoGetText(pName));

	xvoUnref(pData);
}


int main(void)
{
	xrtInit();

	printf("XRT Template - Template Expression Demo\n");
	printf("XRT 模板 - 模板表达式演示\n");
	printf("=======================================\n\n");

	procTestIfExpression();
	procTestInlineBool();
	procTestResolvePath();

	xrtUnit();
	return 0;
}
