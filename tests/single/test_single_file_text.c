#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立接通 BOM 文本写入、检测和 UTF-8 返回。 */
int main(void)
{
	static const char sPath[] = "xrt-single-file-text.tmp";
	str sText;
	size_t iSize;
	int iResult = 1;

	(void)xrtFileDelete(sPath);
	xrtClearError();
	if ( !xrtFileWriteText(sPath, XRT_STR_LITERAL("single text"),
		XENCODING_UTF16_LE, XUTF_STRICT, true) ) {
		return 1;
	}
	sText = xrtFileReadTextLimit(sPath, XENCODING_UNKNOWN,
		XUTF_STRICT, 24u, &iSize);
	if ( (sText != NULL) && (iSize == 11u) &&
		(memcmp(sText, "single text", 11) == 0) && xrtFileDelete(sPath) ) {
		iResult = 0;
	}
	xrtFree(sText);
	return iResult;
}
