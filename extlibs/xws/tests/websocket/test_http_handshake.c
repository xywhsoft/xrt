#include "../test.h"
#include "../../../xhttp/src/internal/xrt_http_server.h"
#include "../../../xhttp/src/internal/xrt_http_server_runtime.h"



typedef struct test_ws_server_case {
	cstr Input;
	xstrview Protocols;
	xwshandshakeerror Error;
} test_ws_server_case;



/* 从完整 HTTP/1 请求头建立拥有型服务端请求快照。 */
static xhttpserverrequest* testWsServerRequest(
	cstr sInput
)
{
	xhttpfield Fields[16];
	xhttp1bodyplan Plan;
	xhttp1head Head;
	size_t iSize = strlen(sInput);

	xrtHttp1HeadInit(
		&Head,
		Fields,
		sizeof(Fields) / sizeof(Fields[0])
	);
	testRequire(
		xrtHttp1RequestParse(
			(xbytesview) {
				(cbytes)sInput,
				iSize
			},
			&Head,
			NULL,
			NULL
		) == XHTTP1_READY,
		"WebSocket server request fixture parse failed"
	);
	testRequire(
		xrtHttp1RequestBodyPlan(&Head, &Plan),
		"WebSocket server request body plan failed"
	);
	return __xrtHttpServerRequestCreate(
		&Head,
		&Plan,
		XHTTP_SERVER_REQUEST_UPGRADE
	);
}



/* 比较借用文本视图与常量。 */
static bool testWsServerText(
	xstrview Text,
	cstr sExpected
)
{
	size_t iSize = strlen(sExpected);

	return (Text.Size == iSize) &&
		(memcmp(Text.Data, sExpected, iSize) == 0);
}



/* 迁移旧版重复子协议字段边界并验证 101 Reply 快照。 */
static void testWsServerSuccess(void)
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
		"Sec-WebSocket-Extensions: permessage-deflate\r\n"
		"\r\n";
	xhttpserverrequest* pRequest =
		testWsServerRequest(RequestText);
	xwsserverconfig Config;
	xwsserverhandshake Handshake;
	xhttpreply* pReply;
	const xhttpfield* pField;

	xrtWsServerConfigInit(&Config);
	Config.Protocols =
		XRT_STR_LITERAL("chat.v1, beta");
	testRequire(
		(pRequest != NULL) &&
		xrtWsServerCheck(
			pRequest,
			&Config,
			&Handshake
		) &&
		(strcmp(
			Handshake.Accept,
			"s3pPLMBiTxaQ9kYGzzhZRbK+xOo="
		 ) == 0) &&
		testWsServerText(
			Handshake.Protocol,
			"beta"
		),
		"WebSocket repeated protocol selection mismatch"
	);
	pReply = xrtWsServerReply(&Handshake);
	testRequire(
		(pReply != NULL) &&
		(xrtHttpReplyStatus(pReply) ==
		 XHTTP_STATUS_SWITCHING_PROTOCOLS) &&
		(xrtHttpReplyHeaderCount(pReply) == 4),
		"WebSocket 101 Reply shape mismatch"
	);
	xrtHttpServerRequestDestroy(pRequest);
	pField = xrtHttpReplyHeader(
		pReply,
		XRT_STR_LITERAL("Sec-WebSocket-Protocol")
	);
	testRequire(
		(pField != NULL) &&
		testWsServerText(pField->Value, "beta") &&
		((pField = xrtHttpReplyHeader(
			pReply,
			XRT_STR_LITERAL("Sec-WebSocket-Accept")
		 )) != NULL) &&
		testWsServerText(
			pField->Value,
			"s3pPLMBiTxaQ9kYGzzhZRbK+xOo="
		),
		"WebSocket Reply did not own handshake fields"
	);
	xrtHttpReplyDestroy(pReply);

	pRequest = testWsServerRequest(RequestText);
	Config.Protocols = XRT_STR_LITERAL("");
	testRequire(
		(pRequest != NULL) &&
		xrtWsServerCheck(
			pRequest,
			&Config,
			&Handshake
		) &&
		(Handshake.Protocol.Size == 0),
		"WebSocket server could not decline all protocols"
	);
	xrtHttpServerRequestDestroy(pRequest);
}



/* 验证全部拒绝路径保留输出并发布稳定握手错误。 */
static void testWsServerFailures(void)
{
	static const test_ws_server_case Cases[] = {
		{
			"POST /chat HTTP/1.1\r\n"
			"Host: example.test\r\n"
			"Upgrade: websocket\r\n"
			"Connection: Upgrade\r\n"
			"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
			"Sec-WebSocket-Version: 13\r\n\r\n",
			XRT_STR_LITERAL(""),
			XWS_HANDSHAKE_ERROR_METHOD
		},
		{
			"GET /chat HTTP/1.0\r\n"
			"Host: example.test\r\n"
			"Upgrade: websocket\r\n"
			"Connection: Upgrade\r\n"
			"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
			"Sec-WebSocket-Version: 13\r\n\r\n",
			XRT_STR_LITERAL(""),
			XWS_HANDSHAKE_ERROR_VERSION
		},
		{
			"GET /chat HTTP/1.1\r\n"
			"Host: one.test\r\n"
			"Host: two.test\r\n"
			"Upgrade: websocket\r\n"
			"Connection: Upgrade\r\n"
			"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
			"Sec-WebSocket-Version: 13\r\n\r\n",
			XRT_STR_LITERAL(""),
			XWS_HANDSHAKE_ERROR_HOST
		},
		{
			"GET /chat HTTP/1.1\r\n"
			"Host: bad host\r\n"
			"Upgrade: websocket\r\n"
			"Connection: Upgrade\r\n"
			"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
			"Sec-WebSocket-Version: 13\r\n\r\n",
			XRT_STR_LITERAL(""),
			XWS_HANDSHAKE_ERROR_HOST
		},
		{
			"GET /chat HTTP/1.1\r\n"
			"Host: example.test\r\n"
			"Upgrade: websocket/13\r\n"
			"Connection: Upgrade\r\n"
			"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
			"Sec-WebSocket-Version: 13\r\n\r\n",
			XRT_STR_LITERAL(""),
			XWS_HANDSHAKE_ERROR_UPGRADE
		},
		{
			"GET /chat HTTP/1.1\r\n"
			"Host: example.test\r\n"
			"Upgrade: websocket\r\n"
			"Connection: keep-alive\r\n"
			"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
			"Sec-WebSocket-Version: 13\r\n\r\n",
			XRT_STR_LITERAL(""),
			XWS_HANDSHAKE_ERROR_CONNECTION
		},
		{
			"GET /chat HTTP/1.1\r\n"
			"Host: example.test\r\n"
			"Upgrade: websocket\r\n"
			"Connection: Upgrade\r\n"
			"Sec-WebSocket-Key: bad\r\n"
			"Sec-WebSocket-Version: 13\r\n\r\n",
			XRT_STR_LITERAL(""),
			XWS_HANDSHAKE_ERROR_KEY
		},
		{
			"GET /chat HTTP/1.1\r\n"
			"Host: example.test\r\n"
			"Upgrade: websocket\r\n"
			"Connection: Upgrade\r\n"
			"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
			"Sec-WebSocket-Version: 12\r\n\r\n",
			XRT_STR_LITERAL(""),
			XWS_HANDSHAKE_ERROR_VERSION
		},
		{
			"GET /chat HTTP/1.1\r\n"
			"Host: example.test\r\n"
			"Upgrade: websocket\r\n"
			"Connection: Upgrade\r\n"
			"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
			"Sec-WebSocket-Version: 13\r\n"
			"Content-Length: 0\r\n\r\n",
			XRT_STR_LITERAL(""),
			XWS_HANDSHAKE_ERROR_BODY
		},
		{
			"GET /chat HTTP/1.1\r\n"
			"Host: example.test\r\n"
			"Upgrade: websocket\r\n"
			"Connection: Upgrade\r\n"
			"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
			"Sec-WebSocket-Version: 13\r\n"
			"Sec-WebSocket-Protocol: alpha\r\n"
			"Sec-WebSocket-Protocol: beta, alpha\r\n\r\n",
			XRT_STR_LITERAL("alpha, beta"),
			XWS_HANDSHAKE_ERROR_PROTOCOL
		}
	};

	for ( size_t i = 0;
		i < (sizeof(Cases) / sizeof(Cases[0]));
		i++ ) {
		xhttpserverrequest* pRequest =
			testWsServerRequest(Cases[i].Input);
		xwsserverconfig Config;
		xwsserverhandshake Handshake;
		xwsserverhandshake Before;

		xrtWsServerConfigInit(&Config);
		Config.Protocols = Cases[i].Protocols;
		testRequire(
			pRequest != NULL,
			"WebSocket failure request creation failed"
		);
		memset(&Handshake, 0xA5, sizeof(Handshake));
		Before = Handshake;
		testRequire(
			!xrtWsServerCheck(
				pRequest,
				&Config,
				&Handshake
			) &&
			(memcmp(
				&Handshake,
				&Before,
				sizeof(Handshake)
			 ) == 0) &&
			(xrtErrorKind(xrtGetError()) != XERR_NONE) &&
			(xrtErrorCode(xrtGetError()) ==
			 (int32)Cases[i].Error) &&
			(strcmp(
				xrtErrorDomain(xrtGetError()),
				"xrt.websocket.handshake"
			 ) == 0),
			"WebSocket server rejection contract mismatch"
		);
		xrtClearError();
		xrtHttpServerRequestDestroy(pRequest);
	}
}



/* 覆盖迁移后的成功边界与严格拒绝矩阵。 */
/* 验证范围失败发布统一的 WebSocket 握手参数错误。 */
static void testWsServerArgumentError(cstr sMessage)
{
	const xerror* pError = xrtGetError();

	testRequire(
		(pError != NULL) &&
		(xrtErrorKind(pError) == XERR_ARGUMENT) &&
		(xrtErrorCode(pError) ==
		 XWS_HANDSHAKE_ERROR_ARGUMENT) &&
		(strcmp(
			xrtErrorDomain(pError),
			"xrt.websocket.handshake"
		 ) == 0),
		sMessage
	);
	xrtClearError();
}



/* 无效异步入口测试只验证提交前契约，不接管任何连接。 */
static void testWsServerRangeDone(
	xhttpconn* pHttp,
	xnetresult Result,
	xwsconn* pConnection,
	const xerror* pError,
	ptr pData
)
{
	(void)pHttp;
	(void)Result;
	(void)pConnection;
	(void)pError;
	(void)pData;
}



/* 压实非对齐快照、地址回绕、输出重叠和提交前参数检查。 */
static void testWsServerRanges(void)
{
	static const char RequestText[] =
		"GET /chat HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Upgrade: websocket\r\n"
		"Connection: Upgrade\r\n"
		"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
		"Sec-WebSocket-Version: 13\r\n\r\n";
	uint8 ConfigStorage[sizeof(xwsserverconfig) + 2u];
	uint8 HandshakeStorage[sizeof(xwsserverhandshake) + 2u];
	xwsserverconfig* pConfig =
		(xwsserverconfig*)(void*)(ConfigStorage + 1u);
	xwsserverhandshake* pHandshake =
		(xwsserverhandshake*)(void*)(HandshakeStorage + 1u);
	const xwsserverconfig* pWrappingConfig =
		(const xwsserverconfig*)(uintptr_t)(UINTPTR_MAX - 1u);
	const xhttpserverrequest* pWrappingRequest =
		(const xhttpserverrequest*)(uintptr_t)(UINTPTR_MAX - 1u);
	const xwsserverhandshake* pWrappingHandshake =
		(const xwsserverhandshake*)(uintptr_t)(UINTPTR_MAX - 1u);
	const xwsconnevents* pWrappingEvents =
		(const xwsconnevents*)(uintptr_t)(UINTPTR_MAX - 1u);
	xhttpconn* pWrappingHttp =
		(xhttpconn*)(uintptr_t)(UINTPTR_MAX - 1u);
	xhttpserverrequest* pRequest;
	xwsserverconfig Config;
	xwsserverhandshake Handshake;
	xwsserverhandshake Before;
	xhttpconn Http;
	xhttpreply* pReply;

	memset(ConfigStorage, 0xC3, sizeof(ConfigStorage));
	memset(HandshakeStorage, 0xD4, sizeof(HandshakeStorage));
	xrtWsServerConfigInit(pConfig);
	memcpy(&Config, pConfig, sizeof(Config));
	testRequire(
		(ConfigStorage[0] == UINT8_C(0xC3)) &&
		(ConfigStorage[sizeof(ConfigStorage) - 1u] ==
		 UINT8_C(0xC3)) &&
		(Config.Connection.Role == XWS_ROLE_SERVER) &&
		xrtWsServerConfigValid(pConfig),
		"WebSocket server rejected an unaligned configuration"
	);
	pRequest = testWsServerRequest(RequestText);
	testRequire(
		(pRequest != NULL) &&
		xrtWsServerCheck(
			pRequest,
			pConfig,
			pHandshake
		) &&
		(HandshakeStorage[0] == UINT8_C(0xD4)) &&
		(HandshakeStorage[sizeof(HandshakeStorage) - 1u] ==
		 UINT8_C(0xD4)),
		"WebSocket server rejected an unaligned handshake output"
	);
	pReply = xrtWsServerReply(pHandshake);
	testRequire(
		(pReply != NULL) &&
		(xrtHttpReplyStatus(pReply) ==
		 XHTTP_STATUS_SWITCHING_PROTOCOLS),
		"WebSocket server rejected an unaligned handshake snapshot"
	);
	xrtHttpReplyDestroy(pReply);

	xrtWsServerConfigInit(
		(xwsserverconfig*)(uintptr_t)(UINTPTR_MAX - 1u)
	);
	testWsServerArgumentError(
		"WebSocket server initialized a wrapping configuration"
	);
	testRequire(
		!xrtWsServerConfigValid(pWrappingConfig),
		"WebSocket server validated a wrapping configuration"
	);

	memset(&Handshake, 0xA5, sizeof(Handshake));
	Before = Handshake;
	testRequire(
		!xrtWsServerCheck(
			pWrappingRequest,
			NULL,
			&Handshake
		) &&
		(memcmp(&Handshake, &Before, sizeof(Handshake)) == 0),
		"WebSocket server accepted a wrapping request range"
	);
	testWsServerArgumentError(
		"WebSocket server request range error mismatch"
	);
	testRequire(
		!xrtWsServerCheck(
			pRequest,
			pWrappingConfig,
			&Handshake
		) &&
		(memcmp(&Handshake, &Before, sizeof(Handshake)) == 0),
		"WebSocket server accepted a wrapping configuration range"
	);
	testWsServerArgumentError(
		"WebSocket server configuration range error mismatch"
	);
	testRequire(
		!xrtWsServerCheck(
			pRequest,
			NULL,
			(xwsserverhandshake*)(uintptr_t)(UINTPTR_MAX - 1u)
		),
		"WebSocket server accepted a wrapping handshake output"
	);
	testWsServerArgumentError(
		"WebSocket server output range error mismatch"
	);
	testRequire(
		!xrtWsServerCheck(
			pRequest,
			NULL,
			(xwsserverhandshake*)(void*)pRequest
		),
		"WebSocket server accepted an overlapping handshake output"
	);
	testWsServerArgumentError(
		"WebSocket server output overlap error mismatch"
	);
	testRequire(
		xrtWsServerReply(pWrappingHandshake) == NULL,
		"WebSocket server accepted a wrapping handshake snapshot"
	);
	testWsServerArgumentError(
		"WebSocket server reply range error mismatch"
	);

	memset(&Http, 0, sizeof(Http));
	testRequire(
		xrtWsUpgrade(
			&Http,
			pWrappingConfig,
			NULL,
			NULL,
			testWsServerRangeDone,
			NULL
		) == XNET_RESULT_ERROR,
		"WebSocket Upgrade accepted a wrapping configuration"
	);
	testWsServerArgumentError(
		"WebSocket Upgrade configuration error mismatch"
	);
	testRequire(
		xrtWsUpgrade(
			&Http,
			NULL,
			pWrappingEvents,
			NULL,
			testWsServerRangeDone,
			NULL
		) == XNET_RESULT_ERROR,
		"WebSocket Upgrade accepted a wrapping event table"
	);
	testWsServerArgumentError(
		"WebSocket Upgrade event range error mismatch"
	);
	testRequire(
		xrtWsUpgrade(
			pWrappingHttp,
			NULL,
			NULL,
			NULL,
			testWsServerRangeDone,
			NULL
		) == XNET_RESULT_ERROR,
		"WebSocket Upgrade accepted a wrapping HTTP connection"
	);
	testWsServerArgumentError(
		"WebSocket Upgrade connection range error mismatch"
	);
	testRequire(
		xrtWsUpgrade(
			&Http,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL
		) == XNET_RESULT_ERROR,
		"WebSocket Upgrade accepted a null callback"
	);
	testWsServerArgumentError(
		"WebSocket Upgrade callback error mismatch"
	);
	testRequire(
		xrtWsServerReject(
			pWrappingHttp,
			NULL
		) == XNET_RESULT_ERROR,
		"WebSocket reject accepted a wrapping HTTP connection"
	);
	testWsServerArgumentError(
		"WebSocket reject connection range error mismatch"
	);

	xrtHttpServerRequestDestroy(pRequest);
}



int main(void)
{
	testWsServerSuccess();
	testWsServerFailures();
	testWsServerRanges();
	printf("[PASS] WebSocket HTTP server handshake\n");
	return 0;
}
