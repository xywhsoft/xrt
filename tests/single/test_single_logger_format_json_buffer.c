#define XRT_MODULE_LOGGER_FORMAT_JSON_BUFFER
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证分配式 JSON 模块精确启用 Buffer。 */
int main(void)
{
	xlogjsonconfig Config;
	xlogrecord Record;
	str sJson;
	size_t iSize;

	#if !defined(XRT_FEATURE_LOGGER_FORMAT_JSON_BUFFER) || \
		!defined(XRT_FEATURE_LOGGER_FORMAT_JSON) || \
		!defined(XRT_FEATURE_JSON_ESCAPE) || \
		!defined(XRT_FEATURE_BUFFER) || \
		defined(XRT_FEATURE_JSON_WRITE) || \
		defined(XRT_FEATURE_VALUE_CONTAINER)
		#error "XRT_MODULE_LOGGER_FORMAT_JSON_BUFFER dependency closure is incorrect"
	#endif

	memset(&Record, 0, sizeof(Record));
	Record.Level = XLOG_INFO;
	Record.Message = XRT_STR_LITERAL("single");
	if ( !xrtLogJsonConfigInit(&Config) ) {
		return 1;
	}
	Config.Flags = XLOG_JSON_MESSAGE;
	sJson = xrtLogJson(&Record, &Config, &iSize);
	if (
		(sJson == NULL) ||
		(iSize != sizeof("{\"message\":\"single\"}") - 1u) ||
		(memcmp(sJson, "{\"message\":\"single\"}", iSize) != 0)
	) {
		xrtFree(sJson);
		return 2;
	}
	xrtFree(sJson);
	return 0;
}
