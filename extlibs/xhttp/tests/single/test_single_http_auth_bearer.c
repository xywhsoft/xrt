#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_HTTP_AUTH_BEARER
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"

#include <string.h>



/* 单头发布必须保留 Bearer 字段的严格解析。 */
int main(void)
{
	xstrview Token;

	if ( !xrtHttpBearerRead(
		XRT_STR_LITERAL("Bearer opaque-token"),
		&Token
	) || (Token.Size != 12u) ||
		(memcmp(Token.Data, "opaque-token", 12u) != 0) ) {
		return 1;
	}
	return 0;
}
