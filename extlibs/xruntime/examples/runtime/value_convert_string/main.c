#include <stdio.h>
#include <xruntime.h>



/* 展示动态 Value 文本解析和标量格式化。 */
int main(void)
{
	xvalue* pSource = xrtValueString(XRT_STR_LITERAL("65535"));
	xvalue* pNumber = xrtValueInt(-17);
	uint16 iValue = 0u;
	str sText = NULL;
	int iResult = 0;

	if (
		(pSource == NULL) || (pNumber == NULL) ||
		!xrtValueConvertTo(
			pSource, xrtTypeUInt16(), &iValue, XTYPE_CONVERT_EXPLICIT
		) ||
		!xrtValueConvertTo(
			pNumber, xrtTypeString(), &sText, XTYPE_CONVERT_EXPLICIT
		)
	) {
		iResult = 1;
	} else {
		printf("value=%u text=%s\n", (unsigned int)iValue, sText);
	}
	xrtTypeDropValue(xrtTypeString(), &sText);
	xrtValueRelease(pNumber);
	xrtValueRelease(pSource);
	return iResult;
}
