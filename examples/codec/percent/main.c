#include <stdio.h>

#include <xrt.h>



/* 演示为 URL path segment 编码并分配解码任意字节。 */
int main(void)
{
	static const char Segment[] = "reports/July 2026";
	char Encoded[64];
	bytes pDecoded;
	size_t iEncodedSize;
	size_t iDecodedSize;

	if ( !xrtPercentEncode(
		Segment, sizeof(Segment) - 1u, XRT_STR_LITERAL(""),
		Encoded, sizeof(Encoded), &iEncodedSize
	) ) {
		return 1;
	}
	pDecoded = xrtPercentDecodeNew(
		(xstrview){ Encoded, iEncodedSize }, &iDecodedSize
	);
	if ( pDecoded == NULL ) {
		return 2;
	}
	printf("encoded: %s\ndecoded: %.*s\n",
		Encoded, (int)iDecodedSize, (const char*)pDecoded);
	xrtFree(pDecoded);
	return 0;
}
