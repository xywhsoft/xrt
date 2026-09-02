#ifdef XRT_MODULE_REGEX
	#undef XRT_MODULE_REGEX
#endif
#define XRT_MODULE_REGEX_SET
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件集合根必须带入编译层并保留精细裁剪边界。 */
int main(void)
{
	const xstrview arrPattern[] = {
		{ "cat", 3u },
		{ "dog", 3u }
	};
	xregexset* pSet;
	xregexresult Result;

	#if !defined(XRT_FEATURE_REGEX_SET) || !defined(XRT_FEATURE_REGEX_CORE)
		#error "XRT_MODULE_REGEX_SET did not enable its dependency closure"
	#endif
	#if defined(XRT_FEATURE_REGEX_MATCH)
		#error "XRT_MODULE_REGEX_SET enabled unrelated regex matching helpers"
	#endif

	pSet = xrtRegexSetCompile(arrPattern, 2u);
	if ( pSet == NULL ) {
		return 1;
	}
	Result = xrtRegexSetTest(pSet, XRT_STR_LITERAL("a dog"));
	xrtRegexSetRelease(pSet);
	return Result == XREGEX_MATCH ? 0 : 2;
}
