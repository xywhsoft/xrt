#include <stdio.h>

#include <xhttp.h>



/* 解析一份完整 Digest Authorization 字段值。 */
int main(void)
{
	xhttpdigestauth Digest;
	char Output[192];
	size_t iSize;

	if ( !xrtHttpDigestAuthRead(
		XRT_STR_LITERAL(
			"Digest username=\"Mufasa\", realm=\"api\", uri=\"/\", "
			"algorithm=SHA-256, nonce=\"server\", nc=00000001, "
			"cnonce=\"client\", qop=auth, "
			"response=\"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\""
		),
		Output, sizeof(Output), &iSize, &Digest
	) ) {
		return 1;
	}
	printf(
		"user=%.*s nc=%u\n",
		(int)Digest.Username.Size,
		Digest.Username.Data,
		(unsigned int)Digest.NonceCount
	);
	return 0;
}
