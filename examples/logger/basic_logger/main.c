/*
 * XRT Example - Basic Logger
 * XRT 范例 - 基础日志
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/logger_basic.exe -lws2_32 -lshell32 -liphlpapi
 *   gcc main.c -o ../../bin/logger_basic -lpthread -lm
 */

#define XRT_NO_NETWORK
#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"


int main()
{
	xrtInit();

	xlogger* pLogger = xlogCreate((str)"example");
	xlogSetLevel(pLogger, XLOG_TRACE);

	xlogAddConsole(pLogger, XLOG_INFO, TRUE);
	xlogAddFile(pLogger, (str)"example.log", XLOG_TRACE);

	xloggerTrace(pLogger, "trace message");
	xloggerDebug(pLogger, "debug value=%d", 123);
	xloggerInfo(pLogger, "service started");
	xloggerWarn(pLogger, "config file missing, use default");
	xloggerError(pLogger, "request failed: code=%d", 500);

	xlogFlush(pLogger);
	xlogDestroy(pLogger);

	xrtUnit();
	return 0;
}
