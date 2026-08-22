#define XRUNTIME_IMPLEMENTATION
#include "../../single/xruntime.h"



/* 验证单头文件中的动态 Value 文本解析和格式化。 */
int main(void)
{
	xvalue* pText = xrtValueString(XRT_STR_LITERAL("42"));
	xvalue* pInteger = xrtValueInt(-7);
	int32 iValue = 0;
	str sText = NULL;
	int iResult = 0;

	if (
		(pText == NULL) || (pInteger == NULL) ||
		!xrtValueConvertTo(
			pText, xrtTypeInt32(), &iValue, XTYPE_CONVERT_EXPLICIT
		) || (iValue != 42) ||
		!xrtValueConvertTo(
			pInteger, xrtTypeString(), &sText, XTYPE_CONVERT_EXPLICIT
		) || (sText == NULL) || (strcmp(sText, "-7") != 0)
	) {
		iResult = 1;
	}
	xrtTypeDropValue(xrtTypeString(), &sText);
	xrtValueRelease(pInteger);
	xrtValueRelease(pText);
	return iResult;
}
