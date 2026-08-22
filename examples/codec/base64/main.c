#include <stdio.h>
#include <string.h>

#include <xrt.h>



/* 演示标准 Base64 的容量查询、调用方缓冲和分配型解码。 */
int main(void)
{
	static const char Message[] = "xrt codec";
	char Encoded[32];
	bytes pDecoded;
	size_t iEncodedSize;
	size_t iDecodedSize;

	if ( !xrtBase64Encode(
		Message, sizeof(Message) - 1u, Encoded, sizeof(Encoded),
		&iEncodedSize, NULL
	) ) {
		return 1;
	}
	pDecoded = xrtBase64DecodeNew(
		Encoded, iEncodedSize, &iDecodedSize, NULL
	);
	if ( (pDecoded == NULL) || (iDecodedSize != sizeof(Message) - 1u) ||
		(memcmp(pDecoded, Message, iDecodedSize) != 0) ) {
		xrtFree(pDecoded);
		return 2;
	}
	printf("%s -> %s\n", Message, Encoded);
	xrtFree(pDecoded);
	return 0;
}
