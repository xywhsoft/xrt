#define XRT_MODULE_JSON_READ
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件只选择 JSON 读取根时必须带入完整解析依赖闭包。 */
int main(void)
{
	xvalue* pRoot;
	int64 iValue;
	int iResult = 1;

	#if !defined(XRT_FEATURE_JSON_READ) || \
		!defined(XRT_FEATURE_VALUE_CONTAINER) || \
		!defined(XRT_FEATURE_NUMBER_INTEGER) || \
		!defined(XRT_FEATURE_NUMBER_FLOAT) || \
		!defined(XRT_FEATURE_UNICODE) || \
		!defined(XRT_FEATURE_BUFFER)
		#error "XRT_MODULE_JSON_READ did not enable its dependency closure"
	#endif

	pRoot = xrtJsonParse(XRT_STR_LITERAL("{\"code\":200}"));
	if (
		(pRoot != NULL) &&
		xrtValueGetInt(
			xrtValueObjectGet(pRoot, XRT_STR_LITERAL("code")),
			&iValue
		) &&
		(iValue == 200)
	) {
		iResult = 0;
	}
	xrtValueRelease(pRoot);
	return iResult;
}
