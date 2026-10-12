#define XHTTP_MODULE_ALL
#define XHTTP_IMPLEMENTATION
#include "../../include/xhttp/features.h"
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
#include "../../../../single/extlibs/xhttp.h"

#if defined(XRT_FEATURE_NET) || defined(XHTTP_FEATURE_HTTP_CLIENT) || defined(XHTTP_FEATURE_HTTP_SERVER) || defined(XHTTP_FEATURE_COOKIE_JAR)
#error "message-only xhttp imported a runtime or policy"
#endif

int main(void)
{
	xurl Url;
	xquerypair Pair;
	size_t Offset = 0;
	if (!xrtUrlParse(XRT_STR_LITERAL("https://example.com/a?q=1"), &Url)) return 1;
	return xrtQueryNext(XRT_STR_LITERAL("q=1"), &Offset, &Pair) == XQUERY_NEXT_ITEM ? 0 : 2;
}
