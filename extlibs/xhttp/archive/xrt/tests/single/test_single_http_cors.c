#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头发布必须保留零分配 CORS 预检读取能力。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{ XRT_STR_INIT("Origin"), XRT_STR_INIT("https://app.example") },
		{ XRT_STR_INIT("Access-Control-Request-Method"), XRT_STR_INIT("PUT") }
	};
	xhttpcorsrequest Request;

	if ( !xrtHttpCorsRequestRead(
		XRT_STR_LITERAL("OPTIONS"), Fields, 2u, &Request
	) || ((Request.Flags & XHTTP_CORS_REQUEST_PREFLIGHT) == 0) ||
		!xrtHttpMethodEqual(
			Request.RequestMethod, XRT_STR_LITERAL("PUT")
		) ) {
		return 1;
	}
	return 0;
}
