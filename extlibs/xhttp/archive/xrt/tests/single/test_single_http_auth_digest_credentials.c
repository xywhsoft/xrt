#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include <string.h>



/* 单头发布必须保留 Digest 凭据解析和 username* 解码。 */
int main(void)
{
	xhttpdigestauth Digest;
	char Output[96];
	size_t iSize;

	if ( !xrtHttpDigestAuthRead(
		XRT_STR_LITERAL(
			"Digest username*=UTF-8''J%C3%A4s%C3%B8n, realm=\"api\", "
			"uri=\"/\", algorithm=SHA-256, nonce=\"n\", nc=00000001, "
			"cnonce=\"c\", qop=auth, "
			"response=\"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\""
		),
		Output, sizeof(Output), &iSize, &Digest
	) || (Digest.Username.Size != 7u) ||
		(memcmp(Digest.Username.Data, "J\xC3\xA4s\xC3\xB8n", 7u) != 0) ) {
		return 1;
	}
	return 0;
}
