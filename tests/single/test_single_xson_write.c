#define XRT_MODULE_XSON_WRITE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件只选择 XSON 写出根时必须支持专用容器和直接 writer。 */
int main(void)
{
	xxsonwriteconfig Config;
	xxsonwriter* pWriter;
	str sText;
	size_t iSize;
	int iResult = 1;

	#if !defined(XRT_FEATURE_XSON_WRITE) || \
		!defined(XRT_FEATURE_VALUE_CONTAINER) || \
		!defined(XRT_FEATURE_CODEC_BASE64) || \
		!defined(XRT_FEATURE_TIME_TEXT) || \
		!defined(XRT_FEATURE_UNICODE) || \
		!defined(XRT_FEATURE_BUFFER)
		#error "XRT_MODULE_XSON_WRITE did not enable its dependency closure"
	#endif

	xrtXsonWriteConfigInit(&Config);
	pWriter = xrtXsonWriterCreate(&Config);
	if (
		(pWriter != NULL) &&
		xrtXsonWriterIntMap(pWriter) &&
		xrtXsonWriterKey(pWriter, 7) &&
		xrtXsonWriterSet(pWriter) &&
		xrtXsonWriterBool(pWriter, true) &&
		xrtXsonWriterEnd(pWriter) &&
		xrtXsonWriterEnd(pWriter) &&
		xrtXsonWriterFinish(pWriter)
	) {
		sText = xrtXsonWriterTake(pWriter, &iSize);
		if (
			(sText != NULL) && (iSize == 19u) &&
			(memcmp(sText, "intmap{7:set[true]}", 20u) == 0)
		) {
			iResult = 0;
		}
		xrtFree(sText);
	}
	xrtXsonWriterFree(pWriter);
	return iResult;
}
