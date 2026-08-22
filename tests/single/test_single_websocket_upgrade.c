#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include <string.h>



/* 单头发布必须保留 HTTP/1.1 Upgrade 的请求校验与响应生成。 */
int main(void)
{
	static const char RequestText[] =
		"GET / HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Upgrade: websocket\r\n"
		"Connection: Upgrade\r\n"
		"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
		"Sec-WebSocket-Version: 13\r\n\r\n";
	xhttpfield Fields[8];
	xhttp1head Head;
	xwsupgrade Upgrade;

	xrtHttp1HeadInit(
		&Head,
		Fields,
		sizeof(Fields) / sizeof(Fields[0])
	);
	if ( (xrtHttp1RequestParse(
		(xbytesview) {
			(cbytes)RequestText,
			sizeof(RequestText) - 1u
		},
		&Head,
		NULL,
		NULL
	) != XHTTP1_READY) || !xrtWsUpgradeRequestCheck(
		&Head,
		NULL,
		&Upgrade
	) || (strcmp(
		Upgrade.Accept,
		"s3pPLMBiTxaQ9kYGzzhZRbK+xOo="
	) != 0) ) {
		return 1;
	}
	return 0;
}
