#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_HTTP_AUTH_DIGEST_MD5
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"

#include <string.h>



/* 单头裁剪选择 MD5 兼容模块时必须启用同一计算 API 的 MD5 分支。 */
int main(void)
{
	char Digest[XHTTP_DIGEST_MAX_TEXT_SIZE];
	size_t iSize;

	if ( !xrtHttpDigestHash(
		XHTTP_DIGEST_ALGORITHM_MD5,
		"abc", 3u, Digest, sizeof(Digest), &iSize
	) || (iSize != 32u) ||
		(memcmp(Digest, "900150983cd24fb0d6963f7d28e17f72", 32u) != 0) ) {
		return 1;
	}
	return 0;
}
