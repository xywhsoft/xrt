#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_HTTP_AUTH_DIGEST
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 单头发布必须保留 Digest 算法和 qop 元数据。 */
int main(void)
{
	return (xrtHttpDigestAlgorithmParse(
		XRT_STR_LITERAL("sha-256-sess")
	) == XHTTP_DIGEST_ALGORITHM_SHA256_SESSION) &&
		(xrtHttpDigestQopParse(
			XRT_STR_LITERAL("AUTH-INT")
		) == XHTTP_DIGEST_QOP_AUTH_INT) ? 0 : 1;
}
