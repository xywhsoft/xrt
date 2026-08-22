#ifdef XREGEX_MODULE_XREGEX
	#undef XREGEX_MODULE_XREGEX
#endif
#define XREGEX_MODULE_REGEX_SPLIT
#define XREGEX_IMPLEMENTATION
#include "../../single/xregex.h"



/* 单头文件拆分根必须带入 matcher、Unicode 和字符串拆分列表。 */
int main(void)
{
	xregex* pRegex;
	xstrlist* pList;
	int iResult;

	#if !defined(XREGEX_FEATURE_REGEX_SPLIT) || \
		!defined(XREGEX_FEATURE_REGEX_MATCH) || \
		!defined(XREGEX_FEATURE_REGEX_CORE) || \
		!defined(XRT_FEATURE_UNICODE) || \
		!defined(XRT_FEATURE_STRING_SPLIT)
		#error "XREGEX_MODULE_REGEX_SPLIT did not enable its dependency closure"
	#endif
	#if defined(XREGEX_FEATURE_REGEX_REPLACE) || defined(XREGEX_FEATURE_REGEX_SET)
		#error "XRT_MODULE_REGEX_SPLIT enabled unrelated regex extensions"
	#endif

	pRegex = xrtRegexCompile(XRT_STR_LITERAL("\\s+"));
	if ( pRegex == NULL ) {
		return 1;
	}
	pList = xrtRegexSplit(pRegex, XRT_STR_LITERAL("one two three"));
	iResult = (pList != NULL) && (pList->Count == 3u) ? 0 : 2;
	xrtStrListFree(pList);
	xrtRegexRelease(pRegex);
	return iResult;
}
