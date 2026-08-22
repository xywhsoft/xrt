#ifdef XREGEX_MODULE_XREGEX
	#undef XREGEX_MODULE_XREGEX
#endif
#define XREGEX_MODULE_REGEX_SET
#define XREGEX_IMPLEMENTATION
#include "../../single/xregex.h"



/* 单头文件集合根必须带入编译层并保留精细裁剪边界。 */
int main(void)
{
	const xstrview arrPattern[] = {
		{ "cat", 3u },
		{ "dog", 3u }
	};
	xregexset* pSet;
	xregexresult Result;

	#if !defined(XREGEX_FEATURE_REGEX_SET) || !defined(XREGEX_FEATURE_REGEX_CORE)
		#error "XREGEX_MODULE_REGEX_SET did not enable its dependency closure"
	#endif
	#if defined(XREGEX_FEATURE_REGEX_MATCH)
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
