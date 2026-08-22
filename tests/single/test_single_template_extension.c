#define XRT_MODULE_TEMPLATE_EXTENSION
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件扩展回调直接写出一个固定分片。 */
static bool testSingleTemplateExtension(xtemplatecall* pCall)
{
	return xrtTemplateCallWrite(pCall, XRT_STR_LITERAL("single"));
}



/* 验证单头文件扩展模块包含完整依赖并能够编译和渲染。 */
int main(void)
{
	xtemplateextension Extension = {
		XRT_STR_LITERAL("word"),
		XTEMPLATE_EXTENSION_FUNCTION,
		0u,
		0u,
		testSingleTemplateExtension,
		NULL,
		NULL
	};
	xtemplateregistry* pRegistry = xrtTemplateRegistryCreate(&Extension, 1u);
	xtemplateconfig Config;
	xtemplate* pTemplate;
	str sOutput;

	#if !defined(XRT_FEATURE_TEMPLATE_EXTENSION) || \
		!defined(XRT_FEATURE_TEMPLATE_COMPOSE) || \
		!defined(XRT_FEATURE_TEMPLATE_CONTROL) || \
		!defined(XRT_FEATURE_TEMPLATE_CORE)
		#error "XRT_MODULE_TEMPLATE_EXTENSION did not enable its dependency closure"
	#endif

	if ( pRegistry == NULL ) {
		return 1;
	}
	xrtTemplateConfigInit(&Config);
	Config.Registry = pRegistry;
	pTemplate = xrtTemplateCompileConfig(
		XRT_STR_LITERAL("{@word}"),
		&Config
	);
	xrtTemplateRegistryRelease(pRegistry);
	if ( pTemplate == NULL ) {
		return 2;
	}
	sOutput = xrtTemplateRender(pTemplate, NULL, NULL);
	if ( (sOutput == NULL) || (strcmp(sOutput, "single") != 0) ) {
		return 3;
	}
	xrtFree(sOutput);
	xrtTemplateRelease(pTemplate);
	return 0;
}
