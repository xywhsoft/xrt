#define XRT_MODULE_LOGGER_FORMAT_JSON
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头流式 Writer 验证 JSON 格式结果。 */
static bool testSingleLogJsonWrite(xbytesview Data, ptr pUserData)
{
	size_t* pSize = (size_t*)pUserData;

	*pSize += Data.Size;
	return true;
}



/* 验证流式 JSON 模块不会拉入 DOM、完整 Writer 或 Buffer。 */
int main(void)
{
	xlogjsonconfig Config;
	xlogrecord Record;
	size_t iSize = 0;

	#if !defined(XRT_FEATURE_LOGGER_FORMAT_JSON) || \
		!defined(XRT_FEATURE_JSON_ESCAPE) || \
		!defined(XRT_FEATURE_NUMBER_INTEGER) || \
		!defined(XRT_FEATURE_NUMBER_FLOAT) || \
		defined(XRT_FEATURE_JSON_WRITE) || \
		defined(XRT_FEATURE_VALUE_CONTAINER) || \
		defined(XRT_FEATURE_BUFFER)
		#error "XRT_MODULE_LOGGER_FORMAT_JSON dependency closure is not minimal"
	#endif

	memset(&Record, 0, sizeof(Record));
	Record.Level = XLOG_INFO;
	Record.Message = XRT_STR_LITERAL("single");
	if (
		!xrtLogJsonConfigInit(&Config) ||
		!xrtLogJsonWrite(
			&Record,
			&Config,
			testSingleLogJsonWrite,
			&iSize,
			NULL
		) ||
		(iSize != sizeof(
			"{\"time\":0,\"level\":\"INFO\",\"logger\":\"\"," 
			"\"message\":\"single\",\"fields\":{}}\n"
		) - 1u)
	) {
		return 1;
	}
	return 0;
}
