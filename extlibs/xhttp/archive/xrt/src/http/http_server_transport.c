#include "../internal/xrt_http_server_runtime.h"



#if defined(XRT_FEATURE_HTTP_SERVER)

/*
	建立不附加任何协议字节的线缆响应计划。
	Body 由计划保留，调用方仍负责释放自己的引用。
*/
xhttp1serverresponse* __xrtHttpConnWireResponse(
	xhttpbody* pBody,
	uint64 iLength,
	bool bClose,
	bool bTunnel
)
{
	xhttp1serverresponse* pResponse;

	if ( (pBody == NULL) || (iLength == 0) ||
		(iLength == XHTTP_BODY_UNKNOWN) ) {
		return NULL;
	}
	pResponse = (xhttp1serverresponse*)xrtCalloc(
		1, sizeof(*pResponse)
	);
	if ( pResponse == NULL ) {
		return NULL;
	}
	pResponse->Body = xrtHttpBodyRef(pBody);
	if ( pResponse->Body == NULL ) {
		xrtFree(pResponse);
		return NULL;
	}
	pResponse->BodyLength = iLength;
	pResponse->BodyRemaining = iLength;
	pResponse->Mode = XHTTP1_BODY_FIXED;
	pResponse->State = XRT_HTTP_SERVER_RESPONSE_BODY;
	pResponse->Close = bClose;
	pResponse->Tunnel = bTunnel;
	return pResponse;
}



/* 复制引用描述符并建立不附加协议字节的线缆响应计划。 */
xhttp1serverresponse* __xrtHttpConnWireRefsResponse(
	const xnetref* pRefs,
	size_t iCount,
	uint64 iLength,
	bool bClose,
	bool bTunnel
)
{
	xhttp1serverresponse* pResponse;
	size_t iAllocation;
	size_t iUsed = 0;

	if ( (pRefs == NULL) || (iCount == 0) ||
		(iLength == 0) || (iLength == XHTTP_BODY_UNKNOWN) ||
		(iCount > ((SIZE_MAX - sizeof(*pResponse)) /
		 sizeof(*pResponse->WireRefs))) ) {
		return NULL;
	}
	iAllocation = sizeof(*pResponse) +
		(iCount * sizeof(*pResponse->WireRefs));
	pResponse = (xhttp1serverresponse*)xrtCalloc(
		1, iAllocation
	);
	if ( pResponse == NULL ) {
		return NULL;
	}
	pResponse->WireRefs = (xhttpbodychunk*)(pResponse + 1);
	for ( size_t i = 0; i < iCount; i++ ) {
		if ( pRefs[i].Size != 0 ) {
			pResponse->WireRefs[iUsed++] = (xhttpbodychunk){
				pRefs[i].Data,
				pRefs[i].Size,
				pRefs[i].Release,
				pRefs[i].Context
			};
		}
	}
	pResponse->WireRefCount = iUsed;
	pResponse->BodyLength = iLength;
	pResponse->BodyRemaining = iLength;
	pResponse->Mode = XHTTP1_BODY_FIXED;
	pResponse->State = XRT_HTTP_SERVER_RESPONSE_BODY;
	pResponse->Close = bClose;
	pResponse->Tunnel = bTunnel;
	return pResponse;
}



/* 发布当前 Connection 稳定错误事件。 */
void __xrtHttpConnEmitError(xhttpconn* pConnection)
{
	xhttpserver* pServer = pConnection->Server;
	const xerror* pError = xrtHttpConnError(pConnection);

	if ( (pError != NULL) &&
		!pConnection->ErrorNotified &&
		(pServer->Events.Error != NULL) ) {
		pConnection->ErrorNotified = true;
		pServer->Events.Error(
			pServer,
			pConnection,
			pError,
			pServer->Events.Data
		);
	}
}



/* 提取当前请求版本、方法和可复用事实，并合并连接终态。 */
static void __xrtHttpConnResponseFacts(
	xhttpconn* pConnection,
	xhttpversion* pVersion,
	xstrview* pMethod,
	uint32* pFlags
)
{
	const xhttpserverrequest* pRequest =
		xrtHttp1ServerExchangeRequest(
			pConnection->Exchange
		);
	xhttpversion Version = XHTTP_VERSION_1_1;
	xstrview Method = XRT_STR_LITERAL("GET");
	uint32 iFlags = 0;
	bool bRequestComplete = xrtHttp1ServerExchangeComplete(
		pConnection->Exchange
	);

	if ( pRequest != NULL ) {
		Version = xrtHttpServerRequestVersion(pRequest);
		Method = xrtHttpServerRequestMethod(pRequest);
		iFlags = xrtHttpServerRequestFlags(pRequest);
		if ( xrtHttpServerRequestBodyMode(pRequest) ==
			XHTTP1_BODY_NONE ) {
			/* Header 已完整固定，无正文请求可以在 Headers 回调后安全复用。 */
			bRequestComplete = true;
		}
	}
	if ( pConnection->ForceClose ||
		pConnection->InputEnded ||
		!bRequestComplete ||
		(xrtHttpServerState(pConnection->Server) !=
		 XHTTP_SERVER_RUNNING) ) {
		iFlags &= ~XHTTP_SERVER_REQUEST_KEEP_ALIVE;
		pConnection->ForceClose = true;
	}
	*pVersion = Version;
	*pMethod = Method;
	*pFlags = iFlags;
}



/* 按当前请求与 Server 排空事实冻结最终响应。 */
static xhttp1serverresponse* __xrtHttpConnPrepareResponse(
	xhttpconn* pConnection,
	const xhttpreply* pReply
)
{
	xhttpversion Version;
	xstrview Method;
	uint32 iFlags;

	__xrtHttpConnResponseFacts(
		pConnection,
		&Version,
		&Method,
		&iFlags
	);
	return xrtHttp1ServerResponsePrepare(
		Version, Method, iFlags, pReply
	);
}



/* 按当前请求版本冻结信息响应。 */
static xhttp1serverresponse* __xrtHttpConnPrepareInformation(
	xhttpconn* pConnection,
	const xhttpreply* pReply
)
{
	const xhttpserverrequest* pRequest =
		xrtHttp1ServerExchangeRequest(
			pConnection->Exchange
		);
	xhttpversion Version = pRequest != NULL ?
		xrtHttpServerRequestVersion(pRequest) :
		XHTTP_VERSION_1_1;

	return xrtHttp1ServerResponseInform(Version, pReply);
}



/* 检查 Connection 能否在当前 Worker 提交响应。 */
bool __xrtHttpConnCanRespond(
	xhttpconn* pConnection,
	cstr sOperation,
	bool bRequireRequest
)
{
	if ( (pConnection == NULL) ||
		!xrtNetWorkerIsCurrent(
			pConnection != NULL ?
				pConnection->Worker : NULL
		) ) {
		__xrtHttpServerSetError(
			pConnection == NULL ? XERR_ARGUMENT : XERR_STATE,
			pConnection == NULL ?
				XHTTP_SERVER_ERROR_ARGUMENT :
				XHTTP_SERVER_ERROR_STATE,
			sOperation,
			"HTTP response submission requires its connection Worker",
			NULL
		);
		return false;
	}
	if ( (xrtHttpConnState(pConnection) ==
		 XHTTP_CONN_CLOSING) ||
		(xrtHttpConnState(pConnection) ==
		 XHTTP_CONN_CLOSED) ||
		(xrtAtomicPtrLoad(
			&pConnection->Transport,
			XMEMORY_ACQUIRE
		) == NULL) ) {
		__xrtHttpServerSetError(
			XERR_CLOSED,
			XHTTP_SERVER_ERROR_STATE,
			sOperation,
			"HTTP connection is closing or closed",
			NULL
		);
		return false;
	}
	if ( bRequireRequest &&
		(xrtHttp1ServerExchangeRequest(
			pConnection->Exchange
		 ) == NULL) ) {
		__xrtHttpServerSetError(
			XERR_STATE,
			XHTTP_SERVER_ERROR_STATE,
			sOperation,
			"HTTP response submission requires an active request",
			NULL
		);
		return false;
	}
	return true;
}



/* 安装信息或最终响应并启动写时限。 */
static xnetresult __xrtHttpConnStartResponse(
	xhttpconn* pConnection,
	xhttp1serverresponse* pResponse,
	bool bInformation
)
{
	__xrt_http_response_queue* pQueued;

	if ( pConnection->Response != NULL ) {
		pQueued = (__xrt_http_response_queue*)xrtCalloc(
			1, sizeof(*pQueued)
		);
		if ( pQueued == NULL ) {
			xrtHttp1ServerResponseDestroy(pResponse);
			__xrtHttpServerSetError(
				XERR_MEMORY,
				XHTTP_SERVER_ERROR_RESPONSE,
				bInformation ?
					"queue-http-information" :
					"queue-http-response",
				"HTTP response queue allocation failed",
				xrtGetError()
			);
			return XNET_RESULT_ERROR;
		}
		pQueued->Response = pResponse;
		pQueued->Information = bInformation;
		if ( !bInformation ) {
			xrtAtomic32Store(
				&pConnection->RequestBodyPaused,
				0,
				XMEMORY_RELEASE
			);
		}
		__xrtHttp1ServerResponseOwnRefs(pResponse);
		if ( pConnection->ResponseTail != NULL ) {
			pConnection->ResponseTail->Next = pQueued;
		} else {
			pConnection->ResponseHead = pQueued;
		}
		pConnection->ResponseTail = pQueued;
		return XNET_RESULT_OK;
	}
	pConnection->Response = pResponse;
	pConnection->ResponseInformation = bInformation;
	if ( !bInformation ) {
		xrtAtomic32Store(
			&pConnection->RequestBodyPaused,
			0,
			XMEMORY_RELEASE
		);
	}
	xrtAtomic32Store(
		&pConnection->State,
		bInformation ?
			XHTTP_CONN_INFORMATION :
			XHTTP_CONN_RESPONSE,
		XMEMORY_RELEASE
	);
	__xrtHttpConnCancelTimer(pConnection);
	__xrtHttpConnPauseInput(pConnection);
	pConnection->WriteDeadline =
		pConnection->Server->Config.WriteTimeout != 0 ?
			xrtDeadlineAfter(
				pConnection->Server->Config.WriteTimeout
			) : 0;
	if ( !__xrtHttpConnArmTimer(
		pConnection,
		XRT_HTTP_SERVER_TIMER_WRITE,
		pConnection->Server->Config.WriteTimeout
	) ) {
		(void)xrtHttpConnAbort(pConnection);
		return XNET_RESULT_ERROR;
	}
	__xrtHttp1ServerResponseOwnRefs(pResponse);
	__xrtHttpConnDriveOutput(pConnection);
	return XNET_RESULT_OK;
}



/*
	把已经拥有的响应计划提交给唯一最终响应门。
	普通响应拒绝 Tunnel，Upgrade 组合层可以显式接管该终态。
*/
xnetresult __xrtHttpConnCommitResponse(
	xhttpconn* pConnection,
	xhttp1serverresponse* pResponse,
	cstr sOperation,
	bool bAllowTunnel
)
{
	uint32 iExpected = 0;

	if ( pResponse == NULL ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_ARGUMENT,
			sOperation,
			"HTTP final response plan is null",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	if ( xrtHttp1ServerResponseTunnel(pResponse) &&
		!bAllowTunnel ) {
		xrtHttp1ServerResponseDestroy(pResponse);
		__xrtHttpServerSetError(
			XERR_UNSUPPORTED,
			XHTTP_SERVER_ERROR_RESPONSE,
			sOperation,
			"HTTP tunnel response requires the Upgrade transport layer",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	#if defined(XRT_FEATURE_HTTP_SERVER_FUTURE)
		__xrtHttpConnFutureDetach(pConnection, true);
	#endif
	if ( !xrtAtomic32CompareExchange(
		&pConnection->FinalGate,
		&iExpected,
		1,
		XMEMORY_ACQ_REL,
		XMEMORY_ACQUIRE
	) ) {
		xrtHttp1ServerResponseDestroy(pResponse);
		__xrtHttpServerSetError(
			XERR_STATE,
			XHTTP_SERVER_ERROR_STATE,
			sOperation,
			"HTTP connection raced another final response",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	{
		xnetresult Result = __xrtHttpConnStartResponse(
			pConnection, pResponse, false
		);

		if ( Result != XNET_RESULT_OK ) {
			xrtAtomic32Store(
				&pConnection->FinalGate,
				0,
				XMEMORY_RELEASE
			);
		}
		return Result;
	}
}



/* 提交一条信息响应。 */
XRT_API xnetresult xrtHttpConnInform(
	xhttpconn* pConnection,
	const xhttpreply* pReply
)
{
	xhttp1serverresponse* pResponse;

	if ( !__xrtHttpConnCanRespond(
		pConnection, "inform-http-connection", true
	) || (pReply == NULL) ) {
		if ( pReply == NULL ) {
			__xrtHttpServerSetError(
				XERR_ARGUMENT,
				XHTTP_SERVER_ERROR_ARGUMENT,
				"inform-http-connection",
				"HTTP information Reply is null",
				NULL
			);
		}
		return XNET_RESULT_ERROR;
	}
	if ( xrtAtomic32Load(
		&pConnection->FinalGate,
		XMEMORY_ACQUIRE
	) ) {
		__xrtHttpServerSetError(
			XERR_STATE,
			XHTTP_SERVER_ERROR_STATE,
			"inform-http-connection",
			"HTTP information cannot follow the final response",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	if ( pConnection->InformationCount >=
		pConnection->Server->Config.MaxInformations ) {
		__xrtHttpServerSetError(
			XERR_RANGE,
			XHTTP_SERVER_ERROR_RESPONSE,
			"inform-http-connection",
			"HTTP information response limit was reached",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	pResponse = __xrtHttpConnPrepareInformation(
		pConnection, pReply
	);
	if ( pResponse == NULL ) {
		__xrtHttpServerSetError(
			__xrtHttpServerCauseKind(
				xrtGetError(),
				XERR_PROTOCOL
			),
			XHTTP_SERVER_ERROR_RESPONSE,
			"inform-http-connection",
			"HTTP information could not be prepared",
			xrtGetError()
		);
		return XNET_RESULT_ERROR;
	}
	{
		xnetresult Result = __xrtHttpConnStartResponse(
		pConnection, pResponse, true
		);

		if ( Result == XNET_RESULT_OK ) {
			pConnection->InformationCount++;
		}
		return Result;
	}
}



/* 提交唯一最终响应；协议错误可在请求头尚未建立时使用此内部路径。 */
static xnetresult __xrtHttpConnSubmitFinal(
	xhttpconn* pConnection,
	const xhttpreply* pReply,
	cstr sOperation,
	bool bRequireRequest
)
{
	xhttp1serverresponse* pResponse;

	if ( !__xrtHttpConnCanRespond(
		pConnection, sOperation, bRequireRequest
	) || (pReply == NULL) ) {
		if ( pReply == NULL ) {
			__xrtHttpServerSetError(
				XERR_ARGUMENT,
				XHTTP_SERVER_ERROR_ARGUMENT,
				sOperation,
				"HTTP final Reply is null",
				NULL
			);
		}
		return XNET_RESULT_ERROR;
	}
	if ( xrtAtomic32Load(
		&pConnection->FinalGate,
		XMEMORY_ACQUIRE
	) ) {
		__xrtHttpServerSetError(
			XERR_STATE,
			XHTTP_SERVER_ERROR_STATE,
			sOperation,
			"HTTP connection already committed its final response",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	pResponse = __xrtHttpConnPrepareResponse(
		pConnection, pReply
	);
	if ( pResponse == NULL ) {
		__xrtHttpServerSetError(
			__xrtHttpServerCauseKind(
				xrtGetError(),
				XERR_PROTOCOL
			),
			XHTTP_SERVER_ERROR_RESPONSE,
			sOperation,
			"HTTP final response could not be prepared",
			xrtGetError()
		);
		return XNET_RESULT_ERROR;
	}
	return __xrtHttpConnCommitResponse(
		pConnection,
		pResponse,
		sOperation,
		false
	);
}



/* 提交应用层唯一最终响应。 */
XRT_API xnetresult xrtHttpConnRespond(
	xhttpconn* pConnection,
	const xhttpreply* pReply
)
{
	return __xrtHttpConnSubmitFinal(
		pConnection,
		pReply,
		"respond-http-connection",
		true
	);
}



/* 验证直接响应的连接、状态和可选 Content-Type。 */
static bool __xrtHttpConnReplyValidate(
	xhttpconn* pConnection,
	uint16 iStatus,
	xstrview ContentType,
	cstr sOperation,
	bool bRequireRequest
)
{
	if ( !__xrtHttpConnCanRespond(
		pConnection,
		sOperation,
		bRequireRequest
	) ) {
		return false;
	}
	if ( (iStatus < 100) || (iStatus > 999) ) {
		__xrtHttpServerSetError(
			XERR_RANGE,
			XHTTP_SERVER_ERROR_RESPONSE,
			sOperation,
			"HTTP response status is outside 100..999",
			NULL
		);
		return false;
	}
	if ( (ContentType.Data == NULL) &&
		(ContentType.Size != 0) ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_RESPONSE,
			sOperation,
			"HTTP response Content-Type view is invalid",
			NULL
		);
		return false;
	}
	if ( !xrtHttpFieldValueValid(ContentType) ) {
		__xrtHttpServerSetError(
			XERR_VALUE,
			XHTTP_SERVER_ERROR_RESPONSE,
			sOperation,
			"HTTP response Content-Type contains an invalid field value",
			NULL
		);
		return false;
	}
	return true;
}



/* 用已经验证的输入冻结 Body 并提交唯一最终响应。 */
static xnetresult __xrtHttpConnSubmitBodyValidated(
	xhttpconn* pConnection,
	uint16 iStatus,
	xstrview ContentType,
	xhttpbody* pBody,
	cstr sOperation
)
{
	xrt_http1_server_response_source Source;
	xhttpfield ContentTypeField;
	xhttp1serverresponse* pResponse;
	xhttpversion Version;
	xstrview Method;
	uint32 iFlags;

	memset(&Source, 0, sizeof(Source));
	ContentTypeField.Name = XRT_STR_LITERAL("Content-Type");
	ContentTypeField.Value = ContentType;
	Source.Status = iStatus;
	Source.Reason = xrtHttpStatusText(iStatus);
	Source.Headers = ContentType.Size != 0 ?
		&ContentTypeField : NULL;
	Source.HeaderCount = ContentType.Size != 0 ? 1 : 0;
	Source.Body = pBody;
	Source.BodyLength = pBody != NULL ?
		xrtHttpBodyLength(pBody) : 0;
	__xrtHttpConnResponseFacts(
		pConnection,
		&Version,
		&Method,
		&iFlags
	);
	pResponse = __xrtHttp1ServerResponsePrepareSource(
		Version,
		Method,
		iFlags,
		&Source
	);
	if ( pResponse == NULL ) {
		__xrtHttpServerSetError(
			__xrtHttpServerCauseKind(
				xrtGetError(), XERR_PROTOCOL
			),
			XHTTP_SERVER_ERROR_RESPONSE,
			sOperation,
			"HTTP response could not be prepared",
			xrtGetError()
		);
		return XNET_RESULT_ERROR;
	}
	return __xrtHttpConnCommitResponse(
		pConnection,
		pResponse,
		sOperation,
		false
	);
}



/* 构建并提交固定正文；内部协议错误可以不依赖完整请求。 */
static xnetresult __xrtHttpConnSubmitFixed(
	xhttpconn* pConnection,
	uint16 iStatus,
	xstrview ContentType,
	xbytesview Body,
	cstr sOperation,
	bool bRequireRequest
)
{
	xhttpbody* pBody = NULL;
	xnetresult Result;

	if ( !__xrtHttpConnReplyValidate(
		pConnection,
		iStatus,
		ContentType,
		sOperation,
		bRequireRequest
	) ) {
		return XNET_RESULT_ERROR;
	}
	if ( (Body.Data == NULL) && (Body.Size != 0) ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_RESPONSE,
			sOperation,
			"HTTP response body view is invalid",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	if ( Body.Size != 0 ) {
		pBody = xrtHttpBodyCopy(Body);
		if ( pBody == NULL ) {
			__xrtHttpServerSetError(
				__xrtHttpServerCauseKind(
					xrtGetError(), XERR_MEMORY
				),
				XHTTP_SERVER_ERROR_RESPONSE,
				sOperation,
				"HTTP response body could not be copied",
				xrtGetError()
			);
			return XNET_RESULT_ERROR;
		}
	}
	Result = __xrtHttpConnSubmitBodyValidated(
		pConnection,
		iStatus,
		ContentType,
		pBody,
		sOperation
	);
	xrtHttpBodyDestroy(pBody);
	return Result;
}



/* 一次调用构建并提交常用固定正文。 */
XRT_API xnetresult xrtHttpConnReply(
	xhttpconn* pConnection,
	uint16 iStatus,
	xstrview ContentType,
	xbytesview Body
)
{
	return __xrtHttpConnSubmitFixed(
		pConnection,
		iStatus,
		ContentType,
		Body,
		"respond-http-connection",
		true
	);
}



/* 一次调用保留正文来源并提交常规 HTTP 响应。 */
XRT_API xnetresult xrtHttpConnReplyBody(
	xhttpconn* pConnection,
	uint16 iStatus,
	xstrview ContentType,
	xhttpbody* pBody
)
{
	if ( !__xrtHttpConnReplyValidate(
		pConnection,
		iStatus,
		ContentType,
		"respond-http-connection",
		true
	) ) {
		return XNET_RESULT_ERROR;
	}
	return __xrtHttpConnSubmitBodyValidated(
		pConnection,
		iStatus,
		ContentType,
		pBody,
		"respond-http-connection"
	);
}



/* 发送自动 100 Continue。 */
bool __xrtHttpConnContinue(xhttpconn* pConnection)
{
	xhttpreply* pReply = xrtHttpReplyCreate(100);
	xnetresult Result;

	if ( pReply == NULL ) {
		return false;
	}
	Result = xrtHttpConnInform(pConnection, pReply);
	xrtHttpReplyDestroy(pReply);
	return Result == XNET_RESULT_OK;
}



/* 完成信息响应后恢复请求读取或等待最终响应。 */
static void __xrtHttpConnInformationDone(
	xhttpconn* pConnection
)
{
	const xhttpserverrequest* pRequest =
		xrtHttp1ServerExchangeRequest(
			pConnection->Exchange
		);
	bool bRequestBodyPaused = xrtAtomic32Load(
		&pConnection->RequestBodyPaused,
		XMEMORY_ACQUIRE
	) != 0;

	if ( xrtHttp1ServerExchangeComplete(
		pConnection->Exchange
	) ) {
		xrtAtomic32Store(
			&pConnection->State,
			XHTTP_CONN_WAITING,
			XMEMORY_RELEASE
		);
		if ( !__xrtHttpConnArmTimer(
			pConnection,
			XRT_HTTP_SERVER_TIMER_REQUEST,
			pConnection->Server->Config.RequestTimeout
		) ) {
			(void)xrtHttpConnAbort(pConnection);
		}
		return;
	}
	if ( !bRequestBodyPaused &&
		xrtHttp1ServerExchangePaused(
			pConnection->Exchange
		) && !__xrtHttpConnResumeExchange(pConnection) ) {
		return;
	}
	xrtAtomic32Store(
		&pConnection->State,
		(pRequest != NULL) &&
		(xrtHttpServerRequestBodyMode(pRequest) !=
		 XHTTP1_BODY_NONE) ?
			XHTTP_CONN_BODY :
			XHTTP_CONN_REQUEST,
		XMEMORY_RELEASE
	);
	if ( !__xrtHttpConnArmTimer(
		pConnection,
		pRequest != NULL ?
			XRT_HTTP_SERVER_TIMER_BODY :
			XRT_HTTP_SERVER_TIMER_HEADER,
		pRequest != NULL ?
			pConnection->Server->Config.BodyTimeout :
			pConnection->Server->Config.HeaderTimeout
	) ) {
		(void)xrtHttpConnAbort(pConnection);
		return;
	}
	if ( bRequestBodyPaused ) {
		__xrtHttpConnPauseInput(pConnection);
		return;
	}
	__xrtHttpConnResumeInput(pConnection);
	__xrtHttpConnDriveInput(pConnection);
}



/* 完成最终响应后关闭或进入下一条 keep-alive 请求。 */
static void __xrtHttpConnFinalDone(
	xhttpconn* pConnection,
	bool bResponseClose
)
{
	xhttpserver* pServer;
	bool bClose;

	/* 捕获 Connection 持有的稳定 Server 引用，避免完成路径反复读取。 */
	if ( pConnection == NULL ) {
		return;
	}
	pServer = pConnection->Server;
	if ( pServer == NULL ) {
		return;
	}

	bClose = bResponseClose ||
		pConnection->ForceClose ||
		pConnection->InputEnded ||
		(xrtHttpServerState(pServer) !=
		 XHTTP_SERVER_RUNNING);

	(void)xrtAtomic64FetchAdd(
		&pServer->Responses,
		1,
		XMEMORY_RELAXED
	);
	if ( bClose ) {
		(void)xrtHttpConnClose(pConnection);
		return;
	}
	if ( !xrtHttp1ServerExchangeNext(
		pConnection->Exchange
	) ) {
		__xrtHttpConnRememberError(
			pConnection,
			XERR_STATE,
			XHTTP_SERVER_ERROR_PROTOCOL,
			"reuse-http-connection",
			"HTTP connection could not enter its next request",
			xrtGetError()
		);
		(void)xrtHttpConnAbort(pConnection);
		return;
	}
	xrtAtomic32Store(
		&pConnection->FinalGate,
		0,
		XMEMORY_RELEASE
	);
	xrtAtomic32Store(
		&pConnection->RequestActive,
		0,
		XMEMORY_RELEASE
	);
	xrtAtomic32Store(
		&pConnection->State,
		XHTTP_CONN_REQUEST,
		XMEMORY_RELEASE
	);
	pConnection->ForceClose = false;
	pConnection->InformationCount = 0;
	if ( !__xrtHttpConnArmTimer(
		pConnection,
		XRT_HTTP_SERVER_TIMER_IDLE,
		pServer->Config.IdleTimeout
	) ) {
		(void)xrtHttpConnAbort(pConnection);
		return;
	}
	__xrtHttpConnResumeInput(pConnection);
	__xrtHttpConnDriveInput(pConnection);
}



/* 回收完成响应并推进 Connection 后续阶段。 */
static void __xrtHttpConnResponseDone(xhttpconn* pConnection)
{
	xhttp1serverresponse* pResponse;
	__xrt_http_response_queue* pQueued;
	bool bInformation;
	bool bClose;

	/* Connection 生命周期必须始终持有 Server；拒绝损坏状态下的空地址原子写。 */
	if ( (pConnection == NULL) || (pConnection->Server == NULL) ) {
		return;
	}
	pResponse = pConnection->Response;
	bInformation = pConnection->ResponseInformation;
	bClose = xrtHttp1ServerResponseClose(pResponse);

	__xrtHttpConnCancelTimer(pConnection);
	pConnection->WriteDeadline = 0;
	pConnection->WritePending = 0;
	pConnection->OutputDraining = false;
	pConnection->Response = NULL;
	pConnection->ResponseInformation = false;
	xrtHttp1ServerResponseDestroy(pResponse);
	if ( bInformation ) {
		(void)xrtAtomic64FetchAdd(
			&pConnection->Server->Informations,
			1,
			XMEMORY_RELAXED
		);
	}
	pQueued = pConnection->ResponseHead;
	if ( pQueued != NULL ) {
		pConnection->ResponseHead = pQueued->Next;
		if ( pConnection->ResponseHead == NULL ) {
			pConnection->ResponseTail = NULL;
		}
		pConnection->Response = pQueued->Response;
		pConnection->ResponseInformation =
			pQueued->Information;
		xrtAtomic32Store(
			&pConnection->State,
			pQueued->Information ?
				XHTTP_CONN_INFORMATION :
				XHTTP_CONN_RESPONSE,
			XMEMORY_RELEASE
		);
		xrtFree(pQueued);
		pConnection->WriteDeadline =
			pConnection->Server->Config.WriteTimeout != 0 ?
				xrtDeadlineAfter(
					pConnection->Server->
						Config.WriteTimeout
				) : 0;
		if ( !__xrtHttpConnArmTimer(
			pConnection,
			XRT_HTTP_SERVER_TIMER_WRITE,
			pConnection->Server->Config.WriteTimeout
		) ) {
			(void)xrtHttpConnAbort(pConnection);
			return;
		}
		__xrtHttpConnDriveOutput(pConnection);
	} else if ( bInformation ) {
		__xrtHttpConnInformationDone(pConnection);
	} else {
		__xrtHttpConnFinalDone(pConnection, bClose);
	}
}



/* 把输出继续工作排入所属 Worker。 */
static bool __xrtHttpConnPostOutput(
	xhttpconn* pConnection
);



/* 一个零复制 TCP 租约离队后才推进 Response 所有权。 */
static void __xrtHttpConnOutputRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	xhttpconn* pConnection = (xhttpconn*)pContext;
	xnetstream* pStream = (xnetstream*)xrtAtomicPtrLoad(
		&pConnection->Transport,
		XMEMORY_ACQUIRE
	);

	if ( pConnection->OutputQueued &&
		(pConnection->Response != NULL) &&
		(pConnection->Offered == iSize) &&
		(xrtHttpConnState(pConnection) !=
		 XHTTP_CONN_CLOSING) &&
		(xrtHttpConnState(pConnection) !=
		 XHTTP_CONN_CLOSED) &&
		(pStream != NULL) &&
		(xrtNetStreamState(pStream) ==
		 XNET_STREAM_OPEN) ) {
		pConnection->OutputQueued = false;
		pConnection->Offered = 0;
		if ( !xrtHttp1ServerResponseOutputConsume(
			pConnection->Response, iSize
		) ) {
			__xrtHttpConnRememberError(
				pConnection,
				XERR_STATE,
				XHTTP_SERVER_ERROR_RESPONSE,
				"consume-http-response-output",
				"HTTP response rejected a completed TCP output lease",
				xrtGetError()
			);
			__xrtHttpConnEmitError(pConnection);
			(void)xrtHttpConnAbort(pConnection);
		} else {
			(void)xrtAtomic64FetchAdd(
				&pConnection->ResponseWireBytes,
				(uint64)iSize,
				XMEMORY_RELAXED
			);
			if ( pConnection->Server->Config.WriteTimeout != 0 ) {
				pConnection->WriteDeadline =
					xrtDeadlineAfter(
						pConnection->Server->
							Config.WriteTimeout
					);
			}
			if ( !__xrtHttpConnPostOutput(pConnection) ) {
				__xrtHttpConnRememberError(
					pConnection,
					XERR_INTERNAL,
					XHTTP_SERVER_ERROR_INTERNAL,
					"continue-http-response-output",
					"HTTP response output could not return to its Worker",
					xrtGetError()
				);
				__xrtHttpConnEmitError(pConnection);
				(void)xrtHttpConnAbort(pConnection);
			}
		}
	}
	(void)pData;
	xrtHttpConnDestroy(pConnection);
}



/* 向明文 TCP 提交当前 Response 的一个稳定租约。 */
static xnetresult __xrtHttpConnSendTcpOutput(
	xhttpconn* pConnection,
	xbytesview Data
)
{
	xnetstream* pStream = (xnetstream*)xrtAtomicPtrLoad(
		&pConnection->Transport,
		XMEMORY_ACQUIRE
	);
	xnetresult Result;

	if ( xrtHttpConnRef(pConnection) == NULL ) {
		return XNET_RESULT_ERROR;
	}
	pConnection->Offered = Data.Size;
	pConnection->OutputQueued = true;
	Result = xrtNetStreamSendRef(
		pStream,
		Data.Data,
		Data.Size,
		__xrtHttpConnOutputRelease,
		pConnection
	);
	if ( Result != XNET_RESULT_OK ) {
		pConnection->OutputQueued = false;
		pConnection->Offered = 0;
		xrtHttpConnDestroy(pConnection);
	}
	return Result;
}



#if defined(XRT_FEATURE_HTTP_SERVER_TLS)

/* 把 TLS 已经接收的明文前缀立即提交给 Response 所有权。 */
static bool __xrtHttpConnTlsOutputConsume(
	xhttpconn* pConnection,
	size_t iSize
)
{
	if ( !xrtHttp1ServerResponseOutputConsume(
		pConnection->Response, iSize
	) ) {
		return false;
	}
	(void)xrtAtomic64FetchAdd(
		&pConnection->ResponseWireBytes,
		(uint64)iSize,
		XMEMORY_RELAXED
	);
	if ( pConnection->Server->Config.WriteTimeout != 0 ) {
		pConnection->WriteDeadline = xrtDeadlineAfter(
			pConnection->Server->Config.WriteTimeout
		);
	}
	return true;
}



/* 向 TLS 提交明文，处理短写并等待下一次 Writable 边沿。 */
static xnetresult __xrtHttpConnSendTlsOutput(
	xhttpconn* pConnection,
	xbytesview Data
)
{
	xtlsstream* pStream = (xtlsstream*)xrtAtomicPtrLoad(
		&pConnection->Transport,
		XMEMORY_ACQUIRE
	);
	xtlsresult Result;
	size_t iWritten = 0;

	if ( pStream == NULL ) {
		return XNET_RESULT_CLOSED;
	}
	Result = xrtTlsStreamSend(
		pStream,
		Data.Data,
		Data.Size,
		&iWritten
	);
	if ( (iWritten != 0) &&
		!__xrtHttpConnTlsOutputConsume(
			pConnection, iWritten
		) ) {
		return XNET_RESULT_ERROR;
	}
	if ( Result == XTLS_ERROR ) {
		return XNET_RESULT_ERROR;
	}
	if ( Result == XTLS_CLOSED ) {
		return XNET_RESULT_CLOSED;
	}
	if ( iWritten < Data.Size ) {
		pConnection->OutputQueued = true;
		return XNET_RESULT_AGAIN;
	}
	pConnection->OutputPending = true;
	return XNET_RESULT_OK;
}

#endif



/* 按 Connection 传输类型提交当前 Response 输出。 */
static xnetresult __xrtHttpConnSendOutput(
	xhttpconn* pConnection,
	xbytesview Data
)
{
	#if defined(XRT_FEATURE_HTTP_SERVER_TLS)
		if ( pConnection->TransportKind ==
			XRT_HTTP_CONN_TRANSPORT_TLS ) {
			return __xrtHttpConnSendTlsOutput(
				pConnection, Data
			);
		}
	#endif
	return __xrtHttpConnSendTcpOutput(
		pConnection, Data
	);
}



/* 在下一轮 Worker 调度中继续达到本轮预算的输出泵。 */
static void __xrtHttpConnOutputTask(
	xnetworker* pWorker,
	ptr pData
)
{
	xhttpconn* pConnection = (xhttpconn*)pData;

	(void)pWorker;
	pConnection->OutputPosted = false;
	__xrtHttpConnDriveOutput(pConnection);
	xrtHttpConnDestroy(pConnection);
}



/* 把响应剩余工作排入所属 Worker，避免重入 TCP 缓冲变更。 */
static bool __xrtHttpConnPostOutput(
	xhttpconn* pConnection
)
{
	if ( pConnection->OutputPosted ) {
		return true;
	}
	if ( xrtHttpConnRef(pConnection) == NULL ) {
		return false;
	}
	pConnection->OutputPosted = true;
	if ( !xrtNetEnginePost(
		pConnection->Server->Engine,
		xrtNetWorkerIndex(pConnection->Worker),
		__xrtHttpConnOutputTask,
		pConnection
	) ) {
		pConnection->OutputPosted = false;
		xrtHttpConnDestroy(pConnection);
		return false;
	}
	return true;
}



/* 单步推进当前响应输出；后续租约由释放回调重新投递。 */
void __xrtHttpConnDriveOutput(xhttpconn* pConnection)
{
	xhttp1serveroutputstatus Status;
	xbytesview Data;
	xnetresult Result;

	if ( pConnection == NULL ) {
		return;
	}
	if ( pConnection->OutputDriving ) {
		pConnection->OutputPending = true;
		return;
	}
	pConnection->OutputDriving = true;
	pConnection->OutputPending = false;
	if ( (pConnection->Response == NULL) ||
		pConnection->OutputQueued ||
		#if defined(XRT_FEATURE_HTTP_SERVER_BODY_ASYNC)
			pConnection->BodyWaiting ||
		#endif
		(xrtHttpConnState(pConnection) ==
		 XHTTP_CONN_CLOSING) ||
		(xrtHttpConnState(pConnection) ==
		 XHTTP_CONN_CLOSED) ) {
		goto Finish;
	}
	#if defined(XRT_FEATURE_HTTP_SERVER_TLS)
		if ( pConnection->OutputDraining ) {
			xtlsstream* pStream = (xtlsstream*)
				xrtAtomicPtrLoad(
					&pConnection->Transport,
					XMEMORY_ACQUIRE
				);

			if ( xrtTlsStreamPending(pStream) != 0 ) {
				goto Finish;
			}
			pConnection->OutputDraining = false;
			pConnection->WritePending = 0;
			__xrtHttpConnResponseDone(pConnection);
			goto Finish;
		}
	#endif
	Status = xrtHttp1ServerResponseOutput(
		pConnection->Response,
		pConnection->Server->Config.WriteSize,
		&Data
	);
	if ( Status == XHTTP1_SERVER_OUTPUT_DATA ) {
		Result = __xrtHttpConnSendOutput(
			pConnection, Data
		);
		if ( (Result != XNET_RESULT_OK) &&
			(Result != XNET_RESULT_AGAIN) ) {
			__xrtHttpConnRememberError(
				pConnection,
				Result == XNET_RESULT_CLOSED ?
					XERR_CLOSED : XERR_IO,
				XHTTP_SERVER_ERROR_CONNECTION,
				"send-http-response",
				"HTTP response could not enter the transport send queue",
				xrtGetError()
			);
			__xrtHttpConnEmitError(pConnection);
			(void)xrtHttpConnAbort(pConnection);
		}
	} else if ( Status == XHTTP1_SERVER_OUTPUT_DONE ) {
		#if defined(XRT_FEATURE_HTTP_SERVER_TLS)
			if ( pConnection->TransportKind ==
				XRT_HTTP_CONN_TRANSPORT_TLS ) {
				xtlsstream* pStream = (xtlsstream*)
					xrtAtomicPtrLoad(
						&pConnection->Transport,
						XMEMORY_ACQUIRE
					);
				size_t iPending =
					xrtTlsStreamPending(pStream);

				if ( iPending != 0 ) {
					pConnection->WritePending =
						iPending;
					pConnection->OutputDraining =
						true;
					goto Finish;
				}
			}
		#endif
		__xrtHttpConnResponseDone(pConnection);
	} else if ( Status == XHTTP1_SERVER_OUTPUT_AGAIN ) {
		#if defined(XRT_FEATURE_HTTP_SERVER_BODY_ASYNC)
			if ( __xrtHttpConnBodyWait(pConnection) ) {
				goto Finish;
			}
			__xrtHttpConnRememberError(
				pConnection,
				__xrtHttpServerCauseKind(
					xrtGetError(),
					XERR_IO
				),
				XHTTP_SERVER_ERROR_RESPONSE,
				"read-http-response-body",
				"HTTP response body readiness wait could not be installed",
				xrtGetError()
			);
		#else
			__xrtHttpConnRememberError(
				pConnection,
				XERR_UNSUPPORTED,
				XHTTP_SERVER_ERROR_RESPONSE,
				"read-http-response-body",
				"HTTP response body requires the async server body layer",
				NULL
			);
		#endif
		__xrtHttpConnEmitError(pConnection);
		(void)xrtHttpConnAbort(pConnection);
	} else if ( Status == XHTTP1_SERVER_OUTPUT_TUNNEL ) {
		#if defined(XRT_FEATURE_HTTP_SERVER_UPGRADE)
			__xrtHttpConnUpgradeFinish(pConnection);
		#else
			__xrtHttpConnRememberError(
				pConnection,
				XERR_UNSUPPORTED,
				XHTTP_SERVER_ERROR_RESPONSE,
				"upgrade-http-connection",
				"HTTP tunnel reached a server without Upgrade support",
				NULL
			);
			__xrtHttpConnEmitError(pConnection);
			(void)xrtHttpConnAbort(pConnection);
		#endif
	} else {
		__xrtHttpConnRememberError(
			pConnection,
			XERR_IO,
			XHTTP_SERVER_ERROR_RESPONSE,
			"write-http-response",
			"HTTP response output failed",
			xrtHttp1ServerResponseError(
				pConnection->Response
			)
		);
		__xrtHttpConnEmitError(pConnection);
		(void)xrtHttpConnAbort(pConnection);
	}

Finish:
	pConnection->OutputDriving = false;
	if ( pConnection->OutputPending &&
		!pConnection->OutputQueued &&
		(xrtHttpConnState(pConnection) !=
		 XHTTP_CONN_CLOSING) &&
		(xrtHttpConnState(pConnection) !=
		 XHTTP_CONN_CLOSED) &&
		!__xrtHttpConnPostOutput(pConnection) ) {
		__xrtHttpConnRememberError(
			pConnection,
			XERR_INTERNAL,
			XHTTP_SERVER_ERROR_INTERNAL,
			"continue-http-response-output",
			"HTTP response output could not yield to its Worker",
			xrtGetError()
		);
		__xrtHttpConnEmitError(pConnection);
		(void)xrtHttpConnAbort(pConnection);
	}
}



/* 保存协议错误并尽力发送一个关闭连接的固定状态响应。 */
void __xrtHttpConnProtocolFail(
	xhttpconn* pConnection,
	uint16 iStatus,
	xhttpservererror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	xstrview Reason;

	if ( (pConnection == NULL) ||
		(xrtHttpConnState(pConnection) ==
		 XHTTP_CONN_CLOSING) ||
		(xrtHttpConnState(pConnection) ==
		 XHTTP_CONN_CLOSED) ) {
		return;
	}
	__xrtHttpConnRememberError(
		pConnection,
		Code >= XHTTP_SERVER_ERROR_TIMEOUT_HEADER &&
		Code <= XHTTP_SERVER_ERROR_TIMEOUT_WRITE ?
			XERR_TIMEOUT : XERR_PROTOCOL,
		Code,
		sOperation,
		sMessage,
		pCause
	);
	(void)xrtAtomic64FetchAdd(
		&pConnection->Server->ProtocolErrors,
		1,
		XMEMORY_RELAXED
	);
	__xrtHttpConnEmitError(pConnection);
	pConnection->ForceClose = true;
	if ( xrtAtomic32Load(
		&pConnection->FinalGate,
		XMEMORY_ACQUIRE
	) || (pConnection->Response != NULL) ) {
		(void)xrtHttpConnAbort(pConnection);
		return;
	}
	Reason = xrtHttpStatusText(iStatus);
	if ( __xrtHttpConnSubmitFixed(
		pConnection,
		iStatus,
		XRT_STR_LITERAL("text/plain; charset=utf-8"),
		(xbytesview){
			(cbytes)Reason.Data,
			Reason.Size
		},
		"respond-http-protocol-error",
		false
	) != XNET_RESULT_OK ) {
		(void)xrtHttpConnAbort(pConnection);
	}
}

#endif
