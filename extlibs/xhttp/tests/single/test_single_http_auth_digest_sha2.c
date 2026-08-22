#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_HTTP_AUTH_DIGEST_SHA2
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"

#include <string.h>



/* 单头发布必须保留 SHA-256 与 SHA-512/256 Digest 计算后端。 */
int main(void)
{
	char Digest[XHTTP_DIGEST_MAX_TEXT_SIZE];
	size_t iSize;

	if ( !xrtHttpDigestHash(
		XHTTP_DIGEST_ALGORITHM_SHA256,
		"abc", 3u, Digest, sizeof(Digest), &iSize
	) || (iSize != 64u) ||
		(memcmp(Digest, "ba7816bf8f01cfea", 16u) != 0) ||
		!xrtHttpDigestHash(
			XHTTP_DIGEST_ALGORITHM_SHA512_256,
			"abc", 3u, Digest, sizeof(Digest), &iSize
		) || (memcmp(Digest, "53048e2681941ef9", 16u) != 0) ) {
		return 1;
	}
	return 0;
}
