#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <xssh.h>



typedef struct testsshsessionstreamcontext testsshsessionstreamcontext;



/* 每个 endpoint 的适配器、Stream 和协议事件都只在同一 Worker 上访问。 */
typedef struct testsshsessionstreamendpoint {
	testsshsessionstreamcontext* Context;
	xsshsessionstream Session;
	xnetstream* Stream;
	xstrview Version;
	bool Client;
} testsshsessionstreamendpoint;



/* 主线程只通过原子计数观察单 Worker 上的驱动进度。 */
struct testsshsessionstreamcontext {
	testsshsessionstreamendpoint Client;
	testsshsessionstreamendpoint Server;
	xnetengine* Engine;
	xatomic32 Accepted;
	xatomic32 Opened;
	xatomic32 Identifications;
	xatomic32 Held;
	xatomic32 Resumed;
	xatomic32 KexActions;
	xatomic32 Errors;
	xatomic32 Ended;
	xatomic32 Closed;
	xatomic32 ListenerClosed;
};



/* 测试失败立即终止，避免异步回调继续使用不完整状态。 */
static void testRequire(bool bValue, const char* sMessage)
{
	if ( !bValue ) {
		fprintf(stderr, "%s\n", sMessage);
		exit(1);
	}
}



/* 等待异步 Worker 发布指定计数。 */
static void testSshSessionStreamWait(
	const xatomic32* pValue,
	uint32 iExpected,
	const char* sMessage
)
{
	uint32 i;

	for ( i = 0u; i < 5000u; ++i ) {
		if ( xrtAtomic32Load(pValue, XMEMORY_ACQUIRE) >= iExpected ) {
			return;
		}
		xrtSleep(1u);
	}
	testRequire(false, sMessage);
}



/* 应用只准备协议要求的输出，适配器负责把完整事务提交到有界 TCP 队列。 */
static void testSshSessionStreamAction(
	xsshsessionstream* pSession,
	xsshsessionaction Action,
	ptr pData
)
{
	testsshsessionstreamendpoint* pEndpoint =
		(testsshsessionstreamendpoint*)pData;
	xsshsessiontcp* pTcp = xrtSshSessionStreamSession(pSession);

	testRequire((pSession == &pEndpoint->Session) &&
		(pTcp != NULL) &&
		(xrtSshSessionStreamReader(pSession) != NULL),
		"ssh stream action accessors failed");
	if ( Action == XSSH_SESSION_ACTION_WRITE_IDENTIFICATION ) {
		testRequire(xrtSshSessionTcpIdentificationWritePrepare(
			pTcp,
			pEndpoint->Version
		) == XSSH_OK, "ssh stream identification prepare failed");
	} else if ( Action == XSSH_SESSION_ACTION_WRITE_KEXINIT ) {
		(void)xrtAtomic32FetchAdd(
			&pEndpoint->Context->KexActions,
			1u,
			XMEMORY_RELEASE
		);
	}
}



/* 客户端先保留 peer identification，服务端沿自动快路径立即提交。 */
static xsshsessionstreamdecision testSshSessionStreamIdentification(
	xsshsessionstream* pSession,
	xstrview Version,
	ptr pData
)
{
	testsshsessionstreamendpoint* pEndpoint =
		(testsshsessionstreamendpoint*)pData;

	testRequire((pSession == &pEndpoint->Session) &&
		(Version.Size >= 8u) &&
		(memcmp(Version.Data, "SSH-2.0-", 8u) == 0),
		"ssh stream peer identification view failed");
	(void)xrtAtomic32FetchAdd(
		&pEndpoint->Context->Identifications,
		1u,
		XMEMORY_RELEASE
	);
	if ( pEndpoint->Client ) {
		(void)xrtAtomic32FetchAdd(
			&pEndpoint->Context->Held,
			1u,
			XMEMORY_RELEASE
		);
		return XSSH_SESSION_STREAM_HOLD;
	}
	return XSSH_SESSION_STREAM_ACCEPT;
}



/* Open 在会话、Reader 和 Stream 已经形成同一 Worker 所有权后发布。 */
static void testSshSessionStreamOpen(
	xsshsessionstream* pSession,
	ptr pData
)
{
	testsshsessionstreamendpoint* pEndpoint =
		(testsshsessionstreamendpoint*)pData;

	testRequire((pSession == &pEndpoint->Session) &&
		(xrtSshSessionStreamState(pSession) == XSSH_SESSION_STREAM_OPEN) &&
		(xrtSshSessionStreamTcp(pSession) == pEndpoint->Stream),
		"ssh stream open ownership failed");
	(void)xrtAtomic32FetchAdd(
		&pEndpoint->Context->Opened,
		1u,
		XMEMORY_RELEASE
	);
}



/* 协议或分配错误在本回环中都属于回归失败。 */
static void testSshSessionStreamError(
	xsshsessionstream* pSession,
	xsshcode Code,
	const xerror* pError,
	ptr pData
)
{
	testsshsessionstreamendpoint* pEndpoint =
		(testsshsessionstreamendpoint*)pData;

	(void)pSession;
	(void)Code;
	(void)pError;
	(void)xrtAtomic32FetchAdd(
		&pEndpoint->Context->Errors,
		1u,
		XMEMORY_RELEASE
	);
}



/* peer EOF 在 HOLD 中也只发布一次，事务决定后再完成关闭。 */
static void testSshSessionStreamEnd(
	xsshsessionstream* pSession,
	ptr pData
)
{
	testsshsessionstreamendpoint* pEndpoint =
		(testsshsessionstreamendpoint*)pData;

	testRequire((pSession == &pEndpoint->Session) &&
		(!pEndpoint->Client ||
		 (xrtSshSessionStreamState(pSession) ==
		  XSSH_SESSION_STREAM_HOLD_IDENTIFICATION)),
		"ssh stream EOF did not preserve held transaction");
	(void)xrtAtomic32FetchAdd(
		&pEndpoint->Context->Ended,
		1u,
		XMEMORY_RELEASE
	);
}



/* Close 回调期间低层对象仍可检查，返回后由适配器释放动态会话块。 */
static void testSshSessionStreamClose(
	xsshsessionstream* pSession,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	testsshsessionstreamendpoint* pEndpoint =
		(testsshsessionstreamendpoint*)pData;

	testRequire((Result == XNET_RESULT_OK) && (pError == NULL) &&
		(xrtSshSessionStreamState(pSession) ==
		 XSSH_SESSION_STREAM_CLOSING) &&
		(xrtSshSessionStreamSession(pSession) != NULL),
		"ssh stream close transaction failed");
	(void)xrtAtomic32FetchAdd(
		&pEndpoint->Context->Closed,
		1u,
		XMEMORY_RELEASE
	);
}



/* Listener 在 accepted Stream 的 Worker 上安装服务端适配器。 */
static bool testSshSessionStreamAccept(
	xnetlistener* pListener,
	xnetstream* pStream,
	ptr pData
)
{
	testsshsessionstreamcontext* pContext =
		(testsshsessionstreamcontext*)pData;

	(void)pListener;
	pContext->Server.Stream = pStream;
	testRequire(xrtSshSessionStreamAttach(
		&pContext->Server.Session,
		pStream
	), "ssh stream accepted attach failed");
	(void)xrtAtomic32FetchAdd(
		&pContext->Accepted,
		1u,
		XMEMORY_RELEASE
	);
	return true;
}



/* Listener 关闭只发布一次完成事件。 */
static void testSshSessionStreamListenerClose(
	xnetlistener* pListener,
	ptr pData
)
{
	testsshsessionstreamcontext* pContext =
		(testsshsessionstreamcontext*)pData;

	(void)pListener;
	(void)xrtAtomic32FetchAdd(
		&pContext->ListenerClosed,
		1u,
		XMEMORY_RELEASE
	);
}



/* HOLD 的协议事务必须回到所属 Worker 后才允许提交。 */
static void testSshSessionStreamResume(
	xnetworker* pWorker,
	ptr pData
)
{
	testsshsessionstreamcontext* pContext =
		(testsshsessionstreamcontext*)pData;
	const xnetstreamevents* pEvents = xrtSshSessionStreamNetEvents();

	(void)pWorker;
	testRequire((pEvents != NULL) && (pEvents->End != NULL) &&
		(xrtSshSessionStreamState(
		&pContext->Client.Session
		) == XSSH_SESSION_STREAM_HOLD_IDENTIFICATION),
		"ssh stream held state missing before EOF");
	pEvents->End(
		pContext->Client.Stream,
		&pContext->Client.Session
	);
	testRequire((xrtAtomic32Load(
		&pContext->Ended,
		XMEMORY_ACQUIRE
	) == 1u) && (xrtSshSessionStreamState(
		&pContext->Client.Session
	) == XSSH_SESSION_STREAM_HOLD_IDENTIFICATION) &&
		(xrtSshSessionStreamVersion(
			&pContext->Client.Session
		).Size >= 8u) &&
		(xrtSshSessionStreamAccept(
			&pContext->Client.Session
		) == XSSH_OK), "ssh stream held identification resume failed");
	(void)xrtAtomic32FetchAdd(
		&pContext->Resumed,
		1u,
		XMEMORY_RELEASE
	);
}



/* 验证真实 select fallback 上的 callback 驱动、HOLD 和自动输出提交。 */
static void testSshSessionStreamLoopback(void)
{
	static const xsshsessionstreamevents Events = {
		testSshSessionStreamOpen,
		testSshSessionStreamAction,
		testSshSessionStreamIdentification,
		NULL,
		NULL,
		testSshSessionStreamError,
		testSshSessionStreamEnd,
		NULL,
		NULL,
		NULL,
		testSshSessionStreamClose
	};
	static const xnetlistenerevents ListenerEvents = {
		testSshSessionStreamAccept,
		NULL,
		testSshSessionStreamListenerClose
	};
	testsshsessionstreamcontext Context;
	xsshsessiontcpconfig ClientConfig;
	xsshsessiontcpconfig ServerConfig;
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetstreamconfig StreamConfig;
	xnetlistener* pListener;
	xnetaddr Address;

	memset(&Context, 0, sizeof(Context));
	Context.Client.Context = &Context;
	Context.Client.Client = true;
	Context.Client.Version = XRT_STR_LITERAL(
		"SSH-2.0-xssh_stream_client"
	);
	Context.Server.Context = &Context;
	Context.Server.Version = XRT_STR_LITERAL(
		"SSH-2.0-xssh_stream_server"
	);
	testRequire(xrtSshSessionTcpConfigInit(
		&ClientConfig,
		XSSH_ROLE_CLIENT
	) && xrtSshSessionTcpConfigInit(
		&ServerConfig,
		XSSH_ROLE_SERVER
	) && xrtSshSessionStreamInit(
		&Context.Client.Session,
		&ClientConfig,
		&Events,
		&Context.Client
	) && xrtSshSessionStreamInit(
		&Context.Server.Session,
		&ServerConfig,
		&Events,
		&Context.Server
	), "ssh stream adapter initialization failed");
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 1u;
	Context.Engine = xrtNetEngineCreate(&EngineConfig);
	testRequire((Context.Engine != NULL) &&
		xrtNetEngineStart(Context.Engine),
		"ssh stream select engine start failed");
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0u
	), "ssh stream listener address failed");
	ListenConfig.Stream.ReadSize = 16u;
	ListenConfig.Stream.ReadLimit = 4096u;
	pListener = xrtNetListen(
		Context.Engine,
		&ListenConfig,
		&ListenerEvents,
		NULL,
		&Context
	);
	testRequire((pListener != NULL) &&
		xrtNetListenerLocal(pListener, &Address),
		"ssh stream listener failed");
	xrtNetStreamConfigInit(&StreamConfig);
	StreamConfig.ReadSize = 16u;
	StreamConfig.ReadLimit = 4096u;
	StreamConfig.WriteHighWater = 2048u;
	StreamConfig.WriteLowWater = 1024u;
	StreamConfig.WriteLimit = 4096u;
	Context.Client.Stream = xrtNetStreamConnect(
		Context.Engine,
		&Address,
		1u,
		&StreamConfig,
		xrtSshSessionStreamNetEvents(),
		&Context.Client.Session
	);
	testRequire((Context.Client.Stream != NULL) &&
		(xrtSshSessionStreamTcp(
			&Context.Client.Session
		) == NULL), "ssh stream connect creation failed");
	testSshSessionStreamWait(&Context.Accepted, 1u,
		"ssh stream accept missing");
	testSshSessionStreamWait(&Context.Opened, 2u,
		"ssh stream open missing or duplicated");
	testSshSessionStreamWait(&Context.Identifications, 2u,
		"ssh stream identifications incomplete");
	testSshSessionStreamWait(&Context.Held, 1u,
		"ssh stream client HOLD missing");
	testRequire(xrtNetEnginePost(
		Context.Engine,
		1u,
		testSshSessionStreamResume,
		&Context
	), "ssh stream resume post failed");
	testSshSessionStreamWait(&Context.Resumed, 1u,
		"ssh stream HOLD resume missing");
	testSshSessionStreamWait(&Context.KexActions, 2u,
		"ssh stream KEX actions missing");
	testRequire((xrtAtomic32Load(
		&Context.Opened,
		XMEMORY_ACQUIRE
	) == 2u) && (xrtAtomic32Load(
		&Context.Errors,
		XMEMORY_ACQUIRE
	) == 0u) && !xrtSshSessionStreamAbort(
		&Context.Client.Session
	), "ssh stream callback or Worker ownership contract failed");
	testSshSessionStreamWait(&Context.Closed, 2u,
		"ssh stream close callbacks missing");
	testRequire(xrtNetListenerClose(pListener),
		"ssh stream listener close failed");
	testSshSessionStreamWait(&Context.ListenerClosed, 1u,
		"ssh stream listener callback missing");
	xrtNetStreamDestroy(Context.Client.Stream);
	xrtNetStreamDestroy(Context.Server.Stream);
	xrtNetListenerDestroy(pListener);
	testRequire(xrtNetEngineDestroy(Context.Engine) &&
		(xrtSshSessionStreamState(
			&Context.Client.Session
		) == XSSH_SESSION_STREAM_CLOSED) &&
		(xrtSshSessionStreamState(
			&Context.Server.Session
		) == XSSH_SESSION_STREAM_CLOSED) &&
		xrtSshSessionStreamClear(&Context.Client.Session) &&
		xrtSshSessionStreamClear(&Context.Server.Session),
		"ssh stream cleanup failed");
}



/* 运行 callback Stream 所有权和状态事务回归。 */
int main(void)
{
	testSshSessionStreamLoopback();
	return 0;
}
