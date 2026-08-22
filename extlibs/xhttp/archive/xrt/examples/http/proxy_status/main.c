#include <stdio.h>

#include <xrt/http_proxy_status.h>



/* 按链路顺序读取代理错误和下一跳诊断。 */
int main(void)
{
	xstrview Value = XRT_STR_LITERAL(
		"OriginProxy, EdgeProxy;error=connection_timeout;"
		"next-hop=origin.example"
	);
	xhttpproxystatuscursor Cursor;
	xhttpproxystatus Status;

	xrtHttpProxyStatusCursorInit(&Cursor);
	while ( xrtHttpProxyStatusNext(
		Value, &Cursor, &Status
	) == XHTTP_NEXT_ITEM ) {
		printf(
			"proxy = %.*s, error = %.*s\n",
			(int)Status.Proxy.Encoded.Size,
			Status.Proxy.Encoded.Data,
			(int)Status.Error.Encoded.Size,
			(Status.Error.Encoded.Data != NULL) ?
				Status.Error.Encoded.Data : ""
		);
	}
	return xrtGetError() == NULL ? 0 : 1;
}
