#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_HTTP_AUTH_DIGEST_NONCE_RANDOM
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 单头发布必须保留安全随机 nonce 便利入口。 */
int main(void)
{
	uint8 KeyData[XHTTP_DIGEST_NONCE_KEY_MIN];
	char Nonce[XHTTP_DIGEST_NONCE_TEXT_SIZE];
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
