#define XRT_MODULE_TEMPLATE_CONTROL
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件控制层必须带入核心模板和完整表达式执行能力。 */
int main(void)
{
	xtemplate* pTemplate;
	str sResult;
	int iResult;

	#if !defined(XRT_FEATURE_TEMPLATE_CONTROL) || \
		!defined(XRT_FEATURE_TEMPLATE_CORE) || \
		!defined(XRT_FEATURE_VALUE_CONTAINER)
		#error "XRT_MODULE_TEMPLATE_CONTROL did not enable its dependency closure"
	#endif

	pTemplate = xrtTemplateCompile(XRT_STR_LITERAL(
		"{#for:1:3}{#if:loop.value != 2}{%loop.value}{#end}{#end}"
	));
	if ( pTemplate == NULL ) {
		return 1;
	}
	sResult = xrtTemplateRender(pTemplate, NULL, NULL);
	iResult = (sResult != NULL) && (strcmp(sResult, "13") == 0)
		? 0 : 2;
	xrtFree(sResult);
	xrtTemplateRelease(pTemplate);
	return iResult;
}
