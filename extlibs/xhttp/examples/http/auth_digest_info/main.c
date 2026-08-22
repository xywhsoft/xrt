#include <stdio.h>

#include <xhttp.h>



/* 解析服务器返回的 Digest Authentication-Info 字段值。 */
int main(void)
{
	xhttpdigestinfo Info;
	char Output[96];
	size_t iSize;

	if ( !xrtHttpDigestInfoRead(
		XRT_STR_LITERAL(
			"nextnonce=\"server-next\", qop=auth, "
			"rspauth=\"0123456789abcdef0123456789abcdef"
			"0123456789abcdef0123456789abcdef\", "
			"cnonce=\"client\", nc=00000001"
		),
		XHTTP_DIGEST_ALGORITHM_SHA256,
		Output, sizeof(Output), &iSize, &Info
	) ) {
		return 1;
	}
	printf(
		"next nonce: %.*s\n",
		(int)Info.NextNonce.Size,
		Info.NextNonce.Data
	);
	return 0;
}
