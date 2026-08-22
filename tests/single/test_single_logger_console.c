#define XRT_MODULE_LOGGER_CONSOLE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证 Console Sink 只拉入文本格式器，不拉入 JSON 或 Buffer。 */
int main(void)
{
	xlogconsoleconfig Config;
	xlogsink* pSink;

	#if !defined(XRT_FEATURE_LOGGER_CONSOLE) || \
		!defined(XRT_FEATURE_LOGGER_FORMAT_TEXT) || \
		defined(XRT_FEATURE_LOGGER_FORMAT_JSON) || \
		defined(XRT_FEATURE_BUFFER)
		#error "XRT_MODULE_LOGGER_CONSOLE dependency closure is incorrect"
	#endif

	if ( !xrtLogConsoleConfigInit(&Config) ) {
		return 1;
	}
	Config.Level = XLOG_OFF;
	pSink = xrtLogConsole(&Config);
	if ( pSink == NULL ) {
		return 2;
	}
	xrtLogSinkFree(pSink);
	return 0;
}
