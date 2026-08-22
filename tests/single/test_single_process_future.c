#define XRT_MODULE_PROCESS_FUTURE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证 Process Future 单头闭包和已退出进程的同步完成。 */
int main(void)
{
	const xprocessstatus* pStatus;
	xprocessconfig Config;
	xprocess* pProcess;
	xfuture* pFuture;

	#if !defined(XRT_FEATURE_PROCESS_FUTURE) || \
		!defined(XRT_FEATURE_PROCESS) || \
		!defined(XRT_FEATURE_FUTURE) || \
		defined(XRT_FEATURE_BUFFER) || \
		defined(XRT_FEATURE_FILE)
		#error "XRT_MODULE_PROCESS_FUTURE dependency closure is incorrect"
	#endif

	#if defined(_WIN32) || defined(_WIN64)
		if ( !xrtProcessShellConfigInit(&Config, "exit /B 4") ) {
			return 1;
		}
	#else
		if ( !xrtProcessShellConfigInit(&Config, "exit 4") ) {
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
	pFuture = xrtProcessWaitAsync(pProcess);
	xrtProcessDestroy(pProcess);
	if ( (pFuture == NULL) || (xrtFutureWait(pFuture) != XWAIT_OK) ) {
		xrtFutureDestroy(pFuture);
		return 3;
	}
	pStatus = (const xprocessstatus*)xrtFutureValue(pFuture);
	if ( (pStatus == NULL) || (pStatus->Kind != XPROCESS_EXIT_CODE) ||
		(pStatus->Code != 4) ) {
		xrtFutureDestroy(pFuture);
		return 4;
	}
	xrtFutureDestroy(pFuture);
	return 0;
}
