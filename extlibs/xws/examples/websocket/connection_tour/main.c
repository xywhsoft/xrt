/*
 * 范例：xws/connection_tour —— 连接全接口：握手构建/自省/发送/关闭
 * ----------------------------------------------------------------
 * 演示 API：
 *   【离线握手】  xrtWsRequestCreate（URL 映射与 userinfo 拒绝）
 *                 xrtWsClientRequestCreate（Key + 协议字段）
 *                 xrtWsClientRequestClone
 *                 xrtWsServerConfigInit / ServerConfigValid
 *                 xrtWsConnConfigInit / ConnConfigValid /
 *                 ConnAttach / ConnAttachTls（负路径）
 *   【在线握手】  xrtWsServerCheck / UpgradeAccept
 *                 xrtWsClientCheck（完成回调内验证真实 101）
 *   【自省】      xrtWsConnRole / Tcp / TcpRef / Tls / TlsRef /
 *                 Worker / Deflate / Pending / Writable / Paused /
 *                 Error / AsyncBytes / AsyncCount
 *   【同步发送】  xrtWsConnSend / Text / Binary / SendRef / TextRef /
 *                 BinaryRef / SendTake / TextTake / BinaryTake /
 *                 Ping / Pong
 *   【流式写入】  xrtWsConnBeginText + WriterWriteTake +
 *                 WriterFinishTake + WriterIsFinished
 *   【异步发送】  xrtWsConnTextAsync / BinaryAsync / SendAsync /
 *                 TextRefAsync / PingAsync / WaitAsync
 *   【流控/关闭】  xrtWsConnPause / Resume / Close / CloseInfo
 * 模块宏：XWS_MODULE_ALL
 * 编译（单头形态，Windows，仓库根目录）：
 *   gcc -O1 -DXWS_MODULE_ALL -DXWS_IMPLEMENTATION -I extlibs/xws/single
 *       extlibs/xws/examples/websocket/connection_tour/main.c
 *       -lws2_32 -liphlpapi
 * 预期输出：
 *   conn-tour: offline request builders ok
 *   conn-tour: live pair upgraded, introspection ok
 *   conn-tour: sync send x9 + writer-take delivered ok
 *   conn-tour: async family + wait barrier ok
 *   conn-tour: pong observed via auto-pong ok
 *   conn-tour: pause main/callback held, resume delivered ok
 *   conn-tour: close handshake clean on both peers ok
 *
 * 全链回环：xhttp 服务器 + 客户端经环回解析器对接；
 *   自省与发送在连接所属 Worker 回调内执行，
 *   Take/Ref 负载的释放回调恰好一次。
 */

#include <stdio.h>
#include <string.h>
#include <xws.h>

/* 等待上限（微秒）。 */
#define TOUR_DEADLINE_US 10000000ull
#ifndef TOUR_BACKEND
#define TOUR_BACKEND XNET_PORT_AUTO
#endif
#ifndef TOUR_WORKERS
#define TOUR_WORKERS 2
#endif

/* 共享状态：两侧计数与阶段标志。 */
typedef struct tourstate {
	xatomicptr Client;
	xatomicptr Server;
	xatomic32 ClientDone;
	xatomic32 ServerDone;
	xatomic32 Messages;    /* 服务端收到的完整消息数 */
	xatomic32 Echoes;      /* 客户端收到的回显数 */
	xatomic32 Pings;       /* 服务端观察到的 Ping */
	xatomic32 Pongs;       /* 客户端观察到的 Pong */
	xatomic32 Closed;
	xatomic32 CleanClosed;
	xatomic32 Errors;
	xatomic32 PauseNext;
	bool bIntroOk;         /* 由 ClientDone 的 release/acquire 发布 */
	bool bClientCheckOk;
	char Key[XWS_KEY_CAPACITY];
} tourstate;

static xwsconnevents g_Events;

/* Ref/Take 负载释放计数。 */
static xatomic32 g_Released;

static void tourRelease(ptr pContext, cbytes pData, size_t iSize)
{
	(void)pContext;
	(void)iSize;
	xrtFree((ptr)pData);
	xrtAtomic32FetchAdd(&g_Released, 1, XMEMORY_RELEASE);
}

/* 客户端消息收到回显。 */
static void tourClientEnd(xwsconn* pConnection, ptr pData)
{
	(void)pConnection;
	xrtAtomic32FetchAdd(&((tourstate*)pData)->Echoes, 1, XMEMORY_RELEASE);
}

/* 服务端：回显策略——收一条回一条。 */
static void tourServerEnd(xwsconn* pConnection, ptr pData)
{
	tourstate* pState = (tourstate*)pData;

	if ( xrtWsConnText(pConnection, XRT_STR_LITERAL("ok")) != XNET_RESULT_OK ) {
		xrtAtomic32FetchAdd(&pState->Errors, 1, XMEMORY_RELEASE);
	}
	if ( xrtAtomic32Exchange(&pState->PauseNext, 0, XMEMORY_ACQ_REL) ) {
		xrtWsConnPause(pConnection);
	}
	xrtAtomic32FetchAdd(&pState->Messages, 1, XMEMORY_RELEASE);
}

static void tourMessageEnd(xwsconn* pConnection, ptr pData)
{
	if ( xrtWsConnRole(pConnection) == XWS_ROLE_SERVER ) {
		tourServerEnd(pConnection, pData);
	}
	else {
		tourClientEnd(pConnection, pData);
	}
}

static void tourPing(xwsconn* pConnection, xbytesview Payload,
	ptr pData)
{
	tourstate* pState = (tourstate*)pData;

	(void)Payload;
	if ( xrtWsConnRole(pConnection) == XWS_ROLE_SERVER ) {
		xrtAtomic32FetchAdd(&pState->Pings, 1, XMEMORY_RELEASE);
	}
}

static void tourPong(xwsconn* pConnection, xbytesview Payload,
	ptr pData)
{
	tourstate* pState = (tourstate*)pData;

	if ( (xrtWsConnRole(pConnection) == XWS_ROLE_CLIENT) &&
		(Payload.Size == 2u) && (memcmp(Payload.Data, "pk", 2u) == 0) ) {
		xrtAtomic32FetchAdd(&pState->Pongs, 1, XMEMORY_RELEASE);
	}
}

static void tourClose(xwsconn* pConnection, const xwsconnclose* pClose,
	ptr pData)
{
	tourstate* pState = (tourstate*)pData;

	/* 干净关闭：双方都见到 1000 且握手完整。 */
	if ( (pClose != NULL) &&
		((pClose->Flags & XWS_CONN_CLOSE_CLEAN) != 0u) &&
		((pClose->Flags & XWS_CONN_CLOSE_SENT) != 0u) &&
		((pClose->Flags & XWS_CONN_CLOSE_RECEIVED) != 0u) &&
		(pClose->LocalCode == 1000u) ) {
		/* CloseInfo 快照同样可读（Worker 上）。 */
		xwsconnclose Snapshot;

		if ( xrtWsConnCloseInfo(pConnection, &Snapshot) &&
			(Snapshot.RemoteCode == 1000u) ) {
			xrtAtomic32FetchAdd(&pState->CleanClosed, 1, XMEMORY_RELEASE);
		}
	}
	xrtAtomic32FetchAdd(&pState->Closed, 1, XMEMORY_RELEASE);
}

static void tourError(xwsconn* pConnection, const xerror* pError,
	ptr pData)
{
	(void)pConnection;
	fprintf(stderr, "conn-tour: error kind=%d code=%d\n",
		(int)xrtErrorKind(pError), (int)xrtErrorCode(pError));
	xrtAtomic32FetchAdd(&((tourstate*)pData)->Errors, 1, XMEMORY_RELEASE);
}

/* 服务端 Upgrade 完成：接管回调转移的连接引用。 */
static void tourServerDone(xhttpconn* pHttp, xnetresult Result,
	xwsconn* pConnection, const xerror* pError, ptr pData)
{
	tourstate* pState = (tourstate*)pData;

	(void)pHttp;
	(void)pError;
	if ( (Result == XNET_RESULT_OK) && (pConnection != NULL) ) {
		xrtAtomicPtrStore(&pState->Server, pConnection,
			XMEMORY_RELEASE);
	}
	xrtAtomic32Store(&pState->ServerDone,
		(Result == XNET_RESULT_OK && pConnection != NULL) ? 1 : 2,
		XMEMORY_RELEASE);
}

/* 客户端握手完成：自省 + 三族发送 + 关闭。 */
static void tourClientDone(xhttpcall* pCall, xnetresult Result,
	xwsconn* pConnection, xhttpresponse* pResponse,
	const xerror* pError, ptr pData)
{
	tourstate* pState = (tourstate*)pData;
	xwsclientconfig Config;
	xwsclienthandshake Handshake;
	xhttprequest* pActual = NULL;
	const xhttpfield* pKey;
	xnetstream* pTcp;
	xtlsstream* pTls;

	(void)pCall;
	(void)pError;
	if ( (Result != XNET_RESULT_OK) || (pConnection == NULL) ) {
		xrtHttpResponseDestroy(pResponse);
		xrtAtomic32Store(&pState->ClientDone, 2, XMEMORY_RELEASE);
		return;
	}
	/* Connect 会重建管理字段并生成新 Key，必须取 Call 的实际请求。
	 * 离线 RequestCreate/Clone 返回的 Key 不属于这次在线握手。 */
	xrtWsClientConfigInit(&Config);
	Config.Protocols = XRT_STR_LITERAL("chat.v1, chat.v2");
	pActual = xrtHttpCallRequestClone(pCall);
	pKey = pActual == NULL ? NULL : xrtHttpRequestHeader(pActual,
		XRT_STR_LITERAL("Sec-WebSocket-Key"));
	if ( (pKey != NULL) &&
		xrtWsClientCheck(pResponse, pKey->Value, &Config, &Handshake) ) {
		pState->bClientCheckOk = true;
	}
	/* 自省族（Worker 上）：角色/传输/Worker/预算/错误。 */
	pTcp = xrtWsConnTcpRef(pConnection);
	pTls = xrtWsConnTlsRef(pConnection);
	if ( (xrtWsConnRole(pConnection) == XWS_ROLE_CLIENT) &&
		(xrtWsConnTcp(pConnection) != NULL) &&
		(pTcp != NULL) &&
		(xrtWsConnTls(pConnection) == NULL) &&
		(pTls == NULL) &&
		(xrtWsConnWorker(pConnection) != NULL) &&
		!xrtWsConnDeflate(pConnection, NULL) &&
		(xrtWsConnWritable(pConnection) > 0u) &&
		!xrtWsConnPaused(pConnection) &&
		(xrtWsConnError(pConnection) == NULL) ) {
		pState->bIntroOk = true;
	}
	xrtNetStreamDestroy(pTcp);
	xrtTlsStreamDestroy(pTls);
	xrtHttpRequestDestroy(pActual);
	xrtHttpResponseDestroy(pResponse);
	xrtAtomicPtrStore(&pState->Client, pConnection,
		XMEMORY_RELEASE);
	xrtAtomic32Store(&pState->ClientDone, 1, XMEMORY_RELEASE);
}

/* 服务端 HTTP 请求处理：解析升级请求并接管。 */
static void tourRequest(xhttpserver* pServer, xhttpconn* pHttp,
	const xhttpserverrequest* pRequest, ptr pData)
{
	tourstate* pState = (tourstate*)pData;
	xwsserverconfig Config;
	xwsserverhandshake Handshake;

	(void)pServer;
	xrtWsServerConfigInit(&Config);
	Config.Protocols = XRT_STR_LITERAL("chat.v1, chat.v2");
	if ( xrtWsServerCheck(pRequest, &Config, &Handshake) ) {
		if ( xrtWsUpgradeAccept(pHttp, &Config,
			&Handshake, NULL, &g_Events, pState,
			tourServerDone, pState) == XNET_RESULT_OK ) return;
	}
	xrtAtomic32FetchAdd(&pState->Errors, 1, XMEMORY_RELEASE);
	xrtAtomic32Store(&pState->ServerDone, 2, XMEMORY_RELEASE);
}

static bool tourWait32Min(xatomic32* pFlag, uint32 iMinimum)
{
	xdeadline iEnd = xrtDeadlineAfter(TOUR_DEADLINE_US);

	while ( xrtAtomic32Load(pFlag, XMEMORY_ACQUIRE) < iMinimum ) {
		if ( xrtDeadlineExpired(iEnd) ) {
			return false;
		}
		xrtThreadYield();
	}
	return true;
}

/* Worker 任务：在客户端连接上执行三族同步发送。 */
typedef struct toursend {
	xwsconn* pConnection;
	xatomic32 Done;
	bool bOk;
} toursend;

static void tourSyncSendTask(xnetworker* pWorker, ptr pUserData)
{
	toursend* pTask = (toursend*)pUserData;
	xwsconn* pConnection = pTask->pConnection;
	xnetref Ref;
	bytes pOwned = NULL;
	xwswriter* pWriter = NULL;

	(void)pWorker;
	/* xnetresult 的 OK 是 0，不能用按位 &= 累积成功状态。 */
#define TOUR_SEND(Call) do { if ( (Call) != XNET_RESULT_OK ) goto Done; } while (0)
	TOUR_SEND(xrtWsConnText(pConnection, XRT_STR_LITERAL("t1")));
	TOUR_SEND(xrtWsConnBinary(pConnection, (xbytesview) { (cbytes)"b1", 2u }));
	TOUR_SEND(xrtWsConnSend(pConnection, XWS_OPCODE_TEXT,
		(xbytesview) { (cbytes)"s1", 2u }));
	/* Ref 仅在 OK 时接管；释放回调真正释放分配而非只计数。 */
	pOwned = (bytes)xrtMalloc(2u);
	if ( pOwned == NULL ) goto Done;
	memcpy(pOwned, "r1", 2u);
	Ref = (xnetref) { pOwned, 2u, tourRelease, NULL };
	TOUR_SEND(xrtWsConnTextRef(pConnection, &Ref));
	pOwned = (bytes)xrtMalloc(2u);
	if ( pOwned == NULL ) goto Done;
	memcpy(pOwned, "r2", 2u);
	Ref.Data = pOwned;
	TOUR_SEND(xrtWsConnBinaryRef(pConnection, &Ref));
	pOwned = (bytes)xrtMalloc(2u);
	if ( pOwned == NULL ) goto Done;
	memcpy(pOwned, "r3", 2u);
	Ref.Data = pOwned;
	TOUR_SEND(xrtWsConnSendRef(pConnection, XWS_OPCODE_TEXT, &Ref));
	pOwned = (bytes)xrtMalloc(2u);
	if ( pOwned == NULL ) goto Done;
	memcpy(pOwned, "k1", 2u);
	TOUR_SEND(xrtWsConnBinaryTake(pConnection, pOwned, 2u));
	pOwned = (bytes)xrtMalloc(2u);
	if ( pOwned == NULL ) goto Done;
	memcpy(pOwned, "k2", 2u);
	TOUR_SEND(xrtWsConnTextTake(pConnection, (str)pOwned, 2u));
	pOwned = (bytes)xrtMalloc(2u);
	if ( pOwned == NULL ) goto Done;
	memcpy(pOwned, "k3", 2u);
	TOUR_SEND(xrtWsConnSendTake(pConnection, XWS_OPCODE_BINARY, pOwned, 2u));
	pOwned = NULL;
	/* 两个 Take 分片组成第十条消息。 */
	pWriter = xrtWsConnBeginText(pConnection);
	if ( pWriter == NULL ) goto Done;
	pOwned = (bytes)xrtMalloc(4u);
	if ( pOwned == NULL ) goto Done;
	memcpy(pOwned, "frag", 4u);
	TOUR_SEND(xrtWsWriterWriteTake(pWriter, pOwned, 4u));
	pOwned = (bytes)xrtMalloc(3u);
	if ( pOwned == NULL ) goto Done;
	memcpy(pOwned, "end", 3u);
	TOUR_SEND(xrtWsWriterFinishTake(pWriter, pOwned, 3u));
	pOwned = NULL;
	if ( !xrtWsWriterIsFinished(pWriter) ) goto Done;
	TOUR_SEND(xrtWsConnPing(pConnection, (xbytesview) { (cbytes)"pk", 2u }));
	TOUR_SEND(xrtWsConnPong(pConnection, (xbytesview) { (cbytes)"pk", 2u }));
	pTask->bOk = true;
Done:
#undef TOUR_SEND
	xrtFree(pOwned);
	xrtWsWriterDestroy(pWriter);
	xrtAtomic32Store(&pTask->Done, 1, XMEMORY_RELEASE);
}

static void tourCloseTask(xnetworker* pWorker, ptr pData)
{
	toursend* pTask = (toursend*)pData;

	(void)pWorker;
	pTask->bOk = xrtWsConnClose(pTask->pConnection, 1000,
		XRT_STR_LITERAL("done")) == XNET_RESULT_OK;
	xrtAtomic32Store(&pTask->Done, 1, XMEMORY_RELEASE);
}

static bool tourFutureReady(xfuture* pFuture)
{
	return (pFuture != NULL) &&
		(xrtFutureWaitFor(pFuture, TOUR_DEADLINE_US) == XWAIT_OK) &&
		(xrtFutureState(pFuture) == XFUTURE_RESOLVED);
}

/* 暂停断言必须失败可见，不能只打印“leaked”然后仍返回成功。 */
static bool tourHeld(tourstate* pState, uint32 iMessages)
{
	xdeadline Deadline = xrtDeadlineAfter(200000u);

	while ( !xrtDeadlineExpired(Deadline) ) {
		if ( (xrtAtomic32Load(&pState->Messages, XMEMORY_ACQUIRE) != iMessages) ||
			(xrtAtomic32Load(&pState->Errors, XMEMORY_ACQUIRE) != 0) ) return false;
		xrtThreadYield();
	}
	return true;
}

/* 握手失败/超时时，完成回调仍可能在收尾阶段才交付引用。 */
static void tourReleaseConnection(xatomicptr* pSlot)
{
	xwsconn* pConnection = (xwsconn*)xrtAtomicPtrExchange(pSlot, NULL, XMEMORY_ACQ_REL);

	if ( pConnection != NULL ) (void)xrtWsConnAbort(pConnection);
	xrtWsConnDestroy(pConnection);
}

int main(void)
{
	tourstate State;
	xnetengineconfig EngineConfig;
	xnetengine* pEngine = NULL;
	xhttpserverconfig ServerConfig;
	xhttpserverevents ServerEvents;
	xhttpserver* pServer = NULL;
	xhttpclientconfig ClientConfig;
	xhttpclient* pClient = NULL;
	xwsclientconfig WsConfig;
	xwsserverconfig WsServerConfig;
	xhttpcall* pCall = NULL;
	xhttprequest* pRequest = NULL;
	xhttprequest* pCloned = NULL;
	xwsconnconfig ConnConfig;
	xwsconn* pConn = NULL;
	xwsconn* pServerConn = NULL;
	xfuture* Futures[6] = { NULL };
	bytes pOwned = NULL;
	xnetref Ref;
	xnetaddr Address;
	toursend Send;
	char sUrl[128];
	char sKey[XWS_KEY_CAPACITY];
	int iLength;
	int iResult = 1;
	xdeadline Deadline;

	memset(&State, 0, sizeof(State));
	xrtAtomicPtrInit(&State.Client, NULL);
	xrtAtomicPtrInit(&State.Server, NULL);
	xrtAtomic32Init(&State.ClientDone, 0);
	xrtAtomic32Init(&State.ServerDone, 0);
	xrtAtomic32Init(&State.Messages, 0);
	xrtAtomic32Init(&State.Echoes, 0);
	xrtAtomic32Init(&State.Pings, 0);
	xrtAtomic32Init(&State.Pongs, 0);
	xrtAtomic32Init(&State.Closed, 0);
	xrtAtomic32Init(&State.CleanClosed, 0);
	xrtAtomic32Init(&State.Errors, 0);
	xrtAtomic32Init(&State.PauseNext, 0);
	xrtAtomic32Init(&g_Released, 0);
	memset(&Send, 0, sizeof(Send));
	xrtAtomic32Init(&Send.Done, 0);
	g_Events.MessageEnd = tourMessageEnd;
	g_Events.Ping = tourPing;
	g_Events.Pong = tourPong;
	g_Events.Close = tourClose;
	g_Events.Error = tourError;

	/* ---- 离线：请求构建器与配置校验。 ---- */
	pRequest = xrtWsRequestCreate(XRT_STR_LITERAL("wss://example.test/chat"));
	xrtWsClientConfigInit(&WsConfig);
	WsConfig.Protocols = XRT_STR_LITERAL("chat.v1, chat.v2");
	pCloned = xrtWsClientRequestCreate(
		XRT_STR_LITERAL("wss://example.test/chat"), &WsConfig, sKey);
	xrtWsConnConfigInit(&ConnConfig);
	xrtWsServerConfigInit(&WsServerConfig);
	WsServerConfig.Protocols = XRT_STR_LITERAL("chat.v1");
	if ( (pRequest == NULL) || (pCloned == NULL) ||
		!xrtWsKeyValid((xstrview) { sKey, XWS_KEY_SIZE }) ||
		xrtWsServerConfigValid(&(xwsserverconfig) { 0 }) ||
		!xrtWsServerConfigValid(&WsServerConfig) ||
		!xrtWsConnConfigValid(&ConnConfig) ||
		xrtWsConnConfigValid(&(xwsconnconfig) { 0 }) ||
		(xrtWsConnAttach(NULL, &ConnConfig, NULL, NULL) != NULL) ||
		(xrtWsConnAttachTls(NULL, &ConnConfig, NULL, NULL) != NULL) ) goto Cleanup;
	{
		xhttprequest* pClone = xrtWsClientRequestClone(pCloned, &WsConfig, State.Key);
		bool bValid = (pClone != NULL) &&
			xrtWsKeyValid((xstrview) { State.Key, XWS_KEY_SIZE });

		xrtHttpRequestDestroy(pClone);
		if ( !bValid ) goto Cleanup;
	}
	xrtHttpRequestDestroy(pCloned);
	pCloned = NULL;
	xrtHttpRequestDestroy(pRequest);
	pRequest = NULL;
	xrtClearError();  /* 以上负路径不能污染在线阶段的错误诊断。 */
	printf("conn-tour: offline request builders ok\n");

	/* ---- 在线：事件表和事件上下文必须同时绑定到客户端与服务端。 ---- */
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Workers = TOUR_WORKERS;
	EngineConfig.Backend = TOUR_BACKEND;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	if ( (pEngine == NULL) || !xrtNetEngineStart(pEngine) ) goto Cleanup;
	xrtHttpServerConfigInit(&ServerConfig);
	if ( !xrtNetAddrLoopback(&ServerConfig.Network.Listen.Address,
		XNET_FAMILY_IPV4, 0) ) goto Cleanup;
	xrtHttpServerEventsInit(&ServerEvents);
	ServerEvents.Request = tourRequest;
	ServerEvents.Data = &State;
	pServer = xrtHttpServerStart(pEngine, &ServerConfig, &ServerEvents);
	if ( (pServer == NULL) || !xrtHttpServerLocal(pServer, 0, &Address) ) goto Cleanup;
	xrtHttpClientConfigInit(&ClientConfig);
	pClient = xrtHttpClientCreate(pEngine, &ClientConfig);
	if ( pClient == NULL ) goto Cleanup;
	iLength = snprintf(sUrl, sizeof(sUrl), "ws://127.0.0.1:%u/chat", (unsigned)Address.Port);
	if ( (iLength < 0) || ((size_t)iLength >= sizeof(sUrl)) ) goto Cleanup;
	pCall = xrtWsConnect(pClient, (xstrview) { sUrl, (size_t)iLength },
		&WsConfig, &g_Events, &State, tourClientDone, &State);
	if ( (pCall == NULL) || !tourWait32Min(&State.ClientDone, 1) ||
		!tourWait32Min(&State.ServerDone, 1) ||
		(xrtAtomic32Load(&State.ClientDone, XMEMORY_ACQUIRE) != 1) ||
		(xrtAtomic32Load(&State.ServerDone, XMEMORY_ACQUIRE) != 1) ||
		!State.bIntroOk || !State.bClientCheckOk ) goto Cleanup;
	pConn = (xwsconn*)xrtAtomicPtrLoad(&State.Client, XMEMORY_ACQUIRE);
	pServerConn = (xwsconn*)xrtAtomicPtrLoad(&State.Server, XMEMORY_ACQUIRE);
	if ( (pConn == NULL) || (pServerConn == NULL) ) goto Cleanup;
	printf("conn-tour: live pair upgraded, introspection ok\n");

	/* ---- 同步发送九形态 + Writer-Take：上下文留在 main 作用域。 ---- */
	Send.pConnection = pConn;
	if ( !xrtNetEnginePost(pEngine, xrtNetWorkerIndex(xrtWsConnWorker(pConn)),
			tourSyncSendTask, &Send) ||
		!tourWait32Min(&Send.Done, 1) || !Send.bOk ||
		!tourWait32Min(&State.Messages, 10) || !tourWait32Min(&State.Echoes, 10) ) goto Cleanup;
	printf("conn-tour: sync send x9 + writer-take delivered ok\n");

	/* ---- 异步发送：从主线程逐一检查 Future 的成功终态。 ---- */
	Futures[0] = xrtWsConnTextAsync(pConn, XRT_STR_LITERAL("a1"));
	Futures[1] = xrtWsConnBinaryAsync(pConn, (xbytesview) { (cbytes)"a2", 2u });
	pOwned = (bytes)xrtMalloc(2u);
	if ( pOwned == NULL ) goto Cleanup;
	memcpy(pOwned, "a3", 2u);
	Ref = (xnetref) { pOwned, 2u, tourRelease, NULL };
	Futures[2] = xrtWsConnTextRefAsync(pConn, &Ref);
	if ( Futures[2] == NULL ) goto Cleanup;
	pOwned = NULL;  /* 非空 Future 已受理，立即转移 Ref 所有权。 */
	Futures[3] = xrtWsConnSendAsync(pConn, XWS_OPCODE_TEXT,
		(xbytesview) { (cbytes)"a4", 2u });
	Futures[4] = xrtWsConnPingAsync(pConn, (xbytesview) { (cbytes)"pk", 2u });
	Futures[5] = xrtWsConnWaitAsync(pConn, XWS_CONN_WAIT_WRITE);
	for ( size_t i = 0; i < 6; i++ ) {
		if ( !tourFutureReady(Futures[i]) ) goto Cleanup;
		xrtFutureDestroy(Futures[i]);
		Futures[i] = NULL;
	}
	if ( !tourWait32Min(&State.Messages, 14) || !tourWait32Min(&State.Echoes, 14) ||
		!tourWait32Min(&State.Pongs, 2) || !tourWait32Min(&State.Pings, 2) ||
		!tourWait32Min(&g_Released, 4) ||
		(xrtAtomic32Load(&g_Released, XMEMORY_ACQUIRE) != 4) ||
		(xrtWsConnState(pConn) != XWS_CONN_OPEN) ||
		(xrtWsConnState(pServerConn) != XWS_CONN_OPEN) ) goto Cleanup;
	printf("conn-tour: async family + wait barrier ok\n");
	printf("conn-tour: pong observed via auto-pong ok\n");

	/* ---- 主线程 Pause 与回调内 Pause 分别验证，控制帧不代替 Resume。 ---- */
	xrtWsConnPause(pServerConn);
	Futures[0] = xrtWsConnTextAsync(pConn, XRT_STR_LITERAL("paused-main"));
	if ( !xrtWsConnPaused(pServerConn) || !tourFutureReady(Futures[0]) ||
		!tourHeld(&State, 14) || !xrtWsConnResume(pServerConn) ||
		!tourWait32Min(&State.Messages, 15) || !tourWait32Min(&State.Echoes, 15) ) goto Cleanup;
	xrtFutureDestroy(Futures[0]);
	Futures[0] = NULL;
	xrtAtomic32Store(&State.PauseNext, 1, XMEMORY_RELEASE);
	Futures[0] = xrtWsConnTextAsync(pConn, XRT_STR_LITERAL("pause-in-callback"));
	Futures[1] = xrtWsConnTextAsync(pConn, XRT_STR_LITERAL("after-callback"));
	if ( !tourFutureReady(Futures[0]) || !tourFutureReady(Futures[1]) ||
		!tourWait32Min(&State.Messages, 16) || !xrtWsConnPaused(pServerConn) ||
		!tourHeld(&State, 16) || !xrtWsConnResume(pServerConn) ||
		!tourWait32Min(&State.Messages, 17) || !tourWait32Min(&State.Echoes, 17) ) goto Cleanup;
	for ( size_t i = 0; i < 2; i++ ) {
		xrtFutureDestroy(Futures[i]);
		Futures[i] = NULL;
	}
	Futures[0] = xrtWsConnWaitAsync(pConn, XWS_CONN_WAIT_WRITE);
	if ( !tourFutureReady(Futures[0]) || (xrtWsConnPending(pConn) != 0) ||
		(xrtWsConnAsyncBytes(pConn) != 0) || (xrtWsConnAsyncCount(pConn) != 0) ) goto Cleanup;
	xrtFutureDestroy(Futures[0]);
	Futures[0] = NULL;
	printf("conn-tour: pause main/callback held, resume delivered ok\n");

	/* ---- 双方干净关闭，观察 Close 回调和 CloseInfo 而非假定成功。 ---- */
	Send.bOk = false;
	xrtAtomic32Store(&Send.Done, 0, XMEMORY_RELEASE);
	if ( !xrtNetEnginePost(pEngine, xrtNetWorkerIndex(xrtWsConnWorker(pConn)),
			tourCloseTask, &Send) || !tourWait32Min(&Send.Done, 1) || !Send.bOk ||
		!tourWait32Min(&State.Closed, 2) ||
		(xrtAtomic32Load(&State.CleanClosed, XMEMORY_ACQUIRE) != 2) ||
		(xrtAtomic32Load(&State.Errors, XMEMORY_ACQUIRE) != 0) ||
		(xrtAtomic32Load(&State.Messages, XMEMORY_ACQUIRE) != 17) ||
		(xrtAtomic32Load(&State.Echoes, XMEMORY_ACQUIRE) != 17) ||
		(xrtWsConnError(pConn) != NULL) || (xrtWsConnError(pServerConn) != NULL) ) goto Cleanup;
	printf("conn-tour: close handshake clean on both peers ok\n");
	iResult = 0;

Cleanup:
	xrtFree(pOwned);
	for ( size_t i = 0; i < 6; i++ ) xrtFutureDestroy(Futures[i]);
	xrtHttpRequestDestroy(pCloned);
	xrtHttpRequestDestroy(pRequest);
	if ( pCall != NULL ) (void)xrtHttpCallCancel(pCall);
	/* 回调可能在失败收尾中才完成；先释放已经交付的连接引用。 */
	tourReleaseConnection(&State.Client);
	tourReleaseConnection(&State.Server);
	xrtHttpCallDestroy(pCall);
	xrtHttpClientDestroy(pClient);
	/* Server 的 Destroy 只释放引用，不隐式停止监听。 */
	if ( pServer != NULL ) (void)xrtHttpServerAbort(pServer);
	xrtHttpServerDestroy(pServer);
	/* main 内的 Send / State 在停止 Worker 的整个过程中保持有效。 */
	Deadline = xrtDeadlineAfter(TOUR_DEADLINE_US);
	/* 上层 Destroy 会投递异步关闭；最后一个内部对象释放后才能 Stop。 */
	while ( (pEngine != NULL) && !xrtNetEngineStop(pEngine) ) {
		tourReleaseConnection(&State.Client);
		tourReleaseConnection(&State.Server);
		if ( xrtDeadlineExpired(Deadline) ) { iResult = 1; break; }
		xrtThreadYield();
	}
	tourReleaseConnection(&State.Client);
	tourReleaseConnection(&State.Server);
	if ( (pEngine != NULL) && !xrtNetEngineDestroy(pEngine) ) iResult = 1;
	if ( iResult != 0 ) fprintf(stderr, "conn-tour: verification failed\n");
	return iResult;
}
