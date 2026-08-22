#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头发布必须保留 RFC 9530 单摘要写出。 */
int main(void)
{
	static const uint8 Digest[] = { 0u, 1u, 2u };
	char arrOutput[32];
	size_t iSize;

	return xrtHttpDigestWrite(
		XRT_STR_LITERAL("sha-256"),
		(xbytesview){ Digest, sizeof(Digest) },
		arrOutput, sizeof(arrOutput), &iSize
	) && (iSize == 14u) ? 0 : 1;
}
