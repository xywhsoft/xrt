/*
 * XRT Example - Template Control
 * XRT 范例 - 模板控制语句
 *
 * Description / 说明:
 *   EN: Demonstrates if/else, for, foreach, break and continue rendering.
 *   CN: 演示 if/else、for、foreach、break、continue 的渲染效果。
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"


void procTestConditional(void)
{
	xvalue pData = xvoCreateTable();
	xtetemplate hTemplate = xteParse(
		"{#if:show_message}Message={$message}{#else}Message=hidden{#end}|"
		"{#if:is_admin}Admin{#else}User{#end}",
		0,
		NULL);
	str sResult = NULL;
	size_t iRetSize = 0;

	xvoTableSetBool(pData, "show_message", 0, TRUE);
	xvoTableSetText(pData, "message", 0, "Hello from XRT", 0, FALSE);
	xvoTableSetBool(pData, "is_admin", 0, TRUE);

	sResult = xteMake(hTemplate, pData, NULL, NULL, &iRetSize);
	printf("conditional #1: %s\n", sResult);

	xrtFree(sResult);
	xvoTableSetBool(pData, "is_admin", 0, FALSE);
	sResult = xteMake(hTemplate, pData, NULL, NULL, &iRetSize);
	printf("conditional #2: %s\n\n", sResult);

	xrtFree(sResult);
	xteDestroyTemplate(hTemplate);
	xvoUnref(pData);
}


void procTestForLoop(void)
{
	xtetemplate hTemplate = xteParse("{#for:1:5:2}{%__index__},{#end}", 0, NULL);
	str sResult = NULL;
	size_t iRetSize = 0;

	sResult = xteMake(hTemplate, NULL, NULL, NULL, &iRetSize);
	printf("for loop: %s\n", sResult);
	printf("for 循环: %s\n\n", sResult);

	xrtFree(sResult);
	xteDestroyTemplate(hTemplate);
}


void procTestForeach(void)
{
	xvalue pData = xvoCreateTable();
	xvalue pUsers = xvoCreateArray();
	xvalue pUserA = xvoCreateTable();
	xvalue pUserB = xvoCreateTable();
	xvalue pUserC = xvoCreateTable();
	xtetemplate hTemplate = xteParse(
		"{#foreach:users}"
		"{#if:skip}{#continue}{#end}"
		"{$name}:{?active:1:0}|"
		"{#if:stop}{#break}{#end}"
		"{#end}",
		0,
		NULL);
	str sResult = NULL;
	size_t iRetSize = 0;

	xvoTableSetText(pUserA, "name", 0, "Alice", 0, FALSE);
	xvoTableSetBool(pUserA, "active", 0, TRUE);
	xvoTableSetBool(pUserA, "skip", 0, FALSE);
	xvoTableSetBool(pUserA, "stop", 0, FALSE);
	xvoArrayAppendValue(pUsers, pUserA, TRUE);

	xvoTableSetText(pUserB, "name", 0, "Bob", 0, FALSE);
	xvoTableSetBool(pUserB, "active", 0, FALSE);
	xvoTableSetBool(pUserB, "skip", 0, FALSE);
	xvoTableSetBool(pUserB, "stop", 0, TRUE);
	xvoArrayAppendValue(pUsers, pUserB, TRUE);

	xvoTableSetText(pUserC, "name", 0, "Cat", 0, FALSE);
	xvoTableSetBool(pUserC, "active", 0, TRUE);
	xvoTableSetBool(pUserC, "skip", 0, TRUE);
	xvoTableSetBool(pUserC, "stop", 0, FALSE);
	xvoArrayAppendValue(pUsers, pUserC, TRUE);

	xvoTableSetValue(pData, "users", 0, pUsers, TRUE);
	sResult = xteMake(hTemplate, pData, NULL, NULL, &iRetSize);
	printf("foreach: %s\n", sResult);
	printf("foreach: %s\n\n", sResult);

	xrtFree(sResult);
	xteDestroyTemplate(hTemplate);
	xvoUnref(pData);
}


int main(void)
{
	xrtInit();

	printf("XRT Template - Template Control Demo\n");
	printf("XRT 模板 - 模板控制演示\n");
	printf("====================================\n\n");

	procTestConditional();
	procTestForLoop();
	procTestForeach();

	xrtUnit();
	return 0;
}
