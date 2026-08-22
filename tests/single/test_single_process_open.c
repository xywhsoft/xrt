#define XRT_MODULE_PROCESS_OPEN
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证 Process Open 单头闭包和参数边界。 */
int main(void)
{
	#if !defined(XRT_FEATURE_PROCESS_OPEN) || \
		!defined(XRT_FEATURE_PROCESS) || \
		!defined(XRT_FEATURE_UNICODE) || \
		defined(XRT_FEATURE_BUFFER) || \
		defined(XRT_FEATURE_FUTURE) || \
		defined(XRT_FEATURE_FILE)
		#error "XRT_MODULE_PROCESS_OPEN dependency closure is incorrect"
	#endif

	return !xrtProcessOpen(NULL) &&
		(xrtErrorCode(xrtGetError()) == XPROCESS_ERROR_OPEN) ? 0 : 1;
}
