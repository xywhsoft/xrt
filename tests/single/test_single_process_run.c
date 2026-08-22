#define XRT_MODULE_PROCESS_RUN
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证 Process Run 单头闭包和最小 Shell 捕获路径。 */
int main(void)
{
	xprocessresult Result;
	bool bOk;

	#if !defined(XRT_FEATURE_PROCESS_RUN) || \
		!defined(XRT_FEATURE_PROCESS) || \
		!defined(XRT_FEATURE_BUFFER) || \
		!defined(XRT_FEATURE_CANCEL) || \
		defined(XRT_FEATURE_FUTURE) || \
		defined(XRT_FEATURE_FILE)
		#error "XRT_MODULE_PROCESS_RUN dependency closure is incorrect"
	#endif

	#if defined(_WIN32) || defined(_WIN64)
		bOk = xrtProcessShell("echo single-run", &Result);
	#else
		bOk = xrtProcessShell("printf single-run", &Result);
	#endif
	if ( !bOk ) {
		return 1;
	}
	if ( !xrtProcessResultSuccess(&Result) ||
		(Result.StdoutSize < 10u) ||
		(memcmp(Result.Stdout, "single-run", 10u) != 0) ) {
		xrtProcessResultUnit(&Result);
		return 2;
	}
	xrtProcessResultUnit(&Result);
	return 0;
}
