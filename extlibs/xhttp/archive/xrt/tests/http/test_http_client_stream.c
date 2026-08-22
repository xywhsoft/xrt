#include "../test.h"

#if defined(TEST_HTTP_CLIENT_STREAM_OOM) || \
	defined(TEST_HTTP_CLIENT_STREAM_BODY_SPURIOUS)
	#include "../../src/internal/xrt_http_client_stream.h"
#endif



#if !defined(TEST_HTTP_CLIENT_STREAM_BACKEND)
	#define TEST_HTTP_CLIENT_STREAM_BACKEND XNET_PORT_SELECT
	#define TEST_HTTP_CLIENT_STREAM_BACKEND_NAME "select"
#endif

#if !defined(TEST_HTTP_CLIENT_STREAM_RESPONSE)
	#define TEST_HTTP_CLIENT_STREAM_RESPONSE \
		"HTTP/1.1 200 OK\r\n" \
		"Content-Length: 2\r\n" \
		"Connection: keep-alive\r\n" \
		"\r\n" \
		"OK"
	#define TEST_HTTP_CLIENT_STREAM_STATUS 200
	#define TEST_HTTP_CLIENT_STREAM_BODY "OK"
	#define TEST_HTTP_CLIENT_STREAM_REMAINDER ""
	#define TEST_HTTP_CLIENT_STREAM_REUSABLE 1
	#define TEST_HTTP_CLIENT_STREAM_UPGRADED 0
	#define TEST_HTTP_CLIENT_STREAM_SCENARIO "response"
#endif

#if !defined(TEST_HTTP_CLIENT_STREAM_CANCEL)
	#define TEST_HTTP_CLIENT_STREAM_CANCEL 0
#endif

#if !defined(TEST_HTTP_CLIENT_STREAM_CANCEL_PROGRESS)
	#define TEST_HTTP_CLIENT_STREAM_CANCEL_PROGRESS 0
#endif

#if !defined(TEST_HTTP_CLIENT_STREAM_RESUME)
	#define TEST_HTTP_CLIENT_STREAM_RESUME 0
#endif

#if !defined(TEST_HTTP_CLIENT_STREAM_BODY_TERMINAL)
	#define TEST_HTTP_CLIENT_STREAM_BODY_TERMINAL 0
#endif

#if !defined(TEST_HTTP_CLIENT_STREAM_BODY_IMMEDIATE)
	#define TEST_HTTP_CLIENT_STREAM_BODY_IMMEDIATE 0
#endif

#if !defined(TEST_HTTP_CLIENT_STREAM_BODY_SPURIOUS)
	#define TEST_HTTP_CLIENT_STREAM_BODY_SPURIOUS 0
#endif

#if !defined(TEST_HTTP_CLIENT_STREAM_SERVER_CLOSE)
	#define TEST_HTTP_CLIENT_STREAM_SERVER_CLOSE 0
#endif

#if !defined(TEST_HTTP_CLIENT_STREAM_EARLY_FINAL)
	#define TEST_HTTP_CLIENT_STREAM_EARLY_FINAL 0
#endif

#if !defined(TEST_HTTP_CLIENT_STREAM_REQUEST_DONE)
	#define TEST_HTTP_CLIENT_STREAM_REQUEST_DONE 1
#endif

#if !defined(TEST_HTTP_CLIENT_STREAM_INPUT_PAUSE)
	#define TEST_HTTP_CLIENT_STREAM_INPUT_PAUSE 0
#endif

#define TEST_HTTP_CLIENT_STREAM_SUCCEEDS \
	(!TEST_HTTP_CLIENT_STREAM_CANCEL && \
	 (!TEST_HTTP_CLIENT_STREAM_RESUME || \
	  (TEST_HTTP_CLIENT_STREAM_BODY_TERMINAL == 0)))

#define TEST_HTTP_CLIENT_STREAM_TRANSFERS \
	(TEST_HTTP_CLIENT_STREAM_SUCCEEDS && \
	 (TEST_HTTP_CLIENT_STREAM_REUSABLE || \
	  TEST_HTTP_CLIENT_STREAM_UPGRADED))



typedef struct test_http_client_stream {
	xnetengine* Engine;
	xnetlistener* Listener;
	xnetstream* Client;
	xnetstream* Server;
	xnetstream* Returned;
	xhttp1exchange* Exchange;
	xhttp1call* Call;
	xhttpresponse* Response;
	xatomic32 Accepted;
	xatomic32 Started;
	xatomic32 Completed;
	xatomic32 ServerClosed;
	xatomic32 BodyStep;
	xatomic32 BodyAgain;
	xatomic32 BodyClosed;
	xatomic32 RequestDone;
	xatomic32 ProgressCancelled;
	xatomic64 Written;
	xatomic64 Read;
	#if TEST_HTTP_CLIENT_STREAM_INPUT_PAUSE
		xatomic32 InputPaused;
		xatomic32 InputCalls;
		char InputBody[32];
		size_t InputSize;
	#endif
	#if TEST_HTTP_CLIENT_STREAM_RESUME
		xpromise* BodyPromise;
		xfuture* BodyFuture;
		xatomic32 BodyWaited;
	#endif
	#if defined(TEST_HTTP_CLIENT_STREAM_OOM)
		xatomic32 AllocFail;
		ptr AllocBlocks[4096];
		size_t AllocBlockCount;
	#endif
	bool Responded;
} test_http_client_stream;



#if defined(TEST_HTTP_CLIENT_STREAM_OOM)

/* 正常阶段转发到底层堆，故障阶段拒绝新的 backing span。 */
static ptr testHttpClientStreamOomAlloc(
	ptr pData,
	size_t iSize
)
{
	test_http_client_stream* pState =
		(test_http_client_stream*)pData;

	return xrtAtomic32Load(
		&pState->AllocFail,
		XMEMORY_ACQUIRE
	) ? NULL : malloc(iSize);
}



/* 调用对象测试不依赖重分配，仍保持完整分配器语义。 */
static ptr testHttpClientStreamOomRealloc(
	ptr pData,
	ptr pMemory,
	size_t iSize
)
{
	test_http_client_stream* pState =
		(test_http_client_stream*)pData;

	return xrtAtomic32Load(
		&pState->AllocFail,
		XMEMORY_ACQUIRE
	) ? NULL : realloc(pMemory, iSize);
}



/* 释放故障前已经取得的底层内存。 */
static void testHttpClientStreamOomFree(
	ptr pData,
	ptr pMemory
)
{
	(void)pData;
	free(pMemory);
}

#endif



/* 在截止时间前等待 HTTP 调用完成。 */
static void testHttpClientStreamWait(
	xatomic32* pValue,
	cstr sMessage
)
{
	xdeadline iDeadline = xrtDeadlineAfter(5000000u);

	while ( xrtAtomic32Load(
		pValue,
		XMEMORY_ACQUIRE
	) == 0 ) {
		testRequire(
			!xrtDeadlineExpired(iDeadline),
			sMessage
		);
		xrtThreadYield();
	}
}



/* 服务端收到完整请求后返回一条可复用的固定长度响应。 */
static void testHttpClientStreamServerRead(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	ptr pData
)
{
	static const char Response[] =
		TEST_HTTP_CLIENT_STREAM_RESPONSE;
	char Request[1024];
	test_http_client_stream* pState =
		(test_http_client_stream*)pData;
	size_t iSize = xrtNetBufSize(pBuffer);
	size_t i;
	bool bComplete = false;

	testRequire(iSize != 0,
		"HTTP test server received an empty request");
	testRequire(iSize < sizeof(Request),
		"HTTP test request exceeded the fixture limit");
	testRequire(xrtNetBufPeek(
		pBuffer,
		0,
		Request,
		iSize
	) == iSize, "HTTP test server request peek failed");
	for ( i = 3; i < iSize; ++i ) {
		if ( (Request[i - 3] == '\r') &&
			(Request[i - 2] == '\n') &&
			(Request[i - 1] == '\r') &&
			(Request[i] == '\n') ) {
			bComplete = true;
			break;
		}
	}
	if ( !bComplete ) {
		return;
	}
	#if TEST_HTTP_CLIENT_STREAM_RESUME
		#if !TEST_HTTP_CLIENT_STREAM_EARLY_FINAL
			if ( iSize < ((i + 1u) + 5u) ) {
				return;
			}
			testRequire(memcmp(
				Request + iSize - 5u,
				"ready",
				5u
			) == 0, "HTTP resumed request body mismatch");
		#endif
	#endif
	testRequire(!pState->Responded,
		"HTTP test server sent more than one response");
	pState->Responded = true;
	testRequire(xrtNetBufConsume(
		pBuffer,
		iSize
	) == iSize, "HTTP test server request consume failed");
	if ( sizeof(Response) > 1u ) {
		testRequire(xrtNetStreamSend(
			pStream,
			Response,
			sizeof(Response) - 1u
		) == XNET_RESULT_OK,
			"HTTP test server response send failed");
	}
	#if TEST_HTTP_CLIENT_STREAM_SERVER_CLOSE
		testRequire(
			xrtNetStreamClose(pStream),
			"HTTP test server response close failed"
		);
	#endif
}



/* 记录服务端 Stream 已经关闭。 */
static void testHttpClientStreamServerClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_http_client_stream* pState =
		(test_http_client_stream*)pData;

	(void)pStream;
	(void)Result;
	(void)pError;
	xrtAtomic32Store(
		&pState->ServerClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* 接管服务端连接并安装最小 HTTP 测试处理器。 */
static bool testHttpClientStreamAccept(
	xnetlistener* pListener,
	xnetstream* pStream,
	ptr pData
)
{
	test_http_client_stream* pState =
		(test_http_client_stream*)pData;
	xnetstreamevents Events;

	(void)pListener;
	memset(&Events, 0, sizeof(Events));
	Events.Read = testHttpClientStreamServerRead;
	Events.Close = testHttpClientStreamServerClose;
	testRequire(xrtNetStreamSetEvents(
		pStream,
		&Events,
		pState
	), "HTTP test server event takeover failed");
	pState->Server = pStream;
	xrtAtomic32Store(
		&pState->Accepted,
		1,
		XMEMORY_RELEASE
	);
	return true;
}



/* 接收调用结果及其可复用 TCP 引用。 */
static void testHttpClientStreamDone(
	xhttp1call* pCall,
	const xhttp1callresult* pResult,
	ptr pData
)
{
	test_http_client_stream* pState =
		(test_http_client_stream*)pData;
	static const char Remainder[] =
		TEST_HTTP_CLIENT_STREAM_REMAINDER;
	char Buffered[sizeof(Remainder)];
	const xnetbuf* pBuffer;

	testRequire(pCall == pState->Call,
		"HTTP call callback identity mismatch");
	#if TEST_HTTP_CLIENT_STREAM_CANCEL
		testRequire(
			(pResult != NULL) &&
			(pResult->Result == XNET_RESULT_CANCELLED) &&
			(pResult->Response == NULL) &&
			(pResult->Tcp == NULL) &&
			(pResult->Error != NULL) &&
			(xrtErrorKind(pResult->Error) ==
			 XERR_CANCELLED) &&
			(pResult->Buffered == 0) &&
			!pResult->Reusable &&
			!pResult->Upgraded,
			"HTTP cancelled call result mismatch"
		);
		xrtAtomic32Store(
			&pState->Completed,
			1,
			XMEMORY_RELEASE
		);
		return;
	#endif
	#if TEST_HTTP_CLIENT_STREAM_RESUME && \
		(TEST_HTTP_CLIENT_STREAM_BODY_TERMINAL != 0)
		testRequire(
			(pResult != NULL) &&
			(pResult->Result == XNET_RESULT_ERROR) &&
			(pResult->Response == NULL) &&
			(pResult->Tcp == NULL) &&
			(pResult->Error != NULL) &&
			(xrtErrorKind(pResult->Error) ==
				#if TEST_HTTP_CLIENT_STREAM_BODY_TERMINAL == 1
					XERR_IO
				#else
					XERR_CANCELLED
				#endif
			) &&
			(pResult->Buffered == 0) &&
			!pResult->Reusable &&
			!pResult->Upgraded,
			"HTTP body Future failure result mismatch"
		);
		xrtAtomic32Store(
			&pState->Completed,
			1,
			XMEMORY_RELEASE
		);
		return;
	#endif
	testRequire(
		(pResult != NULL) &&
		(pResult->Result == XNET_RESULT_OK) &&
		(pResult->Error == NULL) &&
		(pResult->Response != NULL) &&
		((pResult->Tcp != NULL) ==
		 (TEST_HTTP_CLIENT_STREAM_TRANSFERS != 0)) &&
		(pResult->Reusable ==
		 (TEST_HTTP_CLIENT_STREAM_REUSABLE != 0)) &&
		(pResult->Upgraded ==
		 (TEST_HTTP_CLIENT_STREAM_UPGRADED != 0)) &&
		(pResult->Buffered == (sizeof(Remainder) - 1u)),
		"HTTP TCP call result mismatch"
	);
	if ( pResult->Upgraded ) {
		testRequire(xrtNetStreamSetEvents(
			pResult->Tcp,
			NULL,
			pState
		), "HTTP upgraded stream event takeover failed");
	}
	if ( pResult->Tcp != NULL ) {
		pBuffer = xrtNetStreamBuffer(pResult->Tcp);
		testRequire((pBuffer != NULL) &&
			(xrtNetBufSize(pBuffer) ==
			 (sizeof(Remainder) - 1u)),
			"HTTP returned stream buffer size mismatch");
		if ( sizeof(Remainder) > 1u ) {
			testRequire(
				(xrtNetBufPeek(
					pBuffer,
					0,
					Buffered,
					sizeof(Remainder) - 1u
				) == (sizeof(Remainder) - 1u)) &&
				(memcmp(
					Buffered,
					Remainder,
					sizeof(Remainder) - 1u
				) == 0),
				"HTTP upgraded protocol remainder mismatch"
			);
		}
	}
	pState->Response = pResult->Response;
	pState->Returned = pResult->Tcp;
	xrtAtomic32Store(
		&pState->Completed,
		1,
		XMEMORY_RELEASE
	);
}



/* 验证低级调用只报告已经接受的真实 I/O 进度。 */
static void testHttpClientStreamProgress(
	xhttp1call* pCall,
	xhttp1progress Progress,
	size_t iBytes,
	ptr pData
)
{
	test_http_client_stream* pState =
		(test_http_client_stream*)pData;

	testRequire(pCall == pState->Call,
		"HTTP progress callback identity mismatch");
	if ( Progress == XHTTP1_PROGRESS_WRITE ) {
		testRequire(
			(iBytes != 0) &&
			(xrtAtomic32Load(
				&pState->RequestDone,
				XMEMORY_ACQUIRE
			) == 0),
			"HTTP write progress boundary mismatch"
		);
		(void)xrtAtomic64FetchAdd(
			&pState->Written,
			(uint64)iBytes,
			XMEMORY_RELAXED
		);
		#if TEST_HTTP_CLIENT_STREAM_CANCEL_PROGRESS
			if ( xrtAtomic32Load(
				&pState->ProgressCancelled,
				XMEMORY_ACQUIRE
			) == 0 ) {
				testRequire(
					xrtHttp1CallCancel(pCall),
					"HTTP progress callback cancellation failed"
				);
				xrtAtomic32Store(
					&pState->ProgressCancelled,
					1,
					XMEMORY_RELEASE
				);
			}
		#endif
	} else if ( Progress == XHTTP1_PROGRESS_READ ) {
		testRequire(iBytes != 0,
			"HTTP read progress reported zero bytes");
		(void)xrtAtomic64FetchAdd(
			&pState->Read,
			(uint64)iBytes,
			XMEMORY_RELAXED
		);
	} else {
		testRequire(
			(Progress == XHTTP1_PROGRESS_REQUEST_DONE) &&
			(iBytes == 0) &&
			(xrtAtomic32FetchAdd(
				&pState->RequestDone,
				1,
				XMEMORY_ACQ_REL
			) == 0),
			"HTTP request completion progress was not unique"
		);
	}
}



/* TCP Open 后把已经准备好的 Exchange 转移给公开调用驱动器。 */
static void testHttpClientStreamOpen(
	xnetstream* pStream,
	ptr pData
)
{
	test_http_client_stream* pState =
		(test_http_client_stream*)pData;
	uint8 CallConfigStorage[sizeof(xhttp1callconfig) + 2u];
	uint8 CallEventsStorage[sizeof(xhttp1callevents) + 2u];
	xhttp1callevents CallEvents;

	xrtHttp1CallEventsInit(&CallEvents);
	CallEvents.Done = testHttpClientStreamDone;
	CallEvents.Progress = testHttpClientStreamProgress;
	CallEvents.Data = pState;

	#if defined(TEST_HTTP_CLIENT_STREAM_OOM)
		{
			xhttp1callconfig InvalidConfig;
			xerror* pOldError = xrtErrorCreate(
				XERR_VALUE,
				"test.old",
				19,
				"stale error"
			);

			testRequire(
				pOldError != NULL,
				"HTTP call stale error setup failed"
			);
			xrtSetError(pOldError);
			xrtErrorFree(pOldError);
			xrtHttp1CallConfigInit(&InvalidConfig);
			InvalidConfig.WriteSize = 0;
			testRequire(
				(xrtHttp1CallTcp(
					pStream,
					pState->Exchange,
					&InvalidConfig,
					&CallEvents
				) == NULL) &&
				(xrtGetError() != NULL) &&
				(xrtErrorKind(xrtGetError()) ==
				 XERR_ARGUMENT) &&
				(strcmp(
					xrtErrorDomain(xrtGetError()),
					"xrt.http.call"
				) == 0),
				"HTTP call invalid config kept a stale error"
			);
		}
		xrtAtomic32Store(
			&pState->AllocFail,
			1,
			XMEMORY_RELEASE
		);
		while ( pState->AllocBlockCount <
			(sizeof(pState->AllocBlocks) /
			 sizeof(pState->AllocBlocks[0])) ) {
			ptr pBlock = xrtCalloc(
				1,
				sizeof(xhttp1call)
			);

			if ( pBlock == NULL ) {
				break;
			}
			pState->AllocBlocks[
				pState->AllocBlockCount++
			] = pBlock;
		}
		testRequire(
			pState->AllocBlockCount <
				(sizeof(pState->AllocBlocks) /
				 sizeof(pState->AllocBlocks[0])),
			"HTTP call OOM size class did not exhaust"
		);
		xrtClearError();
		testRequire(
			(xrtHttp1CallTcp(
				pStream,
				pState->Exchange,
				NULL,
				&CallEvents
			) == NULL) &&
			(xrtGetError() != NULL) &&
			(xrtErrorKind(xrtGetError()) ==
			 XERR_MEMORY) &&
			(xrtNetStreamState(pStream) ==
			 XNET_STREAM_OPEN),
			"HTTP call constructor OOM changed inputs"
		);
		xrtAtomic32Store(
			&pState->AllocFail,
			0,
			XMEMORY_RELEASE
		);
		for ( size_t i = 0;
			i < pState->AllocBlockCount;
			++i ) {
			xrtFree(pState->AllocBlocks[i]);
		}
		pState->AllocBlockCount = 0;
		xrtClearError();
	#endif
	memset(CallConfigStorage, 0xA5, sizeof(CallConfigStorage));
	xrtHttp1CallConfigInit((xhttp1callconfig*)(void*)(
		CallConfigStorage + 1u
	));
	memset(CallEventsStorage, 0xA5, sizeof(CallEventsStorage));
	memcpy(CallEventsStorage + 1u, &CallEvents, sizeof(CallEvents));
	pState->Call = xrtHttp1CallTcp(
		pStream,
		pState->Exchange,
		(const xhttp1callconfig*)(const void*)(
			CallConfigStorage + 1u
		),
		(const xhttp1callevents*)(const void*)(
			CallEventsStorage + 1u
		)
	);
	testRequire((pState->Call != NULL) &&
		(CallConfigStorage[0] == 0xA5) &&
		(CallConfigStorage[sizeof(CallConfigStorage) - 1u] == 0xA5) &&
		(CallEventsStorage[0] == 0xA5) &&
		(CallEventsStorage[sizeof(CallEventsStorage) - 1u] == 0xA5),
		"HTTP TCP call creation failed");
	pState->Exchange = NULL;
	pState->Client = NULL;
	xrtAtomic32Store(
		&pState->Started,
		1,
		XMEMORY_RELEASE
	);
}



#if TEST_HTTP_CLIENT_STREAM_RESUME

/* 静态异步正文片段不需要回收。 */
static void testHttpClientStreamBodyRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	(void)pContext;
	(void)pData;
	(void)iSize;
}



/* 第一次返回 AGAIN，发布后返回固定正文，再结束 Reader。 */
static xhttpbodystatus testHttpClientStreamBodyNext(
	ptr pContext,
	size_t iMaximum,
	xhttpbodychunk* pChunk
)
{
	test_http_client_stream* pState =
		(test_http_client_stream*)pContext;
	uint32 iStep = xrtAtomic32Load(
		&pState->BodyStep,
		XMEMORY_ACQUIRE
	);

	(void)iMaximum;
	if ( iStep == 0 ) {
		uint32 iAgain = xrtAtomic32FetchAdd(
			&pState->BodyAgain,
			1,
			XMEMORY_ACQ_REL
		);

		#if TEST_HTTP_CLIENT_STREAM_BODY_SPURIOUS
			if ( iAgain != 0 ) {
				xrtAtomic32Store(
					&pState->BodyStep,
					1,
					XMEMORY_RELEASE
				);
			}
		#else
			(void)iAgain;
		#endif
		return XHTTP_BODY_AGAIN;
	}
	if ( iStep == 1 ) {
		xrtAtomic32Store(
			&pState->BodyStep,
			2,
			XMEMORY_RELEASE
		);
		pChunk->Data = (cbytes)"ready";
		pChunk->Size = 5;
		pChunk->Release =
			testHttpClientStreamBodyRelease;
		return XHTTP_BODY_DATA;
	}
	return XHTTP_BODY_EOF;
}



/* 返回测试持有的可读性 Future，允许完成与订阅发生竞态。 */
static xfuture* testHttpClientStreamBodyWait(ptr pContext)
{
	test_http_client_stream* pState =
		(test_http_client_stream*)pContext;
	uint32 iWait = xrtAtomic32FetchAdd(
		&pState->BodyWaited,
		1,
		XMEMORY_ACQ_REL
	);

	#if TEST_HTTP_CLIENT_STREAM_BODY_SPURIOUS
		if ( iWait != 0 ) {
			testRequire(
				!pState->Call->OutputWaiter.Calling,
				"HTTP body waiter was reused inside its completion callback"
			);
		}
	#else
		(void)iWait;
	#endif
	#if TEST_HTTP_CLIENT_STREAM_BODY_IMMEDIATE
		if ( iWait == 0 ) {
			xrtAtomic32Store(
				&pState->BodyStep,
				1,
				XMEMORY_RELEASE
			);
			testRequire(
				xrtPromiseResolve(
					pState->BodyPromise,
					NULL
				),
				"HTTP immediate body Future completion failed"
			);
		}
	#endif
	return xrtFutureRef(pState->BodyFuture);
}



#if TEST_HTTP_CLIENT_STREAM_BODY_SPURIOUS

/* 在 Stream Worker 上完成第一次可读通知，覆盖 waiter 回调重入边界。 */
static void testHttpClientStreamBodyReadyTask(
	xnetworker* pWorker,
	ptr pData
)
{
	test_http_client_stream* pState =
		(test_http_client_stream*)pData;

	(void)pWorker;
	testRequire(
		xrtPromiseResolve(pState->BodyPromise, NULL),
		"HTTP spurious body Future completion failed"
	);
}

#endif



/* 记录异步请求正文 Reader 的唯一关闭。 */
static void testHttpClientStreamBodyClose(ptr pContext)
{
	test_http_client_stream* pState =
		(test_http_client_stream*)pContext;

	(void)xrtAtomic32FetchAdd(
		&pState->BodyClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* 为单次测试打开共享状态对应的 Reader。 */
static bool testHttpClientStreamBodyOpen(
	ptr pFactory,
	xhttpbodyreaderops* pOps,
	ptr* ppReader
)
{
	memset(pOps, 0, sizeof(*pOps));
	pOps->Next = testHttpClientStreamBodyNext;
	pOps->Close = testHttpClientStreamBodyClose;
	pOps->Wait = testHttpClientStreamBodyWait;
	*ppReader = pFactory;
	return true;
}

#endif



#if TEST_HTTP_CLIENT_STREAM_INPUT_PAUSE

/* 收集流式响应，并在第一片正文回调内立即暂停 Call 输入。 */
static bool testHttpClientStreamInputBody(
	const xhttpresponse* pResponse,
	xbytesview Data,
	ptr pData
)
{
	test_http_client_stream* pState =
		(test_http_client_stream*)pData;
	uint32 iCall;

	(void)pResponse;
	testRequire(
		Data.Size <= (sizeof(pState->InputBody) - pState->InputSize),
		"HTTP paused input fixture overflow"
	);
	memcpy(
		pState->InputBody + pState->InputSize,
		Data.Data,
		Data.Size
	);
	pState->InputSize += Data.Size;
	iCall = xrtAtomic32FetchAdd(
		&pState->InputCalls,
		1,
		XMEMORY_ACQ_REL
	);
	if ( iCall == 0 ) {
		testRequire(
			xrtHttp1CallPause(pState->Call) &&
			xrtHttp1CallPause(pState->Call) &&
			xrtHttp1CallPaused(pState->Call),
			"HTTP call did not pause inside Body callback"
		);
		xrtAtomic32Store(
			&pState->InputPaused,
			1,
			XMEMORY_RELEASE
		);
	}
	return true;
}

#endif



/* 创建一条无正文 GET Exchange。 */
static xhttp1exchange* testHttpClientStreamExchange(
	test_http_client_stream* pState
)
{
	xhttprequest* pRequest = xrtHttpRequestCreate(
		#if TEST_HTTP_CLIENT_STREAM_RESUME
			XRT_STR_LITERAL("POST"),
		#else
			XRT_STR_LITERAL("GET"),
		#endif
		XRT_STR_LITERAL("http://127.0.0.1/test")
	);
	xhttp1requestplan* pPlan;
	xhttp1exchange* pExchange;
	#if TEST_HTTP_CLIENT_STREAM_INPUT_PAUSE
		xhttp1exchangeevents Events;
	#endif

	if ( pRequest == NULL ) {
		return NULL;
	}
	#if TEST_HTTP_CLIENT_STREAM_RESUME
		{
			static const xhttpbodyops Ops = {
				testHttpClientStreamBodyOpen,
				NULL
			};
			xhttpbody* pBody = xrtHttpBodyCreate(
				&Ops,
				pState,
				5,
				XHTTP_BODY_REPLAYABLE
			);

			if ( (pBody == NULL) ||
				!xrtHttpRequestSetBody(
					pRequest,
					pBody
				) ) {
				xrtHttpBodyDestroy(pBody);
				xrtHttpRequestDestroy(pRequest);
				return NULL;
			}
			xrtHttpBodyDestroy(pBody);
		}
	#else
		(void)pState;
	#endif
	#if TEST_HTTP_CLIENT_STREAM_UPGRADED
		if ( !xrtHttpRequestSetHeader(
			pRequest,
			XRT_STR_LITERAL("Connection"),
			XRT_STR_LITERAL("Upgrade")
		) || !xrtHttpRequestSetHeader(
			pRequest,
			XRT_STR_LITERAL("Upgrade"),
			XRT_STR_LITERAL("websocket")
		) ) {
			xrtHttpRequestDestroy(pRequest);
			return NULL;
		}
	#endif
	pPlan = xrtHttp1RequestPrepare(pRequest, NULL);
	xrtHttpRequestDestroy(pRequest);
	if ( pPlan == NULL ) {
		return NULL;
	}
	#if TEST_HTTP_CLIENT_STREAM_INPUT_PAUSE
		memset(&Events, 0, sizeof(Events));
		Events.Body = testHttpClientStreamInputBody;
		Events.Data = pState;
	#endif
	pExchange = xrtHttp1ExchangeCreate(
		pPlan,
		NULL,
		#if TEST_HTTP_CLIENT_STREAM_INPUT_PAUSE
			&Events
		#else
			NULL
		#endif
	);
	if ( pExchange == NULL ) {
		xrtHttp1RequestPlanDestroy(pPlan);
	}
	return pExchange;
}



/* 验证 TCP 驱动、响应所有权和可复用连接归还。 */
int main(void)
{
	test_http_client_stream State;
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetlistenerevents ListenerEvents;
	xnetstreamevents ClientEvents;
	xnetaddr Address;
	xdeadline iDeadline;

	memset(&State, 0, sizeof(State));
	xrtAtomic32Init(&State.RequestDone, 0);
	xrtAtomic32Init(&State.ProgressCancelled, 0);
	xrtAtomic64Init(&State.Written, 0);
	xrtAtomic64Init(&State.Read, 0);
	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	memset(&ClientEvents, 0, sizeof(ClientEvents));
	xrtAtomic32Init(&State.Accepted, 0);
	xrtAtomic32Init(&State.Started, 0);
	xrtAtomic32Init(&State.Completed, 0);
	xrtAtomic32Init(&State.ServerClosed, 0);
	xrtAtomic32Init(&State.BodyStep, 0);
	xrtAtomic32Init(&State.BodyAgain, 0);
	xrtAtomic32Init(&State.BodyClosed, 0);
	#if TEST_HTTP_CLIENT_STREAM_INPUT_PAUSE
		xrtAtomic32Init(&State.InputPaused, 0);
		xrtAtomic32Init(&State.InputCalls, 0);
	#endif
	#if defined(TEST_HTTP_CLIENT_STREAM_OOM)
		{
			xallocator Allocator;

			xrtAtomic32Init(&State.AllocFail, 0);
			Allocator.Context = &State;
			Allocator.Alloc =
				testHttpClientStreamOomAlloc;
			Allocator.Realloc =
				testHttpClientStreamOomRealloc;
			Allocator.Free =
				testHttpClientStreamOomFree;
			testRequire(
				xrtSetAllocator(&Allocator),
				"HTTP call OOM allocator install failed"
			);
		}
	#endif
	#if TEST_HTTP_CLIENT_STREAM_RESUME
		xrtAtomic32Init(&State.BodyWaited, 0);
		State.BodyPromise = xrtPromiseCreate(
			&State.BodyFuture,
			NULL
		);
		testRequire(
			(State.BodyPromise != NULL) &&
			(State.BodyFuture != NULL),
			"HTTP request body Future creation failed"
		);
	#endif
	State.Exchange = testHttpClientStreamExchange(&State);
	testRequire(State.Exchange != NULL,
		"HTTP test Exchange creation failed");

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_HTTP_CLIENT_STREAM_BACKEND;
	EngineConfig.Workers = 1;
	State.Engine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(State.Engine != NULL) &&
		xrtNetEngineStart(State.Engine),
		"HTTP test Engine start failed"
	);

	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	), "HTTP test loopback address failed");
	ListenerEvents.Accept = testHttpClientStreamAccept;
	State.Listener = xrtNetListen(
		State.Engine,
		&ListenConfig,
		&ListenerEvents,
		NULL,
		&State
	);
	testRequire(State.Listener != NULL,
		"HTTP test Listener creation failed");
	testRequire(xrtNetListenerLocal(
		State.Listener,
		&Address
	), "HTTP test Listener address query failed");

	ClientEvents.Open = testHttpClientStreamOpen;
	State.Client = xrtNetStreamConnect(
		State.Engine,
		&Address,
		0,
		NULL,
		&ClientEvents,
		&State
	);
	testRequire(State.Client != NULL,
		"HTTP test client connection failed");
	#if TEST_HTTP_CLIENT_STREAM_CANCEL
		testHttpClientStreamWait(
			#if TEST_HTTP_CLIENT_STREAM_CANCEL_PROGRESS
				&State.ProgressCancelled,
				"HTTP progress callback did not cancel the call"
			#elif TEST_HTTP_CLIENT_STREAM_INPUT_PAUSE
				&State.InputPaused,
				"HTTP response input did not pause before cancellation"
			#elif TEST_HTTP_CLIENT_STREAM_RESUME
				&State.BodyAgain,
				"HTTP request body did not pause before cancellation"
			#else
				&State.Started,
				"HTTP TCP call did not start"
			#endif
		);
		#if !TEST_HTTP_CLIENT_STREAM_CANCEL_PROGRESS
			testRequire(xrtHttp1CallCancel(State.Call),
				"HTTP TCP call cancellation failed");
		#endif
		testRequire(!xrtHttp1CallCancel(State.Call),
			"HTTP TCP call accepted duplicate cancellation");
	#endif
	#if TEST_HTTP_CLIENT_STREAM_RESUME && \
		!TEST_HTTP_CLIENT_STREAM_CANCEL && \
		!TEST_HTTP_CLIENT_STREAM_BODY_IMMEDIATE && \
		!TEST_HTTP_CLIENT_STREAM_EARLY_FINAL
		testHttpClientStreamWait(
			&State.BodyAgain,
			"HTTP request body did not pause"
		);
		#if TEST_HTTP_CLIENT_STREAM_BODY_TERMINAL == 0
			#if TEST_HTTP_CLIENT_STREAM_BODY_SPURIOUS
				testRequire(
					xrtNetEnginePost(
						State.Engine,
						0,
						testHttpClientStreamBodyReadyTask,
						&State
					),
					"HTTP body Worker completion post failed"
				);
			#else
				xrtAtomic32Store(
					&State.BodyStep,
					1,
					XMEMORY_RELEASE
				);
				testRequire(xrtPromiseResolve(
					State.BodyPromise,
					NULL
				), "HTTP request body Future completion failed");
			#endif
		#elif TEST_HTTP_CLIENT_STREAM_BODY_TERMINAL == 1
			{
				xerror* pError = xrtErrorCreate(
					XERR_IO,
					"test.http.body",
					71,
					"body readiness failed"
				);

				testRequire(
					(pError != NULL) &&
					xrtPromiseReject(
						State.BodyPromise,
						pError
					),
					"HTTP request body Future rejection failed"
				);
				xrtErrorFree(pError);
			}
		#else
			testRequire(xrtPromiseCancel(
				State.BodyPromise
			), "HTTP request body Future cancellation failed");
		#endif
	#endif
	#if TEST_HTTP_CLIENT_STREAM_INPUT_PAUSE && \
		!TEST_HTTP_CLIENT_STREAM_CANCEL
		testHttpClientStreamWait(
			&State.InputPaused,
			"HTTP response Body callback did not pause input"
		);
		for ( size_t i = 0; i < 1000u; i++ ) {
			xrtThreadYield();
		}
		testRequire(
			(xrtAtomic32Load(
				&State.InputCalls,
				XMEMORY_ACQUIRE
			) == 1u) &&
			(xrtAtomic32Load(
				&State.Completed,
				XMEMORY_ACQUIRE
			) == 0u) &&
			(xrtHttp1CallState(State.Call) ==
			 XHTTP1_CALL_RUNNING) &&
			xrtHttp1CallPaused(State.Call),
			"HTTP call delivered input or completed while paused"
		);
		testRequire(
			!xrtHttp1CallPause(State.Call) &&
			(xrtErrorKind(xrtGetError()) == XERR_STATE),
			"HTTP call accepted Pause outside its Worker"
		);
		xrtClearError();
		testRequire(
			xrtHttp1CallResume(State.Call) &&
			!xrtHttp1CallResume(State.Call) &&
			xrtHttp1CallPaused(State.Call),
			"HTTP call Resume did not coalesce cross-thread requests"
		);
	#endif
	testHttpClientStreamWait(
		&State.Completed,
		"HTTP TCP call did not complete"
	);
	#if TEST_HTTP_CLIENT_STREAM_CANCEL
		testRequire(
			(xrtHttp1CallState(State.Call) ==
			 XHTTP1_CALL_CANCELLED) &&
			!xrtHttp1CallPaused(State.Call) &&
			(xrtHttp1CallError(State.Call) != NULL) &&
			(xrtErrorKind(
				xrtHttp1CallError(State.Call)
			) == XERR_CANCELLED),
			"HTTP cancelled call state mismatch"
		);
		#if TEST_HTTP_CLIENT_STREAM_INPUT_PAUSE
			testRequire(
				xrtAtomic32Load(
					&State.InputCalls,
					XMEMORY_ACQUIRE
				) == 1u,
				"HTTP cancellation consumed paused response input"
			);
		#endif
	#elif TEST_HTTP_CLIENT_STREAM_RESUME && \
		(TEST_HTTP_CLIENT_STREAM_BODY_TERMINAL != 0)
		testRequire(
			(xrtHttp1CallState(State.Call) ==
			 XHTTP1_CALL_FAILED) &&
			(xrtHttp1CallError(State.Call) != NULL) &&
			(xrtErrorKind(
				xrtHttp1CallError(State.Call)
			) ==
				#if TEST_HTTP_CLIENT_STREAM_BODY_TERMINAL == 1
					XERR_IO
				#else
					XERR_CANCELLED
				#endif
			),
			"HTTP body Future failure state mismatch"
		);
	#else
		testRequire(
			(xrtHttpResponseStatus(State.Response) ==
			 TEST_HTTP_CLIENT_STREAM_STATUS) &&
			(xrtHttpResponseSuccess(State.Response) ==
			 ((TEST_HTTP_CLIENT_STREAM_STATUS >= 200) &&
			  (TEST_HTTP_CLIENT_STREAM_STATUS < 300))) &&
			#if TEST_HTTP_CLIENT_STREAM_INPUT_PAUSE
				(xrtHttpResponseBody(State.Response).Size == 0u) &&
				(State.InputSize ==
				 (sizeof(TEST_HTTP_CLIENT_STREAM_BODY) - 1u)) &&
				(memcmp(
					State.InputBody,
					TEST_HTTP_CLIENT_STREAM_BODY,
					State.InputSize
				 ) == 0) &&
				(xrtAtomic32Load(
					&State.InputCalls,
					XMEMORY_ACQUIRE
				 ) == 2u) &&
				!xrtHttp1CallPaused(State.Call),
			#else
				(xrtHttpResponseBody(State.Response).Size ==
				 (sizeof(TEST_HTTP_CLIENT_STREAM_BODY) - 1u)) &&
				((sizeof(TEST_HTTP_CLIENT_STREAM_BODY) == 1u) ||
				 (memcmp(
					xrtHttpResponseBody(State.Response).Data,
					TEST_HTTP_CLIENT_STREAM_BODY,
					sizeof(TEST_HTTP_CLIENT_STREAM_BODY) - 1u
				 ) == 0)),
			#endif
			"HTTP TCP response mismatch"
		);
		testRequire(
			xrtHttp1CallState(State.Call) ==
				XHTTP1_CALL_SUCCEEDED,
			"HTTP call state mismatch"
		);
		testRequire(
			(xrtAtomic64Load(
				&State.Written,
				XMEMORY_ACQUIRE
			) != 0) &&
			(xrtAtomic64Load(
				&State.Read,
				XMEMORY_ACQUIRE
			) != 0) &&
			(xrtAtomic32Load(
				&State.RequestDone,
				XMEMORY_ACQUIRE
			) == TEST_HTTP_CLIENT_STREAM_REQUEST_DONE),
			"HTTP call progress contract mismatch"
		);
	#endif
	#if TEST_HTTP_CLIENT_STREAM_RESUME
		testRequire(
			(xrtAtomic32Load(
				&State.BodyAgain,
				XMEMORY_ACQUIRE
			) ==
				#if TEST_HTTP_CLIENT_STREAM_BODY_SPURIOUS
					2
				#else
					1
				#endif
			) &&
			(xrtAtomic32Load(
				&State.BodyStep,
				XMEMORY_ACQUIRE
			) ==
				#if TEST_HTTP_CLIENT_STREAM_CANCEL || \
					(TEST_HTTP_CLIENT_STREAM_BODY_TERMINAL != 0) || \
					TEST_HTTP_CLIENT_STREAM_EARLY_FINAL
					0
				#else
					2
				#endif
			) &&
			(xrtAtomic32Load(
				&State.BodyClosed,
				XMEMORY_ACQUIRE
			) == 1) &&
			(xrtAtomic32Load(
				&State.BodyWaited,
				XMEMORY_ACQUIRE
			) ==
				#if TEST_HTTP_CLIENT_STREAM_BODY_SPURIOUS
					2
				#else
					1
				#endif
			),
			"HTTP resumed request body lifecycle mismatch"
		);
	#endif

	testHttpClientStreamWait(
		&State.Accepted,
		"HTTP test server did not accept the connection"
	);
	#if TEST_HTTP_CLIENT_STREAM_TRANSFERS
		testRequire(xrtNetStreamClose(State.Returned),
			"HTTP returned client close failed");
	#endif
	if ( xrtNetStreamState(State.Server) !=
		XNET_STREAM_CLOSED ) {
		testRequire(xrtNetStreamClose(State.Server),
			"HTTP server close failed");
	}
	testRequire(xrtNetListenerClose(State.Listener),
		"HTTP Listener close failed");
	iDeadline = xrtDeadlineAfter(5000000u);
	while (
		#if TEST_HTTP_CLIENT_STREAM_TRANSFERS
			(xrtNetStreamState(State.Returned) !=
			 XNET_STREAM_CLOSED) ||
		#endif
		(xrtNetStreamState(State.Server) !=
			XNET_STREAM_CLOSED) ||
		(xrtNetListenerState(State.Listener) !=
			XNET_LISTENER_CLOSED) ) {
		testRequire(
			!xrtDeadlineExpired(iDeadline),
			"HTTP test network objects did not close"
		);
		xrtThreadYield();
	}
	#if TEST_HTTP_CLIENT_STREAM_TRANSFERS
		xrtHttpResponseDestroy(State.Response);
		xrtNetStreamDestroy(State.Returned);
	#elif TEST_HTTP_CLIENT_STREAM_SUCCEEDS
		xrtHttpResponseDestroy(State.Response);
	#endif
	xrtHttp1CallDestroy(State.Call);
	#if TEST_HTTP_CLIENT_STREAM_RESUME
		xrtPromiseDestroy(State.BodyPromise);
		xrtFutureDestroy(State.BodyFuture);
	#endif
	xrtNetStreamDestroy(State.Server);
	xrtNetListenerDestroy(State.Listener);
	testRequire(xrtNetEngineDestroy(State.Engine),
		"HTTP test Engine destroy failed");
	printf(
		"[PASS] HTTP/1 TCP call %s (%s)\n",
		TEST_HTTP_CLIENT_STREAM_SCENARIO,
		TEST_HTTP_CLIENT_STREAM_BACKEND_NAME
	);
	return 0;
}
