#ifdef XRT_MODULE_REGEX
	#undef XRT_MODULE_REGEX
#endif
#define XRT_MODULE_REGEX_SPLIT
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件拆分根必须带入 matcher、Unicode 和字符串拆分列表。 */
int main(void)
{
	xregex* pRegex;
	xstrlist* pList;
	int iResult;

	#if !defined(XRT_FEATURE_REGEX_SPLIT) || \
		!defined(XRT_FEATURE_REGEX_MATCH) || \
		!defined(XRT_FEATURE_REGEX_CORE) || \
		!defined(XRT_FEATURE_UNICODE) || \
		!defined(XRT_FEATURE_STRING_SPLIT)
		#error "XRT_MODULE_REGEX_SPLIT did not enable its dependency closure"
	#endif
	#if defined(XRT_FEATURE_REGEX_REPLACE) || defined(XRT_FEATURE_REGEX_SET)
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
