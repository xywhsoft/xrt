#include "../test.h"
#include "../../src/internal/xrt_http_client.h"



/* 创建一个带标准必要字段和可选扩展的拥有型 101 响应。 */
static xhttpresponse* testWsClientDeflateResponse(
	xstrview Extensions
)
{
	xhttpresponse* pResponse =
		__xrtHttpResponseCreate(
			XHTTP_VERSION_1_1,
			XHTTP_STATUS_SWITCHING_PROTOCOLS,
			XRT_STR_LITERAL("Switching Protocols"),
			NULL
		);

	testRequire(
		(pResponse != NULL) &&
		__xrtHttpResponseAddHeader(
			pResponse,
			XRT_STR_LITERAL("Upgrade"),
			XRT_STR_LITERAL("websocket")
		) &&
		__xrtHttpResponseAddHeader(
			pResponse,
			XRT_STR_LITERAL("Connection"),
			XRT_STR_LITERAL("Upgrade")
		) &&
		__xrtHttpResponseAddHeader(
			pResponse,
			XRT_STR_LITERAL("Sec-WebSocket-Accept"),
			XRT_STR_LITERAL(
				"s3pPLMBiTxaQ9kYGzzhZRbK+xOo="
			)
		) &&
		((Extensions.Size == 0) ||
		 __xrtHttpResponseAddHeader(
			pResponse,
			XRT_STR_LITERAL(
				"Sec-WebSocket-Extensions"
			),
			Extensions
		 )),
		"WebSocket client Deflate response fixture failed"
	);
	return pResponse;
}



/* 验证请求构建器写入规范 offer 并保留调用方配置。 */
static void testWsClientDeflateRequest(void)
{
	xwsclientconfig Config;
	xhttprequest* pRequest;
	const xhttpfield* pField;
	char sKey[XWS_KEY_CAPACITY];

	xrtWsClientConfigInit(&Config);
	Config.EnableDeflate = true;
	Config.RequireDeflate = true;
	Config.Deflate.Flags =
		XWS_DEFLATE_SERVER_NO_CONTEXT |
		XWS_DEFLATE_CLIENT_MAX_WINDOW |
		XWS_DEFLATE_CLIENT_MAX_WINDOW_ANY;
	pRequest = xrtWsClientRequestCreate(
		XRT_STR_LITERAL("ws://example.test/chat"),
		&Config,
		sKey
	);
	pField = xrtHttpRequestHeader(
		pRequest,
		XRT_STR_LITERAL("Sec-WebSocket-Extensions")
	);
	testRequire(
		(pRequest != NULL) &&
		(pField != NULL) &&
		xrtWsKeyValid(
			(xstrview) { sKey, XWS_KEY_SIZE }
		) &&
		(pField->Value.Size ==
		 strlen(
			"permessage-deflate; "
			"server_no_context_takeover; "
			"client_max_window_bits"
		 )) &&
		(memcmp(
			pField->Value.Data,
			"permessage-deflate; "
			"server_no_context_takeover; "
			"client_max_window_bits",
			pField->Value.Size
		 ) == 0),
		"WebSocket client Deflate offer mismatch"
	);
	xrtHttpRequestDestroy(pRequest);
}



/* 验证响应参数绑定到 offer 并复制到独立握手快照。 */
static void testWsClientDeflateSuccess(void)
{
	xhttpresponse* pResponse =
		testWsClientDeflateResponse(
			XRT_STR_LITERAL(
				"permessage-deflate; "
				"server_no_context_takeover; "
				"client_max_window_bits=12"
			)
		);
	xwsclientconfig Config;
	xwsclienthandshake Handshake;

	xrtWsClientConfigInit(&Config);
	Config.EnableDeflate = true;
	Config.RequireDeflate = true;
	Config.Deflate.Flags =
		XWS_DEFLATE_SERVER_NO_CONTEXT |
		XWS_DEFLATE_CLIENT_MAX_WINDOW |
		XWS_DEFLATE_CLIENT_MAX_WINDOW_ANY;
	testRequire(
		xrtWsClientCheck(
			pResponse,
			XRT_STR_LITERAL(
				"dGhlIHNhbXBsZSBub25jZQ=="
			),
			&Config,
			&Handshake
		) &&
		Handshake.DeflateEnabled &&
		(Handshake.Deflate.ClientMaxWindowBits == 12) &&
		((Handshake.Deflate.Flags &
		  XWS_DEFLATE_SERVER_NO_CONTEXT) != 0),
		"WebSocket client Deflate response mismatch"
	);
	xrtHttpResponseDestroy(pResponse);
}



/* 验证一个非法扩展响应被拒绝，并保持握手输出原子性。 */
static void testWsClientDeflateReject(
	const xwsclientconfig* pConfig,
	xstrview Extensions,
	const char* sMessage
)
{
	xwsclienthandshake Handshake;
	xwsclienthandshake Before;
	xhttpresponse* pResponse =
		testWsClientDeflateResponse(Extensions);

	memset(&Handshake, 0xA5, sizeof(Handshake));
	Before = Handshake;
	testRequire(
		!xrtWsClientCheck(
			pResponse,
			XRT_STR_LITERAL(
				"dGhlIHNhbXBsZSBub25jZQ=="
			),
			pConfig,
			&Handshake
		) &&
		(memcmp(
			&Handshake,
			&Before,
			sizeof(Handshake)
		 ) == 0) &&
		(xrtErrorCode(xrtGetError()) ==
		 XWS_HANDSHAKE_ERROR_EXTENSION),
		sMessage
	);
	xrtClearError();
	xrtHttpResponseDestroy(pResponse);
}



/* 验证禁用、可选和 Required 配置的客户端协商语义。 */
static void testWsClientDeflateOptional(void)
{
	xwsclientconfig Config;
	xwsclienthandshake Handshake;
	xhttpresponse* pResponse;
	xhttprequest* pRequest;
	char sKey[XWS_KEY_CAPACITY];
	char sBefore[XWS_KEY_CAPACITY];

	xrtWsClientConfigInit(&Config);
	pResponse = testWsClientDeflateResponse(
		(xstrview) { 0 }
	);
	testRequire(
		xrtWsClientCheck(
			pResponse,
			XRT_STR_LITERAL(
				"dGhlIHNhbXBsZSBub25jZQ=="
			),
			&Config,
			&Handshake
		) && !Handshake.DeflateEnabled,
		"WebSocket client rejected a response without compression"
	);
	xrtHttpResponseDestroy(pResponse);

	Config.EnableDeflate = true;
	pResponse = testWsClientDeflateResponse(
		(xstrview) { 0 }
	);
	testRequire(
		xrtWsClientCheck(
			pResponse,
			XRT_STR_LITERAL(
				"dGhlIHNhbXBsZSBub25jZQ=="
			),
			&Config,
			&Handshake
		) && !Handshake.DeflateEnabled,
		"WebSocket client rejected an omitted optional compression response"
	);
	xrtHttpResponseDestroy(pResponse);

	Config.EnableDeflate = false;
	testWsClientDeflateReject(
		&Config,
		XRT_STR_LITERAL("permessage-deflate"),
		"WebSocket client accepted unoffered compression"
	);

	Config.RequireDeflate = true;
	memset(sKey, 0xA5, sizeof(sKey));
	memcpy(sBefore, sKey, sizeof(sBefore));
	pRequest = xrtWsClientRequestCreate(
		XRT_STR_LITERAL("ws://example.test/chat"),
		&Config,
		sKey
	);
	testRequire(
		(pRequest == NULL) &&
		(memcmp(sKey, sBefore, sizeof(sKey)) == 0) &&
		(xrtErrorCode(xrtGetError()) ==
		 XWS_HANDSHAKE_ERROR_EXTENSION),
		"WebSocket client accepted Required without EnableDeflate"
	);
	xrtClearError();
}



/* 验证 RFC 7692 中允许主动返回与必须受 offer 约束的参数。 */
static void testWsClientDeflateNegotiation(void)
{
	xwsclientconfig Config;
	xwsclienthandshake Handshake;
	xhttpresponse* pResponse;

	xrtWsClientConfigInit(&Config);
	Config.EnableDeflate = true;
	pResponse = testWsClientDeflateResponse(
		XRT_STR_LITERAL(
			"permessage-deflate; server_no_context_takeover; "
			"client_no_context_takeover; server_max_window_bits=12"
		)
	);
	testRequire(
		xrtWsClientCheck(
			pResponse,
			XRT_STR_LITERAL(
				"dGhlIHNhbXBsZSBub25jZQ=="
			),
			&Config,
			&Handshake
		) && Handshake.DeflateEnabled &&
		((Handshake.Deflate.Flags &
		  XWS_DEFLATE_SERVER_NO_CONTEXT) != 0) &&
		((Handshake.Deflate.Flags &
		  XWS_DEFLATE_CLIENT_NO_CONTEXT) != 0) &&
		(Handshake.Deflate.ServerMaxWindowBits == 12u),
		"WebSocket client rejected legal unsolicited Deflate parameters"
	);
	xrtHttpResponseDestroy(pResponse);

	testWsClientDeflateReject(
		&Config,
		XRT_STR_LITERAL(
			"permessage-deflate; client_max_window_bits=12"
		),
		"WebSocket client accepted unoffered client_max_window_bits"
	);

	Config.Deflate.Flags = XWS_DEFLATE_SERVER_MAX_WINDOW;
	Config.Deflate.ServerMaxWindowBits = 10u;
	testWsClientDeflateReject(
		&Config,
		XRT_STR_LITERAL(
			"permessage-deflate; server_max_window_bits=11"
		),
		"WebSocket client accepted an oversized server window"
	);
	testWsClientDeflateReject(
		&Config,
		XRT_STR_LITERAL("permessage-deflate"),
		"WebSocket client accepted an omitted required server window"
	);

	xrtWsDeflateInit(&Config.Deflate);
	Config.Deflate.Flags = XWS_DEFLATE_CLIENT_MAX_WINDOW;
	Config.Deflate.ClientMaxWindowBits = 10u;
	pResponse = testWsClientDeflateResponse(
		XRT_STR_LITERAL(
			"permessage-deflate; client_max_window_bits=12"
		)
	);
	testRequire(
		xrtWsClientCheck(
			pResponse,
			XRT_STR_LITERAL(
				"dGhlIHNhbXBsZSBub25jZQ=="
			),
			&Config,
			&Handshake
		) && Handshake.DeflateEnabled &&
		(Handshake.Deflate.ClientMaxWindowBits == 12u),
		"WebSocket client treated client window hint as a response limit"
	);
	xrtHttpResponseDestroy(pResponse);
}



/* 验证 Required、未知扩展和重复响应都保持输出原子性。 */
static void testWsClientDeflateFailures(void)
{
	xwsclientconfig Config;
	xwsclienthandshake Handshake;
	xwsclienthandshake Before;
	xhttpresponse* pResponse;

	xrtWsClientConfigInit(&Config);
	Config.EnableDeflate = true;
	Config.RequireDeflate = true;
	testWsClientDeflateReject(
		&Config,
		(xstrview) { 0 },
		"WebSocket client accepted missing required Deflate"
	);

	Config.RequireDeflate = false;
	testWsClientDeflateReject(
		&Config,
		XRT_STR_LITERAL("x-example"),
		"WebSocket client accepted an unknown extension"
	);
	testWsClientDeflateReject(
		&Config,
		XRT_STR_LITERAL(
			"permessage-deflate, permessage-deflate"
		),
		"WebSocket client accepted repeated Deflate in one field"
	);
	pResponse = testWsClientDeflateResponse(
		XRT_STR_LITERAL("permessage-deflate,")
	);
	testRequire(
		xrtWsClientCheck(
			pResponse,
			XRT_STR_LITERAL(
				"dGhlIHNhbXBsZSBub25jZQ=="
			),
			&Config,
			&Handshake
		) && Handshake.DeflateEnabled,
		"WebSocket client rejected an HTTP #rule empty member"
	);
	xrtHttpResponseDestroy(pResponse);

	pResponse = testWsClientDeflateResponse(
		XRT_STR_LITERAL("permessage-deflate")
	);
	memset(&Handshake, 0xA5, sizeof(Handshake));
	Before = Handshake;
	testRequire(
		__xrtHttpResponseAddHeader(
			pResponse,
			XRT_STR_LITERAL(
				"Sec-WebSocket-Extensions"
			),
			XRT_STR_LITERAL("permessage-deflate")
		) &&
		!xrtWsClientCheck(
			pResponse,
			XRT_STR_LITERAL(
				"dGhlIHNhbXBsZSBub25jZQ=="
			),
			&Config,
			&Handshake
		) &&
		(memcmp(
			&Handshake,
			&Before,
			sizeof(Handshake)
		 ) == 0) &&
		(xrtErrorCode(xrtGetError()) ==
		 XWS_HANDSHAKE_ERROR_EXTENSION),
		"WebSocket client accepted duplicate Deflate responses"
	);
	xrtClearError();
	xrtHttpResponseDestroy(pResponse);
}



/* 覆盖客户端 offer、成功绑定和严格拒绝矩阵。 */
int main(void)
{
	testWsClientDeflateRequest();
	testWsClientDeflateSuccess();
	testWsClientDeflateOptional();
	testWsClientDeflateNegotiation();
	testWsClientDeflateFailures();
	printf("[PASS] WebSocket HTTP client Deflate\n");
	return 0;
}
