#include "../fixtures/tls_server.h"

#if defined(TEST_TLS_STREAM_CLIENT_ATTACH)
	#include "../../src/internal/xrt_tcp.h"
	#include "../../src/internal/xrt_tls_session.h"
#endif

#if defined(TEST_TLS_STREAM_ABORT_FAILED)
	#include "../../src/internal/xrt_tls_stream.h"
#endif



#if !defined(TEST_TLS_STREAM_BACKEND)
	#define TEST_TLS_STREAM_BACKEND XNET_PORT_SELECT
	#define TEST_TLS_STREAM_BACKEND_NAME "select"
#endif

#if defined(TEST_TLS_STREAM_BACKPRESSURE)
	#define TEST_TLS_STREAM_PAYLOAD_SIZE (256u * 1024u)
#endif



typedef struct test_tls_stream_context test_tls_stream_context;



/* 每端只保留组合 Stream 的调用方引用和可并发观察的事件结果。 */
typedef struct test_tls_stream_endpoint {
	test_tls_stream_context* Context;
	xtlsstream* Stream;
	xatomic32 Open;
	xatomic32 Read;
	xatomic32 End;
	xatomic32 Writable;
	xatomic32 Drain;
	xatomic32 Close;
	xatomic32 Error;
	xatomic32 Sent;
	xatomic32 Deferred;
	xatomic32 Wrapped;
	uint64 Timer;
	char Data[128];
	bool Server;
	bool InSend;
} test_tls_stream_endpoint;



/* Listener 回调借用服务端配置直到唯一测试连接被接管。 */
struct test_tls_stream_context {
	test_tls_stream_endpoint Client;
	test_tls_stream_endpoint Server;
	xtlsserverconfig ServerConfig;
	xtlsclientconfig ClientConfig;
	xtlsstreamconfig StreamConfig;
	xtlsstreamevents Events;
	xatomic32 Accepted;
	xatomic32 ListenerError;
	xatomic32 ListenerClose;
};



#if defined(TEST_TLS_STREAM_CLIENT_ATTACH)
/* 在普通 TCP Open 回调内把调用方引用转移给 TLS 客户端组合层。 */
static void testTlsStreamClientAttach(
	xnetstream* pTransport,
	ptr pData
)
{
	test_tls_stream_context* pContext =
		(test_tls_stream_context*)pData;
	xtlssession* pSession = xrtTlsClientCreate(
		&pContext->ClientConfig,
		NULL
	);
	xnetbufpool* pFeedPool;
	xnetbufpool* pSendPool;
	xnetbufpool* pPlainPool;
	xnetbufpool* pScratchPool;
	xnetstreamevents TransportEvents;
	ptr pTransportData;
	size_t iWriteLimit;
	xtlsstream* pRejected = (xtlsstream*)pTransport;

	testRequire(pSession != NULL,
		"TLS stream client attach session creation failed");
	pFeedPool = pSession->Feed.Pool;
	pSendPool = pSession->Send.Pool;
	pPlainPool = pSession->Plain.Pool;
	pScratchPool = pSession->Scratch.Pool;
	TransportEvents = pTransport->Events;
	pTransportData = xrtAtomicPtrLoad(
		&pTransport->Data,
		XMEMORY_ACQUIRE
	);
	iWriteLimit = pTransport->Config.WriteLimit;
	pTransport->Config.WriteLimit = 1u;

	xrtClearError();
	testRequire(!xrtTlsStreamAttach(
		pTransport,
		pSession,
		&pContext->StreamConfig,
		&pContext->Events,
		&pContext->Client,
		&pRejected
	) && (pRejected == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE) &&
		(xrtErrorCode(xrtGetError()) == (int32)XTLS_ERROR_LIMIT) &&
		(pSession->Feed.Pool == pFeedPool) &&
		(pSession->Send.Pool == pSendPool) &&
		(pSession->Plain.Pool == pPlainPool) &&
		(pSession->Scratch.Pool == pScratchPool) &&
		(memcmp(
			&pTransport->Events,
			&TransportEvents,
			sizeof(TransportEvents)
		) == 0) && (xrtAtomicPtrLoad(
			&pTransport->Data,
			XMEMORY_ACQUIRE
		) == pTransportData),
		"TLS stream failed attach changed caller ownership");
	pTransport->Config.WriteLimit = iWriteLimit;
	xrtClearError();

	testRequire(xrtTlsStreamAttach(
		pTransport,
		pSession,
		&pContext->StreamConfig,
		&pContext->Events,
		&pContext->Client,
		&pContext->Client.Stream
	), "TLS stream existing client transport attach failed");
	xrtAtomic32Store(
		&pContext->Client.Wrapped,
		1,
		XMEMORY_RELEASE
	);
}
#endif



/* 在测试截止时间前等待原子计数达到给定值。 */
static void testTlsStreamWait(
	xatomic32* pValue,
	uint32 iExpected,
	cstr sMessage
)
{
	xdeadline Deadline = xrtDeadlineAfter(10000000u);

	while ( xrtAtomic32Load(pValue, XMEMORY_ACQUIRE) < iExpected ) {
		testRequire(!xrtDeadlineExpired(Deadline), sMessage);
		xrtThreadYield();
	}
}



/* 在当前可写边沿继续提交一段大明文，保留成功短写语义。 */
#if defined(TEST_TLS_STREAM_BACKPRESSURE)
static void testTlsStreamSendMore(
	test_tls_stream_endpoint* pEndpoint,
	bool bRequireShort
)
{
	static const uint8 Payload[TEST_TLS_STREAM_PAYLOAD_SIZE] = { 0x5Au };
	uint32 iSent = xrtAtomic32Load(&pEndpoint->Sent, XMEMORY_RELAXED);
	size_t iWritten = 0;
	xtlsresult Result;

	testRequire(iSent < TEST_TLS_STREAM_PAYLOAD_SIZE,
		"TLS stream backpressure sent beyond the payload");
	testRequire(!pEndpoint->InSend,
		"TLS stream Writable reentered an active Send");
	pEndpoint->InSend = true;
	Result = xrtTlsStreamSend(
		pEndpoint->Stream,
		Payload + iSent,
		TEST_TLS_STREAM_PAYLOAD_SIZE - iSent,
		&iWritten
	);
	pEndpoint->InSend = false;
	if ( (Result != XTLS_OK) && (Result != XTLS_AGAIN) ) {
		const xerror* pError = xrtTlsStreamError(pEndpoint->Stream);

		if ( pError == NULL ) {
			pError = xrtGetError();
		}

		fprintf(
			stderr,
			"[TLS stream backpressure] result=%d written=%llu "
			"state=%d operation=%s error=%s\n",
			(int)Result,
			(unsigned long long)iWritten,
			(int)xrtTlsStreamState(pEndpoint->Stream),
			pError != NULL ? xrtErrorOperation(pError) : "none",
			pError != NULL ? xrtErrorMessage(pError) : "none"
		);
	}
	testRequire((Result == XTLS_OK) || (Result == XTLS_AGAIN),
		"TLS stream backpressure send failed");
	if ( bRequireShort ) {
		testRequire((Result == XTLS_OK) && (iWritten != 0) &&
			(iWritten < TEST_TLS_STREAM_PAYLOAD_SIZE),
			"TLS stream did not publish its initial short write");
	}
	if ( Result == XTLS_AGAIN ) {
		testRequire(iWritten == 0,
			"TLS stream AGAIN reported accepted plaintext");
	}
	(void)xrtAtomic32FetchAdd(
		&pEndpoint->Sent,
		(uint32)iWritten,
		XMEMORY_RELEASE
	);
}
#endif



/* Open 必须位于 TCP Worker，并已经发布协商后的 http/1.1。 */
static void testTlsStreamOpen(xtlsstream* pStream, ptr pData)
{
	test_tls_stream_endpoint* pEndpoint =
		(test_tls_stream_endpoint*)pData;
	xtlssession* pSession = xrtTlsStreamSession(pStream);
	xbytesview Protocol;
	size_t iZero = SIZE_MAX;
	size_t iOne;
	size_t iRecord;
	size_t iSplit;

	testRequire((pEndpoint != NULL) && (pSession != NULL) &&
		xrtNetWorkerIsCurrent(xrtNetStreamWorker(
			xrtTlsStreamTransport(pStream)
		)), "TLS stream Open callback worker mismatch");
	testRequire(xrtTlsSessionProtocol(pSession, &Protocol) &&
		(Protocol.Size == 8u) &&
		(memcmp(Protocol.Data, "http/1.1", 8u) == 0),
		"TLS stream ALPN selection mismatch");
	testRequire(
		xrtTlsStreamSendBound(pStream, 0, &iZero) &&
		(iZero == 0) &&
		xrtTlsStreamSendBound(pStream, 1u, &iOne) &&
		(iOne > 1u) &&
		xrtTlsStreamSendBound(
			pStream,
			XTLS_RECORD_PLAINTEXT_MAX,
			&iRecord
		) &&
		xrtTlsStreamSendBound(
			pStream,
			XTLS_RECORD_PLAINTEXT_MAX + 1u,
			&iSplit
		) &&
		(iSplit == (iRecord + iOne)),
		"TLS stream send bound did not match record splitting"
	);
	iZero = 17u;
	xrtClearError();
	testRequire(
		!xrtTlsStreamSendBound(pStream, SIZE_MAX, &iZero) &&
		(iZero == 17u) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"TLS stream send bound overflow was not failure-atomic"
	);
	xrtClearError();
	if ( !pEndpoint->Server ) {
		const xx509cert* pCertificate;
		size_t iCertificateCount = xrtTlsClientCertificateCount(
			pSession
		);

		testRequire(iCertificateCount == 1u,
			"TLS stream peer certificate count mismatch");
		pCertificate = xrtTlsClientCertificate(pSession, 0);
		testRequire((pCertificate != NULL) &&
			(pCertificate->Raw.Size == sizeof(X509_LEGACY_RSA_CERT)) &&
			(memcmp(
				pCertificate->Raw.Data,
				X509_LEGACY_RSA_CERT,
				sizeof(X509_LEGACY_RSA_CERT)
			) == 0),
			"TLS stream peer certificate view mismatch");
		xrtClearError();
		testRequire((xrtTlsClientCertificate(
			pSession,
			iCertificateCount
		) == NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_RANGE),
			"TLS stream peer certificate range error mismatch");
		xrtClearError();
	}
	(void)xrtAtomic32FetchAdd(&pEndpoint->Open, 1, XMEMORY_RELEASE);
	{
		size_t iWritten = 1u;

		testRequire((xrtTlsStreamSend(
			pStream,
			NULL,
			0,
			&iWritten
		) == XTLS_OK) && (iWritten == 0),
			"TLS stream zero-length send mismatch");
		iWritten = 1u;
		testRequire((xrtTlsStreamSendVec(
			pStream,
			NULL,
			0,
			&iWritten
		) == XTLS_OK) && (iWritten == 0),
			"TLS stream empty vector send mismatch");
	}
	#if defined(TEST_TLS_STREAM_ABORT_FAILED)
		if ( !pEndpoint->Server ) {
			testRequire(pStream->Error == NULL,
				"TLS stream abort fixture already has an error");
			pStream->Error = xrtErrorCreate(
				XERR_PROTOCOL,
				"xrt.tls",
				XTLS_ERROR_RECORD_TYPE,
				"TLS stream abort root cause"
			);
			testRequire(pStream->Error != NULL,
				"TLS stream abort root cause creation failed");
			xrtAtomic32Store(
				&pStream->TerminalResult,
				XNET_RESULT_ERROR,
				XMEMORY_RELEASE
			);
			xrtAtomic32Store(
				&pStream->State,
				XTLS_STREAM_FAILED,
				XMEMORY_RELEASE
			);
			pStream->Failing = true;
			testRequire(xrtTlsStreamAbort(pStream),
				"TLS stream failed-state abort request failed");
			return;
		}
	#endif
	if ( !pEndpoint->Server ) {
		#if defined(TEST_TLS_STREAM_BACKPRESSURE)
			pEndpoint->Stream = pStream;
			testTlsStreamSendMore(pEndpoint, true);
		#else
		static const char Prefix[] = "TLS stream loopback ";
		static const char Body[] = "payload across the adapter";
		static const char Payload[] =
			"TLS stream loopback payload across the adapter";
		const xnetspan Spans[] = {
			{ (cbytes)Prefix, sizeof(Prefix) - 1u },
			{ NULL, 0 },
			{ (cbytes)Body, sizeof(Body) - 1u }
		};
		const xnetspan InvalidSpans[] = {
			{ NULL, 1u }
		};
		const xnetspan OverflowSpans[] = {
			{ (cbytes)Prefix, SIZE_MAX },
			{ (cbytes)Body, 1u }
		};
		size_t iWritten = 0;

		testRequire((xrtTlsStreamSendVec(
			pStream,
			InvalidSpans,
			1u,
			&iWritten
		) == XTLS_ERROR) && (iWritten == 0) &&
			(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
			"TLS stream invalid vector was not failure-atomic");
		xrtClearError();
		testRequire((xrtTlsStreamSendVec(
			pStream,
			OverflowSpans,
			2u,
			&iWritten
		) == XTLS_ERROR) && (iWritten == 0) &&
			(xrtErrorKind(xrtGetError()) == XERR_RANGE),
			"TLS stream overflowing vector was not failure-atomic");
		xrtClearError();
		testRequire(xrtTlsStreamSendVec(
			pStream,
			Spans,
			sizeof(Spans) / sizeof(Spans[0]),
			&iWritten
		) == XTLS_OK && (iWritten == (sizeof(Payload) - 1u)),
			"TLS stream client vector send failed");
		#endif
	}
}



/* 消费当前全部明文；常规服务端使用组合发送 API 原样回送。 */
static void testTlsStreamConsumePlain(
	xtlsstream* pStream,
	test_tls_stream_endpoint* pEndpoint
)
{
	uint32 iOffset = xrtAtomic32Load(
		&pEndpoint->Read,
		XMEMORY_RELAXED
	);
	size_t iAvailable = xrtTlsStreamAvailable(pStream);
	size_t iRead = 0;
	const xnetbuf* pBuffer = xrtTlsStreamBuffer(pStream);

	testRequire((pBuffer != NULL) && (iAvailable != 0),
		"TLS stream borrowed plaintext buffer mismatch");
	#if defined(TEST_TLS_STREAM_BACKPRESSURE)
		testRequire(xrtTlsStreamConsume(pStream, iAvailable),
			"TLS stream plaintext consume failed");
		iRead = iAvailable;
	#else
		testRequire(
		(iOffset <= sizeof(pEndpoint->Data)) &&
		(iAvailable <= (sizeof(pEndpoint->Data) - iOffset)),
			"TLS stream plaintext test buffer overflow");
		testRequire(xrtTlsStreamRead(
		pStream,
		pEndpoint->Data + iOffset,
		iAvailable,
		&iRead
	) == XTLS_OK && (iRead == iAvailable),
			"TLS stream plaintext read failed");
	#endif
	(void)xrtAtomic32FetchAdd(
		&pEndpoint->Read,
		(uint32)iRead,
		XMEMORY_RELEASE
	);
	if ( pEndpoint->Server ) {
		#if defined(TEST_TLS_STREAM_BACKPRESSURE)
			if ( (iOffset + iRead) == TEST_TLS_STREAM_PAYLOAD_SIZE ) {
				testRequire(xrtTlsStreamClose(pStream),
					"TLS stream backpressure close request failed");
			}
		#else
		size_t iWritten = 0;

		testRequire(xrtTlsStreamSend(
			pStream,
			pEndpoint->Data + iOffset,
			iRead,
			&iWritten
		) == XTLS_OK && (iWritten == iRead),
			"TLS stream server echo failed");
		#endif
	} else {
		#if !defined(TEST_TLS_STREAM_BACKPRESSURE)
		#if defined(TEST_TLS_STREAM_TRUNCATED)
			testRequire(xrtNetStreamClose(xrtTlsStreamTransport(
				pEndpoint->Context->Server.Stream
			)), "TLS stream raw truncation request failed");
		#else
			testRequire(xrtTlsStreamClose(pStream),
				"TLS stream authenticated close request failed");
		#endif
		#endif
	}
}



/* 延迟消费回调验证明文保留期间 TCP 停读及消费后的自动恢复。 */
#if defined(TEST_TLS_STREAM_SLOW_READER)
static void testTlsStreamDeferredRead(
	xnetworker* pWorker,
	uint64 Id,
	xnetresult Result,
	ptr pData
)
{
	test_tls_stream_endpoint* pEndpoint =
		(test_tls_stream_endpoint*)pData;

	(void)pWorker;
	testRequire((Id == pEndpoint->Timer) &&
		(Result == XNET_RESULT_OK),
		"TLS stream deferred read timer failed");
	pEndpoint->Timer = 0;
	testTlsStreamConsumePlain(pEndpoint->Stream, pEndpoint);
	(void)xrtAtomic32FetchAdd(
		&pEndpoint->Deferred,
		1,
		XMEMORY_RELEASE
	);
}
#endif



/* 明文通知可以同步消费，也可以保留到同一 Worker 的后续任务。 */
static void testTlsStreamRead(
	xtlsstream* pStream,
	const xnetbuf* pBuffer,
	ptr pData
)
{
	test_tls_stream_endpoint* pEndpoint =
		(test_tls_stream_endpoint*)pData;

	testRequire((pBuffer != NULL) &&
		(pBuffer == xrtTlsStreamBuffer(pStream)),
		"TLS stream Read buffer snapshot mismatch");
	#if defined(TEST_TLS_STREAM_SLOW_READER)
		if ( pEndpoint->Server && (pEndpoint->Timer == 0) &&
			(xrtAtomic32Load(
				&pEndpoint->Deferred,
				XMEMORY_ACQUIRE
			) == 0) ) {
			xnetworker* pWorker = xrtNetStreamWorker(
				xrtTlsStreamTransport(pStream)
			);

			pEndpoint->Stream = pStream;
			pEndpoint->Timer = xrtNetEngineAfter(
				xrtNetWorkerEngine(pWorker),
				xrtNetWorkerIndex(pWorker),
				50000u,
				testTlsStreamDeferredRead,
				pEndpoint
			);
			testRequire(pEndpoint->Timer != 0,
				"TLS stream deferred read timer setup failed");
			return;
		}
	#endif
	testTlsStreamConsumePlain(pStream, pEndpoint);
}



/* 记录对端已经通过 close_notify 认证关闭发送方向。 */
static void testTlsStreamEnd(xtlsstream* pStream, ptr pData)
{
	test_tls_stream_endpoint* pEndpoint =
		(test_tls_stream_endpoint*)pData;

	(void)pStream;
	(void)xrtAtomic32FetchAdd(&pEndpoint->End, 1, XMEMORY_RELEASE);
}



/* 写阻塞解除后继续提交尚未被 TLS 队列接受的明文。 */
static void testTlsStreamWritable(xtlsstream* pStream, ptr pData)
{
	test_tls_stream_endpoint* pEndpoint =
		(test_tls_stream_endpoint*)pData;

	(void)pStream;
	(void)xrtAtomic32FetchAdd(
		&pEndpoint->Writable,
		1,
		XMEMORY_RELEASE
	);
	#if defined(TEST_TLS_STREAM_BACKPRESSURE)
		if ( !pEndpoint->Server && (xrtAtomic32Load(
			&pEndpoint->Sent,
			XMEMORY_ACQUIRE
		) < TEST_TLS_STREAM_PAYLOAD_SIZE) ) {
			testTlsStreamSendMore(pEndpoint, false);
		}
	#endif
}



/* 记录应用数据与 TLS/TCP 两级密文队列均已排空。 */
static void testTlsStreamDrain(xtlsstream* pStream, ptr pData)
{
	test_tls_stream_endpoint* pEndpoint =
		(test_tls_stream_endpoint*)pData;

	testRequire(!pEndpoint->InSend,
		"TLS stream Drain reentered an active Send");
	testRequire(
		xrtTlsStreamPending(pStream) == 0,
		"TLS stream Drain retained pending ciphertext"
	);
	(void)xrtAtomic32FetchAdd(&pEndpoint->Drain, 1, XMEMORY_RELEASE);
}



/* 正常组合关闭必须发布 CLOSED、OK 且没有错误根因。 */
static void testTlsStreamClose(
	xtlsstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_tls_stream_endpoint* pEndpoint =
		(test_tls_stream_endpoint*)pData;

	if ( (Result != XNET_RESULT_OK) || (pError != NULL) ||
		(xrtTlsStreamState(pStream) != XTLS_STREAM_CLOSED) ) {
		(void)xrtAtomic32FetchAdd(
			&pEndpoint->Error,
			1,
			XMEMORY_RELAXED
		);
	}
	(void)xrtAtomic32FetchAdd(&pEndpoint->Close, 1, XMEMORY_RELEASE);
}



/* Listener Accept 用 TLS Server helper 一步接管原 TCP 调用方引用。 */
static bool testTlsStreamAccept(
	xnetlistener* pListener,
	xnetstream* pTransport,
	ptr pData
)
{
	test_tls_stream_context* pContext =
		(test_tls_stream_context*)pData;
	bool bAccepted;

	(void)pListener;
	bAccepted = xrtTlsStreamAccept(
		pTransport,
		&pContext->ServerConfig,
		&pContext->StreamConfig,
		&pContext->Events,
		&pContext->Server,
		&pContext->Server.Stream
	);
	if ( bAccepted ) {
		(void)xrtAtomic32FetchAdd(
			&pContext->Accepted,
			1,
			XMEMORY_RELEASE
		);
	}
	return bAccepted;
}



/* Listener 错误不能被折叠为 TLS 连接 Close。 */
static void testTlsStreamListenerError(
	xnetlistener* pListener,
	const xerror* pError,
	ptr pData
)
{
	test_tls_stream_context* pContext =
		(test_tls_stream_context*)pData;

	(void)pListener;
	testRequire(pError != NULL, "TLS stream listener error is null");
	(void)xrtAtomic32FetchAdd(
		&pContext->ListenerError,
		1,
		XMEMORY_RELEASE
	);
}



/* 记录 Listener 唯一关闭回调。 */
static void testTlsStreamListenerClose(
	xnetlistener* pListener,
	ptr pData
)
{
	test_tls_stream_context* pContext =
		(test_tls_stream_context*)pData;

	(void)pListener;
	(void)xrtAtomic32FetchAdd(
		&pContext->ListenerClose,
		1,
		XMEMORY_RELEASE
	);
}



/* 覆盖真实证书握手、ALPN、双向明文和认证关闭。 */
int main(void)
{
	static const xstrview Protocols[] = {
		XRT_STR_INIT("http/1.1")
	};
	#if !defined(TEST_TLS_STREAM_BACKPRESSURE) && \
		!defined(TEST_TLS_STREAM_ABORT_FAILED)
	static const char Payload[] =
		"TLS stream loopback payload across the adapter";
	#endif
	test_tls_stream_context Test;
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetstreamconfig TransportConfig;
	xnetlistenerevents ListenerEvents;
	#if defined(TEST_TLS_STREAM_CLIENT_ATTACH)
	xnetstreamevents ClientTransportEvents;
	#endif
	xtlsverifierconfig VerifierConfig;
	xtlsclientconfig ClientConfig;
	xtlslimits Limits;
	xtlscontext* pContext;
	xtlsidentity* pIdentity;
	xtlsverifier* pVerifier;
	xnetengine* pEngine;
	xnetlistener* pListener;
	xnetaddr Address;

	memset(&Test, 0, sizeof(Test));
	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	#if defined(TEST_TLS_STREAM_CLIENT_ATTACH)
		memset(&ClientTransportEvents, 0, sizeof(ClientTransportEvents));
		ClientTransportEvents.Open = testTlsStreamClientAttach;
	#endif
	Test.Client.Context = &Test;
	Test.Server.Context = &Test;
	Test.Server.Server = true;
	Test.Events.Open = testTlsStreamOpen;
	Test.Events.Read = testTlsStreamRead;
	Test.Events.End = testTlsStreamEnd;
	Test.Events.Writable = testTlsStreamWritable;
	Test.Events.Drain = testTlsStreamDrain;
	Test.Events.Close = testTlsStreamClose;
	ListenerEvents.Accept = testTlsStreamAccept;
	ListenerEvents.Error = testTlsStreamListenerError;
	ListenerEvents.Close = testTlsStreamListenerClose;

	#if defined(TEST_TLS_STREAM_BACKPRESSURE)
		xrtTlsLimitsInit(&Limits);
		Limits.FeedLimit = XTLS_RECORD_HEADER_SIZE +
			XTLS12_RECORD_CIPHERTEXT_MAX;
		Limits.SendLimit = 32768u;
		Limits.PlainLimit = 32768u;
		pContext = testTlsServerContextWithLimits(&Limits);
	#else
		(void)Limits;
		pContext = testTlsServerContext();
	#endif
	pIdentity = testTlsServerIdentity();
	testRequire((pContext != NULL) && (pIdentity != NULL),
		"TLS stream fixture creation failed");
	xrtTlsVerifierConfigInit(&VerifierConfig);
	VerifierConfig.Verify = testTlsServerAccept;
	pVerifier = xrtTlsVerifierCreate(&VerifierConfig);
	testRequire(pVerifier != NULL, "TLS stream verifier creation failed");
	xrtTlsServerConfigInit(&Test.ServerConfig);
	Test.ServerConfig.Context = pContext;
	Test.ServerConfig.Identity = pIdentity;
	Test.ServerConfig.Protocols = Protocols;
	Test.ServerConfig.ProtocolCount = 1u;
	Test.ServerConfig.RequireProtocol = true;
	xrtTlsClientConfigInit(&ClientConfig);
	ClientConfig.Context = pContext;
	ClientConfig.ServerName = XRT_STR_LITERAL("example.com");
	ClientConfig.Protocols = Protocols;
	ClientConfig.ProtocolCount = 1u;
	ClientConfig.Verifier = pVerifier;
	Test.ClientConfig = ClientConfig;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_TLS_STREAM_BACKEND;
	EngineConfig.Workers = 2u;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"TLS stream engine start failed");
	xrtNetListenConfigInit(&ListenConfig);
	xrtNetStreamConfigInit(&TransportConfig);
	#if defined(TEST_TLS_STREAM_BACKPRESSURE)
		TransportConfig.ReadSize = 65536u;
		TransportConfig.ReadLimit = 262144u;
		TransportConfig.WriteHighWater = Limits.SendLimit / 2u;
		TransportConfig.WriteLowWater = Limits.SendLimit / 4u;
		TransportConfig.WriteLimit = Limits.SendLimit;
	#endif
	ListenConfig.Stream = TransportConfig;
	xrtTlsStreamConfigInit(&Test.StreamConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	), "TLS stream loopback address setup failed");
	ListenConfig.AcceptConcurrency = 4u;
	#if !defined(TEST_TLS_STREAM_BACKPRESSURE)
		ListenConfig.Stream.ReadSize = 64u;
	#endif
	pListener = xrtNetListen(
		pEngine,
		&ListenConfig,
		&ListenerEvents,
		NULL,
		&Test
	);
	testRequire((pListener != NULL) &&
		xrtNetListenerLocal(pListener, &Address) &&
		(Address.Port != 0), "TLS stream listener creation failed");
	#if defined(TEST_TLS_STREAM_CLIENT_ATTACH)
		testRequire(xrtNetStreamConnect(
			pEngine,
			&Address,
			1u,
			&TransportConfig,
			&ClientTransportEvents,
			&Test
		) != NULL, "TLS stream client TCP creation failed");
		testTlsStreamWait(
			&Test.Client.Wrapped,
			1u,
			"TLS stream client TCP was not attached"
		);
	#else
		Test.Client.Stream = xrtTlsStreamConnect(
			pEngine,
			&Address,
			1u,
			&TransportConfig,
			&ClientConfig,
			&Test.StreamConfig,
			&Test.Events,
			&Test.Client
		);
		testRequire(Test.Client.Stream != NULL,
			"TLS stream client creation failed");
	#endif
	{
		size_t iWritten = 1u;
		size_t iBound = 17u;

		xrtClearError();
		testRequire((xrtTlsStreamSend(
			Test.Client.Stream,
			"X",
			1u,
			&iWritten
		) == XTLS_ERROR) && (iWritten == 0) &&
			(xrtGetError() != NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_STATE),
			"TLS stream send crossed its Worker boundary");
		xrtClearError();
		testRequire(
			!xrtTlsStreamSendBound(
				Test.Client.Stream,
				1u,
				&iBound
			) && (iBound == 17u) &&
			(xrtGetError() != NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_STATE),
			"TLS stream send bound crossed its Worker boundary"
		);
		xrtClearError();
		testRequire((xrtTlsStreamBuffer(Test.Client.Stream) == NULL) &&
			(xrtGetError() != NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_STATE),
			"TLS stream buffer crossed its Worker boundary");
		xrtClearError();
	}

	testTlsStreamWait(&Test.Accepted, 1u,
		"TLS stream server was not accepted");
	testTlsStreamWait(&Test.Client.Open, 1u,
		"TLS stream client Open callback missing");
	testTlsStreamWait(&Test.Server.Open, 1u,
		"TLS stream server Open callback missing");
	#if defined(TEST_TLS_STREAM_ABORT_FAILED)
		testTlsStreamWait(&Test.Client.Close, 1u,
			"TLS stream failed-state Abort did not close the client");
		testTlsStreamWait(&Test.Server.Close, 1u,
			"TLS stream failed-state Abort did not close the server");
		testRequire((xrtAtomic32Load(
			&Test.Client.Error,
			XMEMORY_ACQUIRE
		) == 1u) && (xrtTlsStreamState(
			Test.Client.Stream
		) == XTLS_STREAM_FAILED) && (xrtTlsStreamError(
			Test.Client.Stream
		) != NULL) && (xrtErrorKind(xrtTlsStreamError(
			Test.Client.Stream
		)) == XERR_PROTOCOL) && (xrtErrorCode(xrtTlsStreamError(
			Test.Client.Stream
		)) == (int32)XTLS_ERROR_RECORD_TYPE),
			"TLS stream Abort replaced the first failure");
	#else
	#if defined(TEST_TLS_STREAM_BACKPRESSURE)
		testTlsStreamWait(&Test.Client.Sent,
			TEST_TLS_STREAM_PAYLOAD_SIZE,
			"TLS stream Writable did not resume the short write");
		testTlsStreamWait(&Test.Server.Read,
			TEST_TLS_STREAM_PAYLOAD_SIZE,
			"TLS stream backpressure plaintext missing");
		testRequire((xrtAtomic32Load(
			&Test.Client.Writable,
			XMEMORY_ACQUIRE
		) != 0) && (xrtAtomic32Load(
			&Test.Client.Drain,
			XMEMORY_ACQUIRE
		) != 0), "TLS stream Writable/Drain edge missing");
	#else
		testTlsStreamWait(&Test.Server.Read, sizeof(Payload) - 1u,
			"TLS stream server plaintext missing");
		testTlsStreamWait(&Test.Client.Read, sizeof(Payload) - 1u,
			"TLS stream client echo missing");
	#endif
	#if !defined(TEST_TLS_STREAM_TRUNCATED)
		testTlsStreamWait(&Test.Client.End, 1u,
			"TLS stream client close_notify missing");
		testTlsStreamWait(&Test.Server.End, 1u,
			"TLS stream server close_notify missing");
	#endif
	testTlsStreamWait(&Test.Client.Close, 1u,
		"TLS stream client Close callback missing");
	testTlsStreamWait(&Test.Server.Close, 1u,
		"TLS stream server Close callback missing");
	#if !defined(TEST_TLS_STREAM_BACKPRESSURE)
	testRequire(memcmp(
		Test.Client.Data,
		Payload,
		sizeof(Payload) - 1u
	) == 0 && memcmp(
		Test.Server.Data,
		Payload,
		sizeof(Payload) - 1u
	) == 0, "TLS stream plaintext ordering mismatch");
	#endif
	#if defined(TEST_TLS_STREAM_TRUNCATED)
		testRequire((xrtAtomic32Load(
			&Test.Client.Error,
			XMEMORY_ACQUIRE
		) == 1) && (xrtTlsStreamState(
			Test.Client.Stream
		) == XTLS_STREAM_FAILED) && (xrtTlsStreamError(
			Test.Client.Stream
		) != NULL) && (xrtErrorCode(xrtTlsStreamError(
			Test.Client.Stream
		)) == (int32)XTLS_ERROR_TRUNCATED),
			"TLS stream truncation root cause mismatch");
	#else
		testRequire((xrtAtomic32Load(
			&Test.Client.Error,
			XMEMORY_ACQUIRE
		) == 0) && (xrtAtomic32Load(
			&Test.Server.Error,
			XMEMORY_ACQUIRE
		) == 0) && (xrtTlsStreamError(Test.Client.Stream) == NULL) &&
			(xrtTlsStreamError(Test.Server.Stream) == NULL),
			"TLS stream normal close reported an error");
	#endif
	#if defined(TEST_TLS_STREAM_SLOW_READER)
		testRequire(xrtAtomic32Load(
			&Test.Server.Deferred,
			XMEMORY_ACQUIRE
		) == 1, "TLS stream deferred plaintext was not resumed");
	#endif
	#endif

	testRequire(xrtNetListenerClose(pListener),
		"TLS stream listener close failed");
	testTlsStreamWait(&Test.ListenerClose, 1u,
		"TLS stream listener Close callback missing");
	testRequire(xrtAtomic32Load(
		&Test.ListenerError,
		XMEMORY_ACQUIRE
	) == 0, "TLS stream listener reported an error");
	xrtTlsStreamDestroy(Test.Client.Stream);
	xrtTlsStreamDestroy(Test.Server.Stream);
	xrtNetListenerDestroy(pListener);
	testRequire(xrtNetEngineDestroy(pEngine),
		"TLS stream engine destroy failed");
	xrtTlsVerifierRelease(pVerifier);
	xrtTlsIdentityRelease(pIdentity);
	xrtTlsContextRelease(pContext);
	printf("[PASS] TLS stream %s lifecycle\n", TEST_TLS_STREAM_BACKEND_NAME);
	return 0;
}
