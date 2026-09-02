#ifdef XRT_MODULE_REGEX
	#undef XRT_MODULE_REGEX
#endif
#define XRT_MODULE_REGEX_CORE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件只选择编译层时不应带入 matcher 或 Unicode。 */
int main(void)
{
	xregex* pRegex;
	char arrEscaped[8];
	size_t iSize;

	#if !defined(XRT_FEATURE_REGEX_CORE)
		#error "XRT_MODULE_REGEX_CORE did not enable regex core"
	#endif
	#if defined(XRT_FEATURE_REGEX_MATCH)
		#error "XRT_MODULE_REGEX_CORE unexpectedly enabled regex match"
	#endif

	if ( !xrtRegexEscapeWrite(
		XRT_STR_LITERAL("a+"),
		arrEscaped,
		sizeof(arrEscaped),
		&iSize
	) || (iSize != 3u) ) {
		return 1;
	}
	pRegex = xrtRegexCompile((xstrview){ arrEscaped, iSize });
	if ( pRegex == NULL ) {
		return 1;
	}
	xrtRegexRelease(pRegex);
	return 0;
}
