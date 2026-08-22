#include "../fixtures/tls_server.h"



static const uint8 __g_TestHttpServerUpgradeTlsPayload[] =
	"tls-upgrade-early-payload";



/* 保存 HTTPS Upgrade 两端的传输所有权和异步终态。 */
typedef struct test_http_server_upgrade_tls {
	xnetengine* Engine;
	xhttpserver* Server;
	xtlsstream* Client;
	xtlsstream* Upgraded;
	xatomic32 Completed;
	xatomic32 Received;
	xatomic32 ServerClosed;
	xatomic32 ClientClosed;
	xatomic32 HttpClosed;
	xatomic32 Errors;
	xatomic32 Shutdown;
	char Response[512];
	size_t ResponseSize;
	size_t Buffered;
	bool Submitting;
} test_http_server_upgrade_tls;



/* 在截止时间前等待 Worker 发布一个状态。 */
static void testHttpServerUpgradeTlsWait(
	const xatomic32* pValue,
	cstr sMessage
)
{
	xdeadline Deadline =
		xrtDeadlineAfter(UINT64_C(10000000));

	while ( xrtAtomic32Load(
		pValue,
		XMEMORY_ACQUIRE
	) == 0 ) {
		testRequire(
			!xrtDeadlineExpired(Deadline),
			sMessage
		);
		xrtThreadYield();
	}
}



/* 把升级后已经解密的全部明文回显给 TLS 客户端。 */
static void testHttpServerUpgradeTlsEcho(xtlsstream* pStream)
{
	uint8 Buffer[256];

	while ( xrtTlsStreamAvailable(pStream) != 0 ) {
		size_t iRead = 0;
		size_t iWritten = 0;

		testRequire(
			(xrtTlsStreamRead(
				pStream,
				Buffer,
				sizeof(Buffer),
				&iRead
			 ) == XTLS_OK) &&
			(iRead != 0) &&
			(xrtTlsStreamSend(
				pStream,
				Buffer,
				iRead,
				&iWritten
			 ) == XTLS_OK) &&
			(iWritten == iRead),
			"upgraded HTTPS TLS echo failed"
		);
	}
}



/* 新协议收到 TLS 明文后直接回显，不再进入 HTTP Parser。 */
static void testHttpServerUpgradeTlsRead(
	xtlsstream* pStream,
	const xnetbuf* pBuffer,
	ptr pData
)
{
	(void)pBuffer;
	(void)pData;
	testHttpServerUpgradeTlsEcho(pStream);
}



/* 客户端发送 close_notify 后由升级协议完成认证关闭。 */
static void testHttpServerUpgradeTlsEnd(
	xtlsstream* pStream,
	ptr pData
)
{
	(void)pData;
	testRequire(
		xrtTlsStreamClose(pStream),
		"upgraded HTTPS TLS close_notify failed"
	);
}



/* 记录由升级协议事件表观察到的服务端 TLS 终态。 */
static void testHttpServerUpgradeTlsServerClose(
	xtlsstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_http_server_upgrade_tls* pState =
		(test_http_server_upgrade_tls*)pData;

	(void)pStream;
	testRequire(
		(Result == XNET_RESULT_OK) &&
		(pError == NULL),
		"upgraded HTTPS server TLS terminal mismatch"
	);
	xrtAtomic32Store(
		&pState->ServerClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* 接管 HTTP 已摘除事件的 TLS Stream，并显式处理已解密余量。 */
static void testHttpServerUpgradeTlsComplete(
	xhttpconn* pConnection,
	xnetresult Result,
	xhttpupgrade Upgrade,
	const xerror* pError,
	ptr pData
)
{
	test_http_server_upgrade_tls* pState =
		(test_http_server_upgrade_tls*)pData;
	xtlsstreamevents Events;

	testRequire(
		!pState->Submitting,
		"HTTPS Upgrade completion reentered submit stack"
	);
	testRequire(
		(Result == XNET_RESULT_OK) &&
		(pError == NULL) &&
		(Upgrade.Tcp == NULL) &&
		(Upgrade.Tls != NULL) &&
		(xrtHttpConnState(pConnection) ==
		 XHTTP_CONN_UPGRADED),
		"HTTPS Upgrade completion mismatch"
	);
	memset(&Events, 0, sizeof(Events));
	Events.Read = testHttpServerUpgradeTlsRead;
	Events.End = testHttpServerUpgradeTlsEnd;
	Events.Close = testHttpServerUpgradeTlsServerClose;
	testRequire(
		xrtTlsStreamSetEvents(
			Upgrade.Tls,
			&Events,
			pState
		),
		"upgraded HTTPS TLS event install failed"
	);
	pState->Upgraded = Upgrade.Tls;
	pState->Buffered = Upgrade.Buffered;
	testHttpServerUpgradeTlsEcho(Upgrade.Tls);
	xrtAtomic32Store(
		&pState->Completed,
		1,
		XMEMORY_RELEASE
	);
}



/* 构建 101 响应并注册 TLS 传输接管过程。 */
static void testHttpServerUpgradeTlsRequest(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	ptr pData
)
{
	test_http_server_upgrade_tls* pState =
		(test_http_server_upgrade_tls*)pData;
	xhttpreply* pReply;

	(void)pServer;
	testRequire(
		xrtHttpConnSecure(pConnection) &&
		((xrtHttpServerRequestFlags(pRequest) &
		  XHTTP_SERVER_REQUEST_UPGRADE) != 0),
		"HTTPS Upgrade request contract mismatch"
	);
	pReply = xrtHttpReplyCreate(101);
	testRequire(
		(pReply != NULL) &&
		xrtHttpReplySetHeader(
			pReply,
			XRT_STR_LITERAL("Connection"),
			XRT_STR_LITERAL("Upgrade")
		) &&
		xrtHttpReplySetHeader(
			pReply,
			XRT_STR_LITERAL("Upgrade"),
			XRT_STR_LITERAL("xrt-test")
		),
		"HTTPS Upgrade Reply creation failed"
	);
	pState->Submitting = true;
	testRequire(
		xrtHttpConnUpgrade(
			pConnection,
			pReply,
			testHttpServerUpgradeTlsComplete,
			pState
		) == XNET_RESULT_OK,
		"HTTPS Upgrade submission failed"
	);
	testRequire(
		xrtAtomic32Load(
			&pState->Completed,
			XMEMORY_ACQUIRE
		) == 0,
		"HTTPS Upgrade completed inside submit call"
	);
	pState->Submitting = false;
	xrtHttpReplyDestroy(pReply);
}



/* Upgrade 成功后 HTTP 层不应再发布 Connection Close。 */
static void testHttpServerUpgradeTlsHttpClose(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_http_server_upgrade_tls* pState =
		(test_http_server_upgrade_tls*)pData;

	(void)pServer;
	(void)pConnection;
	(void)Result;
	(void)pError;
	(void)xrtAtomic32FetchAdd(
		&pState->HttpClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* 正常测试路径不允许 HTTP 层发布错误。 */
static void testHttpServerUpgradeTlsError(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xerror* pError,
	ptr pData
)
{
	test_http_server_upgrade_tls* pState =
		(test_http_server_upgrade_tls*)pData;

	(void)pServer;
	(void)pConnection;
	(void)pError;
	(void)xrtAtomic32FetchAdd(
		&pState->Errors,
		1,
		XMEMORY_RELEASE
	);
}



/* Server 排空只等待仍由 HTTP 层持有的连接。 */
static void testHttpServerUpgradeTlsShutdown(
	xhttpserver* pServer,
	ptr pData
)
{
	test_http_server_upgrade_tls* pState =
		(test_http_server_upgrade_tls*)pData;

	testRequire(
		xrtHttpServerState(pServer) ==
			XHTTP_SERVER_CLOSED,
		"HTTPS Upgrade server shutdown state mismatch"
	);
	xrtAtomic32Store(
		&pState->Shutdown,
		1,
		XMEMORY_RELEASE
	);
}



/* TLS 握手完成后一次发送 Upgrade 请求和紧随其后的协议载荷。 */
static void testHttpServerUpgradeTlsClientOpen(
	xtlsstream* pStream,
	ptr pData
)
{
	static const uint8 Request[] =
		"GET /chat HTTP/1.1\r\n"
		"Host: upgrade.test\r\n"
		"Connection: Upgrade\r\n"
		"Upgrade: xrt-test\r\n"
		"\r\n"
		"tls-upgrade-early-payload";
	size_t iWritten = 0;

	(void)pData;
	testRequire(
		(xrtTlsStreamSend(
			pStream,
			Request,
			sizeof(Request) - 1u,
			&iWritten
		 ) == XTLS_OK) &&
		(iWritten == (sizeof(Request) - 1u)),
		"HTTPS Upgrade client request send failed"
	);
}



/* 收集 101 响应和新协议回显，并发布完整接收状态。 */
static void testHttpServerUpgradeTlsClientRead(
	xtlsstream* pStream,
	const xnetbuf* pBuffer,
	ptr pData
)
{
	test_http_server_upgrade_tls* pState =
		(test_http_server_upgrade_tls*)pData;
	size_t iAvailable = xrtTlsStreamAvailable(pStream);
	size_t iPayload =
		sizeof(__g_TestHttpServerUpgradeTlsPayload) - 1u;

	testRequire(
		(iAvailable != 0) &&
		(iAvailable <=
		 (sizeof(pState->Response) -
		  pState->ResponseSize - 1u)) &&
		(xrtNetBufPeek(
			pBuffer,
			0,
			pState->Response + pState->ResponseSize,
			iAvailable
		 ) == iAvailable) &&
		xrtTlsStreamConsume(pStream, iAvailable),
		"HTTPS Upgrade client response consume failed"
	);
	pState->ResponseSize += iAvailable;
	if ( (pState->ResponseSize >= iPayload) &&
		(memcmp(
			pState->Response +
				pState->ResponseSize - iPayload,
			__g_TestHttpServerUpgradeTlsPayload,
			iPayload
		 ) == 0) ) {
		xrtAtomic32Store(
			&pState->Received,
			1,
			XMEMORY_RELEASE
		);
	}
}



/* 客户端收到对端 close_notify 后回送认证关闭。 */
static void testHttpServerUpgradeTlsClientEnd(
	xtlsstream* pStream,
	ptr pData
)
{
	(void)pData;
	testRequire(
		xrtTlsStreamClose(pStream),
		"HTTPS Upgrade client close_notify failed"
	);
}



/* 记录 TLS 客户端已经完成认证关闭。 */
static void testHttpServerUpgradeTlsClientClose(
	xtlsstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_http_server_upgrade_tls* pState =
		(test_http_server_upgrade_tls*)pData;

	testRequire(
		(Result == XNET_RESULT_OK) &&
		(pError == NULL) &&
		(xrtTlsStreamState(pStream) == XTLS_STREAM_CLOSED),
		"HTTPS Upgrade client TLS terminal mismatch"
	);
	xrtAtomic32Store(
		&pState->ClientClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* 验证 HTTPS 明文余量、TLS 所有权和 Server 排空边界。 */
int main(void)
{
	static const xstrview ClientProtocols[] = {
		XRT_STR_INIT("http/1.1")
	};
	test_http_server_upgrade_tls State;
	xnetengineconfig EngineConfig;
	xhttpserverconfig ServerConfig;
	xhttpservertlsconfig ServerTls;
	xhttpserverevents ServerEvents;
	xtlsclientconfig ClientTls;
	xtlsstreamconfig ClientStream;
	xtlsstreamevents ClientEvents;
	xtlsverifierconfig VerifierConfig;
	xtlscontext* pServerContext;
	xtlscontext* pClientContext;
	xtlsidentity* pIdentity;
	xtlsverifier* pVerifier;
	xhttpserverstats Stats;
	xnetaddr Address;
	xstrview Protocol = XRT_STR_LITERAL("http/1.1");
	size_t iPayload =
		sizeof(__g_TestHttpServerUpgradeTlsPayload) - 1u;

	memset(&State, 0, sizeof(State));
	xrtAtomic32Init(&State.Completed, 0);
	xrtAtomic32Init(&State.Received, 0);
	xrtAtomic32Init(&State.ServerClosed, 0);
	xrtAtomic32Init(&State.ClientClosed, 0);
	xrtAtomic32Init(&State.HttpClosed, 0);
	xrtAtomic32Init(&State.Errors, 0);
	xrtAtomic32Init(&State.Shutdown, 0);
	pServerContext = testTlsServerContext();
	pClientContext = testTlsServerContext();
	pIdentity = testTlsServerIdentity();
	testRequire(
		(pServerContext != NULL) &&
		(pClientContext != NULL) &&
		(pIdentity != NULL),
		"HTTPS Upgrade TLS fixtures failed"
	);
	xrtTlsVerifierConfigInit(&VerifierConfig);
	VerifierConfig.Verify = testTlsServerAccept;
	pVerifier = xrtTlsVerifierCreate(&VerifierConfig);
	testRequire(
		pVerifier != NULL,
		"HTTPS Upgrade verifier creation failed"
	);

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 2;
	State.Engine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(State.Engine != NULL) &&
		xrtNetEngineStart(State.Engine),
		"HTTPS Upgrade Engine start failed"
	);
	xrtHttpServerConfigInit(&ServerConfig);
	testRequire(
		xrtNetAddrLoopback(
			&ServerConfig.Network.Listen.Address,
			XNET_FAMILY_IPV4,
			0
		),
		"HTTPS Upgrade address setup failed"
	);
	ServerConfig.WriteSize = 5;
	xrtHttpServerTlsConfigInit(&ServerTls);
	ServerTls.Handshake.Context = pServerContext;
	ServerTls.Handshake.Identity = pIdentity;
	ServerTls.Handshake.Protocols = &Protocol;
	ServerTls.Handshake.ProtocolCount = 1;
	ServerTls.Handshake.RequireProtocol = true;
	xrtHttpServerEventsInit(&ServerEvents);
	ServerEvents.Request = testHttpServerUpgradeTlsRequest;
	ServerEvents.Close = testHttpServerUpgradeTlsHttpClose;
	ServerEvents.Error = testHttpServerUpgradeTlsError;
	ServerEvents.Shutdown = testHttpServerUpgradeTlsShutdown;
	ServerEvents.Data = &State;
	State.Server = xrtHttpServerStartTls(
		State.Engine,
		&ServerConfig,
		&ServerTls,
		&ServerEvents
	);
	testRequire(
		(State.Server != NULL) &&
		xrtHttpServerLocal(State.Server, 0, &Address),
		"HTTPS Upgrade Server start failed"
	);
	xrtTlsIdentityRelease(pIdentity);
	xrtTlsContextRelease(pServerContext);

	xrtTlsClientConfigInit(&ClientTls);
	ClientTls.Context = pClientContext;
	ClientTls.ServerName =
		XRT_STR_LITERAL("example.com");
	ClientTls.Protocols = ClientProtocols;
	ClientTls.ProtocolCount =
		sizeof(ClientProtocols) /
		sizeof(ClientProtocols[0]);
	ClientTls.Verifier = pVerifier;
	xrtTlsStreamConfigInit(&ClientStream);
	memset(&ClientEvents, 0, sizeof(ClientEvents));
	ClientEvents.Open = testHttpServerUpgradeTlsClientOpen;
	ClientEvents.Read = testHttpServerUpgradeTlsClientRead;
	ClientEvents.End = testHttpServerUpgradeTlsClientEnd;
	ClientEvents.Close = testHttpServerUpgradeTlsClientClose;
	State.Client = xrtTlsStreamConnect(
		State.Engine,
		&Address,
		1,
		NULL,
		&ClientTls,
		&ClientStream,
		&ClientEvents,
		&State
	);
	testRequire(
		State.Client != NULL,
		"HTTPS Upgrade client connect failed"
	);
	xrtTlsVerifierRelease(pVerifier);
	xrtTlsContextRelease(pClientContext);

	testHttpServerUpgradeTlsWait(
		&State.Completed,
		"HTTPS Upgrade completion missing"
	);
	testHttpServerUpgradeTlsWait(
		&State.Received,
		"HTTPS Upgrade echoed payload missing"
	);
	State.Response[State.ResponseSize] = '\0';
	testRequire(
		(State.Buffered == iPayload) &&
		(memcmp(
			State.Response,
			"HTTP/1.1 101 Switching Protocols\r\n",
			sizeof("HTTP/1.1 101 Switching Protocols\r\n") - 1u
		 ) == 0) &&
		(strstr(State.Response, "\r\n\r\n") != NULL),
		"HTTPS Upgrade response or buffered plaintext mismatch"
	);
	testRequire(
		xrtHttpServerDrain(State.Server),
		"HTTPS Upgrade Server drain failed"
	);
	testHttpServerUpgradeTlsWait(
		&State.Shutdown,
		"HTTPS Upgrade Server waited for transferred TLS Stream"
	);
	testRequire(
		xrtTlsStreamClose(State.Client),
		"HTTPS Upgrade client close failed"
	);
	testHttpServerUpgradeTlsWait(
		&State.ServerClosed,
		"upgraded HTTPS server TLS close missing"
	);
	testHttpServerUpgradeTlsWait(
		&State.ClientClosed,
		"HTTPS Upgrade client TLS close missing"
	);
	testRequire(
		xrtHttpServerStats(State.Server, &Stats) &&
		(Stats.Accepted == 1) &&
		(Stats.Requests == 1) &&
		(Stats.Responses == 1) &&
		(Stats.Upgraded == 1) &&
		(Stats.Connections == 0) &&
		(xrtAtomic32Load(
			&State.HttpClosed,
			XMEMORY_ACQUIRE
		 ) == 0) &&
		(xrtAtomic32Load(
			&State.Errors,
			XMEMORY_ACQUIRE
		 ) == 0),
		"HTTPS Upgrade statistics or ownership mismatch"
	);
	xrtTlsStreamDestroy(State.Upgraded);
	xrtTlsStreamDestroy(State.Client);
	xrtHttpServerDestroy(State.Server);
	testRequire(
		xrtNetEngineDestroy(State.Engine),
		"HTTPS Upgrade Engine destroy failed"
	);
	printf(
		"[PASS] HTTPS server Upgrade ownership "
		"(select, buffered=%zu)\n",
		State.Buffered
	);
	return 0;
}
