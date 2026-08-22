#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 单头发布必须保留重复 Proxy-Status 字段解析。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{ XRT_STR_INIT("Proxy-Status"), XRT_STR_INIT("OriginProxy") },
		{ XRT_STR_INIT("proxy-status"), XRT_STR_INIT("EdgeProxy;error=dns_error") }
	};
	xhttpproxystatusfieldcursor Cursor;
	xhttpproxystatus Status;

	xrtHttpProxyStatusFieldCursorInit(&Cursor);
	return (xrtHttpProxyStatusFieldNext(
		Fields, 2, &Cursor, &Status
	) == XHTTP_NEXT_ITEM) &&
		(xrtHttpProxyStatusFieldNext(
			Fields, 2, &Cursor, &Status
		) == XHTTP_NEXT_ITEM) &&
		((Status.Flags & XHTTP_PROXY_STATUS_HAS_ERROR) != 0) ?
		0 : 1;
}
