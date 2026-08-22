#include <stdio.h>

#include <xrt.h>



/* 演示从一个借用的 DER SEQUENCE 中读取无符号整数。 */
int main(void)
{
	static const uint8 Document[] = {
		0x30, 0x06,
		0x02, 0x01, 0x07,
		0x02, 0x01, 0x2A
	};
	xdercursor Root;
	xdercursor Items;
	xdervalue Value;
	uint64 iLeft;
	uint64 iRight;

	if ( !xrtDerValidate(Document, sizeof(Document)) ||
		!xrtDerInit(&Root, Document, sizeof(Document)) ||
		!xrtDerExpect(
			&Root, XASN1_UNIVERSAL, (uint32)XASN1_SEQUENCE, true, &Value
		) || !xrtDerEnter(&Value, &Items) ||
		(xrtDerRead(&Items, &Value) != XDER_VALUE) ||
		!xrtDerUInt64(&Value, &iLeft) ||
		(xrtDerRead(&Items, &Value) != XDER_VALUE) ||
		!xrtDerUInt64(&Value, &iRight) || !xrtDerDone(&Items) ) {
		return 1;
	}
	printf("%llu + %llu = %llu\n",
		(unsigned long long)iLeft,
		(unsigned long long)iRight,
		(unsigned long long)(iLeft + iRight));
	return 0;
}
