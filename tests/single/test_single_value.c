#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立提供标量、字符串和哈希能力。 */
int main(void)
{
	xvalue* pValue = xrtValueString(XRT_STR_LITERAL("single"));
	xvalue* pEqual = xrtValueString(XRT_STR_LITERAL("single"));
	xvalue* pUnsigned = xrtValueUInt(UINT64_MAX);
	xstrview Text;
	uint64 iUnsigned;
	uint64 iHash;
	int iResult = 0;

	if ( (pValue == NULL) || (pEqual == NULL) || (pUnsigned == NULL) ||
		!xrtValueGetString(pValue, &Text) ||
		!xrtValueGetUInt(pUnsigned, &iUnsigned) || (iUnsigned != UINT64_MAX) ||
		(Text.Size != 6) ||
		!xrtValueHash(pValue, &iHash) ||
		!xrtValueScalarEqual(pValue, pEqual) ) {
		iResult = 1;
	}
	xrtValueRelease(pUnsigned);
	xrtValueRelease(pEqual);
	xrtValueRelease(pValue);
	return iResult;
}
