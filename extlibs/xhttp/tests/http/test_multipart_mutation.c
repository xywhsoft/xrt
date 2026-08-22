#include "../test.h"



/* 生成可复现的轻量伪随机序列。 */
static uint32 testMultipartRandom(uint32* pState)
{
	uint32 iValue = *pState;

	iValue ^= iValue << 13;
	iValue ^= iValue >> 17;
	iValue ^= iValue << 5;
	*pState = iValue;
	return iValue;
}



/* 随机验证二进制 Part 正文不会被普通字节和近似 boundary 截断。 */
int main(void)
{
	static const char Prefix[] =
		"--mutation-boundary\r\n"
		"Content-Disposition: form-data; name=\"value\"\r\n"
		"\r\n";
	static const char Suffix[] =
		"\r\n--mutation-boundary--\r\n";
	xmultipartboundary Boundary;
	uint32 iState = UINT32_C(0x4D504152);
	size_t iRound;

	testRequire(xrtMultipartBoundaryParse(
		XRT_STR_LITERAL("mutation-boundary"), &Boundary
	), "Multipart mutation boundary init failed");
	for ( iRound = 0; iRound < 5000; iRound++ ) {
		uint8 Body[256];
		xmultiparterrorinfo Error;
		xmultipartpart Part;
		size_t iData = testMultipartRandom(&iState) % 96u;
		size_t iBody = 0;
		size_t iOffset = 0;
		size_t i;

		memcpy(Body + iBody, Prefix, sizeof(Prefix) - 1u);
		iBody += sizeof(Prefix) - 1u;
		for ( i = 0; i < iData; i++ ) {
			uint8 iByte = (uint8)testMultipartRandom(&iState);

			if ( iByte == (uint8)'\r' ) {
				iByte = (uint8)'R';
			}
			Body[iBody++] = iByte;
		}
		memcpy(Body + iBody, Suffix, sizeof(Suffix) - 1u);
		iBody += sizeof(Suffix) - 1u;
		testRequire(xrtMultipartNext(
			(xbytesview){ Body, iBody },
			&Boundary, &iOffset, &Part, &Error
		) == XHTTP_NEXT_ITEM &&
			(Part.Body.Size == iData) &&
			(memcmp(
				Part.Body.Data,
				Body + sizeof(Prefix) - 1u,
				iData
			) == 0) &&
			(xrtMultipartNext(
				(xbytesview){ Body, iBody },
				&Boundary, &iOffset, &Part, &Error
			) == XHTTP_NEXT_END),
			"Multipart mutation roundtrip mismatch");
	}
	printf("[PASS] multipart_mutation\n");
	return 0;
}
