#define XHTTP_MODULE_ALL
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 聚合单头入口必须带入协议对象和客户端传输的完整依赖闭包。 */
int main(void)
{
	xurl Url;
	xquerypair Pair;
	xhttp1callconfig CallConfig;
	size_t iOffset = 0;

	#if !defined(XHTTP_FEATURE_URL) || \
		!defined(XHTTP_FEATURE_QUERY) || \
		!defined(XHTTP_FEATURE_QUERY_CODEC) || \
		!defined(XHTTP_FEATURE_FORM_URLENCODED) || \
		!defined(XHTTP_FEATURE_QUERY_PARAMS) || \
		!defined(XHTTP_FEATURE_FORM_DATA_RANDOM) || \
		!defined(XHTTP_FEATURE_HTTP_CLIENT_STREAM) || \
		!defined(XHTTP_FEATURE_HTTP_CLIENT_STREAM_ASYNC) || \
		!defined(XHTTP_FEATURE_HTTP_CLIENT_TLS) || \
		!defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE) || \
		!defined(XHTTP_FEATURE_HTTP_SERVER_MUX_TLS) || \
		!defined(XHTTP_FEATURE_HTTP_REPLY_COMPRESS) || \
		!defined(XHTTP_FEATURE_COOKIE_JAR_HEADERS)
		#error "XHTTP_MODULE_ALL did not enable its complete dependency closure"
	#endif

	xrtHttp1CallConfigInit(&CallConfig);
	if ( CallConfig.WriteSize == 0 ) {
		return 1;
	}
	if ( !xrtUrlParse(
		XRT_STR_LITERAL("https://example.com/a?q=1"),
		&Url
	) ) {
		return 2;
	}
	return (xrtQueryNext(
		XRT_STR_LITERAL("q=1"), &iOffset, &Pair
	) == XQUERY_NEXT_ITEM) ? 0 : 3;
}
