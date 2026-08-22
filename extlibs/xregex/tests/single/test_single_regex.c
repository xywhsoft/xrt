#define XREGEX_MODULE_REGEX
#define XREGEX_IMPLEMENTATION
#include "../../single/xregex.h"



/* 单头文件 Regex 聚合入口必须带入完整的正则表达式能力。 */
int main(void)
{
	xregex* pRegex;
	str sResult;
	int iResult;

	#if !defined(XREGEX_FEATURE_REGEX) || \
		!defined(XREGEX_FEATURE_REGEX_CORE) || \
		!defined(XREGEX_FEATURE_REGEX_MATCH) || \
		!defined(XREGEX_FEATURE_REGEX_REPLACE) || \
		!defined(XREGEX_FEATURE_REGEX_SPLIT) || \
		!defined(XREGEX_FEATURE_REGEX_SET)
		#error "XREGEX_MODULE_REGEX did not enable its complete dependency closure"
	#endif

	pRegex = xrtRegexCompile(XRT_STR_LITERAL("[0-9]+"));
	if ( pRegex == NULL ) {
		return 1;
	}

	sResult = xrtRegexReplace(
		pRegex,
		XRT_STR_LITERAL("item-42"),
		XRT_STR_LITERAL("$0-ok")
	);
	iResult = (
		(sResult != NULL) &&
		(xrtStrEqual(
			xrtStrView(sResult),
			XRT_STR_LITERAL("item-42-ok")
		) == true)
	) ? 0 : 2;

	xrtFree(sResult);
	xrtRegexRelease(pRegex);
	return iResult;
}
