#include "../test.h"



/* 解析测试使用的 HTTP/1 Header。 */
static xhttp1head testUpgradeDeflateParse(
	cstr sInput,
	bool bRequest,
	xhttpfield* pFields,
	size_t iCapacity
)
{
	xhttp1head Head;
	xhttp1status Status;

	xrtHttp1HeadInit(&Head, pFields, iCapacity);
	Status = bRequest ?
		xrtHttp1RequestParse(
			(xbytesview) {
				(cbytes)sInput,
				strlen(sInput)
			},
			&Head,
			NULL,
			NULL
		) :
		xrtHttp1ResponseParse(
			(xbytesview) {
				(cbytes)sInput,
				strlen(sInput)
			},
			&Head,
			NULL,
			NULL
		);
	testRequire(
		Status == XHTTP1_READY,
		"WebSocket compressed Upgrade fixture parse failed"
	);
	return Head;
}



/* 默认压缩 offer 和响应必须能完成双向绑定校验。 */
static void testUpgradeDeflateRoundTrip(void)
{
	static const char RequestText[] =
		"GET / HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Upgrade: websocket\r\n"
		"Connection: Upgrade\r\n"
		"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
		"Sec-WebSocket-Version: 13\r\n"
		"Sec-WebSocket-Extensions: permessage-deflate\r\n\r\n";
	xhttpfield Fields[16];
	xhttp1head Head = testUpgradeDeflateParse(
		RequestText,
		true,
		Fields,
		sizeof(Fields) / sizeof(Fields[0])
	);
	xwsupgradeserverconfig Server;
	xwsupgradeclientconfig Client;
	xwsupgrade ServerUpgrade;
	xwsupgrade ClientUpgrade;
	xhttpfield ResponseFields[XWS_UPGRADE_RESPONSE_FIELDS_MAX];
	char Response[1024];
	size_t iFieldCount = 0;
	size_t iSize = 0;

	xrtWsUpgradeServerConfigInit(&Server);
	Server.EnableDeflate = true;
	testRequire(
		xrtWsUpgradeRequestCheck(
			&Head,
			&Server,
			&ServerUpgrade
		) && ServerUpgrade.DeflateEnabled &&
		(ServerUpgrade.ExtensionSize != 0) &&
		xrtWsUpgradeResponseFields(
			(xstrview) {
				ServerUpgrade.Accept,
				XWS_ACCEPT_SIZE
			},
			ServerUpgrade.Protocol,
			(xstrview) {
				ServerUpgrade.Extensions,
				ServerUpgrade.ExtensionSize
			},
			ResponseFields,
			sizeof(ResponseFields) / sizeof(ResponseFields[0]),
			&iFieldCount
		) && xrtHttp1ResponseWrite(
			XHTTP_VERSION_1_1,
			XHTTP_STATUS_SWITCHING_PROTOCOLS,
			XRT_STR_LITERAL("Switching Protocols"),
			ResponseFields,
			iFieldCount,
			Response,
			sizeof(Response),
			&iSize
		),
		"WebSocket server compression negotiation failed"
	);
	xrtHttp1HeadInit(
		&Head,
		Fields,
		sizeof(Fields) / sizeof(Fields[0])
	);
	testRequire(
		xrtHttp1ResponseParse(
			(xbytesview) { (cbytes)Response, iSize },
			&Head,
			NULL,
			NULL
		) == XHTTP1_READY,
		"WebSocket compressed response parse failed"
	);
	xrtWsUpgradeClientConfigInit(&Client);
	Client.EnableDeflate = true;
	testRequire(
		xrtWsUpgradeClientConfigValid(&Client) &&
			xrtWsUpgradeResponseCheck(
				&Head,
				XRT_STR_LITERAL("dGhlIHNhbXBsZSBub25jZQ=="),
				&Client,
				&ClientUpgrade
		) && ClientUpgrade.DeflateEnabled &&
		(ClientUpgrade.ExtensionSize == ServerUpgrade.ExtensionSize) &&
		(memcmp(
			ClientUpgrade.Extensions,
			ServerUpgrade.Extensions,
			ServerUpgrade.ExtensionSize
		 ) == 0) &&
		(ClientUpgrade.Extensions[ClientUpgrade.ExtensionSize] == '\0'),
		"WebSocket client compression response validation failed"
	);
}



/* 客户端不得接受未知扩展，服务端不得接受重复压缩 offer。 */
static void testUpgradeDeflateReject(void)
{
	static const char UnknownResponse[] =
		"HTTP/1.1 101 Switching Protocols\r\n"
		"Upgrade: websocket\r\n"
		"Connection: Upgrade\r\n"
		"Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
		"Sec-WebSocket-Extensions: x-test\r\n\r\n";
	static const char DuplicateRequest[] =
		"GET / HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Upgrade: websocket\r\n"
		"Connection: Upgrade\r\n"
		"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
		"Sec-WebSocket-Version: 13\r\n"
		"Sec-WebSocket-Extensions: permessage-deflate\r\n"
		"Sec-WebSocket-Extensions: permessage-deflate\r\n\r\n";
	xhttpfield Fields[16];
	xhttp1head Head;
	xwsupgradeclientconfig Client;
	xwsupgradeserverconfig Server;
	xwsupgrade Upgrade;

	Head = testUpgradeDeflateParse(
		UnknownResponse,
		false,
		Fields,
		sizeof(Fields) / sizeof(Fields[0])
	);
	xrtWsUpgradeClientConfigInit(&Client);
	Client.EnableDeflate = true;
	testRequire(
		!xrtWsUpgradeResponseCheck(
			&Head,
			XRT_STR_LITERAL("dGhlIHNhbXBsZSBub25jZQ=="),
			&Client,
			&Upgrade
		),
		"WebSocket client accepted an unknown extension"
	);
	xrtClearError();
	Head = testUpgradeDeflateParse(
		DuplicateRequest,
		true,
		Fields,
		sizeof(Fields) / sizeof(Fields[0])
	);
	xrtWsUpgradeServerConfigInit(&Server);
	Server.EnableDeflate = true;
	testRequire(
		!xrtWsUpgradeRequestCheck(
			&Head,
			&Server,
			&Upgrade
		),
		"WebSocket server accepted duplicate compression offers"
	);
	xrtClearError();
}



int main(void)
{
	testUpgradeDeflateRoundTrip();
	testUpgradeDeflateReject();
	return 0;
}
