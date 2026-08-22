#define XRT_MODULE_JSON_WRITE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件只选择 JSON 写出根时必须提供直接写入和 Value 序列化。 */
int main(void)
{
	xjsonwriteconfig Config;
	xjsonwriter* pWriter;
	str sText;
	size_t iSize;
	int iResult = 1;

	#if !defined(XRT_FEATURE_JSON_WRITE) || \
		!defined(XRT_FEATURE_VALUE_CONTAINER) || \
		!defined(XRT_FEATURE_NUMBER_INTEGER) || \
		!defined(XRT_FEATURE_NUMBER_FLOAT) || \
		!defined(XRT_FEATURE_UNICODE) || \
		!defined(XRT_FEATURE_BUFFER)
		#error "XRT_MODULE_JSON_WRITE did not enable its dependency closure"
	#endif

	xrtJsonWriteConfigInit(&Config);
	pWriter = xrtJsonWriterCreate(&Config);
	if (
		(pWriter != NULL) &&
		xrtJsonWriterObject(pWriter) &&
		xrtJsonWriterName(pWriter, XRT_STR_LITERAL("ok")) &&
		xrtJsonWriterBool(pWriter, true) &&
		xrtJsonWriterEnd(pWriter) &&
		xrtJsonWriterFinish(pWriter)
	) {
		sText = xrtJsonWriterTake(pWriter, &iSize);
		if (
			(sText != NULL) && (iSize == 11u) &&
			(memcmp(sText, "{\"ok\":true}", 12u) == 0)
		) {
			iResult = 0;
		}
		xrtFree(sText);
	}
	xrtJsonWriterFree(pWriter);
	return iResult;
}
