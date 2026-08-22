#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头发布必须保留 Fetch CORS safelist 判定。 */
int main(void)
{
	if ( !xrtHttpCorsMethodSafelisted(XRT_STR_LITERAL("POST")) ||
		!xrtHttpCorsRequestHeaderSafelisted(
			XRT_STR_LITERAL("Content-Type"),
			XRT_STR_LITERAL("text/plain")
		) || xrtHttpCorsRequestHeaderSafelisted(
			XRT_STR_LITERAL("Content-Type"),
			XRT_STR_LITERAL("application/json")
		) ) {
		return 1;
	}
	return 0;
}
