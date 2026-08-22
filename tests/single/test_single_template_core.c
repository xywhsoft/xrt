#define XRT_MODULE_TEMPLATE_CORE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件模板核心入口必须带入完整值路径与格式化依赖。 */
int main(void)
{
	xvalue* pData;
	xtemplate* pTemplate;
	str sResult;
	int iResult;

	#if !defined(XRT_FEATURE_TEMPLATE_CORE) || \
		!defined(XRT_FEATURE_STRING) || \
		!defined(XRT_FEATURE_VALUE_CONTAINER) || \
		!defined(XRT_FEATURE_NUMBER_FORMAT) || \
		!defined(XRT_FEATURE_TIME_TEXT)
		#error "XRT_MODULE_TEMPLATE_CORE did not enable its dependency closure"
	#endif

	pData = xrtValueObject();
	if ( (pData == NULL) || !xrtValueObjectSetNew(
		pData,
		XRT_STR_LITERAL("name"),
		xrtValueString(XRT_STR_LITERAL("xrt"))
	) ) {
		xrtValueRelease(pData);
		return 1;
	}
	pTemplate = xrtTemplateCompile(XRT_STR_LITERAL("hello {$name}"));
	if ( pTemplate == NULL ) {
		xrtValueRelease(pData);
		return 2;
	}
	sResult = xrtTemplateRender(pTemplate, pData, NULL);
	iResult = (sResult != NULL) && (strcmp(sResult, "hello xrt") == 0)
		? 0 : 3;
	xrtFree(sResult);
	xrtTemplateRelease(pTemplate);
	xrtValueRelease(pData);
	return iResult;
}
