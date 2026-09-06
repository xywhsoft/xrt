/*
 * 范例：tls/listener_tour —— TLS Listener 全接口 + 回调式 Dial 自省
 * ----------------------------------------------------------------
 * 演示 API：
 *   【Listener】  xrtTlsListenerConfigInit / Start（发布完成握手的流）/
 *                 Accept（拉取）/ AcceptAsync（Future）/ AcceptWait（阻塞）/
 *                 Close / State / Local / Data / Stats / Ref / Destroy
 *   【Dial】      xrtTlsDial（回调式，成功回调接管 TLS Stream）/
 *                 xrtTlsDialRef / DialCancel / DialState / DialError /
 *                 DialTransportStats / DialConfigInit / DialDestroy
 *   【Stream 连接】 xrtTlsStreamConnect（数字地址直连，免 DNS）
 * 模块宏：XRT_MODULE_TLS_STREAM（依赖 NET）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/tls/listener_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   tls-listener: local=127.0.0.1:NNNNN state=0 data=ok
 *   accept-pull: dial state=3 transport(attempts=1) error=(none)
 *   greet: client received 9 bytes
 *   accept-async: future stream=ok
 *   accept-wait: blocking stream=ok
 *   stats: accepted=3 handshakes>=3
 *   dial-fail: refused state=4 error-set=1
 *   dial-cancel: midair state=5
 *   cancel-finished=0
 *
 * 服务端身份来自嵌入的自签名 P-256 证书（embedded_identity.h）；
 *   客户端验证器用自定义回调全盘接受——仅限回环演示，
 *   生产环境必须使用真实信任库（参见 tls/dial 范例）。
 */

#include "../embedded_identity.h"

#include <stdio.h>
#include <string.h>

#define EXAMPLE_DEADLINE_US	UINT64_C(5000000)

/* 完成交接块：Dial 回调与主线程之间。 */
typedef struct example_tlssession_slot {
	volatile xtlsdialstate DialState;
	xnetresult Result;
	xtlsstream* pStream;
	volatile bool bDone;
} example_tlssession_slot;

/* 客户端流事件上下文：统计明文到达并在 Close 时收尾。 */
typedef struct example_tls_client {
	volatile size_t Received;
	volatile bool bOpen;
	volatile bool bClosed;
} example_tls_client;

/* 演示用"全盘接受"验证回调——仅回环自签名场景。 */
static xtlsverifydecision exampleAcceptAll(
	const xtlspeer* pPeer, ptr pContext)
{
	(void)pPeer;
	(void)pContext;
	return XTLS_VERIFY_ACCEPT;
}

/* Dial 完成回调：成功接管 Stream 引用。 */
static void exampleDialDone(xtlsdial* pDial, xnetresult Result,
	xtlsstream* pStream, const xerror* pError, ptr pData)
{
	example_tlssession_slot* pSlot = (example_tlssession_slot*)pData;

	(void)pError;
	pSlot->Result = Result;
	pSlot->pStream = pStream;
	pSlot->DialState = xrtTlsDialState(pDial);
	pSlot->bDone = true;
}

static void exampleClientOpen(xtlsstream* pStream, ptr pData)
{
	example_tls_client* pClient = (example_tls_client*)pData;

	(void)pStream;
	pClient->bOpen = true;
}

static void exampleClientRead(xtlsstream* pStream,
	const xnetbuf* pBuffer, ptr pData)
{
	example_tls_client* pClient = (example_tls_client*)pData;

	(void)pStream;
	pClient->Received = pClient->Received + xrtNetBufSize(pBuffer);
}

static void exampleClientClose(xtlsstream* pStream,
	xnetresult Result, const xerror* pError, ptr pData)
{
	example_tls_client* pClient = (example_tls_client*)pData;

	(void)pStream;
	(void)Result;
	(void)pError;
	pClient->bClosed = true;
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

/* 服务端方向发一条问候（TLS Stream 全部 IO 都在 Worker 上执行）。 */
static void exampleServerGreet(xnetworker* pWorker, ptr pData)
{
	xtlsstream* pStream = (xtlsstream*)pData;
	size_t iWritten = 0;

	(void)pWorker;
	(void)xrtTlsStreamSend(pStream, "hello-tls", 9, &iWritten);
}

int main(void)
{
	xnetengineconfig EngineConfig;
	xnetresolverconfig ResolverConfig;
	xtlslistenerconfig ListenerConfig;
	xtlsserverconfig ServerConfig;
	xtlsclientconfig ClientConfig;
	xtlsclientconfig ClientConfigB;
	xtlsverifierconfig VerifierConfig;
	xtlsdialconfig DialConfig;
	xtlsstreamevents ClientEvents;
	xnetengine* pEngine = NULL;
	xnetresolver* pResolver = NULL;
	xtlsverifier* pVerifier = NULL;
	xtlsidentity* pIdentity = NULL;
	xtlslistener* pListener = NULL;
	xtlslistener* pListenerRef = NULL;
	xtlsdial* pDial = NULL;
	xtlsdial* pFailDial = NULL;
	xtlsdial* pCancelDial = NULL;
	xtlsstream* pClientA = NULL;
	xtlsstream* pClientB = NULL;
	xtlsstream* pClientC = NULL;
	xtlsstream* pServerA = NULL;
	xtlsstream* pServerB = NULL;
	xtlsstream* pServerC = NULL;
	xfuture* pAcceptFuture = NULL;
	example_tlssession_slot Slot;
	example_tls_client ClientA;
	example_tls_client ClientB;
	example_tls_client ClientC;
	xnetaddr Address;
	xnetdialstats TransportStats;
	xtlslistenerstats ListenerStats;
	xnetpost Post;
	char sLocal[128];
	size_t i;
	int iResult = 1;

	memset(&ClientA, 0, sizeof(ClientA));
	memset(&ClientB, 0, sizeof(ClientB));
	memset(&ClientC, 0, sizeof(ClientC));

	/* 服务端：嵌入身份 + 动态端口的 TLS Listener（拉取模式）。 */
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

	/* 客户端：接受一切的自定义验证器（仅演示）。 */
	xrtTlsVerifierConfigInit(&VerifierConfig);
	VerifierConfig.Verify = exampleAcceptAll;
	pVerifier = xrtTlsVerifierCreate(&VerifierConfig);
	if ( pVerifier == NULL ) {
		fprintf(stderr, "failed to create verifier\n");
		goto Cleanup;
	}
	xrtTlsClientConfigInit(&ClientConfig);
	ClientConfig.Verifier = pVerifier;
	xrtTlsDialConfigInit(&DialConfig);

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
	pListener = xrtTlsListenerStart(pEngine, &ListenerConfig, NULL,
		NULL, &EngineConfig);
	if ( (pListener == NULL) ||
		!xrtTlsListenerLocal(pListener, &Address) ||
		(xrtTlsListenerState(pListener) != XTLS_LISTENER_OPEN) ||
		(xrtTlsListenerData(pListener) != &EngineConfig) ) {
		iResult = 2;
		goto Cleanup;
	}
	pListenerRef = xrtTlsListenerRef(pListener);
	(void)xrtNetAddrEndpointText(&Address, sLocal, sizeof(sLocal));
	printf("tls-listener: local=%s state=%d data=%s\n", sLocal,
		(int)xrtTlsListenerState(pListener),
		xrtTlsListenerData(pListener) != NULL ? "ok" : "(null)");

	/* 连接一：回调式 Dial + 拉取 Accept；DialRef 配对双 Destroy。 */
	memset(&ClientEvents, 0, sizeof(ClientEvents));
	ClientEvents.Open = exampleClientOpen;
	ClientEvents.Read = exampleClientRead;
	ClientEvents.Close = exampleClientClose;
	memset(&Slot, 0, sizeof(Slot));
	pDial = xrtTlsDial(pEngine, pResolver, "127.0.0.1", Address.Port,
		&ClientConfig, &DialConfig, &ClientEvents, &ClientA,
		exampleDialDone, &Slot);
	if ( (pDial == NULL) || !exampleSpinUntil(&Slot.bDone) ||
		(Slot.Result != XNET_RESULT_OK) ||
		(Slot.pStream == NULL) ||
		(xrtTlsDialRef(pDial) != pDial) ) {
		iResult = 3;
		goto Cleanup;
	}
	pClientA = Slot.pStream;
	if ( !exampleSpinUntil(&ClientA.bOpen) ) {
		iResult = 3;
		goto Cleanup;
	}
	for ( ;; ) {
		pServerA = xrtTlsListenerAccept(pListener);
		if ( pServerA != NULL ) {
			break;
		}
		if ( xrtDeadlineExpired(xrtDeadlineAfter(
				EXAMPLE_DEADLINE_US)) ) {
			iResult = 4;
			goto Cleanup;
		}
		xrtThreadYield();
	}
	if ( !xrtTlsDialTransportStats(pDial, &TransportStats) ||
		(TransportStats.AttemptsStarted < 1u) ) {
		iResult = 4;
		goto Cleanup;
	}
	printf("accept-pull: dial state=%d", (int)Slot.DialState);
	printf(" transport(attempts=%u)",
		(unsigned)TransportStats.AttemptsStarted);
	printf(" error=%s\n",
		xrtTlsDialError(pDial) == NULL ? "(none)" : "err");
	(void)xrtTlsDialError(pDial);

	/* 服务端在所属 Worker 上发一条问候，验证端到端明文通路。 */
	if ( !xrtNetPostInit(&Post) || !xrtNetPost(
		xrtNetStreamWorker(xrtTlsStreamTransport(pServerA)),
		&Post, exampleServerGreet, pServerA) ) {
		iResult = 4;
		goto Cleanup;
	}
	{
		xdeadline iDeadline = xrtDeadlineAfter(EXAMPLE_DEADLINE_US);

		while ( ClientA.Received < 9u ) {
			if ( xrtDeadlineExpired(iDeadline) ) {
				iResult = 4;
				goto Cleanup;
			}
			xrtThreadYield();
		}
		printf("greet: client received %zu bytes\n",
			ClientA.Received);
	}

	/* 连接二：xrtTlsStreamConnect 直连 + AcceptAsync（Future）。
	 * 直连不经过 Dial 的名字推导，验证器必须显式给 VerifyName
	 * （嵌入证书 CN=localhost），否则验证阶段先于回调报错。 */
	ClientConfigB = ClientConfig;
	ClientConfigB.VerifyName = XRT_STR_LITERAL("localhost");
	pClientB = xrtTlsStreamConnect(pEngine, &Address, 0, NULL,
		&ClientConfigB, NULL, &ClientEvents, &ClientB);
	pAcceptFuture = xrtTlsListenerAcceptAsync(pListener);
	if ( (pClientB == NULL) || (pAcceptFuture == NULL) ||
		!exampleSpinUntil(&ClientB.bOpen) ||
		(xrtFutureWaitFor(pAcceptFuture, EXAMPLE_DEADLINE_US) !=
			XWAIT_OK) ||
		(xrtFutureState(pAcceptFuture) != XFUTURE_RESOLVED) ) {
		iResult = 5;
		goto Cleanup;
	}
	/* FutureValue 是借用（引用由 Future 持有）；要跨 FutureDestroy
	 * 保留必须自持引用，否则收尾先销毁 Future 会把流一并释放。 */
	pServerB = xrtTlsStreamRef(
		(xtlsstream*)xrtFutureValue(pAcceptFuture));
	if ( pServerB == NULL ) {
		iResult = 5;
		goto Cleanup;
	}
	printf("accept-async: future stream=ok\n");

	/* 连接三：再来一次 Dial + AcceptWait（阻塞式）。 */
	memset(&Slot, 0, sizeof(Slot));
	pCancelDial = xrtTlsDial(pEngine, pResolver, "127.0.0.1",
		Address.Port, &ClientConfig, &DialConfig, &ClientEvents,
		&ClientC, exampleDialDone, &Slot);
	if ( (pCancelDial == NULL) || !exampleSpinUntil(&Slot.bDone) ) {
		iResult = 6;
		goto Cleanup;
	}
	pClientC = Slot.pStream;
	pServerC = xrtTlsListenerAcceptWait(pListener,
		xrtDeadlineAfter(EXAMPLE_DEADLINE_US), NULL);
	if ( (pServerC == NULL) || !exampleSpinUntil(&ClientC.bOpen) ) {
		iResult = 6;
		goto Cleanup;
	}
	printf("accept-wait: blocking stream=ok\n");

	/* 统计聚合三个连接。 */
	if ( !xrtTlsListenerStats(pListener, &ListenerStats) ||
		(ListenerStats.Accepted < 3u) ||
		(ListenerStats.Handshakes < 3u) ) {
		iResult = 7;
		goto Cleanup;
	}
	printf("stats: accepted=%llu handshakes>=%llu\n",
		(unsigned long long)ListenerStats.Accepted,
		(unsigned long long)ListenerStats.Handshakes);

	/* 连接四（失败路径）：拨未监听端口 → FAILED + 结构化错误。 */
	memset(&Slot, 0, sizeof(Slot));
	pFailDial = xrtTlsDial(pEngine, pResolver, "127.0.0.1", 1,
		&ClientConfig, &DialConfig, NULL, NULL, exampleDialDone,
		&Slot);
	if ( (pFailDial == NULL) || !exampleSpinUntil(&Slot.bDone) ||
		(Slot.Result == XNET_RESULT_OK) ) {
		iResult = 8;
		goto Cleanup;
	}
	printf("dial-fail: refused state=%d error-set=%d\n",
		(int)xrtTlsDialState(pFailDial),
		xrtTlsDialError(pFailDial) != NULL ? 1 : 0);

	/* 连接五（取消路径）：不可达地址在握手中途取消 → CANCELLED。 */
	memset(&Slot, 0, sizeof(Slot));
	{
		xtlsdial* pMidair = xrtTlsDial(pEngine, pResolver,
			"10.255.255.1", 443, &ClientConfig, &DialConfig,
			NULL, NULL, exampleDialDone, &Slot);

		if ( pMidair != NULL ) {
			bool bCancelled = xrtTlsDialCancel(pMidair);

			if ( bCancelled && exampleSpinUntil(&Slot.bDone) &&
				(Slot.Result == XNET_RESULT_CANCELLED) ) {
				printf("dial-cancel: midair state=%d\n",
					(int)xrtTlsDialState(pMidair));
			}
			else {
				printf("dial-cancel: midair result=%d\n",
					(int)Slot.Result);
			}
			xrtTlsDialDestroy(pMidair);
		}
	}
	/* pCancelDial 已成功完成：对其再取消必然返回假。 */
	printf("cancel-finished=%d\n", xrtTlsDialCancel(pCancelDial) ?
		1 : 0);
	iResult = 0;

Cleanup:
	xrtFutureDestroy(pAcceptFuture);
	/* 收尾策略：先向全部六条流发起关闭（close_notify 双向交换），
	 * 再用一个共享截止时间统一等待终态；超时的个别流用 Abort 兜底，
	 * 避免逐条串行等待把最坏退出时间放大到数倍截止时间。 */
	{
		xtlsstream* Streams[6];
		xdeadline iEnd = xrtDeadlineAfter(EXAMPLE_DEADLINE_US);
		bool bSettled;

		Streams[0] = pClientA;
		Streams[1] = pClientB;
		Streams[2] = pClientC;
		Streams[3] = pServerA;
		Streams[4] = pServerB;
		Streams[5] = pServerC;
		for ( i = 0; i < 6; ++i ) {
			if ( Streams[i] != NULL ) {
				(void)xrtTlsStreamClose(Streams[i]);
			}
		}
		do {
			bSettled = true;
			for ( i = 0; i < 6; ++i ) {
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
		for ( i = 0; i < 6; ++i ) {
			if ( Streams[i] != NULL ) {
				xtlsstreamstate State =
					xrtTlsStreamState(Streams[i]);

				if ( (State != XTLS_STREAM_CLOSED) &&
					(State != XTLS_STREAM_FAILED) ) {
					(void)xrtTlsStreamAbort(Streams[i]);
				}
			}
		}
	}
	(void)exampleSpinUntil(&ClientA.bClosed);
	(void)exampleSpinUntil(&ClientB.bClosed);
	(void)exampleSpinUntil(&ClientC.bClosed);
	xrtTlsStreamDestroy(pClientA);
	xrtTlsStreamDestroy(pClientB);
	xrtTlsStreamDestroy(pClientC);
	xrtTlsStreamDestroy(pServerA);
	xrtTlsStreamDestroy(pServerB);
	xrtTlsStreamDestroy(pServerC);
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
	xrtTlsListenerDestroy(pListenerRef);
	xrtTlsListenerDestroy(pListener);
	xrtTlsDialDestroy(pDial);
	xrtTlsDialDestroy(pDial);  /* Ref 那份 */
	xrtTlsDialDestroy(pFailDial);
	xrtTlsDialDestroy(pCancelDial);
	xrtNetResolverDestroy(pResolver);
	if ( (pEngine != NULL) && !xrtNetEngineDestroy(pEngine) &&
		(iResult == 0) ) {
		iResult = 9;
	}
	xrtTlsVerifierRelease(pVerifier);
	xrtTlsIdentityRelease(pIdentity);
	return iResult;
}
