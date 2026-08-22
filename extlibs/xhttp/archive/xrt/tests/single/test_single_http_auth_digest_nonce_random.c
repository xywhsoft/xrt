#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头发布必须保留安全随机 nonce 便利入口。 */
int main(void)
{
	uint8 KeyData[XRT_HTTP_DIGEST_NONCE_KEY_MIN];
	char Nonce[XRT_HTTP_DIGEST_NONCE_TEXT_SIZE];
	xbytesview Key = { KeyData, sizeof(KeyData) };
	size_t iSize;

	memset(KeyData, 0x33, sizeof(KeyData));
	if ( !xrtHttpDigestNonceCreate(
		Key, XRT_BYTES_LITERAL("api"), INT64_C(1700000000),
		Nonce, sizeof(Nonce), &iSize
	) || (xrtHttpDigestNonceVerify(
		(xstrview){ Nonce, iSize }, Key, XRT_BYTES_LITERAL("api"),
		INT64_C(1700000000), 60, 0, NULL
	) != XHTTP_DIGEST_NONCE_VALID) ) {
		return 1;
	}
	return 0;
}
