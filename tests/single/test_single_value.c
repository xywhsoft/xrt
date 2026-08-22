#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立提供标量、字符串和哈希能力。 */
int main(void)
{
	xvalue* pValue = xrtValueString(XRT_STR_LITERAL("single"));
	xvalue* pEqual = xrtValueString(XRT_STR_LITERAL("single"));
	xstrview Text;
	uint64 iHash;
	int iResult = 0;

	if ( (pValue == NULL) || (pEqual == NULL) ||
		!xrtValueGetString(pValue, &Text) ||
		(Text.Size != 6) ||
		!xrtValueHash(pValue, &iHash) ||
		!xrtValueScalarEqual(pValue, pEqual) ) {
		iResult = 1;
	}
	xrtValueRelease(pEqual);
	xrtValueRelease(pValue);
	return iResult;
}
