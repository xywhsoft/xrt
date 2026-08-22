#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头发布必须保留确定性无状态 nonce 往返。 */
int main(void)
{
	uint8 KeyData[XRT_HTTP_DIGEST_NONCE_KEY_MIN];
	uint8 Salt[XRT_HTTP_DIGEST_NONCE_SALT_SIZE];
	char Nonce[XRT_HTTP_DIGEST_NONCE_TEXT_SIZE];
	xbytesview Key = { KeyData, sizeof(KeyData) };
	size_t iSize;

	memset(KeyData, 0x11, sizeof(KeyData));
	memset(Salt, 0x22, sizeof(Salt));
	if ( !xrtHttpDigestNonceWrite(
		Key, XRT_BYTES_LITERAL("api"), INT64_C(1700000000),
		Salt, Nonce, sizeof(Nonce), &iSize
	) || (xrtHttpDigestNonceVerify(
		(xstrview){ Nonce, iSize }, Key, XRT_BYTES_LITERAL("api"),
		INT64_C(1700000030), 60, 5, NULL
	) != XHTTP_DIGEST_NONCE_VALID) ) {
		return 1;
	}
	return 0;
}
