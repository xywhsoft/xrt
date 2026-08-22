#include <stdio.h>
#include <string.h>

#include <xrt/http_auth.h>



/* 使用系统安全随机源生成服务端无状态 nonce。 */
int main(void)
{
	uint8 KeyData[XRT_HTTP_DIGEST_NONCE_KEY_MIN];
	char Nonce[XRT_HTTP_DIGEST_NONCE_TEXT_SIZE + 1u];
	xbytesview Key = { KeyData, sizeof(KeyData) };
	size_t iSize;

	memset(KeyData, 0x33, sizeof(KeyData));
	if ( !xrtHttpDigestNonceCreate(
		Key,
		XRT_BYTES_LITERAL("api@example.test"),
		INT64_C(1700000000),
		Nonce,
		XRT_HTTP_DIGEST_NONCE_TEXT_SIZE,
		&iSize
	) ) {
		return 1;
	}
	Nonce[iSize] = 0;
	printf("nonce=%s\n", Nonce);
	return 0;
}
