#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立接通裁剪后的 FIFO 能力。 */
int main(void)
{
	static const char sPath[] = "xrt-single-fifo";

	#if defined(_WIN32) || defined(_WIN64)
		return (!xrtFifoCreate(sPath, 0600u) &&
			(xrtGetError() != NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_UNSUPPORTED)) ? 0 : 1;
	#elif defined(__ANDROID__)
		return (!xrtFifoCreate(sPath, 0600u) &&
			(xrtGetError() != NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_PERMISSION)) ? 0 : 1;
	#else
		xfileinfo Info;
		int iResult = 1;

		(void)xrtFileDelete(sPath);
		xrtClearError();
		if ( xrtFifoCreate(sPath, 0600u) &&
			xrtPathStat(sPath, false, &Info) &&
			(Info.Type == XFILE_TYPE_FIFO) && xrtFileDelete(sPath) ) {
			iResult = 0;
		}
		return iResult;
	#endif
}
