#define XRT_MODULE_PROCESS
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证 Process 单头闭包和 Shell 基础生命周期。 */
int main(void)
{
	xprocessconfig Config;
	xprocessstatus Status;
	xprocess* pProcess;

	#if !defined(XRT_FEATURE_PROCESS) || \
		!defined(XRT_FEATURE_THREAD) || \
		!defined(XRT_FEATURE_MUTEX) || \
		!defined(XRT_FEATURE_COND) || \
		!defined(XRT_FEATURE_UNICODE) || \
		defined(XRT_FEATURE_BUFFER) || \
		defined(XRT_FEATURE_FUTURE) || \
		defined(XRT_FEATURE_FILE)
		#error "XRT_MODULE_PROCESS dependency closure is incorrect"
	#endif

	#if defined(_WIN32) || defined(_WIN64)
		if ( !xrtProcessShellConfigInit(&Config, "exit /B 0") ) {
			return 1;
		}
	#else
		if ( !xrtProcessShellConfigInit(&Config, "exit 0") ) {
			return 1;
		}
	#endif
	Config.Stdin.Mode = XPROCESS_IO_NULL;
	Config.Stdout.Mode = XPROCESS_IO_NULL;
	Config.Stderr.Mode = XPROCESS_IO_NULL;
	pProcess = xrtProcessSpawn(&Config);
	if ( pProcess == NULL ) {
		return 2;
	}
	if ( xrtProcessWait(pProcess) != XWAIT_OK ) {
		return 3;
	}
	if ( !xrtProcessStatus(pProcess, &Status) ||
		(Status.Kind != XPROCESS_EXIT_CODE) || (Status.Code != 0) ) {
		return 4;
	}
	xrtProcessDestroy(pProcess);
	return 0;
}
