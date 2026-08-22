#include <xrt.h>



/* 用默认 Console Sink 建立一个可直接使用的应用 Logger。 */
int main(void)
{
	xlogger* pLogger = xrtLogCreate(XRT_STR_LITERAL("example"), XLOG_TRACE);

	if ( (pLogger == NULL) || !xrtLogAddConsole(pLogger, NULL) ) {
		xrtLogFree(pLogger);
		return 1;
	}
	(void)xrtLog(pLogger, XLOG_INFO, XRT_STR_LITERAL("service started"));
	(void)xrtLog(pLogger, XLOG_WARN, XRT_STR_LITERAL("using fallback config"));
	(void)xrtLog(pLogger, XLOG_ERROR, XRT_STR_LITERAL("request failed"));
	(void)xrtLogFlush(pLogger);
	xrtLogFree(pLogger);
	return 0;
}
