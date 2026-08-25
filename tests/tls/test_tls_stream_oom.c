#include "../test.h"
#include "../../src/internal/xrt_tls_stream.h"



#define TEST_TLS_STREAM_OOM_BLOCK_LIMIT 2048u
#define TEST_TLS_STREAM_OOM_ADAPTER 1u



#if !defined(TEST_TLS_STREAM_BACKEND)
	#define TEST_TLS_STREAM_BACKEND XNET_PORT_SELECT
#endif



/* 定向底层故障只在目标尺寸类申请新 span 时生效。 */
typedef struct test_tls_stream_oom {
	xatomic32 Capture;
	xatomic64 CapturedSize;
	xatomic64 FailSize;
	xatomic32 FailureTarget;
	xatomic32 FailedTarget;
	xatomic32 WorkerEntered;
	xatomic32 WorkerRelease[2];
	xatomic32 Accepted;
	xatomic32 ClientOpen;
	xatomic32 ClientClose;
	xatomic32 ServerClose;
	xatomic32 ListenerClose;
	xatomic32 ReservedTimerDone;
	xnetstream* Server;
	xnetresult ClientResult;
	xnetresult ReservedTimerResult;
	xerrkind ClientError;
} test_tls_stream_oom;



/* 捕获或拒绝目标 span，其余请求交给 C 运行库。 */
static ptr testTlsStreamOomAlloc(ptr pData, size_t iSize)
{
	test_tls_stream_oom* pContext = (test_tls_stream_oom*)pData;
	uint32 iCapture = 1;
	uint64 iExpected = (uint64)iSize;

	if ( xrtAtomic32CompareExchange(
		&pContext->Capture,
		&iCapture,
		0,
		XMEMORY_ACQ_REL,
		XMEMORY_ACQUIRE
	) ) {
		xrtAtomic64Store(
			&pContext->CapturedSize,
			(uint64)iSize,
			XMEMORY_RELEASE
		);
		return NULL;
	}
	if ( xrtAtomic64CompareExchange(
		&pContext->FailSize,
		&iExpected,
		0,
		XMEMORY_ACQ_REL,
		XMEMORY_ACQUIRE
	) ) {
		xrtAtomic32Store(
			&pContext->FailedTarget,
			xrtAtomic32Load(
				&pContext->FailureTarget,
				XMEMORY_ACQUIRE
			),
			XMEMORY_RELEASE
		);
		return NULL;
	}
	return malloc(iSize);
}



/* 重分配不参与尺寸类故障，保持标准 realloc 语义。 */
static ptr testTlsStreamOomRealloc(
	ptr pData,
	ptr pMemory,
	size_t iSize
)
{
	(void)pData;
	return realloc(pMemory, iSize);
}



/* 释放成功取得的底层内存。 */
static void testTlsStreamOomFree(ptr pData, ptr pMemory)
{
	(void)pData;
	free(pMemory);
}



/* 在截止时间前等待一个并发事件。 */
static void testTlsStreamOomWait(
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



/* 耗尽一个小对象尺寸类，并捕获下一 span 的底层申请尺寸。 */
static size_t testTlsStreamOomExhaust(
	test_tls_stream_oom* pContext,
	size_t iObjectSize,
	ptr* pBlocks,
	size_t iCapacity
)
{
	size_t iCount = 0;

	xrtAtomic64Store(&pContext->CapturedSize, 0, XMEMORY_RELEASE);
	xrtAtomic32Store(&pContext->Capture, 1, XMEMORY_RELEASE);
	while ( iCount < iCapacity ) {
		ptr pBlock = xrtCalloc(1, iObjectSize);

		if ( pBlock == NULL ) {
			break;
		}
		pBlocks[iCount++] = pBlock;
	}
	testRequire((iCount < iCapacity) &&
		(xrtAtomic32Load(
			&pContext->Capture,
			XMEMORY_ACQUIRE
		) == 0) && (xrtAtomic64Load(
			&pContext->CapturedSize,
			XMEMORY_ACQUIRE
		) != 0), "TLS stream OOM class exhaustion failed");
	xrtClearError();
	return iCount;
}



/* 释放用于耗尽尺寸类的全部占位对象。 */
static void testTlsStreamOomReleaseBlocks(ptr* pBlocks, size_t iCount)
{
	for ( size_t i = 0; i < iCount; i++ ) {
		xrtFree(pBlocks[i]);
	}
}



/* 让下一次相同 span 申请失败，并记录它对应的测试目标。 */
static void testTlsStreamOomArm(
	test_tls_stream_oom* pContext,
	uint32 iTarget
)
{
	xrtAtomic32Store(&pContext->FailedTarget, 0, XMEMORY_RELEASE);
	xrtAtomic32Store(
		&pContext->FailureTarget,
		iTarget,
		XMEMORY_RELEASE
	);
	xrtAtomic64Store(
		&pContext->FailSize,
		xrtAtomic64Load(
			&pContext->CapturedSize,
			XMEMORY_ACQUIRE
		),
		XMEMORY_RELEASE
	);
}



/* 占住指定 Worker，隔离客户端 Timer 与服务端 accept 的尺寸类分配。 */
static void testTlsStreamOomBlockWorker(xnetworker* pWorker, ptr pData)
{
	test_tls_stream_oom* pContext = (test_tls_stream_oom*)pData;
	uint32 iIndex = xrtNetWorkerIndex(pWorker);

	(void)xrtAtomic32FetchAdd(
		&pContext->WorkerEntered,
		1,
		XMEMORY_ACQ_REL
	);
	while ( xrtAtomic32Load(
		&pContext->WorkerRelease[iIndex],
		XMEMORY_ACQUIRE
	) == 0 ) {
		xrtThreadYield();
	}
}



/* 预留 Timer 的终态证明取消后槽位与对象均被正常回收。 */
static void testTlsStreamOomReservedTimer(
	xnetworker* pWorker,
	uint64 Id,
	xnetresult Result,
	ptr pData
)
{
	test_tls_stream_oom* pContext = (test_tls_stream_oom*)pData;

	(void)pWorker;
	testRequire(Id != 0, "TLS stream reserved Timer returned zero identity");
	pContext->ReservedTimerResult = Result;
	xrtAtomic32Store(
		&pContext->ReservedTimerDone,
		1,
		XMEMORY_RELEASE
	);
}



/* 原始服务端只接管 TCP，用于让客户端进入 TLS 握手 Timer 路径。 */
static bool testTlsStreamOomAccept(
	xnetlistener* pListener,
	xnetstream* pStream,
	ptr pData
)
{
	test_tls_stream_oom* pContext = (test_tls_stream_oom*)pData;

	(void)pListener;
	testRequire(xrtNetStreamSetData(pStream, pContext),
		"TLS stream OOM accepted stream data failed");
	pContext->Server = pStream;
	xrtAtomic32Store(&pContext->Accepted, 1, XMEMORY_RELEASE);
	return true;
}



/* 记录原始服务端 TCP 的最终关闭。 */
static void testTlsStreamOomServerClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_tls_stream_oom* pContext = (test_tls_stream_oom*)pData;

	(void)pStream;
	(void)Result;
	(void)pError;
	xrtAtomic32Store(&pContext->ServerClose, 1, XMEMORY_RELEASE);
}



/* Timer OOM 必须在发布 TLS Open 之前终止组合连接。 */
static void testTlsStreamOomOpen(xtlsstream* pStream, ptr pData)
{
	test_tls_stream_oom* pContext = (test_tls_stream_oom*)pData;

	(void)pStream;
	xrtAtomic32Store(&pContext->ClientOpen, 1, XMEMORY_RELEASE);
}



/* 保存回调期根错误，稍后与组合对象持有的错误核对。 */
static void testTlsStreamOomClose(
	xtlsstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_tls_stream_oom* pContext = (test_tls_stream_oom*)pData;

	(void)pStream;
	pContext->ClientResult = Result;
	pContext->ClientError = pError != NULL ?
		xrtErrorKind(pError) : XERR_NONE;
	xrtAtomic32Store(&pContext->ClientClose, 1, XMEMORY_RELEASE);
}



/* 记录 Listener 的唯一关闭终态。 */
static void testTlsStreamOomListenerClose(
	xnetlistener* pListener,
	ptr pData
)
{
	test_tls_stream_oom* pContext = (test_tls_stream_oom*)pData;

	(void)pListener;
	xrtAtomic32Store(&pContext->ListenerClose, 1, XMEMORY_RELEASE);
}



/* 精确拒绝组合对象本体，并验证失败后仍可恢复创建和销毁。 */
static void testTlsStreamObjectOom(
	test_tls_stream_oom* pContext,
	const xtlsclientconfig* pClient
)
{
	ptr Blocks[TEST_TLS_STREAM_OOM_BLOCK_LIMIT];
	xtlssession* pSession = xrtTlsClientCreate(pClient, NULL);
	xtlsstream* pStream;
	size_t iCount;

	testRequire(pSession != NULL,
		"TLS stream OOM session creation failed");
	iCount = testTlsStreamOomExhaust(
		pContext,
		sizeof(xtlsstream),
		Blocks,
		TEST_TLS_STREAM_OOM_BLOCK_LIMIT
	);
	testTlsStreamOomArm(pContext, TEST_TLS_STREAM_OOM_ADAPTER);
	xrtClearError();
	pStream = __xrtTlsStreamCreate(
		pSession,
		false,
		NULL,
		NULL,
		NULL
	);
	testRequire((pStream == NULL) && (xrtAtomic32Load(
		&pContext->FailedTarget,
		XMEMORY_ACQUIRE
	) == TEST_TLS_STREAM_OOM_ADAPTER) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"TLS stream object OOM mismatch");
	xrtClearError();
	testTlsStreamOomReleaseBlocks(Blocks, iCount);

	pStream = __xrtTlsStreamCreate(
		pSession,
		false,
		NULL,
		NULL,
		NULL
	);
	testRequire((pStream != NULL) &&
		(xrtTlsStreamState(pStream) == XTLS_STREAM_CONNECTING),
		"TLS stream object did not recover after OOM");
	xrtTlsStreamDestroy(pStream);
	xrtTlsStreamDestroy(pStream);
}



/* 验证组合对象 OOM 与握手 Timer 拒绝均保留根因且可完整回收。 */

static xtlsverifydecision testDialEdgeAcceptPeer(
	const xtlspeer* pPeer,
	ptr pContext
)
{
	(void)pPeer;
	(void)pContext;
	return XTLS_VERIFY_ACCEPT;
}

int main(void)
{
	test_tls_stream_oom Test;
	xallocator Allocator;
	xtlscontext* pTlsContext;
	xtlsclientconfig ClientConfig;
	xtlsstreamconfig TlsConfig;
	xtlsstreamevents TlsEvents;
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetlistenerevents ListenerEvents;
	xnetstreamevents ServerEvents;
	xnetstreamconfig TransportConfig;
	xnetengine* pEngine;
	xtlsverifier* pVerifier;
	xnetlistener* pListener;
	xtlsstream* pClient;
	xnetaddr Address;
	xnetenginestats Stats;
	uint64 iReservedTimer;

	memset(&Test, 0, sizeof(Test));
	Allocator.Context = &Test;
	Allocator.Alloc = testTlsStreamOomAlloc;
	Allocator.Realloc = testTlsStreamOomRealloc;
	Allocator.Free = testTlsStreamOomFree;
	testRequire(xrtSetAllocator(&Allocator),
		"TLS stream OOM allocator install failed");

	pTlsContext = xrtTlsContextCreate(NULL);
	testRequire(pTlsContext != NULL,
		"TLS stream OOM context creation failed");
	xrtTlsClientConfigInit(&ClientConfig);
	ClientConfig.Context = pTlsContext;
	{
		xtlsverifierconfig VerifierConfig;

		xrtTlsVerifierConfigInit(&VerifierConfig);
		VerifierConfig.Verify = testDialEdgeAcceptPeer;
		pVerifier = xrtTlsVerifierCreate(&VerifierConfig);
	}
	testRequire(pVerifier != NULL,
		"TLS stream OOM verifier creation failed");
	ClientConfig.Verifier = pVerifier;
	ClientConfig.ServerName = XRT_STR_LITERAL("example.com");
	testTlsStreamObjectOom(&Test, &ClientConfig);

	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	memset(&ServerEvents, 0, sizeof(ServerEvents));
	memset(&TlsEvents, 0, sizeof(TlsEvents));
	ListenerEvents.Accept = testTlsStreamOomAccept;
	ListenerEvents.Close = testTlsStreamOomListenerClose;
	ServerEvents.Close = testTlsStreamOomServerClose;
	TlsEvents.Open = testTlsStreamOomOpen;
	TlsEvents.Close = testTlsStreamOomClose;
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_TLS_STREAM_BACKEND;
	EngineConfig.Workers = 2u;
	EngineConfig.TimerLimit = 1u;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"TLS stream OOM engine start failed");
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	), "TLS stream OOM listen address failed");
	ListenConfig.Affinity = 0;
	ListenConfig.AcceptConcurrency = 1u;
	pListener = xrtNetListen(
		pEngine,
		&ListenConfig,
		&ListenerEvents,
		&ServerEvents,
		&Test
	);
	testRequire((pListener != NULL) &&
		xrtNetListenerLocal(pListener, &Address),
		"TLS stream OOM listener creation failed");
	testRequire(xrtNetEnginePost(
		pEngine,
		0,
		testTlsStreamOomBlockWorker,
		&Test
	) && xrtNetEnginePost(
		pEngine,
		1,
		testTlsStreamOomBlockWorker,
		&Test
	), "TLS stream OOM workers block failed");
	testTlsStreamOomWait(
		&Test.WorkerEntered,
		2,
		"TLS stream OOM workers did not block"
	);
	iReservedTimer = xrtNetEngineAfter(
		pEngine,
		1,
		60000000u,
		testTlsStreamOomReservedTimer,
		&Test
	);
	testRequire(iReservedTimer != 0,
		"TLS stream Timer slot reservation failed");

	xrtNetStreamConfigInit(&TransportConfig);
	TransportConfig.ConnectTimeout = 0;
	xrtTlsStreamConfigInit(&TlsConfig);
	pClient = xrtTlsStreamConnect(
		pEngine,
		&Address,
		1,
		&TransportConfig,
		&ClientConfig,
		&TlsConfig,
		&TlsEvents,
		&Test
	);
	testRequire(pClient != NULL,
		"TLS stream OOM client creation failed");
	xrtAtomic32Store(&Test.WorkerRelease[1], 1, XMEMORY_RELEASE);
	testTlsStreamOomWait(
		&Test.ClientClose,
		1,
		"TLS stream Timer OOM did not close the client"
	);
	{
		const xerror* pStreamError = xrtTlsStreamError(pClient);
		bool bStats = xrtNetEngineStats(pEngine, &Stats);

		if ( (xrtAtomic32Load(
			&Test.ClientOpen,
			XMEMORY_ACQUIRE
		) != 0) || (Test.ClientError != XERR_AGAIN) ||
			(xrtTlsStreamState(pClient) != XTLS_STREAM_FAILED) ||
			(pStreamError == NULL) ||
			(xrtErrorKind(pStreamError) != XERR_AGAIN) ||
			!bStats || (Stats.TimersRejected == 0) ) {
			fprintf(
				stderr,
				"TLS Timer rejection open=%u result=%d callback=%d "
				"state=%d object=%d stats=%d rejected=%llu\n",
				xrtAtomic32Load(
					&Test.ClientOpen,
					XMEMORY_ACQUIRE
				),
				(int)Test.ClientResult,
				(int)Test.ClientError,
				(int)xrtTlsStreamState(pClient),
				pStreamError != NULL ?
					(int)xrtErrorKind(pStreamError) : -1,
				bStats ? 1 : 0,
				(unsigned long long)(bStats ? Stats.TimersRejected : 0)
			);
		}
	testRequire((xrtAtomic32Load(
		&Test.ClientOpen,
		XMEMORY_ACQUIRE
	) == 0) && (Test.ClientResult == XNET_RESULT_ERROR) &&
		(Test.ClientError == XERR_AGAIN) &&
		(xrtTlsStreamState(pClient) == XTLS_STREAM_FAILED) &&
		(xrtTlsStreamError(pClient) != NULL) &&
		(xrtErrorKind(xrtTlsStreamError(pClient)) == XERR_AGAIN) &&
		bStats && (Stats.TimersRejected != 0),
		"TLS stream Timer rejection root cause mismatch");
	}
	testRequire(xrtNetEngineTimerCancel(pEngine, iReservedTimer),
		"TLS stream reserved Timer cancel failed");
	testTlsStreamOomWait(
		&Test.ReservedTimerDone,
		1,
		"TLS stream reserved Timer did not finish"
	);
	testRequire(Test.ReservedTimerResult == XNET_RESULT_CANCELLED,
		"TLS stream reserved Timer cancel result mismatch");
	xrtAtomic32Store(&Test.WorkerRelease[0], 1, XMEMORY_RELEASE);

	testTlsStreamOomWait(
		&Test.Accepted,
		1,
		"TLS stream OOM raw server was not accepted"
	);
	testRequire(xrtNetStreamClose(Test.Server),
		"TLS stream OOM raw server close failed");
	testTlsStreamOomWait(
		&Test.ServerClose,
		1,
		"TLS stream OOM raw server close callback missing"
	);
	testRequire(xrtNetListenerClose(pListener),
		"TLS stream OOM listener close failed");
	testTlsStreamOomWait(
		&Test.ListenerClose,
		1,
		"TLS stream OOM listener close callback missing"
	);

	xrtTlsStreamDestroy(pClient);
	xrtNetStreamDestroy(Test.Server);
	xrtNetListenerDestroy(pListener);
	testRequire(xrtNetEngineDestroy(pEngine),
		"TLS stream OOM engine destroy failed");
	xrtTlsVerifierRelease(pVerifier);
	xrtTlsContextRelease(pTlsContext);
	xrtClearError();
	printf("[PASS] TLS stream object OOM and Timer rejection\n");
	return 0;
}
