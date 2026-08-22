#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立接通硬链接与共享身份查询。 */
int main(void)
{
	static const char sSource[] = "xrt-single-link-source.tmp";
	static const char sHard[] = "xrt-single-link-hard.tmp";
	xfileinfo SourceInfo;
	xfileinfo HardInfo;
	xfile File;
	int iResult = 1;

	(void)xrtFileDelete(sHard);
	(void)xrtFileDelete(sSource);
	xrtClearError();
	File = xrtOpen(sSource, XFILE_WRITE | XFILE_CREATE | XFILE_EXCLUSIVE);
	if ( (File == NULL) || !xrtClose(File) ) {
		return 1;
	}
	if ( !xrtLinkHard(sSource, sHard) ) {
		#if defined(__ANDROID__)
			iResult = ((xrtGetError() != NULL) &&
				(xrtErrorKind(xrtGetError()) == XERR_PERMISSION)) ? 0 : 1;
			xrtClearError();
			(void)xrtFileDelete(sSource);
			return iResult;
		#else
			return 1;
		#endif
	}
	if ( xrtPathStat(sSource, true, &SourceInfo) &&
		xrtPathStat(sHard, true, &HardInfo) &&
		(SourceInfo.Identity == HardInfo.Identity) &&
		xrtFileDelete(sHard) && xrtFileDelete(sSource) ) {
		iResult = 0;
	}
	return iResult;
}
