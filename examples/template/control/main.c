#include <stdio.h>

#include <xrt.h>



/* 演示条件、容器作用域、范围循环、循环元数据和流程控制。 */
int main(void)
{
	xvalue* pRoot = xrtValueObject();
	xvalue* pUsers = xrtValueArray();
	xtemplate* pTemplate;
	str sOutput;

	if ( (pRoot == NULL) || (pUsers == NULL) ||
		 !xrtValueArrayAppendNew(
			pUsers,
			xrtValueString(XRT_STR_LITERAL("Alice"))
		) || !xrtValueArrayAppendNew(
			pUsers,
			xrtValueString(XRT_STR_LITERAL("Bob"))
		) || !xrtValueObjectSetTake(
			pRoot,
			XRT_STR_LITERAL("users"),
			&pUsers
		) ) {
		xrtValueRelease(pUsers);
		xrtValueRelease(pRoot);
		return 1;
	}
	pTemplate = xrtTemplateCompile(XRT_STR_LITERAL(
		"{#if:users}Users: {#foreach:users}"
		"{?loop.first::, }{$loop.value}{#end}{#else}No users{#end}; "
		"range: {#for:1:5}"
		"{#if:loop.value = 2}{#continue}{#end}"
		"{#if:loop.value = 4}{#break}{#end}"
		"{%loop.value}{#end}"
	));
	if ( pTemplate == NULL ) {
		xrtValueRelease(pRoot);
		return 2;
	}
	sOutput = xrtTemplateRender(pTemplate, pRoot, NULL);
	if ( sOutput == NULL ) {
		xrtTemplateRelease(pTemplate);
		xrtValueRelease(pRoot);
		return 3;
	}
	puts(sOutput);
	xrtFree(sOutput);
	xrtTemplateRelease(pTemplate);
	xrtValueRelease(pRoot);
	return 0;
}
