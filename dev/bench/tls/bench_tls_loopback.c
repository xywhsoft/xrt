#include "../bench_common.h"

#define XRT_MODULE_TLS_STREAM
#define XRT_MODULE_TLS_CLIENT_RESUME
#define XRT_MODULE_TLS_SERVER_RESUME
#define XRT_MODULE_TLS_IDENTITY_RSA
#define XRT_MODULE_TLS_CLIENT_VERIFY
#define XRT_MODULE_TLS_SCHEDULE_SHA256
#define XRT_MODULE_TLS_KEY_EXCHANGE_X25519
#define XRT_MODULE_TLS_RECORD_AES
#define XRT_MODULE_PEM
#define XRT_IMPLEMENTATION
#include "../../../single/xrt.h"
#include "../network/bench_network_common.h"
#include "bench_tls_fixture.h"



#define BENCH_TLS_TIMEOUT UINT64_C(10000000)
#define BENCH_TLS_WARMUP_FULL 2u
#define BENCH_TLS_WARMUP_RESUME 2u



typedef struct benchtlscontext benchtlscontext;



/* 每个端点的明文重组和事件计数只由所属 Worker 修改。 */
typedef struct benchtlsendpoint {
	benchtlscontext* Context;
	xatomicptr Stream;
	xatomic32 Open;
	xatomic32 Messages;
	xatomic32 End;
	xatomic32 Close;
	uint8* Buffer;
	size_t Buffered;
	bool Server;
} benchtlsendpoint;



/* 全部连接共享不可变配置、票据链和当前一轮原子状态。 */
struct benchtlscontext {
	benchtlsendpoint Client;
	benchtlsendpoint Server;
	xtlsserverconfig ServerConfig;
	xtlsstreamconfig StreamConfig;
	xtlsstreamevents StreamEvents;
	xtlsverifier* Verifier;
	xnetengine* Engine;
	xatomicptr ServerResume;
	xatomicptr NextClientResume;
	xatomicptr HandshakeSamples;
	xatomicptr MessageSamples;
	xatomic64 HandshakeStart;
	xatomic64 MessagePhaseStart;
	xatomic64 MessageStart;
	xatomic64 MessageEnd;
	xatomic32 HandshakeIndex;
	xatomic32 TargetMessages;
	xatomic32 CompletedMessages;
	xatomic32 ExpectedResumed;
	xatomic32 Accepted;
	xatomic32 TicketReady;
	xatomic32 Closing;
	xatomic32 Errors;
	xatomic32 ListenerClose;
	xatomic32 ListenerError;
	const uint8* Payload;
	size_t PayloadSize;
};



/* 只打印一条首个失败，避免异步连锁错误淹没真实根因。 */
static void benchTlsFail(benchtlscontext* pContext, cstr sMessage)
{
	uint32 iPrevious = xrtAtomic32FetchAdd(
		&pContext->Errors,
		1,
		XMEMORY_ACQ_REL
	);

	if ( iPrevious == 0 ) {
		fprintf(stderr, "tls benchmark: %s", sMessage);
		xbenchPrintCurrentError();
		fputc('\n', stderr);
	}
}



/* 在统一截止时间前等待累计事件达到目标。 */
static bool benchTlsWait(
	const xatomic32* pValue,
	uint32 iExpected,
	cstr sMessage
)
{
	xdeadline Deadline = xrtDeadlineAfter(BENCH_TLS_TIMEOUT);

	while ( xrtAtomic32Load(pValue, XMEMORY_ACQUIRE) < iExpected ) {
		if ( xrtDeadlineExpired(Deadline) ) {
			fprintf(stderr, "tls benchmark timeout: %s\n", sMessage);
			return false;
		}
		xrtThreadYield();
	}
	return true;
}



/* 在一轮负载和新票据都完成后只发起一次认证关闭。 */
static void benchTlsMaybeClose(benchtlscontext* pContext)
{
	xtlsstream* pClient;

	if (
		(xrtAtomic32Load(
			&pContext->CompletedMessages,
			XMEMORY_ACQUIRE
		) < xrtAtomic32Load(
			&pContext->TargetMessages,
			XMEMORY_ACQUIRE
		)) ||
		(xrtAtomic32Load(
			&pContext->TicketReady,
			XMEMORY_ACQUIRE
		) == 0) ||
		(xrtAtomic32Exchange(
			&pContext->Closing,
			1,
			XMEMORY_ACQ_REL
		) != 0)
	) {
		return;
	}
	pClient = (xtlsstream*)xrtAtomicPtrLoad(
		&pContext->Client.Stream,
		XMEMORY_ACQUIRE
	);
	if ( (pClient == NULL) || !xrtTlsStreamClose(pClient) ) {
		benchTlsFail(pContext, "client close request failed");
	}
}



/* 在所属 Worker 上原子受理一个固定大小负载。 */
static bool benchTlsSendPayload(
	benchtlscontext* pContext,
	xtlsstream* pStream
)
{
	size_t iWritten = 0;
	xtlsresult Result = xrtTlsStreamSend(
		pStream,
		pContext->Payload,
		pContext->PayloadSize,
		&iWritten
	);

	if ( (Result != XTLS_OK) || (iWritten != pContext->PayloadSize) ) {
		benchTlsFail(pContext, "payload send was not accepted atomically");
		(void)xrtTlsStreamAbort(pStream);
		return false;
	}
	return true;
}



/* 客户端票据边沿接管最新票据，并释放同一边沿中的旧票据。 */
static void benchTlsTicket(xtlsstream* pStream, ptr pData)
{
	benchtlsendpoint* pEndpoint = (benchtlsendpoint*)pData;
	benchtlscontext* pContext = pEndpoint->Context;
	xtlssession* pSession = xrtTlsStreamSession(pStream);
	xtlsresume* pLatest = NULL;
	xtlsresume* pResume;

	if ( pEndpoint->Server || (pSession == NULL) ) {
		benchTlsFail(pContext, "ticket event role mismatch");
		(void)xrtTlsStreamAbort(pStream);
		return;
	}
	while ( (pResume = xrtTlsClientTakeResume(pSession)) != NULL ) {
		xrtTlsResumeRelease(pLatest);
		pLatest = pResume;
	}
	if ( pLatest == NULL ) {
		benchTlsFail(pContext, "ticket event did not expose a ticket");
		(void)xrtTlsStreamAbort(pStream);
		return;
	}
	pResume = (xtlsresume*)xrtAtomicPtrExchange(
		&pContext->NextClientResume,
		pLatest,
		XMEMORY_ACQ_REL
	);
	xrtTlsResumeRelease(pResume);
	xrtAtomic32Store(&pContext->TicketReady, 1, XMEMORY_RELEASE);
	benchTlsMaybeClose(pContext);
}



/* 服务端恢复查询返回当前票据快照，状态机立即保留自己的引用。 */
static const xtlsresume* benchTlsResumeLookup(
	ptr pData,
	const xtlsserverresumerequest* pRequest
)
{
	benchtlscontext* pContext = (benchtlscontext*)pData;

	if ( (pRequest == NULL) || (pRequest->Ticket.Size == 0) ) {
		benchTlsFail(pContext, "invalid server resume lookup");
		return NULL;
	}
	return (const xtlsresume*)xrtAtomicPtrLoad(
		&pContext->ServerResume,
		XMEMORY_ACQUIRE
	);
}



/* READY 边沿验证认证类型，服务端轮换票据，客户端启动负载。 */
static void benchTlsOpen(xtlsstream* pStream, ptr pData)
{
	benchtlsendpoint* pEndpoint = (benchtlsendpoint*)pData;
	benchtlscontext* pContext = pEndpoint->Context;
	xtlssession* pSession = xrtTlsStreamSession(pStream);
	bool bExpected = xrtAtomic32Load(
		&pContext->ExpectedResumed,
		XMEMORY_ACQUIRE
	) != 0;

	xrtAtomicPtrStore(&pEndpoint->Stream, pStream, XMEMORY_RELEASE);
	if ( pSession == NULL ) {
		benchTlsFail(pContext, "open event has no TLS session");
		(void)xrtTlsStreamAbort(pStream);
		return;
	}
	if ( pEndpoint->Server ) {
		xtlsresume* pResume = NULL;
		xtlsresume* pPrevious;
		size_t iWritten = 0;

		if ( xrtTlsServerResumed(pSession) != bExpected ) {
			benchTlsFail(pContext, "server resumed state mismatch");
			(void)xrtTlsStreamAbort(pStream);
			return;
		}
		if (
			(xrtTlsServerTicketNew(pSession, &pResume) != XTLS_OK) ||
			(pResume == NULL)
		) {
			benchTlsFail(pContext, "server ticket issue failed");
			(void)xrtTlsStreamAbort(pStream);
			return;
		}
		pPrevious = (xtlsresume*)xrtAtomicPtrExchange(
			&pContext->ServerResume,
			pResume,
			XMEMORY_ACQ_REL
		);
		xrtTlsResumeRelease(pPrevious);
		if (
			(xrtTlsStreamSend(pStream, NULL, 0, &iWritten) != XTLS_OK) ||
			(iWritten != 0)
		) {
			benchTlsFail(pContext, "server ticket flush failed");
			(void)xrtTlsStreamAbort(pStream);
			return;
		}
	} else {
		uint64* pSamples = (uint64*)xrtAtomicPtrLoad(
			&pContext->HandshakeSamples,
			XMEMORY_ACQUIRE
		);
		uint32 iIndex = xrtAtomic32Load(
			&pContext->HandshakeIndex,
			XMEMORY_ACQUIRE
		);
		uint64 iNow = xbenchNowNs();
		uint64 iStart = xrtAtomic64Load(
			&pContext->HandshakeStart,
			XMEMORY_ACQUIRE
		);

		if (
			(xrtTlsClientResumed(pSession) != bExpected) ||
			(xrtTlsClientCertificateCount(pSession) != (bExpected ? 0u : 1u))
		) {
			benchTlsFail(pContext, "client authentication state mismatch");
			(void)xrtTlsStreamAbort(pStream);
			return;
		}
		if ( pSamples != NULL ) {
			pSamples[iIndex] = iNow - iStart;
		}
		xrtAtomic64Store(&pContext->MessageStart, iNow, XMEMORY_RELEASE);
		xrtAtomic64Store(
			&pContext->MessagePhaseStart,
			iNow,
			XMEMORY_RELEASE
		);
		if ( !benchTlsSendPayload(pContext, pStream) ) {
			return;
		}
	}
	(void)xrtAtomic32FetchAdd(&pEndpoint->Open, 1, XMEMORY_RELEASE);
}



/* 每端按完整应用消息重组，服务端回声，客户端记录逐条往返延迟。 */
static void benchTlsRead(
	xtlsstream* pStream,
	const xnetbuf* pBuffer,
	ptr pData
)
{
	benchtlsendpoint* pEndpoint = (benchtlsendpoint*)pData;
	benchtlscontext* pContext = pEndpoint->Context;

	(void)pBuffer;
	while ( xrtTlsStreamAvailable(pStream) != 0 ) {
		size_t iRead = 0;
		size_t iRemaining = pContext->PayloadSize - pEndpoint->Buffered;

		if (
			(iRemaining == 0) ||
			(xrtTlsStreamRead(
				pStream,
				pEndpoint->Buffer + pEndpoint->Buffered,
				iRemaining,
				&iRead
			) != XTLS_OK) ||
			(iRead == 0)
		) {
			benchTlsFail(pContext, "plaintext read made no valid progress");
			(void)xrtTlsStreamAbort(pStream);
			return;
		}
		pEndpoint->Buffered += iRead;
		if ( pEndpoint->Buffered < pContext->PayloadSize ) {
			continue;
		}
		if ( memcmp(
			pEndpoint->Buffer,
			pContext->Payload,
			pContext->PayloadSize
		) != 0 ) {
			benchTlsFail(pContext, "plaintext payload mismatch");
			(void)xrtTlsStreamAbort(pStream);
			return;
		}
		pEndpoint->Buffered = 0;
		(void)xrtAtomic32FetchAdd(
			&pEndpoint->Messages,
			1,
			XMEMORY_RELEASE
		);
		if ( pEndpoint->Server ) {
			if ( !benchTlsSendPayload(pContext, pStream) ) {
				return;
			}
		} else {
			uint64 iNow = xbenchNowNs();
			uint64* pSamples = (uint64*)xrtAtomicPtrLoad(
				&pContext->MessageSamples,
				XMEMORY_ACQUIRE
			);
			uint32 iIndex = xrtAtomic32FetchAdd(
				&pContext->CompletedMessages,
				1,
				XMEMORY_ACQ_REL
			);
			uint32 iTarget = xrtAtomic32Load(
				&pContext->TargetMessages,
				XMEMORY_ACQUIRE
			);

			if ( pSamples != NULL ) {
				pSamples[iIndex] = iNow - xrtAtomic64Load(
					&pContext->MessageStart,
					XMEMORY_ACQUIRE
				);
			}
			if ( (iIndex + 1u) < iTarget ) {
				xrtAtomic64Store(
					&pContext->MessageStart,
					iNow,
					XMEMORY_RELEASE
				);
				if ( !benchTlsSendPayload(pContext, pStream) ) {
					return;
				}
			} else {
				xrtAtomic64Store(
					&pContext->MessageEnd,
					iNow,
					XMEMORY_RELEASE
				);
				benchTlsMaybeClose(pContext);
			}
		}
	}
}



/* 记录已经收到认证 close_notify。 */
static void benchTlsEnd(xtlsstream* pStream, ptr pData)
{
	benchtlsendpoint* pEndpoint = (benchtlsendpoint*)pData;

	(void)pStream;
	(void)xrtAtomic32FetchAdd(&pEndpoint->End, 1, XMEMORY_RELEASE);
}



/* 每轮必须以无根因 CLOSED 终态结束。 */
static void benchTlsClose(
	xtlsstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	benchtlsendpoint* pEndpoint = (benchtlsendpoint*)pData;

	if (
		(Result != XNET_RESULT_OK) ||
		(pError != NULL) ||
		(xrtTlsStreamState(pStream) != XTLS_STREAM_CLOSED)
	) {
		benchTlsFail(pEndpoint->Context, "TLS stream closed with an error");
	}
	(void)xrtAtomic32FetchAdd(&pEndpoint->Close, 1, XMEMORY_RELEASE);
}



/* Listener 在 TCP Accept 回调中建立服务端 TLS 组合对象。 */
static bool benchTlsAccept(
	xnetlistener* pListener,
	xnetstream* pTransport,
	ptr pData
)
{
	benchtlscontext* pContext = (benchtlscontext*)pData;
	xtlsstream* pStream = NULL;
	bool bAccepted;

	(void)pListener;
	bAccepted = xrtTlsStreamAccept(
		pTransport,
		&pContext->ServerConfig,
		&pContext->StreamConfig,
		&pContext->StreamEvents,
		&pContext->Server,
		&pStream
	);
	if ( bAccepted ) {
		xrtAtomicPtrStore(
			&pContext->Server.Stream,
			pStream,
			XMEMORY_RELEASE
		);
		(void)xrtAtomic32FetchAdd(
			&pContext->Accepted,
			1,
			XMEMORY_RELEASE
		);
	} else {
		benchTlsFail(pContext, "TLS stream accept failed");
	}
	return bAccepted;
}



/* Listener 错误与连接错误分开累计。 */
static void benchTlsListenerError(
	xnetlistener* pListener,
	const xerror* pError,
	ptr pData
)
{
	benchtlscontext* pContext = (benchtlscontext*)pData;

	(void)pListener;
	(void)pError;
	(void)xrtAtomic32FetchAdd(
		&pContext->ListenerError,
		1,
		XMEMORY_RELEASE
	);
	benchTlsFail(pContext, "listener reported an error");
}



/* 记录 Listener 唯一关闭完成。 */
static void benchTlsListenerClose(xnetlistener* pListener, ptr pData)
{
	benchtlscontext* pContext = (benchtlscontext*)pData;

	(void)pListener;
	(void)xrtAtomic32FetchAdd(
		&pContext->ListenerClose,
		1,
		XMEMORY_RELEASE
	);
}



/* 清空上一轮端点和跨 Worker 状态，但保留共享票据缓存。 */
static void benchTlsResetRound(
	benchtlscontext* pContext,
	bool bResumed,
	uint32 iMessages,
	uint64* pHandshakeSamples,
	uint32 iHandshakeIndex,
	uint64* pMessageSamples
)
{
	benchtlsendpoint* pEndpoints[] = {
		&pContext->Client,
		&pContext->Server
	};

	for ( size_t i = 0; i < 2u; i++ ) {
		xrtAtomicPtrStore(&pEndpoints[i]->Stream, NULL, XMEMORY_RELEASE);
		xrtAtomic32Store(&pEndpoints[i]->Open, 0, XMEMORY_RELEASE);
		xrtAtomic32Store(&pEndpoints[i]->Messages, 0, XMEMORY_RELEASE);
		xrtAtomic32Store(&pEndpoints[i]->End, 0, XMEMORY_RELEASE);
		xrtAtomic32Store(&pEndpoints[i]->Close, 0, XMEMORY_RELEASE);
		pEndpoints[i]->Buffered = 0;
	}
	xrtAtomicPtrStore(
		&pContext->HandshakeSamples,
		pHandshakeSamples,
		XMEMORY_RELEASE
	);
	xrtAtomicPtrStore(
		&pContext->MessageSamples,
		pMessageSamples,
		XMEMORY_RELEASE
	);
	xrtAtomic64Store(&pContext->HandshakeStart, 0, XMEMORY_RELEASE);
	xrtAtomic64Store(&pContext->MessagePhaseStart, 0, XMEMORY_RELEASE);
	xrtAtomic64Store(&pContext->MessageStart, 0, XMEMORY_RELEASE);
	xrtAtomic64Store(&pContext->MessageEnd, 0, XMEMORY_RELEASE);
	xrtAtomic32Store(
		&pContext->HandshakeIndex,
		iHandshakeIndex,
		XMEMORY_RELEASE
	);
	xrtAtomic32Store(
		&pContext->TargetMessages,
		iMessages,
		XMEMORY_RELEASE
	);
	xrtAtomic32Store(&pContext->CompletedMessages, 0, XMEMORY_RELEASE);
	xrtAtomic32Store(
		&pContext->ExpectedResumed,
		bResumed ? 1u : 0u,
		XMEMORY_RELEASE
	);
	xrtAtomic32Store(&pContext->Accepted, 0, XMEMORY_RELEASE);
	xrtAtomic32Store(&pContext->TicketReady, 0, XMEMORY_RELEASE);
	xrtAtomic32Store(&pContext->Closing, 0, XMEMORY_RELEASE);
}



/* 运行一次完整连接生命周期，并把新客户端票据转移给调用方。 */
static bool benchTlsRunConnection(
	benchtlscontext* pContext,
	const xnetaddr* pAddress,
	const xnetstreamconfig* pTransportConfig,
	const xtlsresume* pResume,
	uint32 iMessages,
	uint64* pHandshakeSamples,
	uint32 iHandshakeIndex,
	uint64* pMessageSamples,
	xtlsresume** ppNextResume
)
{
	static const xstrview Protocols[] = {
		XRT_STR_INIT("http/1.1")
	};
	xtlsclientconfig ClientConfig;
	xtlsstream* pClient = NULL;
	xtlsstream* pServer = NULL;
	xtlsresume* pNext = NULL;
	bool bResumed = pResume != NULL;
	bool bResult = false;

	*ppNextResume = NULL;
	benchTlsResetRound(
		pContext,
		bResumed,
		iMessages,
		pHandshakeSamples,
		iHandshakeIndex,
		pMessageSamples
	);
	if ( xrtAtomicPtrLoad(
		&pContext->NextClientResume,
		XMEMORY_ACQUIRE
	) != NULL ) {
		benchTlsFail(pContext, "previous client ticket was not consumed");
		goto Cleanup;
	}
	xrtTlsClientConfigInit(&ClientConfig);
	ClientConfig.Context = pContext->ServerConfig.Context;
	ClientConfig.ServerName = XRT_STR_LITERAL("example.com");
	ClientConfig.Protocols = Protocols;
	ClientConfig.ProtocolCount = 1u;
	ClientConfig.Verifier = pContext->Verifier;
	ClientConfig.Resume = pResume;
	xrtAtomic64Store(
		&pContext->HandshakeStart,
		xbenchNowNs(),
		XMEMORY_RELEASE
	);
	pClient = xrtTlsStreamConnect(
		pContext->Engine,
		pAddress,
		1u,
		pTransportConfig,
		&ClientConfig,
		&pContext->StreamConfig,
		&pContext->StreamEvents,
		&pContext->Client
	);
	if ( pClient == NULL ) {
		benchTlsFail(pContext, "client TLS stream creation failed");
		goto Cleanup;
	}
	xrtAtomicPtrStore(&pContext->Client.Stream, pClient, XMEMORY_RELEASE);
	if (
		!benchTlsWait(&pContext->Client.Close, 1u, "client close") ||
		!benchTlsWait(&pContext->Server.Close, 1u, "server close")
	) {
		benchTlsFail(pContext, "connection did not close before deadline");
		goto Cleanup;
	}
	pServer = (xtlsstream*)xrtAtomicPtrLoad(
		&pContext->Server.Stream,
		XMEMORY_ACQUIRE
	);
	pNext = (xtlsresume*)xrtAtomicPtrExchange(
		&pContext->NextClientResume,
		NULL,
		XMEMORY_ACQ_REL
	);
	if (
		(xrtAtomic32Load(&pContext->Errors, XMEMORY_ACQUIRE) != 0) ||
		(xrtAtomic32Load(&pContext->Accepted, XMEMORY_ACQUIRE) != 1u) ||
		(xrtAtomic32Load(&pContext->Client.Open, XMEMORY_ACQUIRE) != 1u) ||
		(xrtAtomic32Load(&pContext->Server.Open, XMEMORY_ACQUIRE) != 1u) ||
		(xrtAtomic32Load(&pContext->Client.Messages, XMEMORY_ACQUIRE) !=
			iMessages) ||
		(xrtAtomic32Load(&pContext->Server.Messages, XMEMORY_ACQUIRE) !=
			iMessages) ||
		(xrtAtomic32Load(&pContext->Client.End, XMEMORY_ACQUIRE) != 1u) ||
		(xrtAtomic32Load(&pContext->Server.End, XMEMORY_ACQUIRE) != 1u) ||
		(xrtAtomic32Load(&pContext->TicketReady, XMEMORY_ACQUIRE) != 1u) ||
		(pNext == NULL) ||
		(pServer == NULL)
	) {
		benchTlsFail(pContext, "connection lifecycle counters mismatch");
		goto Cleanup;
	}
	bResult = true;

Cleanup:
	if ( !bResult ) {
		if ( pClient != NULL ) {
			(void)xrtTlsStreamAbort(pClient);
		}
		pServer = (xtlsstream*)xrtAtomicPtrLoad(
			&pContext->Server.Stream,
			XMEMORY_ACQUIRE
		);
		if ( pServer != NULL ) {
			(void)xrtTlsStreamAbort(pServer);
		}
		if ( pClient != NULL ) {
			(void)benchTlsWait(
				&pContext->Client.Close,
				1u,
				"failed client close"
			);
		}
		if ( pServer != NULL ) {
			(void)benchTlsWait(
				&pContext->Server.Close,
				1u,
				"failed server close"
			);
		}
	}
	xrtAtomicPtrStore(&pContext->Client.Stream, NULL, XMEMORY_RELEASE);
	xrtAtomicPtrStore(&pContext->Server.Stream, NULL, XMEMORY_RELEASE);
	xrtTlsStreamDestroy(pClient);
	xrtTlsStreamDestroy(pServer);
	if ( bResult ) {
		*ppNextResume = pNext;
	} else {
		xrtTlsResumeRelease(pNext);
		pNext = (xtlsresume*)xrtAtomicPtrExchange(
			&pContext->NextClientResume,
			NULL,
			XMEMORY_ACQ_REL
		);
		xrtTlsResumeRelease(pNext);
	}
	return bResult;
}



/* 顺序运行一组完整或恢复握手，并持续轮换下一张票据。 */
static bool benchTlsRunHandshakePhase(
	benchtlscontext* pContext,
	const xnetaddr* pAddress,
	const xnetstreamconfig* pTransportConfig,
	bool bResumed,
	uint32 iCount,
	uint64* pSamples,
	xtlsresume** ppResume
)
{
	for ( uint32 i = 0; i < iCount; i++ ) {
		xtlsresume* pCurrent = *ppResume;
		xtlsresume* pNext = NULL;

		if ( bResumed && (pCurrent == NULL) ) {
			benchTlsFail(pContext, "resume phase has no client ticket");
			return false;
		}
		if ( !benchTlsRunConnection(
			pContext,
			pAddress,
			pTransportConfig,
			bResumed ? pCurrent : NULL,
			1u,
			pSamples,
			i,
			NULL,
			&pNext
		) ) {
			return false;
		}
		xrtTlsResumeRelease(pCurrent);
		*ppResume = pNext;
	}
	return true;
}



/* 汇总握手样本总耗时，并生成稳定分位数。 */
static void benchTlsHandshakeMetrics(
	uint64* pSamples,
	uint32 iCount,
	double* pRate,
	double* pP50,
	double* pP99
)
{
	uint64 iTotal = 0;

	for ( uint32 i = 0; i < iCount; i++ ) {
		iTotal += pSamples[i];
	}
	qsort(pSamples, iCount, sizeof(uint64), xbenchCompareU64);
	*pRate = xbenchSafeRate(iCount, iTotal);
	*pP50 = xbenchPercentileUs(pSamples, iCount, 0.50);
	*pP99 = xbenchPercentileUs(pSamples, iCount, 0.99);
}



/* 运行完整握手、票据恢复和加密流公共 API 回环基准。 */
int main(int argc, char** argv)
{
	static const xstrview Protocols[] = {
		XRT_STR_INIT("http/1.1")
	};
	uint32 iFullCount = xbenchArgU32(argc, argv, 1, 100u);
	uint32 iResumeCount = xbenchArgU32(argc, argv, 2, 200u);
	uint32 iMessageCount = xbenchArgU32(argc, argv, 3, 5000u);
	uint32 iMessageSize = xbenchArgU32(argc, argv, 4, 64u);
	benchtlscontext Context;
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetlistenerevents ListenerEvents;
	xnetstreamconfig TransportConfig;
	xtlsverifierconfig VerifierConfig;
	xtlscontext* pTlsContext = NULL;
	xtlsidentity* pIdentity = NULL;
	xtlsverifier* pVerifier = NULL;
	xnetengine* pEngine = NULL;
	xnetlistener* pListener = NULL;
	xtlsresume* pClientResume = NULL;
	xtlsresume* pNextResume = NULL;
	xnetaddr Address;
	xnetenginestats EngineStats;
	uint8* pPayload = NULL;
	uint8* pClientBuffer = NULL;
	uint8* pServerBuffer = NULL;
	uint64* pFullSamples = NULL;
	uint64* pResumeSamples = NULL;
	uint64* pMessageSamples = NULL;
	double fFullRate = 0.0;
	double fFullP50 = 0.0;
	double fFullP99 = 0.0;
	double fResumeRate = 0.0;
	double fResumeP50 = 0.0;
	double fResumeP99 = 0.0;
	double fMessageRate = 0.0;
	double fMessageP50 = 0.0;
	double fMessageP99 = 0.0;
	bool bResult = false;
	bool bClean = true;

	xbenchApplyCpuPinFromEnv();
	if (
		(iFullCount == 0) ||
		(iResumeCount == 0) ||
		(iMessageCount == 0) ||
		(iMessageSize == 0) ||
		(iMessageSize > UINT16_MAX)
	) {
		fprintf(stderr, "tls benchmark arguments are invalid\n");
		return 2;
	}
	memset(&Context, 0, sizeof(Context));
	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	memset(&EngineStats, 0, sizeof(EngineStats));
	pPayload = (uint8*)malloc(iMessageSize);
	pClientBuffer = (uint8*)malloc(iMessageSize);
	pServerBuffer = (uint8*)malloc(iMessageSize);
	pFullSamples = (uint64*)calloc(iFullCount, sizeof(uint64));
	pResumeSamples = (uint64*)calloc(iResumeCount, sizeof(uint64));
	pMessageSamples = (uint64*)calloc(iMessageCount, sizeof(uint64));
	if (
		(pPayload == NULL) ||
		(pClientBuffer == NULL) ||
		(pServerBuffer == NULL) ||
		(pFullSamples == NULL) ||
		(pResumeSamples == NULL) ||
		(pMessageSamples == NULL)
	) {
		goto Cleanup;
	}
	for ( uint32 i = 0; i < iMessageSize; i++ ) {
		pPayload[i] = (uint8)((i * 29u) ^ (i >> 1u) ^ 0xA5u);
	}
	Context.Payload = pPayload;
	Context.PayloadSize = iMessageSize;
	Context.Client.Context = &Context;
	Context.Client.Buffer = pClientBuffer;
	Context.Server.Context = &Context;
	Context.Server.Buffer = pServerBuffer;
	Context.Server.Server = true;
	Context.StreamEvents.Open = benchTlsOpen;
	Context.StreamEvents.Read = benchTlsRead;
	Context.StreamEvents.End = benchTlsEnd;
	Context.StreamEvents.Close = benchTlsClose;
	Context.StreamEvents.Ticket = benchTlsTicket;
	ListenerEvents.Accept = benchTlsAccept;
	ListenerEvents.Error = benchTlsListenerError;
	ListenerEvents.Close = benchTlsListenerClose;

	pTlsContext = benchTlsFixtureContext();
	pIdentity = benchTlsFixtureIdentity();
	xrtTlsVerifierConfigInit(&VerifierConfig);
	VerifierConfig.Verify = benchTlsFixtureAccept;
	pVerifier = xrtTlsVerifierCreate(&VerifierConfig);
	if (
		(pTlsContext == NULL) ||
		(pIdentity == NULL) ||
		(pVerifier == NULL)
	) {
		fprintf(stderr, "TLS fixture creation failed");
		xbenchPrintCurrentError();
		fputc('\n', stderr);
		goto Cleanup;
	}
	Context.Verifier = pVerifier;
	xrtTlsServerConfigInit(&Context.ServerConfig);
	Context.ServerConfig.Context = pTlsContext;
	Context.ServerConfig.Identity = pIdentity;
	Context.ServerConfig.Protocols = Protocols;
	Context.ServerConfig.ProtocolCount = 1u;
	Context.ServerConfig.RequireProtocol = true;
	Context.ServerConfig.Resume = benchTlsResumeLookup;
	Context.ServerConfig.ResumeContext = &Context;
	xrtTlsStreamConfigInit(&Context.StreamConfig);
	xrtNetStreamConfigInit(&TransportConfig);
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Workers = 2u;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	Context.Engine = pEngine;
	if ( (pEngine == NULL) || !xrtNetEngineStart(pEngine) ) {
		fprintf(stderr, "TLS benchmark engine start failed");
		xbenchPrintCurrentError();
		fputc('\n', stderr);
		goto Cleanup;
	}
	xbenchPrintNetworkBackend(pEngine);
	xrtNetListenConfigInit(&ListenConfig);
	if ( !xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	) ) {
		goto Cleanup;
	}
	ListenConfig.Affinity = 0;
	ListenConfig.AcceptConcurrency = 4u;
	ListenConfig.Stream = TransportConfig;
	pListener = xrtNetListen(
		pEngine,
		&ListenConfig,
		&ListenerEvents,
		NULL,
		&Context
	);
	if (
		(pListener == NULL) ||
		!xrtNetListenerLocal(pListener, &Address) ||
		(Address.Port == 0)
	) {
		fprintf(stderr, "TLS benchmark listen failed");
		xbenchPrintCurrentError();
		fputc('\n', stderr);
		goto Cleanup;
	}

	if (
		!benchTlsRunHandshakePhase(
			&Context,
			&Address,
			&TransportConfig,
			false,
			BENCH_TLS_WARMUP_FULL,
			NULL,
			&pClientResume
		) ||
		!benchTlsRunHandshakePhase(
			&Context,
			&Address,
			&TransportConfig,
			true,
			BENCH_TLS_WARMUP_RESUME,
			NULL,
			&pClientResume
		) ||
		!benchTlsRunHandshakePhase(
			&Context,
			&Address,
			&TransportConfig,
			false,
			iFullCount,
			pFullSamples,
			&pClientResume
		) ||
		!benchTlsRunHandshakePhase(
			&Context,
			&Address,
			&TransportConfig,
			true,
			iResumeCount,
			pResumeSamples,
			&pClientResume
		) ||
		!benchTlsRunConnection(
			&Context,
			&Address,
			&TransportConfig,
			pClientResume,
			iMessageCount,
			NULL,
			0,
			pMessageSamples,
			&pNextResume
		)
	) {
		goto Cleanup;
	}
	xrtTlsResumeRelease(pClientResume);
	pClientResume = pNextResume;
	pNextResume = NULL;
	benchTlsHandshakeMetrics(
		pFullSamples,
		iFullCount,
		&fFullRate,
		&fFullP50,
		&fFullP99
	);
	benchTlsHandshakeMetrics(
		pResumeSamples,
		iResumeCount,
		&fResumeRate,
		&fResumeP50,
		&fResumeP99
	);
	qsort(
		pMessageSamples,
		iMessageCount,
		sizeof(uint64),
		xbenchCompareU64
	);
	fMessageRate = xbenchSafeRate(
		iMessageCount,
		xrtAtomic64Load(&Context.MessageEnd, XMEMORY_ACQUIRE) -
			xrtAtomic64Load(&Context.MessagePhaseStart, XMEMORY_ACQUIRE)
	);
	fMessageP50 = xbenchPercentileUs(
		pMessageSamples,
		iMessageCount,
		0.50
	);
	fMessageP99 = xbenchPercentileUs(
		pMessageSamples,
		iMessageCount,
		0.99
	);
	bResult = true;

Cleanup:
	if ( pListener != NULL ) {
		if ( xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED ) {
			bClean = xrtNetListenerClose(pListener) && bClean;
		}
		bClean = benchTlsWait(
			&Context.ListenerClose,
			1u,
			"listener close"
		) && bClean;
		xrtNetListenerDestroy(pListener);
	}
	xrtTlsResumeRelease(pNextResume);
	xrtTlsResumeRelease(pClientResume);
	xrtTlsResumeRelease((xtlsresume*)xrtAtomicPtrExchange(
		&Context.NextClientResume,
		NULL,
		XMEMORY_ACQ_REL
	));
	xrtTlsResumeRelease((xtlsresume*)xrtAtomicPtrExchange(
		&Context.ServerResume,
		NULL,
		XMEMORY_ACQ_REL
	));
	if ( pEngine != NULL ) {
		if (
			!xrtNetEngineStats(pEngine, &EngineStats) ||
			(EngineStats.LiveObjects != 0) ||
			(EngineStats.WaitErrors != 0)
		) {
			bClean = false;
		}
		bClean = xrtNetEngineDestroy(pEngine) && bClean;
	}
	xrtTlsVerifierRelease(pVerifier);
	xrtTlsIdentityRelease(pIdentity);
	xrtTlsContextRelease(pTlsContext);
	free(pMessageSamples);
	free(pResumeSamples);
	free(pFullSamples);
	free(pServerBuffer);
	free(pClientBuffer);
	free(pPayload);
	if (
		!bResult ||
		!bClean ||
		(xrtAtomic32Load(&Context.Errors, XMEMORY_ACQUIRE) != 0) ||
		(xrtAtomic32Load(&Context.ListenerError, XMEMORY_ACQUIRE) != 0)
	) {
		return 1;
	}

	xbenchPrintMetricDouble("tls_full_handshakes_per_sec", fFullRate);
	xbenchPrintMetricDouble("tls_full_handshake_latency_p50_us", fFullP50);
	xbenchPrintMetricDouble("tls_full_handshake_latency_p99_us", fFullP99);
	xbenchPrintMetricDouble("tls_resume_handshakes_per_sec", fResumeRate);
	xbenchPrintMetricDouble("tls_resume_handshake_latency_p50_us", fResumeP50);
	xbenchPrintMetricDouble("tls_resume_handshake_latency_p99_us", fResumeP99);
	xbenchPrintMetricDouble("tls_stream_round_trips_per_sec", fMessageRate);
	xbenchPrintMetricDouble("tls_stream_latency_p50_us", fMessageP50);
	xbenchPrintMetricDouble("tls_stream_latency_p99_us", fMessageP99);
	return 0;
}
