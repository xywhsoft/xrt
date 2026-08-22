#include "../fixtures/tls_server.h"



#define TEST_TLS_STREAM_RESUME_ROUNDS 4u
#define TEST_TLS_STREAM_INITIAL_TICKETS \
	XTLS_CLIENT_RESUME_LIMIT_DEFAULT



#if !defined(TEST_TLS_STREAM_BACKEND)
	#define TEST_TLS_STREAM_BACKEND XNET_PORT_SELECT
#endif



typedef struct test_tls_stream_resume test_tls_stream_resume;



/* 每一端只保存当前组合对象的调用方引用和累计事件。 */
typedef struct test_tls_stream_resume_endpoint {
	test_tls_stream_resume* Test;
	xtlsstream* Stream;
	xatomic32 Open;
	xatomic32 Read;
	xatomic32 End;
	xatomic32 Close;
	xatomic32 Error;
	bool Server;
} test_tls_stream_resume_endpoint;



/* 四轮连接共享监听器、配置、票据缓存和当前应用负载。 */
struct test_tls_stream_resume {
	test_tls_stream_resume_endpoint Client;
	test_tls_stream_resume_endpoint Server;
	xtlsserverconfig ServerConfig;
	xtlsstreamconfig StreamConfig;
	xtlsstreamevents Events;
	xnetengine* Engine;
	xtlsresume* ServerResume;
	xtlsresume* NextClientResume;
	xatomic32 Round;
	xatomic32 Accepted;
	xatomic32 ResumeReady;
	xatomic32 TicketRequested;
	xatomic32 FifthIssued;
	xatomic32 ListenerClose;
	xatomic32 ListenerError;
	char Payload[64];
	size_t PayloadSize;
};



/* 在统一测试截止时间前等待累计事件达到目标。 */
static void testTlsStreamResumeWait(
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



/* 客户端只有同时收到回声和新票据后才发起认证关闭。 */
static void testTlsStreamResumeMaybeClose(
	test_tls_stream_resume* pTest
)
{
	uint32 iRound = xrtAtomic32Load(&pTest->Round, XMEMORY_ACQUIRE);

	if ( (xrtAtomic32Load(
		&pTest->Client.Read,
		XMEMORY_ACQUIRE
	) > iRound) && (xrtAtomic32Load(
		&pTest->ResumeReady,
		XMEMORY_ACQUIRE
	) > iRound) ) {
		testRequire(xrtTlsStreamClose(pTest->Client.Stream),
			"TLS stream resumed close request failed");
	}
}



/* 在服务端 Worker 签发满队列后的第五张票据。 */
static void testTlsStreamResumeFifthTicket(
	xnetworker* pWorker,
	ptr pData
)
{
	test_tls_stream_resume* pTest =
		(test_tls_stream_resume*)pData;
	xtlsresume* pResume = NULL;
	size_t iWritten = 0;

	testRequire(
		(pTest->Server.Stream != NULL) &&
		(pWorker == xrtNetStreamWorker(
			xrtTlsStreamTransport(pTest->Server.Stream)
		)),
		"TLS stream fifth ticket worker mismatch"
	);
	xrtAtomic32Store(
		&pTest->FifthIssued,
		1,
		XMEMORY_RELEASE
	);
	testRequire(
		(xrtTlsServerTicketNew(
			xrtTlsStreamSession(pTest->Server.Stream),
			&pResume
		) == XTLS_OK) &&
		(pResume != NULL),
		"TLS stream fifth ticket issue failed"
	);
	xrtTlsResumeRelease(pTest->ServerResume);
	pTest->ServerResume = pResume;
	testRequire(
		(xrtTlsStreamSend(
			pTest->Server.Stream,
			NULL,
			0,
			&iWritten
		) == XTLS_OK) &&
		(iWritten == 0),
		"TLS stream fifth ticket flush failed"
	);
}



/*
	前四张 ticket 先填满客户端队列，再要求服务端签发第五张。
	只有满队列替换仍发布 Ticket 边沿时，本轮才会取得最新票据并关闭。
*/
static void testTlsStreamResumeTicket(
	xtlsstream* pStream,
	ptr pData
)
{
	test_tls_stream_resume_endpoint* pEndpoint =
		(test_tls_stream_resume_endpoint*)pData;
	test_tls_stream_resume* pTest = pEndpoint->Test;
	xtlssession* pSession = xrtTlsStreamSession(pStream);
	xtlsresume* pResume = NULL;
	size_t iTaken = 0;
	uint32 iRound = xrtAtomic32Load(
		&pTest->Round,
		XMEMORY_ACQUIRE
	);

	testRequire(
		!pEndpoint->Server &&
		(pSession != NULL) &&
		xrtNetWorkerIsCurrent(xrtNetStreamWorker(
			xrtTlsStreamTransport(pStream)
		)),
		"TLS stream ticket event worker mismatch"
	);
	if ( xrtAtomic32Load(
		&pTest->FifthIssued,
		XMEMORY_ACQUIRE
	) == 0 ) {
		if ( (xrtTlsClientResumeCount(pSession) ==
			TEST_TLS_STREAM_INITIAL_TICKETS) &&
			(xrtAtomic32Exchange(
				&pTest->TicketRequested,
				1,
				XMEMORY_ACQ_REL
			) == 0) ) {
			testRequire(
				xrtNetEnginePost(
					pTest->Engine,
					xrtNetWorkerIndex(
						xrtNetStreamWorker(
							xrtTlsStreamTransport(
								pTest->Server.Stream
							)
						)
					),
					testTlsStreamResumeFifthTicket,
					pTest
				),
				"TLS stream fifth ticket task post failed"
			);
		}
		return;
	}
	while ( xrtTlsClientResumeCount(pSession) != 0 ) {
		pResume = xrtTlsClientTakeResume(pSession);
		testRequire(
			pResume != NULL,
			"TLS stream ticket queue take failed"
		);
		xrtTlsResumeRelease(pTest->NextClientResume);
		pTest->NextClientResume = pResume;
		iTaken++;
	}
	testRequire(
		(iTaken == TEST_TLS_STREAM_INITIAL_TICKETS) &&
		(pTest->NextClientResume != NULL) &&
		(xrtTlsClientResumeDropped(pSession) == 1u),
		"TLS stream full resume queue did not retain the latest tickets"
	);
	xrtAtomic32Store(
		&pTest->ResumeReady,
		iRound + 1u,
		XMEMORY_RELEASE
	);
	testTlsStreamResumeMaybeClose(pTest);
}



/* 服务端缓存返回借用票据，状态机负责立即持有自己的引用。 */
static const xtlsresume* testTlsStreamResumeLookup(
	ptr pData,
	const xtlsserverresumerequest* pRequest
)
{
	test_tls_stream_resume* pTest = (test_tls_stream_resume*)pData;

	testRequire((pTest != NULL) && (pRequest != NULL) &&
		(pRequest->Ticket.Size != 0),
		"TLS stream resume lookup request is invalid");
	return pTest->ServerResume;
}



/* Open 验证每轮认证类型，服务端签发下一张票据，客户端发送负载。 */
static void testTlsStreamResumeOpen(xtlsstream* pStream, ptr pData)
{
	test_tls_stream_resume_endpoint* pEndpoint =
		(test_tls_stream_resume_endpoint*)pData;
	test_tls_stream_resume* pTest = pEndpoint->Test;
	xtlssession* pSession = xrtTlsStreamSession(pStream);
	uint32 iRound = xrtAtomic32Load(&pTest->Round, XMEMORY_ACQUIRE);
	size_t iWritten = 0;

	pEndpoint->Stream = pStream;
	testRequire((pSession != NULL) && xrtNetWorkerIsCurrent(
		xrtNetStreamWorker(xrtTlsStreamTransport(pStream))
	), "TLS stream resumed Open worker mismatch");
	if ( pEndpoint->Server ) {
		testRequire(xrtTlsServerResumed(pSession) == (iRound != 0),
			"TLS stream server resumed state mismatch");
		for ( size_t i = 0;
			i < TEST_TLS_STREAM_INITIAL_TICKETS;
			i++ ) {
			xtlsresume* pResume = NULL;

			testRequire(
				(xrtTlsServerTicketNew(
					pSession,
					&pResume
				) == XTLS_OK) &&
				(pResume != NULL),
				"TLS stream initial ticket issue failed"
			);
			xrtTlsResumeRelease(pTest->ServerResume);
			pTest->ServerResume = pResume;
		}
	} else {
		testRequire(xrtTlsClientResumed(pSession) == (iRound != 0),
			"TLS stream client resumed state mismatch");
		testRequire(xrtTlsClientCertificateCount(pSession) ==
			(iRound == 0 ? 1u : 0),
			"TLS stream resumed certificate visibility mismatch");
		testRequire(xrtTlsStreamSend(
			pStream,
			pTest->Payload,
			pTest->PayloadSize,
			&iWritten
		) == XTLS_OK && (iWritten == pTest->PayloadSize),
			"TLS stream resumed client send failed");
	}
	(void)xrtAtomic32FetchAdd(&pEndpoint->Open, 1, XMEMORY_RELEASE);
}



/* 每轮服务端原样回送，客户端核对后等待票据完成。 */
static void testTlsStreamResumeRead(
	xtlsstream* pStream,
	const xnetbuf* pBuffer,
	ptr pData
)
{
	test_tls_stream_resume_endpoint* pEndpoint =
		(test_tls_stream_resume_endpoint*)pData;
	test_tls_stream_resume* pTest = pEndpoint->Test;
	char Data[64];
	size_t iRead = 0;
	size_t iWritten = 0;

	(void)pBuffer;
	testRequire((pTest->PayloadSize <= sizeof(Data)) &&
		(xrtTlsStreamRead(
			pStream,
			Data,
			pTest->PayloadSize,
			&iRead
		) == XTLS_OK) && (iRead == pTest->PayloadSize) &&
		(memcmp(Data, pTest->Payload, iRead) == 0),
		"TLS stream resumed payload mismatch");
	if ( pEndpoint->Server ) {
		testRequire(xrtTlsStreamSend(
			pStream,
			Data,
			iRead,
			&iWritten
		) == XTLS_OK && (iWritten == iRead),
			"TLS stream resumed echo failed");
	}
	(void)xrtAtomic32FetchAdd(&pEndpoint->Read, 1, XMEMORY_RELEASE);
	if ( !pEndpoint->Server ) {
		testTlsStreamResumeMaybeClose(pTest);
	}
}



/* 记录每轮已经收到认证 close_notify。 */
static void testTlsStreamResumeEnd(xtlsstream* pStream, ptr pData)
{
	test_tls_stream_resume_endpoint* pEndpoint =
		(test_tls_stream_resume_endpoint*)pData;

	(void)pStream;
	(void)xrtAtomic32FetchAdd(&pEndpoint->End, 1, XMEMORY_RELEASE);
}



/* 四轮都必须以无根因 CLOSED 终态结束。 */
static void testTlsStreamResumeClose(
	xtlsstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_tls_stream_resume_endpoint* pEndpoint =
		(test_tls_stream_resume_endpoint*)pData;

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



/* Listener 在每轮用同一服务端配置接管新的 TCP Stream。 */
static bool testTlsStreamResumeAccept(
	xnetlistener* pListener,
	xnetstream* pTransport,
	ptr pData
)
{
	test_tls_stream_resume* pTest = (test_tls_stream_resume*)pData;
	bool bAccepted;

	(void)pListener;
	bAccepted = xrtTlsStreamAccept(
		pTransport,
		&pTest->ServerConfig,
		&pTest->StreamConfig,
		&pTest->Events,
		&pTest->Server,
		&pTest->Server.Stream
	);
	if ( bAccepted ) {
		(void)xrtAtomic32FetchAdd(
			&pTest->Accepted,
			1,
			XMEMORY_RELEASE
		);
	}
	return bAccepted;
}



/* Listener 错误独立于连接终态累计。 */
static void testTlsStreamResumeListenerError(
	xnetlistener* pListener,
	const xerror* pError,
	ptr pData
)
{
	test_tls_stream_resume* pTest = (test_tls_stream_resume*)pData;

	(void)pListener;
	testRequire(pError != NULL, "TLS stream resume listener error is null");
	(void)xrtAtomic32FetchAdd(
		&pTest->ListenerError,
		1,
		XMEMORY_RELEASE
	);
}



/* 记录 Listener 的唯一关闭完成。 */
static void testTlsStreamResumeListenerClose(
	xnetlistener* pListener,
	ptr pData
)
{
	test_tls_stream_resume* pTest = (test_tls_stream_resume*)pData;

	(void)pListener;
	(void)xrtAtomic32FetchAdd(
		&pTest->ListenerClose,
		1,
		XMEMORY_RELEASE
	);
}



/* 复用一个 Listener 完成一次完整握手和三次票据轮换恢复。 */
int main(void)
{
	static const xstrview Protocols[] = {
		XRT_STR_INIT("http/1.1")
	};
	test_tls_stream_resume Test;
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetlistenerevents ListenerEvents;
	xnetstreamconfig TransportConfig;
	xtlsverifierconfig VerifierConfig;
	xtlsclientconfig ClientConfig;
	xtlscontext* pContext;
	xtlsidentity* pIdentity;
	xtlsverifier* pVerifier;
	xtlsresume* pClientResume = NULL;
	xnetlistener* pListener;
	xnetaddr Address;

	memset(&Test, 0, sizeof(Test));
	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	Test.Client.Test = &Test;
	Test.Server.Test = &Test;
	Test.Server.Server = true;
	Test.Events.Open = testTlsStreamResumeOpen;
	Test.Events.Read = testTlsStreamResumeRead;
	Test.Events.End = testTlsStreamResumeEnd;
	Test.Events.Close = testTlsStreamResumeClose;
	Test.Events.Ticket = testTlsStreamResumeTicket;
	ListenerEvents.Accept = testTlsStreamResumeAccept;
	ListenerEvents.Error = testTlsStreamResumeListenerError;
	ListenerEvents.Close = testTlsStreamResumeListenerClose;
	pContext = testTlsServerContext();
	pIdentity = testTlsServerIdentity();
	xrtTlsVerifierConfigInit(&VerifierConfig);
	VerifierConfig.Verify = testTlsServerAccept;
	pVerifier = xrtTlsVerifierCreate(&VerifierConfig);
	testRequire((pContext != NULL) && (pIdentity != NULL) &&
		(pVerifier != NULL), "TLS stream resume fixture creation failed");
	xrtTlsServerConfigInit(&Test.ServerConfig);
	Test.ServerConfig.Context = pContext;
	Test.ServerConfig.Identity = pIdentity;
	Test.ServerConfig.Protocols = Protocols;
	Test.ServerConfig.ProtocolCount = 1u;
	Test.ServerConfig.RequireProtocol = true;
	Test.ServerConfig.Resume = testTlsStreamResumeLookup;
	Test.ServerConfig.ResumeContext = &Test;
	xrtTlsStreamConfigInit(&Test.StreamConfig);
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_TLS_STREAM_BACKEND;
	EngineConfig.Workers = 2u;
	Test.Engine = xrtNetEngineCreate(&EngineConfig);
	testRequire((Test.Engine != NULL) && xrtNetEngineStart(Test.Engine),
		"TLS stream resume engine start failed");
	xrtNetListenConfigInit(&ListenConfig);
	xrtNetStreamConfigInit(&TransportConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	), "TLS stream resume loopback setup failed");
	ListenConfig.Affinity = 0;
	ListenConfig.AcceptConcurrency = 4u;
	ListenConfig.Stream = TransportConfig;
	pListener = xrtNetListen(
		Test.Engine,
		&ListenConfig,
		&ListenerEvents,
		NULL,
		&Test
	);
	testRequire((pListener != NULL) &&
		xrtNetListenerLocal(pListener, &Address) &&
		(Address.Port != 0), "TLS stream resume listener failed");

	for ( uint32 iRound = 0; iRound < TEST_TLS_STREAM_RESUME_ROUNDS;
		iRound++ ) {
		xrtAtomic32Store(&Test.Round, iRound, XMEMORY_RELEASE);
		xrtAtomic32Store(
			&Test.TicketRequested,
			0,
			XMEMORY_RELEASE
		);
		xrtAtomic32Store(
			&Test.FifthIssued,
			0,
			XMEMORY_RELEASE
		);
		Test.NextClientResume = NULL;
		Test.PayloadSize = (size_t)snprintf(
			Test.Payload,
			sizeof(Test.Payload),
			"TLS stream resumed round %u",
			(unsigned)iRound
		);
		xrtTlsClientConfigInit(&ClientConfig);
		ClientConfig.Context = pContext;
		ClientConfig.ServerName = XRT_STR_LITERAL("example.com");
		ClientConfig.Protocols = Protocols;
		ClientConfig.ProtocolCount = 1u;
		ClientConfig.Verifier = pVerifier;
		ClientConfig.Resume = pClientResume;
		Test.Client.Stream = xrtTlsStreamConnect(
			Test.Engine,
			&Address,
			1u,
			&TransportConfig,
			&ClientConfig,
			&Test.StreamConfig,
			&Test.Events,
			&Test.Client
		);
		testRequire(Test.Client.Stream != NULL,
			"TLS stream resumed client creation failed");
		xrtTlsResumeRelease(pClientResume);
		pClientResume = NULL;
		testTlsStreamResumeWait(&Test.Accepted, iRound + 1u,
			"TLS stream resumed accept missing");
		testTlsStreamResumeWait(&Test.Client.Open, iRound + 1u,
			"TLS stream resumed client Open missing");
		testTlsStreamResumeWait(&Test.Server.Open, iRound + 1u,
			"TLS stream resumed server Open missing");
		testTlsStreamResumeWait(&Test.Client.Read, iRound + 1u,
			"TLS stream resumed client echo missing");
		testTlsStreamResumeWait(&Test.ResumeReady, iRound + 1u,
			"TLS stream resumed ticket missing");
		testTlsStreamResumeWait(&Test.Client.Close, iRound + 1u,
			"TLS stream resumed client Close missing");
		testTlsStreamResumeWait(&Test.Server.Close, iRound + 1u,
			"TLS stream resumed server Close missing");
		testRequire((xrtAtomic32Load(
			&Test.Client.Error,
			XMEMORY_ACQUIRE
		) == 0) && (xrtAtomic32Load(
			&Test.Server.Error,
			XMEMORY_ACQUIRE
		) == 0), "TLS stream resumed round reported an error");
		xrtTlsStreamDestroy(Test.Client.Stream);
		xrtTlsStreamDestroy(Test.Server.Stream);
		Test.Client.Stream = NULL;
		Test.Server.Stream = NULL;
		pClientResume = Test.NextClientResume;
		Test.NextClientResume = NULL;
	}

	testRequire(xrtNetListenerClose(pListener),
		"TLS stream resume listener close failed");
	testTlsStreamResumeWait(&Test.ListenerClose, 1u,
		"TLS stream resume listener Close missing");
	testRequire(xrtAtomic32Load(
		&Test.ListenerError,
		XMEMORY_ACQUIRE
	) == 0, "TLS stream resume listener reported an error");
	xrtNetListenerDestroy(pListener);
	testRequire(xrtNetEngineDestroy(Test.Engine),
		"TLS stream resume engine destroy failed");
	xrtTlsResumeRelease(pClientResume);
	xrtTlsResumeRelease(Test.ServerResume);
	xrtTlsVerifierRelease(pVerifier);
	xrtTlsIdentityRelease(pIdentity);
	xrtTlsContextRelease(pContext);
	printf("[PASS] TLS stream repeated TLS 1.3 resumption\n");
	return 0;
}
