#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立接通整文件、追加和原子替换 Helper。 */
int main(void)
{
	static const char sPath[] = "xrt-single-file-whole.tmp";
	bytes pData;
	size_t iSize;
	int iResult = 1;

	(void)xrtFileDelete(sPath);
	xrtClearError();
	if ( !xrtFileWriteAll(sPath, XRT_BYTES_LITERAL("ab")) ||
		 !xrtFileAppend(sPath, XRT_BYTES_LITERAL("c")) ||
		 !xrtFileWriteAtomic(sPath, XRT_BYTES_LITERAL("xyz")) ) {
		return 1;
	}
	pData = xrtFileReadAllLimit(sPath, 3u, &iSize);
	if ( (pData != NULL) && (iSize == 3u) &&
		(memcmp(pData, "xyz", 3) == 0) && xrtFileDelete(sPath) ) {
		iResult = 0;
	}
	xrtFree(pData);
	return iResult;
}
