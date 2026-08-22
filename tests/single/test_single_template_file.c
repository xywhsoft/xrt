#define XRT_MODULE_TEMPLATE_FILE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



#define TEST_SINGLE_TEMPLATE_FILE_PATH "xrt-single-template-file.tpl"



/* 验证单头文件文件层拉起文件与模板依赖并可一行编译源码文件。 */
int main(void)
{
	xtemplate* pTemplate;
	str sOutput;

	#if !defined(XRT_FEATURE_TEMPLATE_FILE) || \
		!defined(XRT_FEATURE_TEMPLATE_CORE) || \
		!defined(XRT_FEATURE_FILE_WHOLE)
		#error "XRT_MODULE_TEMPLATE_FILE did not enable its dependency closure"
	#endif

	if ( !xrtFileWriteAll(
		TEST_SINGLE_TEMPLATE_FILE_PATH,
		(xbytesview){ (cbytes)"single file", 11u }
	) ) {
		return 1;
	}
	pTemplate = xrtTemplateCompileFile(TEST_SINGLE_TEMPLATE_FILE_PATH);
	if ( pTemplate == NULL ) {
		return 2;
	}
	sOutput = xrtTemplateRender(pTemplate, NULL, NULL);
	if ( (sOutput == NULL) || (strcmp(sOutput, "single file") != 0) ) {
		return 3;
	}
	xrtFree(sOutput);
	xrtTemplateRelease(pTemplate);
	if ( !xrtFileDelete(TEST_SINGLE_TEMPLATE_FILE_PATH) ) {
		return 4;
	}
	return 0;
}
