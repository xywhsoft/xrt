/*
 * XRT Example - Template Variables
 * XRT 范例 - 模板变量
 *
 * Description / 说明:
 *   EN: Demonstrates nested paths, array indexing and mixed value rendering.
 *   CN: 演示嵌套路径、数组索引和混合值渲染。
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"


void procTestNestedVars(void)
{
	xvalue pRoot = xvoCreateTable();
	xvalue pUser = xvoCreateTable();
	xvalue pContact = xvoCreateTable();
	xtetemplate hTemplate = xteParse("User={$user.name}, Email={$user.contact.email}", 0, NULL);
	str sResult = NULL;
	size_t iRetSize = 0;

	xvoTableSetText(pUser, "name", 0, "John Doe", 0, FALSE);
	xvoTableSetText(pContact, "email", 0, "john@example.com", 0, FALSE);
	xvoTableSetText(pContact, "phone", 0, "123-456-7890", 0, FALSE);
	xvoTableSetValue(pUser, "contact", 0, pContact, TRUE);
	xvoTableSetValue(pRoot, "user", 0, pUser, TRUE);

	sResult = xteMake(hTemplate, pRoot, NULL, NULL, &iRetSize);
	printf("nested vars: %s\n", sResult);
	printf("嵌套变量: %s\n\n", sResult);

	xrtFree(sResult);
	xteDestroyTemplate(hTemplate);
	xvoUnref(pRoot);
}


void procTestArrayAccess(void)
{
	xvalue pRoot = xvoCreateTable();
	xvalue pItems = xvoCreateArray();
	xtetemplate hTemplate = xteParse("First={$items[0]}, Second={$items[1]}, Third={$items[2]}", 0, NULL);
	str sResult = NULL;
	size_t iRetSize = 0;

	xvoArrayAppendText(pItems, "Apple", 0, FALSE);
	xvoArrayAppendText(pItems, "Banana", 0, FALSE);
	xvoArrayAppendText(pItems, "Cherry", 0, FALSE);
	xvoTableSetValue(pRoot, "items", 0, pItems, TRUE);

	sResult = xteMake(hTemplate, pRoot, NULL, NULL, &iRetSize);
	printf("array access: %s\n", sResult);
	printf("数组访问: %s\n\n", sResult);

	xrtFree(sResult);
	xteDestroyTemplate(hTemplate);
	xvoUnref(pRoot);
}


void procTestMixedData(void)
{
	xvalue pRoot = xvoCreateTable();
	xvalue pProduct = xvoCreateTable();
	xtetemplate hTemplate = xteParse("Product={$product.name}, Price={%product.price:.2}, Stock={?product.available:yes:no}", 0, NULL);
	str sResult = NULL;
	size_t iRetSize = 0;

	xvoTableSetText(pProduct, "name", 0, "XRT Library", 0, FALSE);
	xvoTableSetFloat(pProduct, "price", 0, 99.99);
	xvoTableSetBool(pProduct, "available", 0, TRUE);
	xvoTableSetValue(pRoot, "product", 0, pProduct, TRUE);

	sResult = xteMake(hTemplate, pRoot, NULL, NULL, &iRetSize);
	printf("mixed data: %s\n", sResult);
	printf("混合数据: %s\n\n", sResult);

	xrtFree(sResult);
	xteDestroyTemplate(hTemplate);
	xvoUnref(pRoot);
}


int main(void)
{
	xrtInit();

	printf("XRT Template - Template Variables Demo\n");
	printf("XRT 模板 - 模板变量演示\n");
	printf("======================================\n\n");

	procTestNestedVars();
	procTestArrayAccess();
	procTestMixedData();

	xrtUnit();
	return 0;
}
