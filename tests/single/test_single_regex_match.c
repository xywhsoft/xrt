#ifdef XRT_MODULE_REGEX
	#undef XRT_MODULE_REGEX
#endif
#define XRT_MODULE_REGEX_MATCH
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件 matcher 根必须带入编译层与 UTF-8 推进依赖。 */
int main(void)
{
	#if !defined(XRT_FEATURE_REGEX_MATCH) || \
		!defined(XRT_FEATURE_REGEX_CORE) || \
		!defined(XRT_FEATURE_UNICODE)
		#error "XRT_MODULE_REGEX_MATCH did not enable its dependency closure"
	#endif

	xregex* Regex = xrtRegexCompile(XRT_STR_LITERAL("match .*"));
	if (Regex == NULL) return 2;
	xregexmatcher* Matcher = xrtRegexMatcherCreate(Regex);
	xregexcapture Capture;
	int Result = Matcher != NULL &&
		xrtRegexMatcherFind(Matcher, XRT_STR_LITERAL("\t\txrtahomatch Match;"), 0u) == XREGEX_MATCH &&
		xrtRegexMatcherCapture(Matcher, 0u, &Capture) && Capture.Matched &&
		Capture.Span.Begin == 8u && Capture.Span.End == 20u;
	xrtRegexMatcherFree(Matcher);
	xrtRegexRelease(Regex);
	if (!Result) return 3;

	return xrtRegexFullMatch(
		XRT_STR_LITERAL("a|ab"),
		XRT_STR_LITERAL("ab")
	) == XREGEX_MATCH ? 0 : 1;
}
