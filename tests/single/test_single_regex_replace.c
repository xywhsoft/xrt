#ifdef XRT_MODULE_REGEX
	#undef XRT_MODULE_REGEX
#endif
#define XRT_MODULE_REGEX_REPLACE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件替换根必须带入 matcher、Unicode 和字符串构建器。 */
int main(void)
{
	xregex* pRegex;
	str sResult;
	int iResult;

	#if !defined(XRT_FEATURE_REGEX_REPLACE) || \
		!defined(XRT_FEATURE_REGEX_MATCH) || \
		!defined(XRT_FEATURE_REGEX_CORE) || \
		!defined(XRT_FEATURE_UNICODE) || \
		!defined(XRT_FEATURE_STRING)
		#error "XRT_MODULE_REGEX_REPLACE did not enable its dependency closure"
	#endif
	#if defined(XRT_FEATURE_REGEX_SET)
		#error "XRT_MODULE_REGEX_REPLACE enabled unrelated regex set support"
	#endif

	pRegex = xrtRegexCompile(XRT_STR_LITERAL("(\\d+)"));
	if ( pRegex == NULL ) {
		return 1;
	}
	sResult = xrtRegexReplace(
		pRegex,
		XRT_STR_LITERAL("id=42"),
		XRT_STR_LITERAL("<$1>")
	);
	iResult = (sResult != NULL) && (strcmp(sResult, "id=<42>") == 0) ? 0 : 2;
	xrtFree(sResult);
	xrtRegexRelease(pRegex);
	return iResult;
}
