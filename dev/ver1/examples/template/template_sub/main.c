/*
 * XRT Example - Template Sub Templates
 * XRT 范例 - 模板子模板
 *
 * Description / 说明:
 *   EN: Demonstrates local define/include and external include map rendering.
 *   CN: 演示局部 define/include 与外部 include map 渲染。
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"


void procTestLocalDefine(void)
{
	xvalue pData = xvoCreateTable();
	xvalue pUser = xvoCreateTable();
	xtetemplate hTemplate = xteParse(
		"A{#define:'card'}[{$user.name}:{%user.age}]{#end}B{#include:'card'}C",
		0,
		NULL);
	str sResult = NULL;
	size_t iRetSize = 0;

	xvoTableSetText(pUser, "name", 0, "Alice", 0, FALSE);
	xvoTableSetInt(pUser, "age", 0, 30);
	xvoTableSetValue(pData, "user", 0, pUser, TRUE);

	sResult = xteMake(hTemplate, pData, NULL, NULL, &iRetSize);
	printf("local define/include: %s\n", sResult);
	printf("局部 define/include: %s\n\n", sResult);

	xrtFree(sResult);
	xteDestroyTemplate(hTemplate);
	xvoUnref(pData);
}


void procTestExternalInclude(void)
{
	xvalue pData = xvoCreateTable();
	xdict tblInclude = xrtDictCreate(sizeof(ptr), XRT_OBJMODE_LOCAL);
	xtetemplate hHeader = xteParse("<header>{$title}</header>", 0, NULL);
	xtetemplate hFooter = xteParse("<footer>{$copyright}</footer>", 0, NULL);
	xtetemplate hPage = xteParse("{#include:'header'}<main>{$content}</main>{#include:'footer'}", 0, NULL);
	str sResult = NULL;
	size_t iRetSize = 0;

	xvoTableSetText(pData, "title", 0, "XRT Demo", 0, FALSE);
	xvoTableSetText(pData, "content", 0, "Rendered with include map", 0, FALSE);
	xvoTableSetText(pData, "copyright", 0, "2026 XRT", 0, FALSE);

	xrtDictSetPtr(tblInclude, "header", 0, hHeader, NULL);
	xrtDictSetPtr(tblInclude, "footer", 0, hFooter, NULL);

	sResult = xteMake(hPage, pData, NULL, tblInclude, &iRetSize);
	printf("external include: %s\n", sResult);
	printf("外部 include: %s\n\n", sResult);

	xrtFree(sResult);
	xteDestroyTemplate(hPage);
	xteDestroyTemplate(hHeader);
	xteDestroyTemplate(hFooter);
	xrtDictDestroy(tblInclude);
	xvoUnref(pData);
}


void procTestNestedInclude(void)
{
	xvalue pData = xvoCreateTable();
	xdict tblInclude = xrtDictCreate(sizeof(ptr), XRT_OBJMODE_LOCAL);
	xtetemplate hItem = xteParse("<li>{$name}</li>", 0, NULL);
	xtetemplate hList = xteParse("<ul>{#include:'item'}</ul>", 0, NULL);
	xtetemplate hPage = xteParse("{#include:'list'}", 0, NULL);
	str sResult = NULL;
	size_t iRetSize = 0;

	xvoTableSetText(pData, "name", 0, "Nested Item", 0, FALSE);

	xrtDictSetPtr(tblInclude, "item", 0, hItem, NULL);
	xrtDictSetPtr(tblInclude, "list", 0, hList, NULL);

	sResult = xteMake(hPage, pData, NULL, tblInclude, &iRetSize);
	printf("nested include: %s\n", sResult);
	printf("嵌套 include: %s\n\n", sResult);

	xrtFree(sResult);
	xteDestroyTemplate(hPage);
	xteDestroyTemplate(hList);
	xteDestroyTemplate(hItem);
	xrtDictDestroy(tblInclude);
	xvoUnref(pData);
}


int main(void)
{
	xrtInit();

	printf("XRT Template - Template Sub Demo\n");
	printf("XRT 模板 - 模板子模板演示\n");
	printf("================================\n\n");

	procTestLocalDefine();
	procTestExternalInclude();
	procTestNestedInclude();

	xrtUnit();
	return 0;
}
