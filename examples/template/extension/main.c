#include <stdio.h>

#include <xrt.h>



/* 链接块读取 href 参数，写出标签并在中间渲染模板主体。 */
static bool exampleTemplateLink(xtemplatecall* pCall)
{
	xtemplateargview Argument;
	xtemplatevalue Value;

	if ( !xrtTemplateCallArgument(pCall, 0u, &Argument) ||
		 !xrtTemplateCallEval(pCall, &Argument, &Value) ||
		 (Value.Type != XVALUE_STRING) ) {
		return false;
	}
	return xrtTemplateCallWrite(pCall, XRT_STR_LITERAL("<a href=\"")) &&
		xrtTemplateCallWrite(pCall, Value.Text) &&
		xrtTemplateCallWrite(pCall, XRT_STR_LITERAL("\">")) &&
		xrtTemplateCallRender(pCall) &&
		xrtTemplateCallWrite(pCall, XRT_STR_LITERAL("</a>"));
}



/* 展示不可变扩展注册表和可重复使用的块扩展。 */
int main(void)
{
	xtemplateextension Extension = {
		XRT_STR_LITERAL("link"),
		XTEMPLATE_EXTENSION_BLOCK,
		1u,
		1u,
		exampleTemplateLink,
		NULL,
		NULL
	};
	xtemplateregistry* pRegistry = xrtTemplateRegistryCreate(&Extension, 1u);
	xtemplateconfig Config;
	xtemplate* pTemplate;
	xvalue* pData = xrtValueObject();
	str sOutput;

	if ( (pRegistry == NULL) || (pData == NULL) ) {
		return 1;
	}
	xrtTemplateConfigInit(&Config);
	Config.Registry = pRegistry;
	pTemplate = xrtTemplateCompileConfig(
		XRT_STR_LITERAL("{#link:url}{$label}{#end}"),
		&Config
	);
	xrtTemplateRegistryRelease(pRegistry);
	if ( (pTemplate == NULL) || !xrtValueObjectSetNew(
		pData,
		XRT_STR_LITERAL("url"),
		xrtValueString(XRT_STR_LITERAL("/docs"))
	) || !xrtValueObjectSetNew(
		pData,
		XRT_STR_LITERAL("label"),
		xrtValueString(XRT_STR_LITERAL("Documentation"))
	) ) {
		return 2;
	}
	sOutput = xrtTemplateRender(pTemplate, pData, NULL);
	if ( sOutput == NULL ) {
		return 3;
	}
	printf("%s\n", sOutput);
	xrtFree(sOutput);
	xrtValueRelease(pData);
	xrtTemplateRelease(pTemplate);
	return 0;
}
