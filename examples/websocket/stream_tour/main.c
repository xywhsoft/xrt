/*
 * 范例：websocket/stream_tour —— WebSocket Stream 回环双端
 * ----------------------------------------------------------------
 * 配置/接入：ConfigInit / ConfigValid / Attach / AttachTls（负路径）。
 * 生命周期/自省：Ref / Destroy / State / Role / Protocol / Worker /
 *   Tcp / TcpRef / Tls / TlsRef / Deflate / Pending / Writable /
 *   Close / CloseInfo / Error / Abort。
 * 发送：Text / Binary / Send，TextRef / SendRef，TextTake /
 *   BinaryTake / SendTake，TextCompressed / BinaryCompressed /
 *   SendCompressed，Ping / Pong。流控：Pause / Paused / Resume。
 * 模块：websocket_stream；ref、deflate、tls 接口按所选模块启用。
 * 编译（Windows，仓库根目录，完整单头形态）：
 *   gcc -O1 -DXRT_MODULE_ALL -DXRT_IMPLEMENTATION -I single
 *       examples/websocket/stream_tour/main.c -lws2_32 -liphlpapi
 *
 * 双端直接 Attach 已连接 TCP，跳过 HTTP Upgrade，仅用于本地演示。
 * 启用 deflate 时两端采用相同默认参数；真实部署应先完成协商。
 * 同步发送/Close/Attach 在所属 Worker 上执行并检查返回值。
 * 主线程等待原子信号；任务上下文一直存活到 EngineStop 返回。
 *
 * ALL 模块下预期：64 字节回显、自动及手动 Pong、暂停恢复、
 *   压缩三形态解压回显、双方干净关闭（flags=7, code=1000）。
 * 可用 EXAMPLE_WS_BACKEND / EXAMPLE_WS_WORKERS / EXAMPLE_WS_HIGH_WATER
 * 编译宏分别选择后端、Worker 数、TCP 写高水位，便于复验。
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>

#define EXAMPLE_DEADLINE_US UINT64_C(5000000)
#ifndef EXAMPLE_WS_BACKEND
#define EXAMPLE_WS_BACKEND XNET_PORT_AUTO
#endif
#ifndef EXAMPLE_WS_WORKERS
#define EXAMPLE_WS_WORKERS 2
#endif

typedef struct exampleendpoint {
	xnetstream* pTcp;
	xwsstream* pStream;
	xwsstreamconfig Config;
	bool AttachPosted;  /* 仅主线程读写；发布后等待 Attached 才访问结果槽。 */
	xatomic32 Attached;
	xatomic32 Received;
	xatomic32 Pong;
	xatomic32 Closed;
	xatomic32 Errors;
	xwsopcode Opcode;
	size_t Size;
	char Buffer[256];
} exampleendpoint;

typedef struct examplejob {
	xwsstream* pStream;
	xatomic32 Done;
	bool Ok;
} examplejob;

#if defined(XRT_FEATURE_WEBSOCKET_STREAM_REF)
static xatomic32 g_Releases;

static void exampleRelease(ptr pContext, cbytes pData, size_t iSize)
{
	(void)pContext;
	(void)pData;
	(void)iSize;
	xrtAtomic32FetchAdd(&g_Releases, 1, XMEMORY_RELEASE);
}
#endif

static bool exampleWait(xatomic32* pValue, uint32 iMinimum)
{
	xdeadline Deadline = xrtDeadlineAfter(EXAMPLE_DEADLINE_US);

	while ( xrtAtomic32Load(pValue, XMEMORY_ACQUIRE) < iMinimum ) {
		if ( xrtDeadlineExpired(Deadline) ) return false;
		xrtThreadYield();
	}
	return true;
}

static void exampleEndpointInit(exampleendpoint* pEnd, xwsrole Role)
{
	memset(pEnd, 0, sizeof(*pEnd));
	xrtAtomic32Init(&pEnd->Attached, 0);
	xrtAtomic32Init(&pEnd->Received, 0);
	xrtAtomic32Init(&pEnd->Pong, 0);
	xrtAtomic32Init(&pEnd->Closed, 0);
	xrtAtomic32Init(&pEnd->Errors, 0);
	xrtWsStreamConfigInit(&pEnd->Config);
	pEnd->Config.Role = Role;
#if defined(XRT_FEATURE_WEBSOCKET_STREAM_DEFLATE)
	pEnd->Config.DeflateEnabled = true;
#endif
}

static void exampleBegin(xwsstream* pStream, const xwsmessageinfo* pInfo,
	ptr pData)
{
	exampleendpoint* pEnd = (exampleendpoint*)pData;

	pEnd->Opcode = pInfo->Opcode;
	if ( xrtWsStreamRole(pStream) == XWS_ROLE_SERVER ) pEnd->Size = 0;
}

static void exampleData(xwsstream* pStream, xbytesview Data, ptr pData)
{
	exampleendpoint* pEnd = (exampleendpoint*)pData;

	if ( Data.Size > sizeof(pEnd->Buffer) - pEnd->Size ) {
		xrtAtomic32FetchAdd(&pEnd->Errors, 1, XMEMORY_RELEASE);
		(void)xrtWsStreamAbort(pStream);
		return;
	}
	memcpy(pEnd->Buffer + pEnd->Size, Data.Data, Data.Size);
	pEnd->Size += Data.Size;
	/* 发布已写入的字节；主线程收到预期长度后检查内容。 */
	xrtAtomic32FetchAdd(&pEnd->Received, (uint32)Data.Size, XMEMORY_RELEASE);
}

static void exampleEnd(xwsstream* pStream, ptr pData)
{
	exampleendpoint* pEnd = (exampleendpoint*)pData;

	/* 保留消息边界，避免把任意分块误当成完整 Text 消息。 */
	if ( (xrtWsStreamRole(pStream) == XWS_ROLE_SERVER) &&
		(xrtWsStreamSend(pStream, pEnd->Opcode,
			(xbytesview) { (cbytes)pEnd->Buffer, pEnd->Size }) !=
			XNET_RESULT_OK) ) {
		xrtAtomic32FetchAdd(&pEnd->Errors, 1, XMEMORY_RELEASE);
	}
}

static void examplePong(xwsstream* pStream, xbytesview Payload, ptr pData)
{
	exampleendpoint* pEnd = (exampleendpoint*)pData;
	bool bClient = xrtWsStreamRole(pStream) == XWS_ROLE_CLIENT;
	const char* pExpected = bClient ? "ping" : "manual";
	size_t iSize = bClient ? 4u : 6u;

	if ( (Payload.Size == iSize) &&
		(memcmp(Payload.Data, pExpected, iSize) == 0) ) {
		xrtAtomic32Store(&pEnd->Pong, 1, XMEMORY_RELEASE);
	}
}

static void exampleClose(xwsstream* pStream, const xwsstreamclose* pClose,
	ptr pData)
{
	exampleendpoint* pEnd = (exampleendpoint*)pData;

	(void)pStream;
	(void)pClose;
	xrtAtomic32Store(&pEnd->Closed, 1, XMEMORY_RELEASE);
}

static void exampleError(xwsstream* pStream, const xerror* pError, ptr pData)
{
	exampleendpoint* pEnd = (exampleendpoint*)pData;

	(void)pStream;
	fprintf(stderr, "ws-stream: error kind=%d code=%d\n",
		(int)xrtErrorKind(pError), (int)xrtErrorCode(pError));
	xrtAtomic32FetchAdd(&pEnd->Errors, 1, XMEMORY_RELEASE);
}

static void exampleAttachTask(xnetworker* pWorker, ptr pData)
{
	exampleendpoint* pEnd = (exampleendpoint*)pData;
	xwsstreamevents Events = { 0 };

	(void)pWorker;
	Events.MessageBegin = exampleBegin;
	Events.MessageData = exampleData;
	Events.MessageEnd = exampleEnd;
	Events.Pong = examplePong;
	Events.Close = exampleClose;
	Events.Error = exampleError;
	pEnd->pStream = xrtWsStreamAttach(pEnd->pTcp, 0,
		&pEnd->Config, &Events, pEnd);
	/* 成功立刻转移所有权，即使另一端失败也不能再释放此 TCP 引用。 */
	if ( pEnd->pStream != NULL ) pEnd->pTcp = NULL;
	xrtAtomic32Store(&pEnd->Attached, 1, XMEMORY_RELEASE);
}

static bool exampleRun(xnetengine* pEngine, examplejob* pJob,
	xwsstream* pStream, xnettaskproc Proc)
{
	pJob->pStream = pStream;
	pJob->Ok = false;
	xrtAtomic32Store(&pJob->Done, 0, XMEMORY_RELEASE);
	return xrtNetEnginePost(pEngine,
		xrtNetWorkerIndex(xrtWsStreamWorker(pStream)), Proc, pJob) &&
		exampleWait(&pJob->Done, 1) && pJob->Ok;
}

static bool exampleAttach(xnetengine* pEngine, exampleendpoint* pEnd)
{
	pEnd->AttachPosted = xrtNetEnginePost(pEngine,
		xrtNetWorkerIndex(xrtNetStreamWorker(pEnd->pTcp)), exampleAttachTask, pEnd);
	return pEnd->AttachPosted && exampleWait(&pEnd->Attached, 1) &&
		(pEnd->pStream != NULL);
}

/* 可重复调用：收回已交付的引用，不读取尚在 Worker 中写入的结果。 */
static void exampleEndpointRelease(exampleendpoint* pEnd)
{
	if ( pEnd->AttachPosted &&
		!xrtAtomic32Load(&pEnd->Attached, XMEMORY_ACQUIRE) ) return;
	if ( pEnd->pStream != NULL ) {
		(void)xrtWsStreamAbort(pEnd->pStream);
		xrtWsStreamDestroy(pEnd->pStream);
		pEnd->pStream = NULL;
	}
	if ( pEnd->pTcp != NULL ) {
		(void)xrtNetStreamAbort(pEnd->pTcp);
		xrtNetStreamDestroy(pEnd->pTcp);
		pEnd->pTcp = NULL;
	}
}

static void exampleSendTask(xnetworker* pWorker, ptr pData)
{
	examplejob* pJob = (examplejob*)pData;
	xwsstream* pStream = pJob->pStream;
#if defined(XRT_FEATURE_WEBSOCKET_STREAM_REF)
	ptr pTake = NULL;
	xnetref Ref = { (cbytes)"text-ref", 8u, exampleRelease, NULL };
#endif

	(void)pWorker;
	if ( (xrtWsStreamText(pStream, XRT_STR_LITERAL("hello ws")) !=
			XNET_RESULT_OK) ||
		(xrtWsStreamBinary(pStream,
			(xbytesview) { (cbytes)"\x01\x02\x03", 3 }) != XNET_RESULT_OK) ||
		(xrtWsStreamSend(pStream, XWS_OPCODE_TEXT,
			(xbytesview) { (cbytes)"plain-send", 10 }) != XNET_RESULT_OK) ) {
		goto Done;
	}
#if defined(XRT_FEATURE_WEBSOCKET_STREAM_REF)
	if ( xrtWsStreamTextRef(pStream, &Ref) != XNET_RESULT_OK ) goto Done;
	Ref.Data = (cbytes)"send-ref";
	if ( xrtWsStreamSendRef(pStream, XWS_OPCODE_BINARY, &Ref) !=
		XNET_RESULT_OK ) goto Done;
	pTake = xrtMalloc(9);
	if ( pTake == NULL ) goto Done;
	memcpy(pTake, "text-take", 9);
	if ( xrtWsStreamTextTake(pStream, pTake, 9) != XNET_RESULT_OK ) goto Done;
	pTake = xrtMalloc(10);
	if ( pTake == NULL ) goto Done;
	memcpy(pTake, "binarytake", 10);
	if ( xrtWsStreamBinaryTake(pStream, pTake, 10) != XNET_RESULT_OK ) goto Done;
	pTake = xrtMalloc(8);
	if ( pTake == NULL ) goto Done;
	memcpy(pTake, "sendtake", 8);
	if ( xrtWsStreamSendTake(pStream, XWS_OPCODE_TEXT, pTake, 8) !=
		XNET_RESULT_OK ) goto Done;
	pTake = NULL;
#endif
	pJob->Ok = (xrtWsStreamPing(pStream,
		(xbytesview) { (cbytes)"ping", 4 }) == XNET_RESULT_OK) &&
		(xrtWsStreamPong(pStream,
			(xbytesview) { (cbytes)"manual", 6 }) == XNET_RESULT_OK);
Done:
#if defined(XRT_FEATURE_WEBSOCKET_STREAM_REF)
	xrtFree(pTake);  /* 只有未被成功接管的缓冲仍由调用方释放。 */
#endif
	xrtAtomic32Store(&pJob->Done, 1, XMEMORY_RELEASE);
}

static void exampleQueryTask(xnetworker* pWorker, ptr pData)
{
	examplejob* pJob = (examplejob*)pData;
	xnetstream* pTcp = xrtWsStreamTcpRef(pJob->pStream);

	(void)pWorker;
	pJob->Ok = (pTcp != NULL) && (xrtWsStreamTcp(pJob->pStream) == pTcp);
	xrtNetStreamDestroy(pTcp);
#if defined(XRT_FEATURE_WEBSOCKET_STREAM_TLS)
	{
		xtlsstream* pTls = xrtWsStreamTlsRef(pJob->pStream);

		pJob->Ok = pJob->Ok && (pTls == NULL) &&
			(xrtWsStreamTls(pJob->pStream) == NULL) &&
			(xrtWsStreamAttachTls(NULL, 0, NULL, NULL, NULL) == NULL);
		xrtTlsStreamDestroy(pTls);
		xrtClearError();  /* 上面的空传输是有意构造的负路径。 */
	}
#endif
	xrtAtomic32Store(&pJob->Done, 1, XMEMORY_RELEASE);
}

static void exampleAfterTask(xnetworker* pWorker, ptr pData)
{
	examplejob* pJob = (examplejob*)pData;

	(void)pWorker;
	pJob->Ok = xrtWsStreamText(pJob->pStream,
		XRT_STR_LITERAL("after-pause")) == XNET_RESULT_OK;
	xrtAtomic32Store(&pJob->Done, 1, XMEMORY_RELEASE);
}

#if defined(XRT_FEATURE_WEBSOCKET_STREAM_DEFLATE)
static void exampleCompressedTask(xnetworker* pWorker, ptr pData)
{
	examplejob* pJob = (examplejob*)pData;

	(void)pWorker;
	pJob->Ok = (xrtWsStreamTextCompressed(pJob->pStream,
		XRT_STR_LITERAL("compressed")) == XNET_RESULT_OK) &&
		(xrtWsStreamBinaryCompressed(pJob->pStream,
			(xbytesview) { (cbytes)"c-b", 3 }) == XNET_RESULT_OK) &&
		(xrtWsStreamSendCompressed(pJob->pStream, XWS_OPCODE_TEXT,
			(xbytesview) { (cbytes)"c-s", 3 }) == XNET_RESULT_OK);
	xrtAtomic32Store(&pJob->Done, 1, XMEMORY_RELEASE);
}
#endif

static void exampleCloseTask(xnetworker* pWorker, ptr pData)
{
	examplejob* pJob = (examplejob*)pData;

	(void)pWorker;
	pJob->Ok = xrtWsStreamClose(pJob->pStream, 1000,
		XRT_STR_LITERAL("done")) == XNET_RESULT_OK;
	xrtAtomic32Store(&pJob->Done, 1, XMEMORY_RELEASE);
}

int main(void)
{
	static const char Expected[] = "hello ws" "\x01\x02\x03" "plain-send"
#if defined(XRT_FEATURE_WEBSOCKET_STREAM_REF)
		"text-ref" "send-ref" "text-take" "binarytake" "sendtake"
#endif
		;
	exampleendpoint Client;
	exampleendpoint Server;
	examplejob Job;
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetstreamconfig TcpConfig;
	xnetengine* pEngine = NULL;
	xnetlistener* pListener = NULL;
	xwsstream* pClientRef = NULL;
	xnetaddr Address;
	xwsstreamclose CloseInfo;
	xdeadline Deadline;
	uint32 iExpected = (uint32)sizeof(Expected) - 1u;
	int iResult = 1;

	exampleEndpointInit(&Client, XWS_ROLE_CLIENT);
	exampleEndpointInit(&Server, XWS_ROLE_SERVER);
	xrtAtomic32Init(&Job.Done, 0);
#if defined(XRT_FEATURE_WEBSOCKET_STREAM_REF)
	xrtAtomic32Init(&g_Releases, 0);
#endif
	if ( !xrtWsStreamConfigValid(&Client.Config) ) goto Cleanup;
	Client.Config.MessageLimit = 0;
	if ( xrtWsStreamConfigValid(&Client.Config) ) goto Cleanup;
	Client.Config.MessageLimit = XWS_STREAM_MESSAGE_LIMIT_DEFAULT;
	xrtClearError();
	printf("ws-stream: config valid=1 corrupt=0\n");
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Workers = EXAMPLE_WS_WORKERS;
	EngineConfig.Backend = EXAMPLE_WS_BACKEND;
	xrtNetListenConfigInit(&ListenConfig);
	xrtNetStreamConfigInit(&TcpConfig);
#if defined(EXAMPLE_WS_HIGH_WATER)
	TcpConfig.WriteHighWater = ListenConfig.Stream.WriteHighWater = EXAMPLE_WS_HIGH_WATER;
	TcpConfig.WriteLowWater = ListenConfig.Stream.WriteLowWater = 0;
#endif
	if ( !xrtNetAddrLoopback(&ListenConfig.Address, XNET_FAMILY_IPV4, 0) ) goto Cleanup;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	if ( (pEngine == NULL) || !xrtNetEngineStart(pEngine) ) goto Cleanup;
	pListener = xrtNetListen(pEngine, &ListenConfig, NULL, NULL, NULL);
	if ( (pListener == NULL) || !xrtNetListenerLocal(pListener, &Address) ) goto Cleanup;
	Client.pTcp = xrtNetStreamConnect(pEngine, &Address, 0, &TcpConfig, NULL, NULL);
	if ( Client.pTcp == NULL ) goto Cleanup;
	Deadline = xrtDeadlineAfter(EXAMPLE_DEADLINE_US);
	while ( (Server.pTcp = xrtNetListenerAccept(pListener)) == NULL ) {
		if ( xrtDeadlineExpired(Deadline) ) goto Cleanup;
		xrtThreadYield();
	}
	while ( xrtNetStreamState(Client.pTcp) != XNET_STREAM_OPEN ) {
		if ( xrtDeadlineExpired(Deadline) ) goto Cleanup;
		xrtThreadYield();
	}
	if ( !exampleAttach(pEngine, &Client) ||
		!exampleAttach(pEngine, &Server) ) goto Cleanup;
	pClientRef = xrtWsStreamRef(Client.pStream);
	if ( (pClientRef == NULL) ||
		!exampleRun(pEngine, &Job, Client.pStream, exampleQueryTask) ||
		!exampleRun(pEngine, &Job, Client.pStream, exampleSendTask) ||
		!exampleWait(&Client.Received, iExpected) ||
		!exampleWait(&Client.Pong, 1) || !exampleWait(&Server.Pong, 1) ||
		(xrtAtomic32Load(&Client.Received, XMEMORY_ACQUIRE) != iExpected) ||
		(memcmp(Client.Buffer, Expected, iExpected) != 0) ) goto Cleanup;
#if defined(XRT_FEATURE_WEBSOCKET_STREAM_REF)
	if ( !exampleWait(&g_Releases, 2) ||
		(xrtAtomic32Load(&g_Releases, XMEMORY_ACQUIRE) != 2) ) goto Cleanup;
#endif
	/* 无需额外 Resume：背压排空与显式应用暂停是两个不同的状态。 */
	printf("ws-stream: echo verified %u bytes, pong=auto+manual\n", (unsigned)iExpected);
	xrtWsStreamPause(Client.pStream);
	if ( !xrtWsStreamPaused(Client.pStream) ||
		!exampleRun(pEngine, &Job, Server.pStream, exampleAfterTask) ) goto Cleanup;
	Deadline = xrtDeadlineAfter(200000u);
	while ( !xrtDeadlineExpired(Deadline) ) {
		if ( xrtAtomic32Load(&Client.Received, XMEMORY_ACQUIRE) != iExpected ) goto Cleanup;
		xrtThreadYield();
	}
	if ( !xrtWsStreamResume(Client.pStream) ||
		!exampleWait(&Client.Received, iExpected + 11u) ||
		(memcmp(Client.Buffer + iExpected, "after-pause", 11u) != 0) ) goto Cleanup;
	iExpected += 11u;
	printf("ws-stream: flow pause=held resume=delivered\n");
	if ( (xrtWsStreamRole(Client.pStream) != XWS_ROLE_CLIENT) ||
		(xrtWsStreamRole(Server.pStream) != XWS_ROLE_SERVER) ||
		(xrtWsStreamProtocol(Client.pStream).Size != 0) ||
		(xrtWsStreamProtocol(Server.pStream).Size != 0) ) goto Cleanup;
	printf("ws-stream: roles c=0 s=1 protocol=1\n");
#if defined(XRT_FEATURE_WEBSOCKET_STREAM_DEFLATE)
	{
		xwsdeflate Deflate;

		if ( !xrtWsStreamDeflate(Client.pStream, &Deflate) ||
			!exampleRun(pEngine, &Job, Client.pStream, exampleCompressedTask) ||
			!exampleWait(&Client.Received, iExpected + 16u) ||
			(memcmp(Client.Buffer + iExpected, "compressedc-bc-s", 16u) != 0) ) goto Cleanup;
		iExpected += 16u;
	}
	printf("ws-stream: compressed trio echo verified\n");
#endif
	Deadline = xrtDeadlineAfter(EXAMPLE_DEADLINE_US);
	while ( (xrtWsStreamPending(Client.pStream) != 0u) ||
		(xrtWsStreamPending(Server.pStream) != 0u) ) {
		if ( xrtDeadlineExpired(Deadline) ) goto Cleanup;
		xrtThreadYield();
	}
	if ( (xrtWsStreamWritable(Client.pStream) == 0u) ||
		(xrtAtomic32Load(&Client.Received, XMEMORY_ACQUIRE) != iExpected) ||
		!exampleRun(pEngine, &Job, Client.pStream, exampleCloseTask) ||
		!exampleWait(&Client.Closed, 1) || !exampleWait(&Server.Closed, 1) ||
		!xrtWsStreamCloseInfo(Client.pStream, &CloseInfo) ||
		((CloseInfo.Flags & (XWS_STREAM_CLOSE_SENT | XWS_STREAM_CLOSE_RECEIVED |
			XWS_STREAM_CLOSE_CLEAN)) != (XWS_STREAM_CLOSE_SENT |
			XWS_STREAM_CLOSE_RECEIVED | XWS_STREAM_CLOSE_CLEAN)) ||
		(CloseInfo.RemoteCode != 1000u) ||
		(xrtWsStreamError(Client.pStream) != NULL) ||
		(xrtWsStreamError(Server.pStream) != NULL) ||
		(xrtAtomic32Load(&Client.Errors, XMEMORY_ACQUIRE) != 0) ||
		(xrtAtomic32Load(&Server.Errors, XMEMORY_ACQUIRE) != 0) ||
		/* 双方干净关闭后：并发可读的会话状态为 CLOSED。 */
		(xrtWsStreamState(Client.pStream) != XWS_STREAM_CLOSED) ||
		(xrtWsStreamState(Server.pStream) != XWS_STREAM_CLOSED) ) goto Cleanup;
	printf("ws-stream: close flags=%u code=%u clean=1 error=(none)\n",
		(unsigned)CloseInfo.Flags, (unsigned)CloseInfo.RemoteCode);
	iResult = 0;
Cleanup:
	/* 先关闭并归还引用，Stop 返回后栈上的任务/事件上下文才结束。 */
	exampleEndpointRelease(&Client);
	exampleEndpointRelease(&Server);
	xrtWsStreamDestroy(pClientRef);
	if ( pListener != NULL ) (void)xrtNetListenerClose(pListener);
	/* ListenerClose 是异步请求；必须等关闭完成才能停止 Engine。 */
	Deadline = xrtDeadlineAfter(EXAMPLE_DEADLINE_US);
	while ( (pListener != NULL) &&
		(xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED) &&
		!xrtDeadlineExpired(Deadline) ) xrtThreadYield();
	xrtNetListenerDestroy(pListener);
	/* Close 状态先于最后一个内部引用释放；等待 Worker 完成收尾。 */
	while ( (pEngine != NULL) && !xrtNetEngineStop(pEngine) ) {
		/* 超时的 Attach 若刚刚完成，也须先收回结果再尝试 Stop。 */
		exampleEndpointRelease(&Client);
		exampleEndpointRelease(&Server);
		if ( xrtDeadlineExpired(Deadline) ) { iResult = 1; break; }
		xrtThreadYield();
	}
	exampleEndpointRelease(&Client);
	exampleEndpointRelease(&Server);
	if ( (pEngine != NULL) && !xrtNetEngineDestroy(pEngine) ) iResult = 1;
	if ( iResult != 0 ) fprintf(stderr, "ws-stream: verification failed\n");
	return iResult;
}
