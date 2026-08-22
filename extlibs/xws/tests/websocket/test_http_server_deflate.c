#include "../test.h"
#include "../../../xhttp/src/internal/xrt_http_server.h"



/* 从完整 HTTP/1 请求头建立拥有型服务端请求快照。 */
static xhttpserverrequest* testWsServerDeflateRequest(
	cstr sInput
)
{
	xhttpfield Fields[20];
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
		"WebSocket Deflate server fixture parse failed"
	);
	testRequire(
		xrtHttp1RequestBodyPlan(&Head, &Plan),
		"WebSocket Deflate server body plan failed"
	);
	return __xrtHttpServerRequestCreate(
		&Head,
		&Plan,
		XHTTP_SERVER_REQUEST_UPGRADE
	);
}



/* 比较借用文本视图与常量。 */
static bool testWsServerDeflateText(
	xstrview Text,
	cstr sExpected
)
{
	size_t iSize = strlen(sExpected);

	return (Text.Size == iSize) &&
		(memcmp(Text.Data, sExpected, iSize) == 0);
}



/* 可选策略通过无错误的 false 主动放弃压缩。 */
static bool testWsServerDeflateDecline(
	const xwsdeflate* pOffer,
	xwsdeflate* pResponse,
	ptr pData
)
{
	(void)pOffer;
	(void)pResponse;
	(void)pData;
	return false;
}



/* 策略失败时把调用方错误保留为握手错误原因。 */
static bool testWsServerDeflateFail(
	const xwsdeflate* pOffer,
	xwsdeflate* pResponse,
	ptr pData
)
{
	xerror* pError;

	(void)pOffer;
	(void)pResponse;
	(void)pData;
	pError = xrtErrorCreate(
		XERR_STATE,
		"test.websocket",
		7,
		"compression policy failed"
	);
	testRequire(
		pError != NULL,
		"WebSocket Deflate callback error allocation failed"
	);
	xrtSetError(pError);
	xrtErrorFree(pError);
	return false;
}



/* 策略成功时留下的内部错误不得污染成功握手。 */
static bool testWsServerDeflateAcceptNoisy(
	const xwsdeflate* pOffer,
	xwsdeflate* pResponse,
	ptr pData
)
{
	xerror* pError;

	(void)pOffer;
	(void)pResponse;
	(void)pData;
	pError = xrtErrorCreate(
		XERR_STATE,
		"test.websocket.callback",
		8,
		"recovered callback error"
	);
	testRequire(
		pError != NULL,
		"WebSocket Deflate noisy callback allocation failed"
	);
	xrtSetError(pError);
	xrtErrorFree(pError);
	return true;
}



/* 策略返回协议未提供的参数时，适配器必须拒绝响应。 */
static bool testWsServerDeflateInvalid(
	const xwsdeflate* pOffer,
	xwsdeflate* pResponse,
	ptr pData
)
{
	(void)pOffer;
	(void)pData;
	pResponse->Flags |= XWS_DEFLATE_CLIENT_MAX_WINDOW;
	return true;
}



/* 验证重复字段合并、未知扩展忽略和规范响应快照。 */
static void testWsServerDeflateSuccess(void)
{
	static const char RequestText[] =
		"GET /chat HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Upgrade: websocket\r\n"
		"Connection: Upgrade\r\n"
		"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
		"Sec-WebSocket-Version: 13\r\n"
		"Sec-WebSocket-Extensions: x-example\r\n"
		"Sec-WebSocket-Extensions: permessage-deflate; "
		"server_no_context_takeover; "
		"client_max_window_bits\r\n\r\n";
	xhttpserverrequest* pRequest =
		testWsServerDeflateRequest(RequestText);
	xwsserverconfig Config;
	xwsserverhandshake Handshake;
	xhttpreply* pReply;
	const xhttpfield* pField;

	xrtWsServerConfigInit(&Config);
	Config.EnableDeflate = true;
	Config.RequireDeflate = true;
	testRequire(
		(pRequest != NULL) &&
		xrtWsServerCheck(
			pRequest,
			&Config,
			&Handshake
		) &&
		Handshake.DeflateEnabled &&
		((Handshake.Deflate.Flags &
		  XWS_DEFLATE_SERVER_NO_CONTEXT) != 0) &&
		testWsServerDeflateText(
			(xstrview) {
				Handshake.Extensions,
				strlen(Handshake.Extensions)
			},
			"permessage-deflate; "
			"server_no_context_takeover"
		),
		"WebSocket server Deflate negotiation mismatch"
	);
	pReply = xrtWsServerReply(&Handshake);
	pField = xrtHttpReplyHeader(
		pReply,
		XRT_STR_LITERAL("Sec-WebSocket-Extensions")
	);
	testRequire(
		(pReply != NULL) &&
		(pField != NULL) &&
		testWsServerDeflateText(
			pField->Value,
			"permessage-deflate; "
			"server_no_context_takeover"
		),
		"WebSocket server Deflate Reply mismatch"
	);
	xrtHttpReplyDestroy(pReply);
	xrtHttpServerRequestDestroy(pRequest);
}



/* 验证策略主动放弃、错误隔离、非法响应和带原因失败。 */
static void testWsServerDeflatePolicy(void)
{
	static const char RequestText[] =
		"GET /chat HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Upgrade: websocket\r\n"
		"Connection: Upgrade\r\n"
		"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
		"Sec-WebSocket-Version: 13\r\n"
		"Sec-WebSocket-Extensions: permessage-deflate\r\n\r\n";
	xhttpserverrequest* pRequest =
		testWsServerDeflateRequest(RequestText);
	xwsserverconfig Config;
	xwsserverhandshake Handshake;
	xwsserverhandshake Before;
	xerror* pPrevious;

	xrtWsServerConfigInit(&Config);
	Config.EnableDeflate = true;
	Config.AcceptDeflate = testWsServerDeflateDecline;
	testRequire(
		xrtWsServerCheck(
			pRequest,
			&Config,
			&Handshake
		) &&
		!Handshake.DeflateEnabled,
		"WebSocket server could not decline optional Deflate"
	);

	Config.RequireDeflate = true;
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
		(xrtErrorCode(xrtGetError()) ==
		 XWS_HANDSHAKE_ERROR_EXTENSION),
		"WebSocket server accepted declined required Deflate"
	);
	xrtClearError();

	Config.RequireDeflate = false;
	Config.AcceptDeflate = testWsServerDeflateAcceptNoisy;
	pPrevious = xrtErrorCreate(
		XERR_STATE,
		"test.websocket.previous",
		9,
		"previous thread error"
	);
	testRequire(
		pPrevious != NULL,
		"WebSocket Deflate previous error allocation failed"
	);
	xrtSetError(pPrevious);
	xrtErrorFree(pPrevious);
	testRequire(
		xrtWsServerCheck(
			pRequest,
			&Config,
			&Handshake
		) && Handshake.DeflateEnabled &&
		(xrtGetError() != NULL) &&
		(strcmp(
			xrtErrorDomain(xrtGetError()),
			"test.websocket.previous"
		 ) == 0) &&
		(xrtErrorCode(xrtGetError()) == 9),
		"WebSocket server leaked a recovered policy error"
	);
	xrtClearError();

	Config.AcceptDeflate = testWsServerDeflateInvalid;
	memset(&Handshake, 0x5A, sizeof(Handshake));
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
		(xrtErrorCode(xrtGetError()) ==
		 XWS_HANDSHAKE_ERROR_EXTENSION) &&
		(xrtErrorCause(xrtGetError()) != NULL) &&
		(strcmp(
			xrtErrorDomain(xrtErrorCause(xrtGetError())),
			"xrt.websocket.deflate"
		 ) == 0),
		"WebSocket server accepted an invalid policy response"
	);
	xrtClearError();

	Config.RequireDeflate = false;
	Config.AcceptDeflate = testWsServerDeflateFail;
	testRequire(
		!xrtWsServerCheck(
			pRequest,
			&Config,
			&Handshake
		) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE) &&
		(xrtErrorCause(xrtGetError()) != NULL) &&
		(strcmp(
			xrtErrorDomain(xrtErrorCause(xrtGetError())),
			"test.websocket"
		 ) == 0),
		"WebSocket server lost Deflate policy error"
	);
	xrtClearError();
	xrtHttpServerRequestDestroy(pRequest);
}



/* 验证禁用时忽略 offer，以及 Required 缺失时保持输出原子性。 */
static void testWsServerDeflateRequirement(void)
{
	static const char OfferedText[] =
		"GET /chat HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Upgrade: websocket\r\n"
		"Connection: Upgrade\r\n"
		"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
		"Sec-WebSocket-Version: 13\r\n"
		"Sec-WebSocket-Extensions: permessage-deflate\r\n\r\n";
	static const char MissingText[] =
		"GET /chat HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Upgrade: websocket\r\n"
		"Connection: Upgrade\r\n"
		"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
		"Sec-WebSocket-Version: 13\r\n\r\n";
	xhttpserverrequest* pOffered =
		testWsServerDeflateRequest(OfferedText);
	xhttpserverrequest* pMissing =
		testWsServerDeflateRequest(MissingText);
	xwsserverconfig Config;
	xwsserverhandshake Handshake;
	xwsserverhandshake Before;

	xrtWsServerConfigInit(&Config);
	testRequire(
		(pOffered != NULL) && (pMissing != NULL) &&
		xrtWsServerCheck(
			pOffered,
			&Config,
			&Handshake
		) && !Handshake.DeflateEnabled,
		"WebSocket server did not ignore a disabled Deflate offer"
	);

	Config.EnableDeflate = true;
	Config.RequireDeflate = true;
	memset(&Handshake, 0x3C, sizeof(Handshake));
	Before = Handshake;
	testRequire(
		xrtWsServerConfigValid(&Config) &&
		!xrtWsServerCheck(
			pMissing,
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
		"WebSocket server accepted a missing required Deflate offer"
	);
	xrtClearError();

	Config.EnableDeflate = false;
	testRequire(
		!xrtWsServerConfigValid(&Config),
		"WebSocket server accepted Required without EnableDeflate"
	);
	xrtHttpServerRequestDestroy(pMissing);
	xrtHttpServerRequestDestroy(pOffered);
}



/* 同一逻辑扩展跨字段重复时必须拒绝。 */
static void testWsServerDeflateDuplicate(void)
{
	static const char RequestText[] =
		"GET /chat HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Upgrade: websocket\r\n"
		"Connection: Upgrade\r\n"
		"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
		"Sec-WebSocket-Version: 13\r\n"
		"Sec-WebSocket-Extensions: permessage-deflate\r\n"
		"Sec-WebSocket-Extensions: permessage-deflate\r\n\r\n";
	xhttpserverrequest* pRequest =
		testWsServerDeflateRequest(RequestText);
	xwsserverconfig Config;
	xwsserverhandshake Handshake;

	xrtWsServerConfigInit(&Config);
	Config.EnableDeflate = true;
	testRequire(
		!xrtWsServerCheck(
			pRequest,
			&Config,
			&Handshake
		) &&
		(xrtErrorCode(xrtGetError()) ==
		 XWS_HANDSHAKE_ERROR_EXTENSION),
		"WebSocket server accepted duplicate Deflate offers"
	);
	xrtClearError();
	xrtHttpServerRequestDestroy(pRequest);
}



/* 覆盖服务端压缩协商的成功、策略和重复项边界。 */
int main(void)
{
	testWsServerDeflateSuccess();
	testWsServerDeflatePolicy();
	testWsServerDeflateRequirement();
	testWsServerDeflateDuplicate();
	printf("[PASS] WebSocket HTTP server Deflate\n");
	return 0;
}
