#define XRT_MODULE_LOGGER_FORMAT_TEXT_BUFFER
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证分配式文本模块精确启用 Buffer。 */
int main(void)
{
	xlogtextconfig Config;
	xlogrecord Record;
	str sText;
	size_t iSize;

	#if !defined(XRT_FEATURE_LOGGER_FORMAT_TEXT_BUFFER) || \
		!defined(XRT_FEATURE_LOGGER_FORMAT_TEXT) || \
		!defined(XRT_FEATURE_BUFFER)
		#error "XRT_MODULE_LOGGER_FORMAT_TEXT_BUFFER dependency closure is incomplete"
	#endif

	memset(&Record, 0, sizeof(Record));
	Record.Level = XLOG_INFO;
	Record.Message = XRT_STR_LITERAL("single");
	if ( !xrtLogTextConfigInit(&Config, XLOG_TEXT_MESSAGE) ) {
		return 1;
	}
	sText = xrtLogText(&Record, &Config, &iSize);
	if (
		(sText == NULL) || (iSize != 7u) ||
		(memcmp(sText, "single\n", 7u) != 0)
	) {
		xrtFree(sText);
		return 2;
	}
	xrtFree(sText);
	return 0;
}
