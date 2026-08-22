#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_HTTP_AUTH
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"

#include <string.h>



/* 单头发布必须保留 challenge 的参数边界识别。 */
int main(void)
{
	xhttpauth Auth;
	size_t iOffset = 0;

	if ( (xrtHttpChallengeNext(
		XRT_STR_LITERAL("Digest realm=\"x\", Basic abc"),
		&iOffset,
		&Auth
	) != XHTTP_NEXT_ITEM) ||
		!xrtHttpTokenEqual(
			Auth.Scheme, XRT_STR_LITERAL("Digest")
		) || (Auth.Kind != XHTTP_AUTH_PARAMS) ) {
		return 1;
	}
	return 0;
}
