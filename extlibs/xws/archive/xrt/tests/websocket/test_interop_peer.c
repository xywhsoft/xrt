#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <xrt.h>



#define INTEROP_PROTOCOL "xrt.interop"
#define INTEROP_ORIGIN "https://interop.test"
#define INTEROP_TIMEOUT UINT64_C(10000000)
#define INTEROP_TEXT_CAPACITY ((size_t)2048u)
#define INTEROP_BINARY_CAPACITY ((size_t)64u)
#define INTEROP_REASON_CAPACITY ((size_t)124u)
#define INTEROP_ERROR_CAPACITY ((size_t)256u)
#define INTEROP_ERROR_WRITING UINT32_C(1)
#define INTEROP_ERROR_PUBLISHED UINT32_C(2)



typedef struct interop_context {
	xnetengine* Engine;
	xhttpserver* Server;
	xhttpclient* Client;
	xhttpcall* Call;
	xhttpresponse* Response;
	xwsconn* Connection;
	xwsconnevents Events;
	xatomic32 Opened;
	xatomic32 Text;
	xatomic32 Binary;
	xatomic32 Ping;
	xatomic32 Pong;
	xatomic32 Closed;
	xatomic32 Errors;
	xatomic32 Shutdown;
	xwsopcode Opcode;
	size_t MessageSize;
	uint8 Message[INTEROP_TEXT_CAPACITY];
	char ExpectedText[INTEROP_TEXT_CAPACITY];
	size_t ExpectedTextSize;
	uint8 ExpectedBinary[INTEROP_BINARY_CAPACITY];
	size_t ExpectedBinarySize;
	uint16 RemoteCode;
	char RemoteReason[INTEROP_REASON_CAPACITY];
	char Error[INTEROP_ERROR_CAPACITY];
} interop_context;



/* 比较借用文本视图和以零结尾的测试常量。 */
static bool interopTextEqual(xstrview Text, cstr sExpected)
{
	size_t iSize = strlen(sExpected);

	return (Text.Size == iSize) &&
		((iSize == 0) ||
		 (memcmp(Text.Data, sExpected, iSize) == 0));
}



/* 用固定单元构造长消息，覆盖压缩和分片边界。 */
static bool interopFillText(
	char* sOutput,
	size_t iCapacity,
	size_t* pSize,
	cstr sUnit,
	size_t iCount
)
{
	size_t iUnit = strlen(sUnit);

	if ( (iUnit == 0) ||
		(iCount > ((iCapacity - 1u) / iUnit)) ) {
		return false;
	}
	for ( size_t i = 0; i < iCount; i++ ) {
		memcpy(
			sOutput + (i * iUnit),
			sUnit,
			iUnit
		);
	}
	*pSize = iUnit * iCount;
	sOutput[*pSize] = '\0';
	return true;
}



/* 初始化跨 Worker 发布的状态和 WebSocket 事件表。 */
static void interopContextInit(interop_context* pContext)
{
	memset(pContext, 0, sizeof(*pContext));
	xrtAtomic32Init(&pContext->Opened, 0);
	xrtAtomic32Init(&pContext->Text, 0);
	xrtAtomic32Init(&pContext->Binary, 0);
	xrtAtomic32Init(&pContext->Ping, 0);
	xrtAtomic32Init(&pContext->Pong, 0);
	xrtAtomic32Init(&pContext->Closed, 0);
	xrtAtomic32Init(&pContext->Errors, 0);
	xrtAtomic32Init(&pContext->Shutdown, 0);
}



/* 输出完整互操作终态，避免将关闭竞态隐藏成无信息的契约失败。 */
static void interopReportContract(
	interop_context* pContext,
	cstr sRole
)
{
	fprintf(
		stderr,
		"%s interop failed: errors=%u opened=%u text=%u binary=%u "
		"ping=%u pong=%u closed=%u shutdown=%u remote=%u reason='%s' "
		"detail='%s'\n",
		sRole,
		(unsigned int)xrtAtomic32Load(
			&pContext->Errors,
			XMEMORY_ACQUIRE
		),
		(unsigned int)xrtAtomic32Load(
			&pContext->Opened,
			XMEMORY_ACQUIRE
		),
		(unsigned int)xrtAtomic32Load(
			&pContext->Text,
			XMEMORY_ACQUIRE
		),
		(unsigned int)xrtAtomic32Load(
			&pContext->Binary,
			XMEMORY_ACQUIRE
		),
		(unsigned int)xrtAtomic32Load(
			&pContext->Ping,
			XMEMORY_ACQUIRE
		),
		(unsigned int)xrtAtomic32Load(
			&pContext->Pong,
			XMEMORY_ACQUIRE
		),
		(unsigned int)xrtAtomic32Load(
			&pContext->Closed,
			XMEMORY_ACQUIRE
		),
		(unsigned int)xrtAtomic32Load(
			&pContext->Shutdown,
			XMEMORY_ACQUIRE
		),
		(unsigned int)pContext->RemoteCode,
		pContext->RemoteReason,
		pContext->Error
	);
}



/* 保存首个测试失败，后续错误不覆盖最接近根因的现场。 */
static void interopFail(
	interop_context* pContext,
	cstr sMessage
)
{
	uint32 iExpected = 0;

	if ( !xrtAtomic32CompareExchange(
		&pContext->Errors,
		&iExpected,
		INTEROP_ERROR_WRITING,
		XMEMORY_ACQ_REL,
		XMEMORY_ACQUIRE
	) ) {
		return;
	}
	(void)snprintf(
		pContext->Error,
		sizeof(pContext->Error),
		"%s",
		sMessage == NULL ? "unknown interop failure" : sMessage
	);
	xrtAtomic32Store(
		&pContext->Errors,
		INTEROP_ERROR_PUBLISHED,
		XMEMORY_RELEASE
	);
}



/* 保存借用错误的稳定文本副本。 */
static void interopFailError(
	interop_context* pContext,
	const xerror* pError
)
{
	cstr sMessage = pError == NULL ?
		"operation failed without structured error" :
		xrtErrorMessage(pError);

	interopFail(pContext, sMessage);
}



/* 等待 Connection 关闭或测试发布错误。 */
static bool interopWaitTerminal(interop_context* pContext)
{
	xdeadline Deadline = xrtDeadlineAfter(INTEROP_TIMEOUT);

	for ( ;; ) {
		uint32 iError = xrtAtomic32Load(
			&pContext->Errors,
			XMEMORY_ACQUIRE
		);

		if ( iError == INTEROP_ERROR_PUBLISHED ) {
			return false;
		}
		if ( (iError == 0) &&
			(xrtAtomic32Load(
				&pContext->Closed,
				XMEMORY_ACQUIRE
			 ) != 0) ) {
			return true;
		}
		if ( xrtDeadlineExpired(Deadline) ) {
			interopFail(pContext, "interop peer timed out");
			return false;
		}
		xrtThreadYield();
	}
}



/* 等待 HTTP Server 进入终态并完成 Shutdown 回调发布。 */
static bool interopWaitServer(interop_context* pContext)
{
	xdeadline Deadline = xrtDeadlineAfter(INTEROP_TIMEOUT);

	while ( (xrtHttpServerState(pContext->Server) !=
			 XHTTP_SERVER_CLOSED) ||
		(xrtAtomic32Load(
			&pContext->Shutdown,
			XMEMORY_ACQUIRE
		 ) != 1) ) {
		if ( xrtDeadlineExpired(Deadline) ) {
			interopFail(pContext, "HTTP server shutdown timed out");
			return false;
		}
		xrtThreadYield();
	}
	return true;
}



/* 按客户端 offer 接受双方无上下文压缩，形成稳定互操作字段。 */
static bool interopAcceptDeflate(
	const xwsdeflate* pOffer,
	xwsdeflate* pResponse,
	ptr pData
)
{
	(void)pData;
	if ( !xrtWsDeflateAccept(pOffer, pResponse) ) {
		return false;
	}
	pResponse->Flags |= pOffer->Flags &
		XWS_DEFLATE_CLIENT_NO_CONTEXT;
	return true;
}



/* 开始接收一条逻辑消息并复用有界暂存。 */
static void interopMessageBegin(
	xwsconn* pConnection,
	const xwsmessageinfo* pInfo,
	ptr pData
)
{
	interop_context* pContext = (interop_context*)pData;

	(void)pConnection;
	if ( pInfo == NULL ) {
		interopFail(pContext, "WebSocket message info is null");
		return;
	}
	pContext->Opcode = (xwsopcode)pInfo->Opcode;
	pContext->MessageSize = 0;
}



/* 汇总流式消息分块，越界时立即中止会话。 */
static void interopMessageData(
	xwsconn* pConnection,
	xbytesview Data,
	ptr pData
)
{
	interop_context* pContext = (interop_context*)pData;

	if ( Data.Size >
		(sizeof(pContext->Message) -
		 pContext->MessageSize) ) {
		interopFail(pContext, "WebSocket message exceeded interop storage");
		(void)xrtWsConnAbort(pConnection);
		return;
	}
	if ( Data.Size != 0 ) {
		memcpy(
			pContext->Message + pContext->MessageSize,
			Data.Data,
			Data.Size
		);
	}
	pContext->MessageSize += Data.Size;
}



/* 核对完整消息；服务端按压缩文本和原始二进制两条路径回显。 */
static void interopMessageEnd(
	xwsconn* pConnection,
	ptr pData
)
{
	interop_context* pContext = (interop_context*)pData;
	xnetresult Result;

	if ( pContext->Opcode == XWS_OPCODE_TEXT ) {
		if ( (pContext->MessageSize !=
			pContext->ExpectedTextSize) ||
			(memcmp(
				pContext->Message,
				pContext->ExpectedText,
				pContext->MessageSize
			 ) != 0) ) {
			interopFail(pContext, "WebSocket text payload mismatch");
			(void)xrtWsConnAbort(pConnection);
			return;
		}
		xrtAtomic32Store(
			&pContext->Text,
			1,
			XMEMORY_RELEASE
		);
		if ( xrtWsConnRole(pConnection) != XWS_ROLE_SERVER ) {
			return;
		}
		Result = xrtWsConnTextCompressed(
			pConnection,
			(xstrview) {
				pContext->ExpectedText,
				pContext->ExpectedTextSize
			}
		);
	} else if ( pContext->Opcode == XWS_OPCODE_BINARY ) {
		if ( (pContext->MessageSize !=
			pContext->ExpectedBinarySize) ||
			(memcmp(
				pContext->Message,
				pContext->ExpectedBinary,
				pContext->MessageSize
			 ) != 0) ) {
			interopFail(pContext, "WebSocket binary payload mismatch");
			(void)xrtWsConnAbort(pConnection);
			return;
		}
		xrtAtomic32Store(
			&pContext->Binary,
			1,
			XMEMORY_RELEASE
		);
		if ( xrtWsConnRole(pConnection) != XWS_ROLE_SERVER ) {
			return;
		}
		Result = xrtWsConnBinary(
			pConnection,
			(xbytesview) {
				pContext->ExpectedBinary,
				pContext->ExpectedBinarySize
			}
		);
	} else {
		interopFail(pContext, "unexpected WebSocket data opcode");
		(void)xrtWsConnAbort(pConnection);
		return;
	}
	if ( Result != XNET_RESULT_OK ) {
		interopFailError(pContext, xrtGetError());
		(void)xrtWsConnAbort(pConnection);
	}
}



/* 服务端核对外部客户端 Ping；自动 Pong 由 Connection 发送。 */
static void interopPing(
	xwsconn* pConnection,
	xbytesview Payload,
	ptr pData
)
{
	interop_context* pContext = (interop_context*)pData;

	if ( (xrtWsConnRole(pConnection) != XWS_ROLE_SERVER) ||
		(Payload.Size != strlen("python-ping")) ||
		(memcmp(
			Payload.Data,
			"python-ping",
			Payload.Size
		 ) != 0) ) {
		interopFail(pContext, "WebSocket Ping payload mismatch");
		(void)xrtWsConnAbort(pConnection);
		return;
	}
	xrtAtomic32Store(
		&pContext->Ping,
		1,
		XMEMORY_RELEASE
	);
}



/* 客户端核对外部服务器 Pong。 */
static void interopPong(
	xwsconn* pConnection,
	xbytesview Payload,
	ptr pData
)
{
	interop_context* pContext = (interop_context*)pData;

	if ( (xrtWsConnRole(pConnection) != XWS_ROLE_CLIENT) ||
		(Payload.Size != strlen("xrt-ping")) ||
		(memcmp(Payload.Data, "xrt-ping", Payload.Size) != 0) ) {
		interopFail(pContext, "WebSocket Pong payload mismatch");
		(void)xrtWsConnAbort(pConnection);
		return;
	}
	xrtAtomic32Store(
		&pContext->Pong,
		1,
		XMEMORY_RELEASE
	);
}



/* 保存结构化 WebSocket 错误。 */
static void interopError(
	xwsconn* pConnection,
	const xerror* pError,
	ptr pData
)
{
	(void)pConnection;
	interopFailError((interop_context*)pData, pError);
}



/* 保存远端 Close 代码和原因的稳定副本。 */
static void interopClose(
	xwsconn* pConnection,
	const xwsconnclose* pClose,
	ptr pData
)
{
	interop_context* pContext = (interop_context*)pData;
	size_t iCopy;

	(void)pConnection;
	if ( pClose == NULL ) {
		interopFail(pContext, "WebSocket Close omitted snapshot");
		return;
	}
	pContext->RemoteCode = pClose->RemoteCode;
	iCopy = pClose->Reason.Size <
		(sizeof(pContext->RemoteReason) - 1u) ?
			pClose->Reason.Size :
			(sizeof(pContext->RemoteReason) - 1u);
	if ( iCopy != 0 ) {
		memcpy(
			pContext->RemoteReason,
			pClose->Reason.Data,
			iCopy
		);
	}
	pContext->RemoteReason[iCopy] = '\0';
	xrtAtomic32Store(
		&pContext->Closed,
		1,
		XMEMORY_RELEASE
	);
}



/* 安装两种 peer 共用的流式会话事件。 */
static void interopEventsInit(interop_context* pContext)
{
	memset(&pContext->Events, 0, sizeof(pContext->Events));
	pContext->Events.MessageBegin = interopMessageBegin;
	pContext->Events.MessageData = interopMessageData;
	pContext->Events.MessageEnd = interopMessageEnd;
	pContext->Events.Ping = interopPing;
	pContext->Events.Pong = interopPong;
	pContext->Events.Error = interopError;
	pContext->Events.Close = interopClose;
}



/* 检查协商结果并接管服务端 Connection 引用。 */
static void interopServerUpgrade(
	xhttpconn* pHttp,
	xnetresult Result,
	xwsconn* pConnection,
	const xerror* pError,
	ptr pData
)
{
	interop_context* pContext = (interop_context*)pData;
	xwsdeflate Deflate;

	(void)pHttp;
	if ( (Result != XNET_RESULT_OK) ||
		(pConnection == NULL) ) {
		interopFailError(pContext, pError);
		return;
	}
	pContext->Connection = pConnection;
	if ( !interopTextEqual(
		xrtWsConnProtocol(pConnection),
		INTEROP_PROTOCOL
	) || !xrtWsConnDeflate(pConnection, &Deflate) ||
		((Deflate.Flags & (
			XWS_DEFLATE_SERVER_NO_CONTEXT |
			XWS_DEFLATE_CLIENT_NO_CONTEXT
		 )) != (
			XWS_DEFLATE_SERVER_NO_CONTEXT |
			XWS_DEFLATE_CLIENT_NO_CONTEXT
		 )) ) {
		interopFail(pContext, "WebSocket server negotiation mismatch");
		(void)xrtWsConnAbort(pConnection);
		return;
	}
	xrtAtomic32Store(
		&pContext->Opened,
		1,
		XMEMORY_RELEASE
	);
}



/* 验证原始外部请求后执行标准 HTTP Upgrade。 */
static void interopServerRequest(
	xhttpserver* pServer,
	xhttpconn* pHttp,
	const xhttpserverrequest* pRequest,
	ptr pData
)
{
	interop_context* pContext = (interop_context*)pData;
	const xhttpfield* pOrigin;
	xwsserverconfig Config;

	(void)pServer;
	pOrigin = xrtHttpServerRequestHeader(
		pRequest,
		XRT_STR_LITERAL("Origin")
	);
	if ( !interopTextEqual(
		xrtHttpServerRequestTarget(pRequest),
		"/interop?peer=python"
	) || (pOrigin == NULL) ||
		!interopTextEqual(pOrigin->Value, INTEROP_ORIGIN) ) {
		interopFail(pContext, "WebSocket server request metadata mismatch");
		(void)xrtHttpConnAbort(pHttp);
		return;
	}
	xrtWsServerConfigInit(&Config);
	Config.Protocols = XRT_STR_LITERAL(INTEROP_PROTOCOL);
	Config.EnableDeflate = true;
	Config.RequireDeflate = true;
	Config.AcceptDeflate = interopAcceptDeflate;
	if ( xrtWsUpgrade(
		pHttp,
		&Config,
		&pContext->Events,
		pContext,
		interopServerUpgrade,
		pContext
	) != XNET_RESULT_OK ) {
		interopFailError(pContext, xrtGetError());
		(void)xrtHttpConnAbort(pHttp);
	}
}



/* HTTP Server 停止后发布销毁门禁。 */
static void interopServerShutdown(
	xhttpserver* pServer,
	ptr pData
)
{
	interop_context* pContext = (interop_context*)pData;

	(void)pServer;
	xrtAtomic32Store(
		&pContext->Shutdown,
		1,
		XMEMORY_RELEASE
	);
}



/* 运行 Python 客户端到 XRT 服务端的互操作 peer。 */
static int interopRunServer(uint16 iPort)
{
	static const uint8 Binary[] = {
		0x00, 0x01, 'p', 'y', 't', 'h', 'o', 'n', 0xFF
	};
	interop_context Context;
	xnetengineconfig EngineConfig;
	xhttpserverconfig ServerConfig;
	xhttpserverevents ServerEvents;
	bool bSuccess;

	interopContextInit(&Context);
	interopEventsInit(&Context);
	if ( !interopFillText(
		Context.ExpectedText,
		sizeof(Context.ExpectedText),
		&Context.ExpectedTextSize,
		"python-client-",
		64
	) ) {
		return 2;
	}
	memcpy(Context.ExpectedBinary, Binary, sizeof(Binary));
	Context.ExpectedBinarySize = sizeof(Binary);

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Workers = 1;
	Context.Engine = xrtNetEngineCreate(&EngineConfig);
	if ( Context.Engine == NULL ) {
		fprintf(stderr, "server engine start failed\n");
		return 1;
	}
	if ( !xrtNetEngineStart(Context.Engine) ) {
		fprintf(stderr, "server engine start failed\n");
		(void)xrtNetEngineDestroy(Context.Engine);
		return 1;
	}
	xrtHttpServerConfigInit(&ServerConfig);
	if ( !xrtNetAddrLoopback(
		&ServerConfig.Network.Listen.Address,
		XNET_FAMILY_IPV4,
		iPort
	) ) {
		interopFailError(&Context, xrtGetError());
	}
	xrtHttpServerEventsInit(&ServerEvents);
	ServerEvents.Request = interopServerRequest;
	ServerEvents.Shutdown = interopServerShutdown;
	ServerEvents.Data = &Context;
	if ( xrtAtomic32Load(&Context.Errors, XMEMORY_ACQUIRE) == 0 ) {
		Context.Server = xrtHttpServerStart(
			Context.Engine,
			&ServerConfig,
			&ServerEvents
		);
		if ( Context.Server == NULL ) {
			interopFailError(&Context, xrtGetError());
		}
	}
	if ( Context.Server != NULL ) {
		(void)interopWaitTerminal(&Context);
	}
	bSuccess =
		(xrtAtomic32Load(&Context.Errors, XMEMORY_ACQUIRE) == 0) &&
		(xrtAtomic32Load(&Context.Opened, XMEMORY_ACQUIRE) == 1) &&
		(xrtAtomic32Load(&Context.Text, XMEMORY_ACQUIRE) == 1) &&
		(xrtAtomic32Load(&Context.Binary, XMEMORY_ACQUIRE) == 1) &&
		(xrtAtomic32Load(&Context.Ping, XMEMORY_ACQUIRE) == 1) &&
		(xrtAtomic32Load(&Context.Closed, XMEMORY_ACQUIRE) == 1) &&
		(Context.RemoteCode == XWS_CLOSE_NORMAL) &&
		(strcmp(Context.RemoteReason, "python-client-done") == 0);

	if ( (Context.Connection != NULL) &&
		(xrtWsConnState(Context.Connection) != XWS_CONN_CLOSED) ) {
		(void)xrtWsConnAbort(Context.Connection);
	}
	if ( Context.Server != NULL ) {
		(void)xrtHttpServerAbort(Context.Server);
		if ( !interopWaitServer(&Context) ) {
			bSuccess = false;
		}
	}
	if ( Context.Connection != NULL ) {
		xrtWsConnDestroy(Context.Connection);
	}
	if ( Context.Server != NULL ) {
		xrtHttpServerDestroy(Context.Server);
	}
	if ( !xrtNetEngineDestroy(Context.Engine) ) {
		interopFailError(&Context, xrtGetError());
		bSuccess = false;
	}
	bSuccess = bSuccess &&
		(xrtAtomic32Load(
			&Context.Errors,
			XMEMORY_ACQUIRE
		) == 0);
	if ( !bSuccess ) {
		interopReportContract(&Context, "server");
	}
	return bSuccess ? 0 : 1;
}



/* 客户端握手完成后验证元数据并发送三类线路消息。 */
static void interopClientConnected(
	xhttpcall* pCall,
	xnetresult Result,
	xwsconn* pConnection,
	xhttpresponse* pResponse,
	const xerror* pError,
	ptr pData
)
{
	interop_context* pContext = (interop_context*)pData;
	xwsdeflate Deflate;

	(void)pCall;
	pContext->Response = pResponse;
	if ( (Result != XNET_RESULT_OK) ||
		(pConnection == NULL) ||
		(pResponse == NULL) ) {
		interopFailError(pContext, pError);
		return;
	}
	pContext->Connection = pConnection;
	if ( (xrtHttpResponseStatus(pResponse) !=
		XHTTP_STATUS_SWITCHING_PROTOCOLS) ||
		!interopTextEqual(
			xrtWsConnProtocol(pConnection),
			INTEROP_PROTOCOL
		) || !xrtWsConnDeflate(pConnection, &Deflate) ||
		((Deflate.Flags & (
			XWS_DEFLATE_SERVER_NO_CONTEXT |
			XWS_DEFLATE_CLIENT_NO_CONTEXT
		 )) != (
			XWS_DEFLATE_SERVER_NO_CONTEXT |
			XWS_DEFLATE_CLIENT_NO_CONTEXT
		 )) ) {
		interopFail(pContext, "WebSocket client negotiation mismatch");
		(void)xrtWsConnAbort(pConnection);
		return;
	}
	if ( (xrtWsConnTextCompressed(
		pConnection,
		(xstrview) {
			pContext->ExpectedText,
			pContext->ExpectedTextSize
		}
	 ) != XNET_RESULT_OK) ||
		(xrtWsConnBinary(
			pConnection,
			(xbytesview) {
				pContext->ExpectedBinary,
				pContext->ExpectedBinarySize
			}
		 ) != XNET_RESULT_OK) ||
		(xrtWsConnPing(
			pConnection,
			XRT_BYTES_LITERAL("xrt-ping")
		 ) != XNET_RESULT_OK) ) {
		interopFailError(pContext, xrtGetError());
		(void)xrtWsConnAbort(pConnection);
		return;
	}
	xrtAtomic32Store(
		&pContext->Opened,
		1,
		XMEMORY_RELEASE
	);
}



/* 运行 XRT 客户端到 Python 服务端的互操作 peer。 */
static int interopRunClient(uint16 iPort)
{
	static const uint8 Binary[] = {
		0x10, 'x', 'r', 't', 0x00, 0xFF
	};
	interop_context Context;
	xnetengineconfig EngineConfig;
	xhttpclientconfig ClientConfig;
	xwsclientconfig WsConfig;
	xhttprequest* pRequest = NULL;
	char sUrl[128];
	int iLength;
	bool bSuccess;

	interopContextInit(&Context);
	interopEventsInit(&Context);
	if ( !interopFillText(
		Context.ExpectedText,
		sizeof(Context.ExpectedText),
		&Context.ExpectedTextSize,
		"xrt-client-",
		64
	) ) {
		return 2;
	}
	memcpy(Context.ExpectedBinary, Binary, sizeof(Binary));
	Context.ExpectedBinarySize = sizeof(Binary);

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Workers = 1;
	Context.Engine = xrtNetEngineCreate(&EngineConfig);
	if ( Context.Engine == NULL ) {
		fprintf(stderr, "client engine start failed\n");
		return 1;
	}
	if ( !xrtNetEngineStart(Context.Engine) ) {
		fprintf(stderr, "client engine start failed\n");
		(void)xrtNetEngineDestroy(Context.Engine);
		return 1;
	}
	xrtHttpClientConfigInit(&ClientConfig);
	ClientConfig.Dial.MaxAttempts = 1;
	Context.Client = xrtHttpClientCreate(
		Context.Engine,
		&ClientConfig
	);
	if ( Context.Client == NULL ) {
		interopFailError(&Context, xrtGetError());
	}
	iLength = snprintf(
		sUrl,
		sizeof(sUrl),
		"http://127.0.0.1:%u/interop?peer=xrt",
		(unsigned int)iPort
	);
	if ( (iLength <= 0) ||
		((size_t)iLength >= sizeof(sUrl)) ) {
		interopFail(&Context, "client URL exceeded fixed storage");
	}
	if ( xrtAtomic32Load(&Context.Errors, XMEMORY_ACQUIRE) == 0 ) {
		pRequest = xrtHttpRequestCreate(
			XRT_STR_LITERAL("GET"),
			(xstrview) { sUrl, (size_t)iLength }
		);
		if ( (pRequest == NULL) ||
			!xrtHttpRequestSetHeader(
				pRequest,
				XRT_STR_LITERAL("Origin"),
				XRT_STR_LITERAL(INTEROP_ORIGIN)
			) || !xrtHttpRequestSetHeader(
				pRequest,
				XRT_STR_LITERAL("X-Interop"),
				XRT_STR_LITERAL("xrt-client")
			) ) {
			interopFailError(&Context, xrtGetError());
		}
	}
	xrtWsClientConfigInit(&WsConfig);
	WsConfig.Protocols =
		XRT_STR_LITERAL("other, " INTEROP_PROTOCOL);
	WsConfig.EnableDeflate = true;
	WsConfig.RequireDeflate = true;
	WsConfig.Deflate.Flags =
		XWS_DEFLATE_SERVER_NO_CONTEXT |
		XWS_DEFLATE_CLIENT_NO_CONTEXT;
	if ( xrtAtomic32Load(&Context.Errors, XMEMORY_ACQUIRE) == 0 ) {
		Context.Call = xrtWsConnectRequest(
			Context.Client,
			pRequest,
			&WsConfig,
			&Context.Events,
			&Context,
			interopClientConnected,
			&Context
		);
		if ( Context.Call == NULL ) {
			interopFailError(&Context, xrtGetError());
		}
	}
	xrtHttpRequestDestroy(pRequest);
	if ( Context.Call != NULL ) {
		(void)interopWaitTerminal(&Context);
	}
	bSuccess =
		(xrtAtomic32Load(&Context.Errors, XMEMORY_ACQUIRE) == 0) &&
		(xrtAtomic32Load(&Context.Opened, XMEMORY_ACQUIRE) == 1) &&
		(xrtAtomic32Load(&Context.Text, XMEMORY_ACQUIRE) == 1) &&
		(xrtAtomic32Load(&Context.Binary, XMEMORY_ACQUIRE) == 1) &&
		(xrtAtomic32Load(&Context.Pong, XMEMORY_ACQUIRE) == 1) &&
		(xrtAtomic32Load(&Context.Closed, XMEMORY_ACQUIRE) == 1) &&
		(Context.RemoteCode == XWS_CLOSE_NORMAL) &&
		(strcmp(Context.RemoteReason, "python-server-done") == 0);

	if ( (Context.Connection != NULL) &&
		(xrtWsConnState(Context.Connection) != XWS_CONN_CLOSED) ) {
		(void)xrtWsConnAbort(Context.Connection);
	}
	if ( Context.Connection != NULL ) {
		xrtWsConnDestroy(Context.Connection);
	}
	xrtHttpResponseDestroy(Context.Response);
	xrtHttpCallDestroy(Context.Call);
	xrtHttpClientDestroy(Context.Client);
	if ( !xrtNetEngineDestroy(Context.Engine) ) {
		interopFailError(&Context, xrtGetError());
		bSuccess = false;
	}
	bSuccess = bSuccess &&
		(xrtAtomic32Load(
			&Context.Errors,
			XMEMORY_ACQUIRE
		) == 0);
	if ( !bSuccess ) {
		interopReportContract(&Context, "client");
	}
	return bSuccess ? 0 : 1;
}



/* 外部驱动传入角色和端口；普通模块回归只验证 peer 可构建。 */
int main(int iArgumentCount, char** pArguments)
{
	unsigned long iPort;
	char* pEnd = NULL;

	if ( iArgumentCount == 1 ) {
		printf("[PASS] WebSocket interop peer build\n");
		return 0;
	}
	if ( (iArgumentCount != 3) ||
		((strcmp(pArguments[1], "server") != 0) &&
		 (strcmp(pArguments[1], "client") != 0)) ) {
		fprintf(
			stderr,
			"usage: %s server|client port\n",
			iArgumentCount == 0 ?
				"test_interop_peer" : pArguments[0]
		);
		return 2;
	}
	iPort = strtoul(pArguments[2], &pEnd, 10);
	if ( (pEnd == NULL) || (*pEnd != '\0') ||
		(iPort == 0) || (iPort > UINT16_MAX) ) {
		fprintf(stderr, "invalid interop port\n");
		return 2;
	}
	return strcmp(pArguments[1], "server") == 0 ?
		interopRunServer((uint16)iPort) :
		interopRunClient((uint16)iPort);
}
