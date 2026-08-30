#include "../test.h"



/* 解析一份完整请求 Header。 */
static xhttp1head testUpgradeRequest(
	cstr sInput,
	xhttpfield* pFields,
	size_t iCapacity
)
{
	xhttp1head Head;

	xrtHttp1HeadInit(&Head, pFields, iCapacity);
	testRequire(
		xrtHttp1RequestParse(
			(xbytesview) {
				(cbytes)sInput,
				strlen(sInput)
			},
			&Head,
			NULL,
			NULL
		) == XHTTP1_READY,
		"WebSocket Upgrade request fixture parse failed"
	);
	return Head;
}



/* 解析一份完整响应 Header。 */
static xhttp1head testUpgradeResponse(
	cstr sInput,
	xhttpfield* pFields,
	size_t iCapacity
)
{
	xhttp1head Head;

	xrtHttp1HeadInit(&Head, pFields, iCapacity);
	testRequire(
		xrtHttp1ResponseParse(
			(xbytesview) {
				(cbytes)sInput,
				strlen(sInput)
			},
			&Head,
			NULL,
			NULL
		) == XHTTP1_READY,
		"WebSocket Upgrade response fixture parse failed"
	);
	return Head;
}



/* 比较借用文本视图与常量。 */
static bool testUpgradeText(xstrview Text, cstr sExpected)
{
	size_t iSize = strlen(sExpected);

	return (Text.Size == iSize) &&
		(memcmp(Text.Data, sExpected, iSize) == 0);
}



/* 验证最近一次错误属于指定握手边界。 */
static void testUpgradeError(
	xwshandshakeerror Code,
	cstr sMessage
)
{
	const xerror* pError = xrtGetError();

	testRequire(
		(pError != NULL) &&
		(strcmp(
			xrtErrorDomain(pError),
			"xrt.websocket.handshake"
		) == 0) &&
		(xrtErrorCode(pError) == (int32)Code),
		sMessage
	);
	xrtClearError();
}



/* 重复 Upgrade 和子协议字段必须按 RFC 线路顺序正确协商。 */
static void testUpgradeRequestCheck(void)
{
	static const char RequestText[] =
		"GET /chat HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Upgrade: h2c\r\n"
		"Upgrade: websocket\r\n"
		"Connection: keep-alive, Upgrade\r\n"
		"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
		"Sec-WebSocket-Version: 13\r\n"
		"Sec-WebSocket-Protocol: alpha\r\n"
		"Sec-WebSocket-Protocol: beta, chat.v1\r\n"
		"\r\n";
	xhttpfield Fields[16];
	xhttp1head Request = testUpgradeRequest(
		RequestText,
		Fields,
		sizeof(Fields) / sizeof(Fields[0])
	);
	xwsupgradeserverconfig Config;
	xwsupgrade Upgrade;

	xrtWsUpgradeServerConfigInit(&Config);
	Config.Protocols = XRT_STR_LITERAL("chat.v1, beta");
	testRequire(
		xrtWsUpgradeServerConfigValid(&Config) &&
		xrtWsUpgradeRequestCheck(
			&Request,
			&Config,
			&Upgrade
		) && testUpgradeText(Upgrade.Protocol, "beta") &&
		(strcmp(
			Upgrade.Accept,
			"s3pPLMBiTxaQ9kYGzzhZRbK+xOo="
		) == 0),
		"WebSocket Upgrade request negotiation mismatch"
	);
}



/* 字段构建器应能直接组合 HTTP/1 写入器和客户端响应校验。 */
static void testUpgradeWriteAndResponse(void)
{
	static const char Key[] = "dGhlIHNhbXBsZSBub25jZQ==";
	xwsupgradeserverconfig Server;
	xwsupgradeclientconfig Client;
	xwsupgrade ServerUpgrade;
	xwsupgrade ClientUpgrade;
	xhttpfield RequestFields[XWS_UPGRADE_REQUEST_FIELDS_MAX];
	xhttpfield ResponseFields[XWS_UPGRADE_RESPONSE_FIELDS_MAX];
	xhttpfield ParsedFields[16];
	xhttp1head Head;
	char Request[1024];
	char Response[1024];
	size_t iFields = 0;
	size_t iSize = 0;

	testRequire(
		xrtWsUpgradeRequestFields(
			XRT_STR_LITERAL("example.test"),
			XRT_STR_LITERAL("dGhlIHNhbXBsZSBub25jZQ=="),
			XRT_STR_LITERAL("chat, telemetry"),
			(xstrview) { 0 },
			NULL,
			0,
			&iFields
		) && (iFields == 6u) &&
		xrtWsUpgradeRequestFields(
			XRT_STR_LITERAL("example.test"),
			XRT_STR_LITERAL("dGhlIHNhbXBsZSBub25jZQ=="),
			XRT_STR_LITERAL("chat, telemetry"),
			(xstrview) { 0 },
			RequestFields,
			sizeof(RequestFields) / sizeof(RequestFields[0]),
			&iFields
		) && xrtHttp1RequestWrite(
			XRT_STR_LITERAL("GET"),
			XRT_STR_LITERAL("/socket"),
			XHTTP_VERSION_1_1,
			RequestFields,
			iFields,
			Request,
			sizeof(Request),
			&iSize
		),
		"WebSocket Upgrade request field composition failed"
	);
	xrtHttp1HeadInit(
		&Head,
		ParsedFields,
		sizeof(ParsedFields) / sizeof(ParsedFields[0])
	);
	testRequire(
		xrtHttp1RequestParse(
			(xbytesview) { (cbytes)Request, iSize },
			&Head,
			NULL,
			NULL
		) == XHTTP1_READY,
		"Generated WebSocket request did not parse"
	);
	xrtWsUpgradeServerConfigInit(&Server);
	Server.Protocols = XRT_STR_LITERAL("telemetry, chat");
	testRequire(
		xrtWsUpgradeRequestCheck(
			&Head,
			&Server,
			&ServerUpgrade
		),
		"Generated WebSocket request did not validate"
	);
	iFields = 0;
	testRequire(
		xrtWsUpgradeResponseFields(
			(xstrview) {
				ServerUpgrade.Accept,
				XWS_ACCEPT_SIZE
			},
			ServerUpgrade.Protocol,
			(xstrview) { 0 },
			ResponseFields,
			sizeof(ResponseFields) / sizeof(ResponseFields[0]),
			&iFields
		) && (iFields == 4u) &&
		xrtHttp1ResponseWrite(
			XHTTP_VERSION_1_1,
			XHTTP_STATUS_SWITCHING_PROTOCOLS,
			XRT_STR_LITERAL("Switching Protocols"),
			ResponseFields,
			iFields,
			Response,
			sizeof(Response),
			&iSize
		),
		"WebSocket Upgrade response field composition failed"
	);
	xrtHttp1HeadInit(
		&Head,
		ParsedFields,
		sizeof(ParsedFields) / sizeof(ParsedFields[0])
	);
	testRequire(
		xrtHttp1ResponseParse(
			(xbytesview) { (cbytes)Response, iSize },
			&Head,
			NULL,
			NULL
		) == XHTTP1_READY,
		"Generated WebSocket response did not parse"
	);
	xrtWsUpgradeClientConfigInit(&Client);
	Client.Protocols = XRT_STR_LITERAL("chat, telemetry");
	testRequire(
		xrtWsUpgradeResponseCheck(
			&Head,
			(xstrview) { Key, XWS_KEY_SIZE },
			&Client,
			&ClientUpgrade
		) && testUpgradeText(ClientUpgrade.Protocol, "chat"),
		"Generated WebSocket response did not validate"
	);
}



/* 正文分帧字段和跨重复字段的子协议重复必须拒绝。 */
static void testUpgradeFailures(void)
{
	static const char BodyRequest[] =
		"GET / HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Upgrade: websocket\r\n"
		"Connection: Upgrade\r\n"
		"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
		"Sec-WebSocket-Version: 13\r\n"
		"Content-Length: 0\r\n\r\n";
	static const char DuplicateRequest[] =
		"GET / HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Upgrade: websocket\r\n"
		"Connection: Upgrade\r\n"
		"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
		"Sec-WebSocket-Version: 13\r\n"
		"Sec-WebSocket-Protocol: chat\r\n"
		"Sec-WebSocket-Protocol: telemetry, chat\r\n\r\n";
	static const char LowercaseRequest[] =
		"get / HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Upgrade: websocket\r\n"
		"Connection: Upgrade\r\n"
		"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
		"Sec-WebSocket-Version: 13\r\n\r\n";
	static const char ExtensionResponse[] =
		"HTTP/1.1 101 Switching Protocols\r\n"
		"Upgrade: websocket\r\n"
		"Connection: Upgrade\r\n"
		"Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
		"Sec-WebSocket-Extensions: x-test\r\n\r\n";
	xhttpfield Fields[16];
	xhttp1head Head;
	xwsupgradeserverconfig Server;
	xwsupgradeclientconfig Client;
	xwsupgrade Upgrade;

	xrtWsUpgradeServerConfigInit(&Server);
	Head = testUpgradeRequest(
		BodyRequest,
		Fields,
		sizeof(Fields) / sizeof(Fields[0])
	);
	testRequire(
		!xrtWsUpgradeRequestCheck(&Head, &Server, &Upgrade),
		"WebSocket Upgrade accepted Content-Length"
	);
	testUpgradeError(
		XWS_HANDSHAKE_ERROR_BODY,
		"WebSocket Upgrade body error mismatch"
	);
	Head = testUpgradeRequest(
		LowercaseRequest,
		Fields,
		sizeof(Fields) / sizeof(Fields[0])
	);
	testRequire(
		(Head.MethodCode == XHTTP_METHOD_OTHER) &&
		!xrtWsUpgradeRequestCheck(&Head, &Server, &Upgrade),
		"WebSocket Upgrade accepted a lowercase GET extension token"
	);
	testUpgradeError(
		XWS_HANDSHAKE_ERROR_METHOD,
		"WebSocket Upgrade lowercase method error mismatch"
	);
	Server.Protocols = XRT_STR_LITERAL("chat, telemetry");
	Head = testUpgradeRequest(
		DuplicateRequest,
		Fields,
		sizeof(Fields) / sizeof(Fields[0])
	);
	testRequire(
		!xrtWsUpgradeRequestCheck(&Head, &Server, &Upgrade),
		"WebSocket Upgrade accepted a repeated protocol"
	);
	testUpgradeError(
		XWS_HANDSHAKE_ERROR_PROTOCOL,
		"WebSocket Upgrade repeated protocol error mismatch"
	);
	xrtWsUpgradeClientConfigInit(&Client);
	Head = testUpgradeResponse(
		ExtensionResponse,
		Fields,
		sizeof(Fields) / sizeof(Fields[0])
	);
	testRequire(
		!xrtWsUpgradeResponseCheck(
			&Head,
			XRT_STR_LITERAL("dGhlIHNhbXBsZSBub25jZQ=="),
			&Client,
			&Upgrade
		),
		"WebSocket Upgrade accepted an unoffered extension"
	);
	testUpgradeError(
		XWS_HANDSHAKE_ERROR_EXTENSION,
		"WebSocket Upgrade extension error mismatch"
	);
}



/* 字段输出不得覆盖其描述符继续借用的输入文本。 */
static void testUpgradeFieldOverlap(void)
{
	union {
		xhttpfield Align;
		uint8 Bytes[256];
	} Storage;
	static const char Host[] = "example.test";
	static const char Key[] = "dGhlIHNhbXBsZSBub25jZQ==";
	char KeyOutput[sizeof(Key) + sizeof(size_t)];
	size_t iCount = 99u;

	memset(&Storage, 0xA5, sizeof(Storage));
	memcpy(Storage.Bytes, Host, sizeof(Host) - 1u);
	testRequire(
		!xrtWsUpgradeRequestFields(
			(xstrview) {
				(cstr)Storage.Bytes,
				sizeof(Host) - 1u
			},
			(xstrview) { Key, sizeof(Key) - 1u },
			(xstrview) { 0 },
			(xstrview) { 0 },
			(xhttpfield*)(void*)Storage.Bytes,
			1u,
			&iCount
		) && (iCount == 99u) &&
		(memcmp(Storage.Bytes, Host, sizeof(Host) - 1u) == 0),
		"WebSocket Upgrade field output overlapped Host input"
	);
	memcpy(KeyOutput, Key, sizeof(Key));
	testRequire(
		!xrtWsUpgradeRequestFields(
			XRT_STR_LITERAL("example.test"),
			(xstrview) { KeyOutput, sizeof(Key) - 1u },
			(xstrview) { 0 },
			(xstrview) { 0 },
			NULL,
			0,
			(size_t*)(void*)KeyOutput
		) && (memcmp(KeyOutput, Key, sizeof(Key)) == 0),
		"WebSocket Upgrade field count overlapped Key input"
	);
}



int main(void)
{
	testUpgradeRequestCheck();
	testUpgradeWriteAndResponse();
	testUpgradeFailures();
	testUpgradeFieldOverlap();
	return 0;
}
