#define XRT_MODULE_TEMPLATE_COMPOSE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件组合层能够编译本地定义并直接渲染。 */
int main(void)
{
	xtemplate* pTemplate = xrtTemplateCompile(XRT_STR_LITERAL(
		"{#define:'item'}[{$name}]{#end}{#include:'item'}"
	));
	xvalue* pRoot = xrtValueObject();
	str sOutput;

	#if !defined(XRT_FEATURE_TEMPLATE_COMPOSE) || \
		!defined(XRT_FEATURE_TEMPLATE_CONTROL) || \
		!defined(XRT_FEATURE_TEMPLATE_CORE)
		#error "XRT_MODULE_TEMPLATE_COMPOSE did not enable its dependency closure"
	#endif

	if ( (pTemplate == NULL) || (pRoot == NULL) ||
		 !xrtValueObjectSetNew(
			pRoot,
			XRT_STR_LITERAL("name"),
			xrtValueString(XRT_STR_LITERAL("single"))
		 ) ) {
		return 1;
	}
	sOutput = xrtTemplateRender(pTemplate, pRoot, NULL);
	if ( (sOutput == NULL) || (strcmp(sOutput, "[single]") != 0) ) {
		return 2;
	}
	xrtFree(sOutput);
	xrtValueRelease(pRoot);
	xrtTemplateRelease(pTemplate);
	return 0;
}
