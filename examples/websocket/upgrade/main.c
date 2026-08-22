#include <stdio.h>

#include <xrt.h>



/* 生成可直接交给 TCP 或 TLS Stream 发送的 WebSocket 请求 Header。 */
int main(void)
{
	char Key[XWS_KEY_CAPACITY];
	char Request[1024];
	xhttpfield Fields[XWS_UPGRADE_REQUEST_FIELDS_MAX];
	size_t iFieldCount = 0;
	size_t iRequest = 0;

	if ( !xrtWsKeyGenerate(Key, sizeof(Key)) ||
		!xrtWsUpgradeRequestFields(
			XRT_STR_LITERAL("example.test"),
			(xstrview) { Key, XWS_KEY_SIZE },
			XRT_STR_LITERAL("chat"),
			(xstrview) { 0 },
			Fields,
			sizeof(Fields) / sizeof(Fields[0]),
			&iFieldCount
		) || !xrtHttp1RequestWrite(
			XRT_STR_LITERAL("GET"),
			XRT_STR_LITERAL("/socket"),
			XHTTP_VERSION_1_1,
			Fields,
			iFieldCount,
			Request,
			sizeof(Request),
			&iRequest
		) ) {
		return 1;
	}
	printf("%.*s", (int)iRequest, Request);
	return 0;
}
