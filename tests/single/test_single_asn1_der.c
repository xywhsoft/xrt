#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include <stdio.h>



/* 验证单头文件中的 DER 游标和无符号整数辅助层。 */
int main(void)
{
	static const uint8 Document[] = {
		0x30, 0x03, 0x02, 0x01, 0x2A
	};
	xdercursor Root;
	xdercursor Child;
	xdervalue Value;
	uint64 iNumber;

	if ( !xrtDerValidate(Document, sizeof(Document)) ||
		!xrtDerInit(&Root, Document, sizeof(Document)) ||
		!xrtDerExpect(
			&Root, XASN1_UNIVERSAL, (uint32)XASN1_SEQUENCE, true, &Value
		) || !xrtDerEnter(&Value, &Child) ||
		(xrtDerRead(&Child, &Value) != XDER_VALUE) ||
		!xrtDerUInt64(&Value, &iNumber) || (iNumber != 42u) ||
		!xrtDerDone(&Child) ) {
		return 1;
	}
	printf("[PASS] single-asn1-der\n");
	return 0;
}
