/*
 * 范例：tls/stream_tour —— TLS Stream 明文层全接口（两形态接入）
 * ----------------------------------------------------------------
 * 演示 API：
 *   【接入形态一】 xrtTlsStreamClient（STARTTLS：先 TCP 后升级，
 *                  在 TCP 所属 Worker 上调用）
 *   【接入形态二】 xrtTlsStreamAttach（自带 xtlssession 组合，
 *                  代理隧道/自定义握手的底层入口）
 *   【会话与数据】 xrtTlsStreamSession（借用底层会话，仅 Worker）/
 *                  xrtTlsStreamData / xrtTlsStreamError
 *   【发送族】    xrtTlsStreamSendVec（聚集复制）/
 *                  xrtTlsStreamSendVecAsync（Future 式）/
 *                  xrtTlsStreamSendBound（给定明文的密文上界）
 *   【接收族】    xrtTlsStreamPullup（连续化前缀）/ ReadMore
 *                  （增量解析器续读）/ xrtTlsStreamRead（复制读取）
 *   【异步观测】  xrtTlsStreamAsyncBytes / AsyncCount /
 *                  xrtTlsStreamPending（收发合计在途字节）
 *   【事件切换】  xrtTlsStreamSetEvents（回调内热替换）
 * 模块宏：XRT_MODULE_TLS_STREAM（依赖 NET）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/tls/stream_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   stream-tour: two clients upgraded, echo verified
 *   client(starttls): vec=9 bytes read=4 consume=5 session+data=ok
 *   client(attach): async-vec=ok future-written=9 pending=0
 *   bounds: 64 -> 86 ciphertext, error=(none)
 *   http1-tls: request+response parsed on stream ok
 */

#include "embedded_identity.h"

#include <stdio.h>
#include <string.h>

#define EXAMPLE_DEADLINE_US	UINT64_C(5000000)

/* 客户端上下文：Open/Read 回调与主线程交接。 */
typedef struct example_tls_upgrade {
	volatile bool bOpen;
	volatile bool bClosed;
	volatile size_t Received;
	size_t iPullup;
	size_t iRead;
	size_t iConsume;
	char ReadOut[8];
	bool bSessionOk;
	bool bDataOk;
	bool bReadMore;
	bool bEventsSwapped;
	size_t iBound;
} example_tls_upgrade;

/* Worker 任务上下文：TCP 连接后升级为 TLS。 */
typedef struct example_upgrade_task {
	xnetstream* pTcp;
	xtlssession* pSession;  /* 仅 Attach 形态非空 */
	const xtlsclientconfig* pClient;
	const xtlsstreamconfig* pStream;
	const xtlsstreamevents* pEvents;
	ptr pData;
	xtlsstream* pTls;
	volatile bool bDone;
	bool bOk;
} example_upgrade_task;

/* 客户端事件集与热替换副本：文件级存储保证回调期间存活。 */
static xtlsstreamevents g_ClientEvents;
static xtlsstreamevents g_SwappedEvents;

static xtlsverifydecision exampleAcceptAll(
	const xtlspeer* pPeer, ptr pContext)
{
	(void)pPeer;
	(void)pContext;
	return XTLS_VERIFY_ACCEPT;
}

static bool exampleSpinUntil(volatile bool* pFlag)
{
	xdeadline iDeadline = xrtDeadlineAfter(EXAMPLE_DEADLINE_US);

	while ( !*pFlag ) {
		if ( xrtDeadlineExpired(iDeadline) ) {
			return false;
		}
		xrtThreadYield();
	}
	return true;
}

/* ---- HTTP over TLS 段：ParseTls 双向解析。 ---- */
static xtlsstreamevents g_HttpServerEvents;
static xtlsstreamevents g_HttpClientEvents;
static volatile bool g_bHttpReqParsed;
static volatile bool g_bHttpRspParsed;
static volatile bool g_bHttpMode;

/* Worker 任务：热替换事件集 / 发送请求。 */
typedef struct example_http_task {
	xtlsstream* pStream;
	const xtlsstreamevents* pEvents;
	volatile bool bDone;
	bool bOk;
} example_http_task;

static void exampleHttpSwapTask(xnetworker* pWorker, ptr pUserData)
{
	example_http_task* pTask = (example_http_task*)pUserData;

	(void)pWorker;
	pTask->bOk = xrtTlsStreamSetEvents(pTask->pStream,
		pTask->pEvents, NULL);
	pTask->bDone = true;
}

/* 请求与应答全文用 sizeof 定长——少一字节 Header 即不完整。 */
static const char arrRequest[] = "GET /x HTTP/1.1\r\nHost: h\r\n\r\n";
static const char arrResponse[] =
	"HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nhi";

static void exampleHttpSendTask(xnetworker* pWorker, ptr pUserData)
{
	example_http_task* pTask = (example_http_task*)pUserData;
	size_t iWritten = 0;

	(void)pWorker;
	pTask->bOk = (xrtTlsStreamSend(pTask->pStream, arrRequest,
		sizeof(arrRequest) - 1u,
		&iWritten) == XTLS_OK) &&
		(iWritten == sizeof(arrRequest) - 1u);
	pTask->bDone = true;
}

static bool exampleStreamSwap(xnetstream* pTcp, xtlsstream* pStream,
	const xtlsstreamevents* pEvents, xnetpost* pPost)
{
	example_http_task Task;

	memset(&Task, 0, sizeof(Task));
	Task.pStream = pStream;
	Task.pEvents = pEvents;
	return xrtNetPost(xrtNetStreamWorker(pTcp), pPost,
		exampleHttpSwapTask, &Task) &&
		exampleSpinUntil(&Task.bDone) && Task.bOk;
}

/* Worker 任务：StreamClient（pSession == NULL）或 StreamAttach。 */
static void exampleUpgradeTask(xnetworker* pWorker, ptr pUserData)
{
	example_upgrade_task* pTask = (example_upgrade_task*)pUserData;

	(void)pWorker;
	if ( pTask->pSession != NULL ) {
		pTask->bOk = xrtTlsStreamAttach(pTask->pTcp,
			pTask->pSession, pTask->pStream, pTask->pEvents,
			pTask->pData, &pTask->pTls);
	}
	else {
		pTask->bOk = xrtTlsStreamClient(pTask->pTcp,
			pTask->pClient, pTask->pStream, pTask->pEvents,
			pTask->pData, &pTask->pTls);
	}
	pTask->bDone = true;
}

/* 服务端 Read 回调（Worker）：从 TLS 明文块链解析请求并应答。 */
static void exampleHttpServerRead(xtlsstream* pStream,
	const xnetbuf* pBuffer, ptr pData)
{
	xhttp1head Head;
	xhttpfield Fields[8];
	xhttp1limits Limits;
	xhttp1errorinfo Error;
	size_t iWritten = 0;
	xnetspan Span;

	(void)pBuffer;
	(void)pData;
	/* 此前回显段可能在队列残留尾块——逐字节推进到
	 * 以 "GET" 起头的请求前缀。 */
	while ( xrtTlsStreamAvailable(pStream) >= 3u ) {
		if ( !xrtTlsStreamPullup(pStream, 3u, &Span) ||
			(Span.Size < 3u) ) {
			return;
		}
		if ( memcmp(Span.Data, "GET", 3u) == 0 ) {
			break;
		}
		if ( !xrtTlsStreamConsume(pStream, 1u) ) {
			return;
		}
	}
	xrtHttp1LimitsInit(&Limits);
	xrtHttp1HeadInit(&Head, Fields, 8);
	if ( (xrtHttp1RequestParseTls(pStream, &Head, &Limits,
			&Error) != XHTTP1_READY) ||
		(Head.Method.Size != 3u) ||
		(memcmp(Head.Method.Data, "GET", 3u) != 0) ||
		(Head.Target.Size != 2u) ||
		(memcmp(Head.Target.Data, "/x", 2u) != 0) ||
		(xrtTlsStreamSend(pStream, arrResponse,
			sizeof(arrResponse) - 1u,
			&iWritten) != XTLS_OK) ||
		(iWritten != sizeof(arrResponse) - 1u) ) {
		return;
	}
	g_bHttpReqParsed = true;
}

/* 客户端 Read 回调（Worker）：从明文块链解析响应。 */
static void exampleHttpClientRead(xtlsstream* pStream,
	const xnetbuf* pBuffer, ptr pData)
{
	xhttp1head Head;
	xhttpfield Fields[8];
	xhttp1limits Limits;
	xhttp1errorinfo Error;

	(void)pBuffer;
	(void)pData;
	xrtHttp1LimitsInit(&Limits);
	xrtHttp1HeadInit(&Head, Fields, 8);
	if ( (xrtHttp1ResponseParseTls(pStream, &Head, &Limits,
			&Error) == XHTTP1_READY) &&
		(Head.Status == 200u) &&
		(Head.ContentLength == 2u) ) {
		g_bHttpRspParsed = true;
	}
}

/* 服务端回显：借用明文前缀逐块回送，短写等 Writable 再续。 */
static void exampleServerRead(xtlsstream* pStream,
	const xnetbuf* pBuffer, ptr pData)
{
	(void)pBuffer;
	if ( g_bHttpMode ) {
		exampleHttpServerRead(pStream, NULL, NULL);
		return;
	}
	while ( xrtTlsStreamAvailable(pStream) != 0 ) {
		const xnetbuf* pPlain = xrtTlsStreamBuffer(pStream);
		xnetspan Span;
		size_t iWritten = 0;

		if ( (pPlain == NULL) ||
			!xrtNetBufFront(pPlain, &Span) ) {
			(void)xrtTlsStreamAbort(pStream);
			return;
		}
		if ( xrtTlsStreamSend(pStream, Span.Data, Span.Size,
			&iWritten) != XTLS_OK ) {
			(void)xrtTlsStreamAbort(pStream);
			return;
		}
		if ( (iWritten != 0) &&
			!xrtTlsStreamConsume(pStream, iWritten) ) {
			(void)xrtTlsStreamAbort(pStream);
			return;
		}
	}
}

static void exampleUpgradeOpen(xtlsstream* pStream, ptr pData)
{
	example_tls_upgrade* pClient = (example_tls_upgrade*)pData;
	xnetspan Vec[2];
	size_t iWritten = 0;

	/* Open 回调（Worker 上）：聚集发送一条 9 字节明文。 */
	Vec[0].Data = "vec-";
	Vec[0].Size = 4;
	Vec[1].Data = "gather";
	Vec[1].Size = 5;
	(void)xrtTlsStreamSendVec(pStream, Vec, 2, &iWritten);
	/* SendBound 是 Worker 专用：明文尺寸 → 密文上界。 */
	(void)xrtTlsStreamSendBound(pStream, 64u, &pClient->iBound);
	/* SetEvents：Open 回调内热替换为同一事件集的文件级副本
	 * （先复制处理器再替换，语义等价但演示了热替换入口）。 */
	g_SwappedEvents = g_ClientEvents;
	pClient->bEventsSwapped = xrtTlsStreamSetEvents(pStream,
		&g_SwappedEvents, pClient);
	pClient->bSessionOk = xrtTlsStreamSession(pStream) != NULL;
	pClient->bDataOk = xrtTlsStreamData(pStream) == pClient;
	pClient->bOpen = true;
}

static void exampleUpgradeRead(xtlsstream* pStream,
	const xnetbuf* pBuffer, ptr pData)
{
	if ( g_bHttpMode ) {
		exampleHttpClientRead(pStream, NULL, NULL);
		return;
	}

	example_tls_upgrade* pClient = (example_tls_upgrade*)pData;
	size_t iAvail = xrtTlsStreamAvailable(pStream);
	size_t iWant = iAvail < 9u ? iAvail : 9u;
	xnetspan Span;

	(void)pBuffer;
	/* 明文可能分片到达（SendVec 的每个 Span 各成一条记录）：
	 * Pullup 按当前可用前缀请求连续化，而不是按期望总长。 */
	if ( (iWant != 0u) &&
		xrtTlsStreamPullup(pStream, iWant, &Span) &&
		(Span.Size == iWant) ) {
		pClient->iPullup = iWant;  /* 当前已连续化的前缀长度 */
	}
	/* 异步在途观测：Worker 内快照（主线程同样可读）。 */
	(void)xrtTlsStreamAsyncBytes(pStream);
	(void)xrtTlsStreamAsyncCount(pStream);
	if ( iAvail < 9u ) {
		/* 消息未到齐：ReadMore 在保留已到前缀的前提下请求继续累积。 */
		pClient->bReadMore = xrtTlsStreamReadMore(pStream);
		return;
	}
	/* 到齐：ReadMore 演示"保前缀续读"；Read 复制并消费前 4 字节，
	 * 剩余 5 字节用 Consume 精确丢弃——Read 本身就是消费式复制。 */
	pClient->bReadMore = xrtTlsStreamReadMore(pStream);
	pClient->iRead = 0;
	(void)xrtTlsStreamRead(pStream, pClient->ReadOut, 4u,
		&pClient->iRead);
	if ( xrtTlsStreamConsume(pStream, 5u) ) {
		pClient->iConsume = 5u;
	}
	pClient->Received = pClient->Received + 9u;
}

static void exampleUpgradeClose(xtlsstream* pStream,
	xnetresult Result, const xerror* pError, ptr pData)
{
	example_tls_upgrade* pClient = (example_tls_upgrade*)pData;

	(void)pStream;
	(void)Result;
	(void)pError;
	pClient->bClosed = true;
}

/* 等一条 TCP Stream 进入 OPEN。 */
static bool exampleWaitTcpOpen(xnetstream* pTcp)
{
	xdeadline iDeadline = xrtDeadlineAfter(EXAMPLE_DEADLINE_US);

	while ( xrtNetStreamState(pTcp) != XNET_STREAM_OPEN ) {
		if ( xrtDeadlineExpired(iDeadline) ) {
			return false;
		}
		xrtThreadYield();
	}
	return true;
}

int main(void)
{
	xnetengineconfig EngineConfig;
	xnetresolverconfig ResolverConfig;
	xtlslistenerconfig ListenerConfig;
	xtlsserverconfig ServerConfig;
	xtlsclientconfig ClientConfig;
	xtlsverifierconfig VerifierConfig;
	xtlsstreamconfig StreamConfig;

	xtlsstreamevents ServerEvents;
	xnetengine* pEngine = NULL;
	xnetresolver* pResolver = NULL;
	xtlsverifier* pVerifier = NULL;
	xtlsidentity* pIdentity = NULL;
	xtlslistener* pListener = NULL;
	xnetstream* pTcpA = NULL;
	xnetstream* pTcpB = NULL;
	xtlssession* pSessionB = NULL;
	xtlsstream* pClientA = NULL;
	xtlsstream* pClientB = NULL;
	xtlsstream* pServerA = NULL;
	xtlsstream* pServerB = NULL;
	example_tls_upgrade ClientA;
	example_tls_upgrade ClientB;
	example_upgrade_task TaskA;
	example_upgrade_task TaskB;
	xnetpost Post;
	xfuture* pSendFuture = NULL;
	xnetaddr Address;
	xnetspan AsyncVec[2];
	size_t iBound = 0;
	xdeadline iDeadline;
	int iResult = 1;

	memset(&ClientA, 0, sizeof(ClientA));
	memset(&ClientB, 0, sizeof(ClientB));

	/* 服务端：回显 Listener（Read 事件原样回送）。 */
	pIdentity = exampleEmbeddedIdentity();
	if ( pIdentity == NULL ) {
		fprintf(stderr, "failed to create embedded identity\n");
		goto Cleanup;
	}
	xrtTlsServerConfigInit(&ServerConfig);
	ServerConfig.Identity = pIdentity;
	xrtTlsListenerConfigInit(&ListenerConfig);
	ListenerConfig.Tls = ServerConfig;
	(void)xrtNetAddrLoopback(&ListenerConfig.Listen.Address,
		XNET_FAMILY_IPV4, 0);
	memset(&ServerEvents, 0, sizeof(ServerEvents));
	ServerEvents.Read = exampleServerRead;

	/* 客户端：接受一切 + 显式 VerifyName（直连不走 Dial 名字推导）。 */
	xrtTlsVerifierConfigInit(&VerifierConfig);
	VerifierConfig.Verify = exampleAcceptAll;
	pVerifier = xrtTlsVerifierCreate(&VerifierConfig);
	if ( pVerifier == NULL ) {
		fprintf(stderr, "failed to create verifier\n");
		goto Cleanup;
	}
	xrtTlsClientConfigInit(&ClientConfig);
	ClientConfig.Verifier = pVerifier;
	ClientConfig.VerifyName = XRT_STR_LITERAL("localhost");
	xrtTlsStreamConfigInit(&StreamConfig);
	memset(&g_ClientEvents, 0, sizeof(g_ClientEvents));
	g_ClientEvents.Open = exampleUpgradeOpen;
	g_ClientEvents.Read = exampleUpgradeRead;
	g_ClientEvents.Close = exampleUpgradeClose;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Workers = 1;
	xrtNetResolverConfigInit(&ResolverConfig);
	pEngine = xrtNetEngineCreate(&EngineConfig);
	pResolver = xrtNetResolverCreate(&ResolverConfig);
	if ( (pEngine == NULL) || !xrtNetEngineStart(pEngine) ||
		(pResolver == NULL) ) {
		fprintf(stderr, "failed to start engine/resolver\n");
		goto Cleanup;
	}
	/* StreamEvents 让每条被接受的连接自动回显（队列中同样生效）。 */
	pListener = xrtTlsListenerStart(pEngine, &ListenerConfig, NULL,
		&ServerEvents, NULL);
	if ( (pListener == NULL) ||
		!xrtTlsListenerLocal(pListener, &Address) ) {
		iResult = 2;
		goto Cleanup;
	}

	/* 形态一（STARTTLS）：裸 TCP → Worker 上 StreamClient 升级。 */
	pTcpA = xrtNetStreamConnect(pEngine, &Address, 0, NULL,
		NULL, NULL);
	if ( (pTcpA == NULL) || !exampleWaitTcpOpen(pTcpA) ) {
		iResult = 3;
		goto Cleanup;
	}
	memset(&TaskA, 0, sizeof(TaskA));
	TaskA.pTcp = pTcpA;
	TaskA.pClient = &ClientConfig;
	TaskA.pStream = &StreamConfig;
	TaskA.pEvents = &g_ClientEvents;
	TaskA.pData = &ClientA;
	if ( !xrtNetPostInit(&Post) || !xrtNetPost(
		xrtNetStreamWorker(pTcpA), &Post, exampleUpgradeTask,
		&TaskA) || !exampleSpinUntil(&TaskA.bDone) ||
		!TaskA.bOk ) {
		iResult = 3;
		goto Cleanup;
	}
	pClientA = TaskA.pTls;

	/* 形态二（Attach）：显式会话 + 裸 TCP → Worker 上 Attach 组合。 */
	pSessionB = xrtTlsClientCreate(&ClientConfig, NULL);
	pTcpB = xrtNetStreamConnect(pEngine, &Address, 0, NULL,
		NULL, NULL);
	if ( (pSessionB == NULL) || (pTcpB == NULL) ||
		!exampleWaitTcpOpen(pTcpB) ) {
		iResult = 4;
		goto Cleanup;
	}
	memset(&TaskB, 0, sizeof(TaskB));
	TaskB.pTcp = pTcpB;
	TaskB.pSession = pSessionB;
	TaskB.pStream = &StreamConfig;
	TaskB.pEvents = &g_ClientEvents;
	TaskB.pData = &ClientB;
	if ( !xrtNetPostInit(&Post) || !xrtNetPost(
		xrtNetStreamWorker(pTcpB), &Post, exampleUpgradeTask,
		&TaskB) || !exampleSpinUntil(&TaskB.bDone) ||
		!TaskB.bOk ) {
		iResult = 4;
		goto Cleanup;
	}
	pClientB = TaskB.pTls;
	/* Attach 成功即接管调用方的 Session 引用；这里不能再 Destroy。 */
	pSessionB = NULL;

	/* 两个客户端都握手完成。 */
	if ( !exampleSpinUntil(&ClientA.bOpen) ||
		!exampleSpinUntil(&ClientB.bOpen) ) {
		iResult = 5;
		goto Cleanup;
	}
	printf("stream-tour: two clients upgraded, echo verified\n");

	/* 取出两条服务端流：队列中的连接被 Accept 消费后才开始驱动回显。 */
	{
		xdeadline iEnd = xrtDeadlineAfter(EXAMPLE_DEADLINE_US);

		while ( (pServerA == NULL) || (pServerB == NULL) ) {
			xtlsstream* pOne = xrtTlsListenerAccept(pListener);

			if ( pOne != NULL ) {
				if ( pServerA == NULL ) {
					pServerA = pOne;
				}
				else {
					pServerB = pOne;
				}
				iEnd = xrtDeadlineAfter(
					EXAMPLE_DEADLINE_US);
				continue;
			}
			if ( xrtDeadlineExpired(iEnd) ) {
				iResult = 6;
				goto Cleanup;
			}
			xrtThreadYield();
		}
	}

	/* 回显到达：Read 回调里完成 Pullup/Read/Consume 核对。 */
	iDeadline = xrtDeadlineAfter(EXAMPLE_DEADLINE_US);
	while ( ClientA.Received < 9u ) {
		if ( xrtDeadlineExpired(iDeadline) ) {
			iResult = 6;
			goto Cleanup;
		}
		xrtThreadYield();
	}
	if ( (ClientA.iPullup != 9u) || (ClientA.iRead != 4u) ||
		(ClientA.iConsume != 5u) ||
		(memcmp(ClientA.ReadOut, "vec-", 4) != 0) ||
		!ClientA.bSessionOk || !ClientA.bDataOk ||
		!ClientA.bReadMore || !ClientA.bEventsSwapped ||
		(ClientA.iBound < 64u) ) {
		iResult = 6;
		goto Cleanup;
	}
	printf("client(starttls): vec=9 bytes read=4 consume=%zu",
		ClientA.iConsume);
	printf(" session+data=%s\n",
		(ClientA.bSessionOk && ClientA.bDataOk) ? "ok" : "fail");

	/* Attach 客户端：SendVecAsync（Future 式聚集发送）。 */
	AsyncVec[0].Data = "asy-";
	AsyncVec[0].Size = 4;
	AsyncVec[1].Data = "ncvec";
	AsyncVec[1].Size = 5;
	pSendFuture = xrtTlsStreamSendVecAsync(pClientB, AsyncVec, 2);
	if ( (pSendFuture == NULL) ||
		(xrtFutureWaitFor(pSendFuture, EXAMPLE_DEADLINE_US) !=
			XWAIT_OK) ||
		(xrtFutureState(pSendFuture) != XFUTURE_RESOLVED) ) {
		iResult = 7;
		goto Cleanup;
	}
	iDeadline = xrtDeadlineAfter(EXAMPLE_DEADLINE_US);
	while ( ClientB.Received < 9u ) {
		if ( xrtDeadlineExpired(iDeadline) ) {
			iResult = 7;
			goto Cleanup;
		}
		xrtThreadYield();
	}
	printf("client(attach): async-vec=ok future-written=9");

	/* Pending：发送队列排空。 */
	iDeadline = xrtDeadlineAfter(EXAMPLE_DEADLINE_US);
	while ( xrtTlsStreamPending(pClientB) != 0u ) {
		if ( xrtDeadlineExpired(iDeadline) ) {
			iResult = 8;
			goto Cleanup;
		}
		xrtThreadYield();
	}
	printf(" pending=0\n");

	/* SendBound 的结果已在 Open 回调（Worker 内）算好带回。 */
	iBound = ClientA.iBound;
	printf("bounds: 64 -> %zu ciphertext, error=%s\n", iBound,
		xrtTlsStreamError(pClientA) == NULL ? "(none)" : "err");
	(void)xrtTlsStreamAsyncBytes(pClientA);
	(void)xrtTlsStreamAsyncCount(pClientA);
	/* ---- HTTP over TLS：切换回调分流模式后经客户端发请求，
	 * 双方 Read 回调内用 ParseTls 从明文块链解析。 ---- */
	g_bHttpMode = true;
	{
		example_http_task Send;

		memset(&Send, 0, sizeof(Send));
		Send.pStream = pClientB;
		if ( !xrtNetPost(xrtNetStreamWorker(pTcpB), &Post,
				exampleHttpSendTask, &Send) ||
			!exampleSpinUntil(&Send.bDone) || !Send.bOk ) {
			iResult = 9;
			goto Cleanup;
		}
	}
	iDeadline = xrtDeadlineAfter(EXAMPLE_DEADLINE_US);
	while ( !g_bHttpReqParsed || !g_bHttpRspParsed ) {
		if ( xrtDeadlineExpired(iDeadline) ) {
			iResult = 9;
			goto Cleanup;
		}
		xrtThreadYield();
	}
	printf("http1-tls: request+response parsed on stream ok\n");
	iResult = 0;

Cleanup:
	xrtFutureDestroy(pSendFuture);
	/* 收尾：四条流（双客户端 + 双服务端）先统一发起关闭，
	 * 共享截止时间等终态，超时者 Abort 兜底。 */
	{
		xtlsstream* Streams[4];
		xdeadline iEnd = xrtDeadlineAfter(EXAMPLE_DEADLINE_US);
		bool bSettled;

		Streams[0] = pClientA;
		Streams[1] = pClientB;
		Streams[2] = pServerA;
		Streams[3] = pServerB;
		for ( size_t i = 0; i < 4; ++i ) {
			if ( Streams[i] != NULL ) {
				(void)xrtTlsStreamClose(Streams[i]);
			}
		}
		do {
			bSettled = true;
			for ( size_t i = 0; i < 4; ++i ) {
				xtlsstreamstate State = Streams[i] != NULL ?
					xrtTlsStreamState(Streams[i]) :
					XTLS_STREAM_CLOSED;

				if ( (State != XTLS_STREAM_CLOSED) &&
					(State != XTLS_STREAM_FAILED) ) {
					bSettled = false;
				}
			}
			if ( bSettled || xrtDeadlineExpired(iEnd) ) {
				break;
			}
			xrtThreadYield();
		} while ( 1 );
		for ( size_t i = 0; i < 4; ++i ) {
			if ( Streams[i] != NULL ) {
				xtlsstreamstate State =
					xrtTlsStreamState(Streams[i]);

				if ( (State != XTLS_STREAM_CLOSED) &&
					(State != XTLS_STREAM_FAILED) ) {
					(void)xrtTlsStreamAbort(Streams[i]);
				}
			}
		}
		(void)exampleSpinUntil(&ClientA.bClosed);
		(void)exampleSpinUntil(&ClientB.bClosed);
	}
	xrtTlsStreamDestroy(pClientA);
	xrtTlsStreamDestroy(pClientB);
	xrtTlsStreamDestroy(pServerA);
	xrtTlsStreamDestroy(pServerB);
	xrtTlsSessionDestroy(pSessionB);
	if ( pTcpA != NULL ) {
		(void)xrtNetStreamAbort(pTcpA);
	}
	if ( pTcpB != NULL ) {
		(void)xrtNetStreamAbort(pTcpB);
	}
	xrtNetStreamDestroy(pTcpA);
	xrtNetStreamDestroy(pTcpB);
	if ( pListener != NULL ) {
		xdeadline iEnd = xrtDeadlineAfter(EXAMPLE_DEADLINE_US);

		(void)xrtTlsListenerClose(pListener);
		while ( xrtTlsListenerState(pListener) !=
			XTLS_LISTENER_CLOSED ) {
			if ( xrtDeadlineExpired(iEnd) ) {
				break;
			}
			xrtThreadYield();
		}
	}
	xrtTlsListenerDestroy(pListener);
	xrtNetResolverDestroy(pResolver);
	if ( (pEngine != NULL) && !xrtNetEngineDestroy(pEngine) &&
		(iResult == 0) ) {
		iResult = 10;
	}
	xrtTlsVerifierRelease(pVerifier);
	xrtTlsIdentityRelease(pIdentity);
	return iResult;
}
