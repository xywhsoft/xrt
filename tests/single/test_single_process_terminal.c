#define XRT_MODULE_PROCESS_TERMINAL
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证 Process Terminal 单头闭包与非交互命令输出。 */
int main(void)
{
	xprocessconfig Config;
	xprocess* pProcess;
	char sOutput[128];
	int64 iRead;

	#if !defined(XRT_FEATURE_PROCESS_TERMINAL) || \
		!defined(XRT_FEATURE_PROCESS) || \
		!defined(XRT_FEATURE_THREAD) || \
		!defined(XRT_FEATURE_MUTEX) || \
		!defined(XRT_FEATURE_COND) || \
		!defined(XRT_FEATURE_UNICODE) || \
		defined(XRT_FEATURE_BUFFER) || \
		defined(XRT_FEATURE_FUTURE) || \
		defined(XRT_FEATURE_FILE)
		#error "XRT_MODULE_PROCESS_TERMINAL dependency closure is incorrect"
	#endif

	if ( !xrtProcessTerminalSupported() ) {
		return 0;
	}
	#if defined(_WIN32) || defined(_WIN64)
		if ( !xrtProcessShellConfigInit(&Config, "echo single-terminal") ) {
			return 1;
		}
	#else
		if ( !xrtProcessShellConfigInit(&Config, "printf single-terminal") ) {
			return 1;
		}
	#endif
	Config.Terminal = true;
	pProcess = xrtProcessSpawn(&Config);
	if ( pProcess == NULL ) {
		return 2;
	}
	iRead = xrtProcessRead(
		pProcess,
		XPROCESS_STDOUT,
		sOutput,
		sizeof(sOutput)
	);
	if ( (iRead <= 0) || (xrtProcessWait(pProcess) != XWAIT_OK) ) {
		xrtProcessDestroy(pProcess);
		return 3;
	}
	xrtProcessDestroy(pProcess);
	return 0;
}
