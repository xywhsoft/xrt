#define XRT_MODULE_XLON_FILE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件 XLON 文件根必须带入读、写和完整文件依赖。 */
int main(void)
{
	cstr sPath = ".xrt-single-xlon-file.xlon";
	xtime Expected;
	xtime Time;
	xvalue* pValue;
	xvalue* pRead = NULL;
	int iResult = 1;

	#if !defined(XRT_FEATURE_XLON_FILE) || \
		!defined(XRT_FEATURE_XLON_READ) || \
		!defined(XRT_FEATURE_XLON_WRITE) || \
		!defined(XRT_FEATURE_FILE_WHOLE)
		#error "XRT_MODULE_XLON_FILE did not enable its dependency closure"
	#endif

	(void)xrtFileDelete(sPath);
	if (
		xrtTimeParseRFC3339(
			XRT_STR_LITERAL("2026-07-31T08:00:00Z"),
			&Expected
		)
	) {
		pValue = xrtValueTime(Expected);
		if (
			(pValue != NULL) &&
			xrtXlonStringifyFile(sPath, pValue, false)
		) {
			pRead = xrtXlonParseFile(sPath);
			if (
				(pRead != NULL) && xrtValueGetTime(pRead, &Time) &&
				(Time == Expected)
			) {
				iResult = 0;
			}
		}
		xrtValueRelease(pValue);
	}
	xrtValueRelease(pRead);
	(void)xrtFileDelete(sPath);
	return iResult;
}
