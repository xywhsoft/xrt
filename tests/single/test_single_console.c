#define XRT_MODULE_CONSOLE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证 Console 单模块只拉入核心错误能力。 */
int main(void)
{
	#if !defined(XRT_FEATURE_CONSOLE) || \
		defined(XRT_FEATURE_LOGGER_CORE) || \
		defined(XRT_FEATURE_FILE) || \
		defined(XRT_FEATURE_THREAD)
		#error "XRT_MODULE_CONSOLE dependency closure is incorrect"
	#endif

	return xrtConsoleWrite(
		XCONSOLE_STDOUT,
		(xstrview){ NULL, 0 }
	) ? 0 : 1;
}
