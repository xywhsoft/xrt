#include <stdio.h>

#include <xhttp.h>



/* 从可持久化 secret 计算一份 Digest Authorization 响应值。 */
int main(void)
{
	char Secret[XHTTP_DIGEST_MAX_TEXT_SIZE];
	char Response[XHTTP_DIGEST_MAX_TEXT_SIZE];
	size_t iSize;
	xhttpdigestproof Proof = {
		XHTTP_DIGEST_ALGORITHM_SHA256,
		XHTTP_DIGEST_QOP_AUTH,
		1u,
		{ Secret, 64u },
		XRT_STR_INIT("server-nonce"),
		XRT_STR_INIT("client-nonce"),
		XRT_STR_INIT("/api/items"),
		{ NULL, 0 }
	};

	if ( !xrtHttpDigestSecret(
		Proof.Algorithm,
		XRT_STR_LITERAL("user"),
		XRT_STR_LITERAL("api"),
		XRT_STR_LITERAL("password"),
		Secret, sizeof(Secret), &iSize
	) || !xrtHttpDigestRequest(
		&Proof, XRT_STR_LITERAL("GET"),
		Response, sizeof(Response), &iSize
	) ) {
		return 1;
	}
	printf("response=%.*s\n", (int)iSize, Response);
	return 0;
}
