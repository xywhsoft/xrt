#define XRT_MODULE_JSON_FILE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件 JSON 文件根必须带入读、写和完整文件依赖。 */
int main(void)
{
	cstr sPath = ".xrt-single-json-file.json";
	xvalue* pValue = xrtValueInt(7);
	xvalue* pRead = NULL;
	int64 iValue;
	int iResult = 1;

	#if !defined(XRT_FEATURE_JSON_FILE) || \
		!defined(XRT_FEATURE_JSON_READ) || \
		!defined(XRT_FEATURE_JSON_WRITE) || \
		!defined(XRT_FEATURE_FILE_WHOLE)
		#error "XRT_MODULE_JSON_FILE did not enable its dependency closure"
	#endif

	(void)xrtFileDelete(sPath);
	if (
		(pValue != NULL) &&
		xrtJsonStringifyFile(sPath, pValue, false)
	) {
		pRead = xrtJsonParseFile(sPath);
		if (
			(pRead != NULL) && xrtValueGetInt(pRead, &iValue) &&
			(iValue == 7)
		) {
			iResult = 0;
		}
	}
	xrtValueRelease(pRead);
	xrtValueRelease(pValue);
	(void)xrtFileDelete(sPath);
	return iResult;
}
