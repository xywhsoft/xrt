#ifdef XREGEX_MODULE_XREGEX
	#undef XREGEX_MODULE_XREGEX
#endif
#define XREGEX_MODULE_REGEX_MATCH
#define XREGEX_IMPLEMENTATION
#include "../../single/xregex.h"



/* 单头文件 matcher 根必须带入编译层与 UTF-8 推进依赖。 */
int main(void)
{
	#if !defined(XREGEX_FEATURE_REGEX_MATCH) || \
		!defined(XREGEX_FEATURE_REGEX_CORE) || \
		!defined(XRT_FEATURE_UNICODE)
		#error "XREGEX_MODULE_REGEX_MATCH did not enable its dependency closure"
	#endif

	return xrtRegexFullMatch(
		XRT_STR_LITERAL("a|ab"),
		XRT_STR_LITERAL("ab")
	) == XREGEX_MATCH ? 0 : 1;
}
