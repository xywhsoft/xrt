/*
 * XRT Example - Basic Template
 * XRT 范例 - 基础模板
 *
 * Description / 说明:
 *   EN: Demonstrates basic template parsing and rendering with current XTE API.
 *   CN: 使用当前 XTE API 演示基础模板解析与渲染。
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"


void procRenderGreeting(void)
{
	xvalue pData = xvoCreateTable();
	xtetemplate hTemplate = xteParse("Hello {$name}! Welcome to {$place}.", 0, NULL);
	str sResult = NULL;
	size_t iRetSize = 0;

	xvoTableSetText(pData, "name", 0, "World", 0, FALSE);
	xvoTableSetText(pData, "place", 0, "XRT", 0, FALSE);
	sResult = xteMake(hTemplate, pData, NULL, NULL, &iRetSize);

	printf("template: Hello {$name}! Welcome to {$place}.\n");
	printf("result  : %s\n\n", sResult);

	xrtFree(sResult);
	xteDestroyTemplate(hTemplate);
	xvoUnref(pData);
}


void procRenderNumber(void)
{
	xvalue pData = xvoCreateTable();
	xtetemplate hTemplate = xteParse("Count = {%count:,}", 0, NULL);
	str sResult = NULL;
	size_t iRetSize = 0;

	xvoTableSetInt(pData, "count", 0, 1234567);
	sResult = xteMake(hTemplate, pData, NULL, NULL, &iRetSize);

	printf("template: Count = {%%count:,}\n");
	printf("result  : %s\n\n", sResult);

	xrtFree(sResult);
	xteDestroyTemplate(hTemplate);
	xvoUnref(pData);
}


void procRenderTime(void)
{
	xvalue pData = xvoCreateTable();
	xtetemplate hTemplate = xteParse("Created = {&created:yyyy-MM-dd}", 0, NULL);
	str sResult = NULL;
	size_t iRetSize = 0;

	xvoTableSetTimeSerial(pData, "created", 0, 2026, 4, 2, 12, 30, 0);
	sResult = xteMake(hTemplate, pData, NULL, NULL, &iRetSize);

	printf("template: Created = {&created:yyyy-MM-dd}\n");
	printf("result  : %s\n\n", sResult);

	xrtFree(sResult);
	xteDestroyTemplate(hTemplate);
	xvoUnref(pData);
}


int main(void)
{
	xrtInit();

	printf("XRT Template - Basic Template Demo\n");
	printf("XRT 模板 - 基础模板演示\n");
	printf("==================================\n\n");

	procRenderGreeting();
	procRenderNumber();
	procRenderTime();

	xrtUnit();
	return 0;
}
