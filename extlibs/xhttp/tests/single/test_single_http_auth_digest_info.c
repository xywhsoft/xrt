#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_HTTP_AUTH_DIGEST_INFO
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"

#include <string.h>



/* 单头发布必须保留 Authentication-Info 的严格解析和规范写出。 */
int main(void)
{
	xhttpdigestinfo Info;
	char Decoded[72];
	char Value[160];
	size_t iSize;

	if ( !xrtHttpDigestInfoRead(
		XRT_STR_LITERAL(
			"qop=auth, rspauth=\"0123456789abcdef0123456789abcdef"
			"0123456789abcdef0123456789abcdef\", "
			"cnonce=\"client\", nc=00000001"
		),
		XHTTP_DIGEST_ALGORITHM_SHA256,
		Decoded, sizeof(Decoded), &iSize, &Info
	) || !xrtHttpDigestInfoWrite(
		&Info, Value, sizeof(Value), &iSize
	) || (iSize == 0u) ||
		(memcmp(Value, "qop=auth", 8u) != 0) ) {
		return 1;
	}
	return 0;
}
