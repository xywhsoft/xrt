#define XRT_MODULE_XSON_READ
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件只选择 XSON 读取根时必须带入扩展标签解析依赖。 */
int main(void)
{
	xvalue* pValue;
	xbytesview Data;
	int iResult = 1;

	#if !defined(XRT_FEATURE_XSON_READ) || \
		!defined(XRT_FEATURE_VALUE_CONTAINER) || \
		!defined(XRT_FEATURE_CODEC_BASE64) || \
		!defined(XRT_FEATURE_TIME_TEXT) || \
		!defined(XRT_FEATURE_UNICODE) || \
		!defined(XRT_FEATURE_BUFFER)
		#error "XRT_MODULE_XSON_READ did not enable its dependency closure"
	#endif

	pValue = xrtXsonParse(XRT_STR_LITERAL("bytes(\"AQI=\")"));
	if (
		(pValue != NULL) && xrtValueGetBytes(pValue, &Data) &&
		(Data.Size == 2u) && (Data.Data[0] == 1u) &&
		(Data.Data[1] == 2u)
	) {
		iResult = 0;
	}
	xrtValueRelease(pValue);
	return iResult;
}
