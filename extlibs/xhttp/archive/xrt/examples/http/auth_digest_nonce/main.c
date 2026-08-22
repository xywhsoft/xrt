#include <stdio.h>
#include <string.h>

#include <xrt/http_auth.h>



/* 使用外部 salt 构建并验证可复现的无状态 nonce。 */
int main(void)
{
	uint8 KeyData[XRT_HTTP_DIGEST_NONCE_KEY_MIN];
	uint8 Salt[XRT_HTTP_DIGEST_NONCE_SALT_SIZE];
	char Nonce[XRT_HTTP_DIGEST_NONCE_TEXT_SIZE + 1u];
	xbytesview Key = { KeyData, sizeof(KeyData) };
	size_t iSize;

	memset(KeyData, 0x11, sizeof(KeyData));
	memset(Salt, 0x22, sizeof(Salt));
	if ( !xrtHttpDigestNonceWrite(
		Key,
		XRT_BYTES_LITERAL("api@example.test"),
		INT64_C(1700000000),
		Salt,
		Nonce,
		XRT_HTTP_DIGEST_NONCE_TEXT_SIZE,
		&iSize
	) ) {
		return 1;
	}
	Nonce[iSize] = 0;
	printf("nonce=%s\n", Nonce);
	return xrtHttpDigestNonceVerify(
		(xstrview){ Nonce, iSize },
		Key,
		XRT_BYTES_LITERAL("api@example.test"),
		INT64_C(1700000030),
		60,
		5,
		NULL
	) == XHTTP_DIGEST_NONCE_VALID ? 0 : 1;
}
