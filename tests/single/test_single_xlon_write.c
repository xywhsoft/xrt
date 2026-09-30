#define XRT_MODULE_XLON_WRITE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件只选择 XLON 写出根时必须支持专用容器和直接 writer。 */
int main(void)
{
	xxlonwriteconfig Config;
	xxlonwriter* pWriter;
	str sText;
	size_t iSize;
	int iResult = 1;

	#if !defined(XRT_FEATURE_XLON_WRITE) || \
		!defined(XRT_FEATURE_VALUE_CONTAINER) || \
		!defined(XRT_FEATURE_CODEC_BASE64) || \
		!defined(XRT_FEATURE_TIME_TEXT) || \
		!defined(XRT_FEATURE_UNICODE) || \
		!defined(XRT_FEATURE_BUFFER)
		#error "XRT_MODULE_XLON_WRITE did not enable its dependency closure"
	#endif

	xrtXlonWriteConfigInit(&Config);
	pWriter = xrtXlonWriterCreate(&Config);
	if (
		(pWriter != NULL) &&
		xrtXlonWriterIntMap(pWriter) &&
		xrtXlonWriterKey(pWriter, 7) &&
		xrtXlonWriterSet(pWriter) &&
		xrtXlonWriterBool(pWriter, true) &&
		xrtXlonWriterEnd(pWriter) &&
		xrtXlonWriterEnd(pWriter) &&
		xrtXlonWriterFinish(pWriter)
	) {
		sText = xrtXlonWriterTake(pWriter, &iSize);
		if (
			(sText != NULL) && (iSize == 19u) &&
			(memcmp(sText, "intmap{7:set[true]}", 20u) == 0)
		) {
			iResult = 0;
		}
		xrtFree(sText);
	}
	xrtXlonWriterFree(pWriter);
	return iResult;
}
