#define XRT_MODULE_PROCESS_FILE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证 Process File 单头闭包与文件输出重定向。 */
int main(void)
{
	static const cstr sPath = "xrt-single-process-file.tmp";
	xprocessconfig Config;
	xprocess* pProcess;
	xfile File;

	#if !defined(XRT_FEATURE_PROCESS_FILE) || \
		!defined(XRT_FEATURE_PROCESS) || \
		!defined(XRT_FEATURE_FILE) || \
		defined(XRT_FEATURE_BUFFER) || \
		defined(XRT_FEATURE_FUTURE)
		#error "XRT_MODULE_PROCESS_FILE dependency closure is incorrect"
	#endif

	File = xrtOpen(sPath, XFILE_WRITE | XFILE_CREATE | XFILE_TRUNCATE);
	if ( File == NULL ) {
		return 1;
	}
	#if defined(_WIN32) || defined(_WIN64)
		if ( !xrtProcessShellConfigInit(&Config, "echo single-file") ) {
			(void)xrtClose(File);
			return 2;
		}
	#else
		if ( !xrtProcessShellConfigInit(&Config, "printf single-file") ) {
			(void)xrtClose(File);
			return 2;
		}
	#endif
	Config.Stdin.Mode = XPROCESS_IO_NULL;
	Config.Stdout = xrtProcessFile(File);
	Config.Stderr.Mode = XPROCESS_IO_NULL;
	pProcess = xrtProcessSpawn(&Config);
	(void)xrtClose(File);
	if ( (pProcess == NULL) || (xrtProcessWait(pProcess) != XWAIT_OK) ) {
		xrtProcessDestroy(pProcess);
		(void)xrtFileDelete(sPath);
		return 3;
	}
	xrtProcessDestroy(pProcess);
	return xrtFileDelete(sPath) ? 0 : 4;
}
