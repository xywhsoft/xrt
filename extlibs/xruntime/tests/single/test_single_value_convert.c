#define XRUNTIME_IMPLEMENTATION
#include "../../single/xruntime.h"



/* 验证单头文件中的动态 Value 标量转换。 */
int main(void)
{
	xvalue* pSource = xrtValueInt(42);
	xvalue* pBool = xrtValueBool(true);
	int32 iTarget = 0;
	int32 iBool32 = 0;
	int iResult = 0;

	if (
		(pSource == NULL) || (pBool == NULL) ||
		!xrtValueConvertTo(
			pSource, xrtTypeInt32(), &iTarget, XTYPE_CONVERT_EXPLICIT
		) || (iTarget != 42) ||
		!xrtValueConvertTo(
			pBool, xrtTypeBool32(), &iBool32, XTYPE_CONVERT_WIDEN
		) || (iBool32 != 1)
	) {
		iResult = 1;
	}
	xrtValueRelease(pBool);
	xrtValueRelease(pSource);
	return iResult;
}
