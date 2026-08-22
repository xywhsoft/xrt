#define XRT_MODULE_LOGGER_FORMAT_TEXT
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头流式 Writer 验证文本格式结果。 */
static bool testSingleLogTextWrite(xbytesview Data, ptr pUserData)
{
	size_t* pSize = (size_t*)pUserData;

	*pSize += Data.Size;
	return true;
}



/* 验证流式文本模块不会拉入 Buffer 或 String。 */
int main(void)
{
	xlogtextconfig Config;
	xlogrecord Record;
	size_t iSize = 0;

	#if !defined(XRT_FEATURE_LOGGER_FORMAT_TEXT) || \
		!defined(XRT_FEATURE_NUMBER_INTEGER) || \
		!defined(XRT_FEATURE_NUMBER_FLOAT) || \
		defined(XRT_FEATURE_BUFFER) || \
		defined(XRT_FEATURE_STRING)
		#error "XRT_MODULE_LOGGER_FORMAT_TEXT dependency closure is not minimal"
	#endif

	memset(&Record, 0, sizeof(Record));
	Record.Level = XLOG_INFO;
	Record.Message = XRT_STR_LITERAL("single");
	if (
		!xrtLogTextConfigInit(&Config, XLOG_TEXT_MESSAGE) ||
		!xrtLogTextWrite(
			&Record,
			&Config,
			testSingleLogTextWrite,
			&iSize,
			NULL
		) ||
		(iSize != 7u)
	) {
		return 1;
	}
	return 0;
}
