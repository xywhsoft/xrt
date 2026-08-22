#ifndef XRT_TEST_HTTP_ORIGIN_H
#define XRT_TEST_HTTP_ORIGIN_H

#include "../test.h"



/* 最小 HTTP origin 夹具只负责一条连接和一条完整请求。 */
typedef struct testhttporigin {
	xnetlistener* Listener;
	xnetstream* Stream;
	const char* Response;
	size_t ResponseSize;
	size_t ResponseSplit;
	uint64 ResponseDelay;
	xstrview ExpectedMethod;
	xstrview ExpectedTarget;
	xstrview ExpectedContentType;
	xbytesview ExpectedBody;
	xatomic32 Accepted;
	xatomic32 Requests;
	xatomic32 StreamClosed;
	xatomic32 ListenerClosed;
	xatomic32 Sent;
	bool VerifyRequest;
	bool WaitChunkedRequest;
} testhttporigin;



/* 在固定测试截止时间前等待一个原子计数达到目标值。 */
static inline void testHttpOriginWait(
	const xatomic32* pValue,
	uint32 iExpected,
	cstr sMessage
)
{
	xdeadline Deadline = xrtDeadlineAfter(UINT64_C(10000000));

	while ( xrtAtomic32Load(
		pValue,
		XMEMORY_ACQUIRE
	) < iExpected ) {
		testRequire(
			!xrtDeadlineExpired(Deadline),
			sMessage
		);
		xrtThreadYield();
	}
}



/* 返回完整 HTTP/1 Header 占用的字节数，输入不足时返回零。 */
static inline size_t testHttpOriginHeaderSize(
	const char* pData,
	size_t iSize
)
{
	for ( size_t i = 3; i < iSize; i++ ) {
		if ( (pData[i - 3u] == '\r') &&
			(pData[i - 2u] == '\n') &&
			(pData[i - 1u] == '\r') &&
			(pData[i] == '\n') ) {
			return i + 1u;
		}
	}
	return 0;
}



/* 返回无 trailer 的 chunked 请求总长度，数据不完整时返回零。 */
static inline size_t testHttpOriginChunkedSize(
	const char* pData,
	size_t iSize,
	size_t iHeader
)
{
	for ( size_t i = iHeader; (i + 5u) <= iSize; i++ ) {
		if (
			((i == iHeader) ||
			 ((i >= 2u) &&
			  (pData[i - 2u] == '\r') &&
			  (pData[i - 1u] == '\n'))) &&
			(memcmp(pData + i, "0\r\n\r\n", 5u) == 0)
		) {
			return i + 5u;
		}
	}
	return 0;
}



/* 在完整 Header 内查找一个由便利客户端生成的精确字段行。 */
static inline bool testHttpOriginHeaderLine(
	const char* sRequest,
	size_t iHeader,
	xstrview Name,
	xstrview Value
)
{
	size_t iLine = 0;

	while ( (iLine + 2u) <= iHeader ) {
		size_t iEnd = iLine;

		while ( (iEnd + 1u) < iHeader ) {
			if ( (sRequest[iEnd] == '\r') &&
				(sRequest[iEnd + 1u] == '\n') ) {
				break;
			}
			iEnd++;
		}
		if ( (iEnd + 1u) >= iHeader ) {
			return false;
		}
		if ( ((iEnd - iLine) ==
			(Name.Size + 2u + Value.Size)) &&
			(memcmp(sRequest + iLine, Name.Data, Name.Size) == 0) &&
			(sRequest[iLine + Name.Size] == ':') &&
			(sRequest[iLine + Name.Size + 1u] == ' ') &&
			(memcmp(
				sRequest + iLine + Name.Size + 2u,
				Value.Data,
				Value.Size
			) == 0) ) {
			return true;
		}
		iLine = iEnd + 2u;
	}
	return false;
}



/* 验证请求行、可选 Content-Type 和完整正文。 */
static inline void testHttpOriginVerifyRequest(
	const testhttporigin* pOrigin,
	const char* sRequest,
	size_t iHeader
)
{
	size_t iOffset = 0;

	testRequire(
		(iHeader >= (
			pOrigin->ExpectedMethod.Size +
			pOrigin->ExpectedTarget.Size +
			12u
		)) &&
		(memcmp(
			sRequest,
			pOrigin->ExpectedMethod.Data,
			pOrigin->ExpectedMethod.Size
		) == 0),
		"HTTP origin request method mismatch"
	);
	iOffset += pOrigin->ExpectedMethod.Size;
	testRequire(
		(sRequest[iOffset++] == ' ') &&
		(memcmp(
			sRequest + iOffset,
			pOrigin->ExpectedTarget.Data,
			pOrigin->ExpectedTarget.Size
		) == 0),
		"HTTP origin request target mismatch"
	);
	iOffset += pOrigin->ExpectedTarget.Size;
	testRequire(
		memcmp(
			sRequest + iOffset,
			" HTTP/1.1\r\n",
			11u
		) == 0,
		"HTTP origin request version mismatch"
	);
	if ( pOrigin->ExpectedContentType.Size != 0 ) {
		testRequire(
			testHttpOriginHeaderLine(
				sRequest,
				iHeader,
				XRT_STR_LITERAL("Content-Type"),
				pOrigin->ExpectedContentType
			),
			"HTTP origin request Content-Type mismatch"
		);
	}
	testRequire(
		(pOrigin->ExpectedBody.Size == 0) ||
		(memcmp(
			sRequest + iHeader,
			pOrigin->ExpectedBody.Data,
			pOrigin->ExpectedBody.Size
		) == 0),
		"HTTP origin request body mismatch"
	);
}



/* 为下一条请求安装可选的完整线路断言。 */
static inline void testHttpOriginExpect(
	testhttporigin* pOrigin,
	xstrview Method,
	xstrview Target,
	xstrview ContentType,
	xbytesview Body
)
{
	pOrigin->ExpectedMethod = Method;
	pOrigin->ExpectedTarget = Target;
	pOrigin->ExpectedContentType = ContentType;
	pOrigin->ExpectedBody = Body;
	pOrigin->VerifyRequest = true;
}



/* 要求 origin 收到完整的无 trailer chunked 请求后才发送响应。 */
static inline void testHttpOriginExpectChunked(
	testhttporigin* pOrigin,
	xstrview Method,
	xstrview Target
)
{
	pOrigin->ExpectedMethod = Method;
	pOrigin->ExpectedTarget = Target;
	pOrigin->VerifyRequest = true;
	pOrigin->WaitChunkedRequest = true;
}



/* 对端结束发送后完成服务端半关闭。 */
static inline void testHttpOriginEnd(
	xnetstream* pStream,
	ptr pData
)
{
	(void)pData;
	if ( xrtNetStreamState(pStream) != XNET_STREAM_CLOSED ) {
		(void)xrtNetStreamClose(pStream);
	}
}



/* 记录服务端 Stream 已经释放底层传输。 */
static inline void testHttpOriginStreamClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	testhttporigin* pOrigin = (testhttporigin*)pData;

	(void)pStream;
	(void)Result;
	(void)pError;
	xrtAtomic32Store(
		&pOrigin->StreamClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* 延迟发送响应尾段，确保测试可以精确控制 Header 与正文的交付边界。 */
static inline void testHttpOriginResponseTail(
	xnetworker* pWorker,
	uint64 Id,
	xnetresult Result,
	ptr pData
)
{
	testhttporigin* pOrigin = (testhttporigin*)pData;

	testRequire(
		(pOrigin->Stream != NULL) &&
		(pWorker == xrtNetStreamWorker(pOrigin->Stream)) &&
		(Id != 0) &&
		(Result == XNET_RESULT_OK) &&
		(pOrigin->ResponseSplit < pOrigin->ResponseSize),
		"HTTP origin response tail timer mismatch"
	);
	testRequire(
		xrtNetStreamSend(
			pOrigin->Stream,
			pOrigin->Response + pOrigin->ResponseSplit,
			pOrigin->ResponseSize - pOrigin->ResponseSplit
		) == XNET_RESULT_OK,
		"HTTP origin response tail send failed"
	);
}



/*
	消费首条完整请求 Header。
	Response 为空时故意保持挂起，用于取消和超时测试。
*/
static inline void testHttpOriginRead(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	ptr pData
)
{
	testhttporigin* pOrigin = (testhttporigin*)pData;
	char Request[4096];
	size_t iSize = xrtNetBufSize(pBuffer);
	size_t iHeader;
	size_t iRequest;
	uint32 iExpected = 0;

	if ( iSize == 0 ) {
		return;
	}
	testRequire(
		iSize < sizeof(Request),
		"HTTP origin request exceeded fixture capacity"
	);
	testRequire(
		xrtNetBufPeek(
			pBuffer,
			0,
			Request,
			iSize
		) == iSize,
		"HTTP origin request peek failed"
	);
	iHeader = testHttpOriginHeaderSize(Request, iSize);
	if ( iHeader == 0 ) {
		return;
	}
	iRequest = iHeader;
	if ( pOrigin->WaitChunkedRequest ) {
		iRequest = testHttpOriginChunkedSize(
			Request,
			iSize,
			iHeader
		);
		if ( iRequest == 0 ) {
			return;
		}
	}
	if ( pOrigin->VerifyRequest ) {
		if ( !pOrigin->WaitChunkedRequest &&
			(pOrigin->ExpectedBody.Size > (iSize - iHeader)) ) {
			return;
		}
		if ( !pOrigin->WaitChunkedRequest ) {
			iRequest += pOrigin->ExpectedBody.Size;
		}
		testHttpOriginVerifyRequest(
			pOrigin,
			Request,
			iHeader
		);
	}
	testRequire(
		xrtNetBufConsume(pBuffer, iRequest) == iRequest,
		"HTTP origin request consume failed"
	);
	(void)xrtAtomic32FetchAdd(
		&pOrigin->Requests,
		1,
		XMEMORY_RELEASE
	);
	if ( (pOrigin->Response == NULL) ||
		!xrtAtomic32CompareExchange(
			&pOrigin->Sent,
			&iExpected,
			1,
			XMEMORY_ACQ_REL,
			XMEMORY_ACQUIRE
		) ) {
		return;
	}
	if ( (pOrigin->ResponseSplit != 0) &&
		(pOrigin->ResponseSplit < pOrigin->ResponseSize) ) {
		testRequire(
			xrtNetStreamSend(
				pStream,
				pOrigin->Response,
				pOrigin->ResponseSplit
			) == XNET_RESULT_OK,
			"HTTP origin response prefix send failed"
		);
		testRequire(
			xrtNetEngineAfter(
				xrtNetWorkerEngine(xrtNetStreamWorker(pStream)),
				xrtNetWorkerIndex(xrtNetStreamWorker(pStream)),
				pOrigin->ResponseDelay,
				testHttpOriginResponseTail,
				pOrigin
			) != 0,
			"HTTP origin response tail timer start failed"
		);
	} else {
		testRequire(
			xrtNetStreamSend(
				pStream,
				pOrigin->Response,
				pOrigin->ResponseSize
			) == XNET_RESULT_OK,
			"HTTP origin response send failed"
		);
	}
}



/* 接管唯一服务端 Stream 并安装通用请求处理器。 */
static inline bool testHttpOriginAccept(
	xnetlistener* pListener,
	xnetstream* pStream,
	ptr pData
)
{
	testhttporigin* pOrigin = (testhttporigin*)pData;
	xnetstreamevents Events;

	(void)pListener;
	testRequire(
		pOrigin->Stream == NULL,
		"HTTP origin accepted an unexpected second connection"
	);
	memset(&Events, 0, sizeof(Events));
	Events.Read = testHttpOriginRead;
	Events.End = testHttpOriginEnd;
	Events.Close = testHttpOriginStreamClose;
	testRequire(
		xrtNetStreamSetEvents(
			pStream,
			&Events,
			pOrigin
		),
		"HTTP origin stream takeover failed"
	);
	pOrigin->Stream = pStream;
	xrtAtomic32Store(
		&pOrigin->Accepted,
		1,
		XMEMORY_RELEASE
	);
	return true;
}



/* 记录 Listener 已经排空在途 Accept。 */
static inline void testHttpOriginListenerClose(
	xnetlistener* pListener,
	ptr pData
)
{
	testhttporigin* pOrigin = (testhttporigin*)pData;

	(void)pListener;
	xrtAtomic32Store(
		&pOrigin->ListenerClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* 在指定 Engine 上启动一个环回 HTTP origin。 */
static inline void testHttpOriginStart(
	testhttporigin* pOrigin,
	xnetengine* pEngine,
	const char* sResponse,
	size_t iResponseSize
)
{
	xnetlistenconfig Config;
	xnetlistenerevents Events;

	memset(pOrigin, 0, sizeof(*pOrigin));
	xrtAtomic32Init(&pOrigin->Accepted, 0);
	xrtAtomic32Init(&pOrigin->Requests, 0);
	xrtAtomic32Init(&pOrigin->StreamClosed, 0);
	xrtAtomic32Init(&pOrigin->ListenerClosed, 0);
	xrtAtomic32Init(&pOrigin->Sent, 0);
	pOrigin->Response = sResponse;
	pOrigin->ResponseSize = iResponseSize;
	xrtNetListenConfigInit(&Config);
	testRequire(
		xrtNetAddrLoopback(
			&Config.Address,
			XNET_FAMILY_IPV4,
			0
		),
		"HTTP origin loopback address failed"
	);
	Config.AcceptConcurrency = 2;
	memset(&Events, 0, sizeof(Events));
	Events.Accept = testHttpOriginAccept;
	Events.Close = testHttpOriginListenerClose;
	pOrigin->Listener = xrtNetListen(
		pEngine,
		&Config,
		&Events,
		NULL,
		pOrigin
	);
	testRequire(
		pOrigin->Listener != NULL,
		"HTTP origin listener create failed"
	);
}



/* 把响应拆成一次前缀发送和一次延迟尾段发送。 */
static inline void testHttpOriginSplitResponse(
	testhttporigin* pOrigin,
	size_t iOffset,
	uint64 iDelay
)
{
	testRequire(
		(pOrigin != NULL) &&
		(iOffset != 0) &&
		(iOffset < pOrigin->ResponseSize),
		"HTTP origin response split is invalid"
	);
	pOrigin->ResponseSplit = iOffset;
	pOrigin->ResponseDelay = iDelay;
}



/* 返回 origin 当前绑定的环回端口。 */
static inline uint16 testHttpOriginPort(
	const testhttporigin* pOrigin
)
{
	xnetaddr Address;

	testRequire(
		xrtNetListenerLocal(pOrigin->Listener, &Address),
		"HTTP origin local address query failed"
	);
	return Address.Port;
}



/* 中止剩余传输、关闭 Listener 并释放夹具持有的引用。 */
static inline void testHttpOriginStop(testhttporigin* pOrigin)
{
	if ( (pOrigin->Stream != NULL) &&
		(xrtNetStreamState(pOrigin->Stream) !=
		 XNET_STREAM_CLOSED) ) {
		(void)xrtNetStreamAbort(pOrigin->Stream);
	}
	if ( (pOrigin->Listener != NULL) &&
		(xrtNetListenerState(pOrigin->Listener) !=
		 XNET_LISTENER_CLOSED) ) {
		(void)xrtNetListenerClose(pOrigin->Listener);
	}
	if ( pOrigin->Stream != NULL ) {
		testHttpOriginWait(
			&pOrigin->StreamClosed,
			1,
			"HTTP origin stream did not close"
		);
	}
	if ( pOrigin->Listener != NULL ) {
		testHttpOriginWait(
			&pOrigin->ListenerClosed,
			1,
			"HTTP origin listener did not close"
		);
	}
	xrtNetStreamDestroy(pOrigin->Stream);
	xrtNetListenerDestroy(pOrigin->Listener);
	pOrigin->Stream = NULL;
	pOrigin->Listener = NULL;
}



#endif
