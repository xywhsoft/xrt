#define XRUNTIME_IMPLEMENTATION
#include "../../include/xruntime/features.h"
#ifndef XRT_IMPLEMENTATION
#define XRT_IMPLEMENTATION
#endif
#include "../../../../single/xrt.h"
#include "../../../../single/extlibs/xruntime.h"



/* 验证单头文件中的拥有型字符串动态转换。 */
int main(void)
{
	const xrttype* pType = xrtTypeString();
	xvalue* pValue = xrtValueString(XRT_STR_LITERAL("single"));
	str sText = NULL;
	int iResult = 0;

	if (
		(pValue == NULL) ||
		!xrtValueToTyped(pValue, pType, &sText, NULL) ||
		(sText == NULL) || (strcmp(sText, "single") != 0)
	) {
		iResult = 1;
	}
	xrtTypeDropValue(pType, &sText);
	xrtValueRelease(pValue);
	return iResult;
}
