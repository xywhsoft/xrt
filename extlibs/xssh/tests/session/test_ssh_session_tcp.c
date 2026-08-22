#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <xssh.h>



typedef struct testsshsessiontcpcontext testsshsessiontcpcontext;



/* 每个 endpoint 只在所属 Worker 上推进会话对象。 */
typedef struct testsshsessiontcpendpoint {
	testsshsessiontcpcontext* Context;
	xsshsessiontcp Session;
	xnetstream* Stream;
	bool Client;
	bool SentKexInit;
	bool Initialized;
} testsshsessiontcpendpoint;



/* 主线程只通过原子计数观察单 Worker 上的协议进度。 */
struct testsshsessiontcpcontext {
	testsshsessiontcpendpoint Client;
	testsshsessiontcpendpoint Server;
	xatomic32 Accepted;
	xatomic32 Opened;
	xatomic32 Versions;
	xatomic32 Packets;
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



/* 在确定性闭包中使用固定 padding，便于验证事务回滚。 */
static bool testSshSessionTcpPadding(
	void* pOutput,
	size_t iSize,
	ptr pUserData
)
{
	memset(pOutput, *(const uint8*)pUserData, iSize);
	return true;
}



/* 等待异步 Worker 发布指定计数。 */
static void testSshSessionTcpWait(
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



/* 构建一条最小但完整的客户端 KEXINIT payload。 */
static xbytesview testSshSessionTcpKexInit(
	void* pOutput,
	size_t iCapacity
)
{
	uint8 arrCookie[XSSH_KEX_COOKIE_SIZE];
	xsshkexinitconfig Config;
	xsshwriter Writer;
	size_t i;

	for ( i = 0u; i < sizeof(arrCookie); ++i ) {
		arrCookie[i] = (uint8)(0x20u + (uint8)i);
	}
	testRequire(xrtSshKexInitConfigInit(
		&Config,
		XSSH_ROLE_CLIENT,
		true
	) && xrtSshWriterInit(
		&Writer,
		pOutput,
		iCapacity
	) && (xrtSshKexInitWrite(
		&Writer,
		(xbytesview){ arrCookie, sizeof(arrCookie) },
		&Config
	) == XSSH_OK), "ssh TCP session KEXINIT build failed");
	return (xbytesview){ pOutput, Writer.Size };
}



/* 客户端识别到 peer 后发送一条经过两层绑定的 KEXINIT。 */
static void testSshSessionTcpSendKexInit(
	testsshsessiontcpendpoint* pEndpoint
)
{
	unsigned char arrPayload[512];
	uint8 iPadding = 0x5au;
	xsshrekeydecision Decision;
	xsshsessionpacketkind Kind;
	xbytesview Payload;

	if ( !pEndpoint->Client || pEndpoint->SentKexInit ) {
		return;
	}
	Payload = testSshSessionTcpKexInit(arrPayload, sizeof(arrPayload));
	testRequire((xrtSshSessionTcpWritePrepareWithPadding(
		&pEndpoint->Session,
		Payload,
		NULL,
		NULL,
		0u,
		testSshSessionTcpPadding,
		&iPadding,
		1u,
		&Kind
	) == XSSH_OK) && (Kind == XSSH_SESSION_PACKET_KEXINIT) &&
		(xrtSshSessionTcpWriteAbort(
			&pEndpoint->Session
		) == XSSH_OK) && (xrtSshSessionTcpWritePrepareWithPadding(
		&pEndpoint->Session,
		Payload,
		NULL,
		NULL,
		0u,
		testSshSessionTcpPadding,
		&iPadding,
		2u,
		&Kind
	) == XSSH_OK) && (xrtSshSessionTcpWriteSubmit(
		&pEndpoint->Session,
		pEndpoint->Stream,
		2u,
		&Decision
	) == XNET_RESULT_OK) && (Decision == XSSH_REKEY_NONE),
		"ssh TCP session KEXINIT transaction failed");
	pEndpoint->SentKexInit = true;
}



/* 增量消费 identification，随后在同一回调中继续处理残留 packet。 */
static void testSshSessionTcpRead(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	ptr pData
)
{
	testsshsessiontcpendpoint* pEndpoint =
		(testsshsessiontcpendpoint*)pData;

	(void)pStream;
	for ( ;; ) {
		xsshsessionphase Phase = xrtSshSessionTcpPhase(
			&pEndpoint->Session
		);

		if ( Phase == XSSH_SESSION_IDENTIFICATION ) {
			xsshrekeydecision Decision;
			xstrview Version;
			xsshcode Code = xrtSshSessionTcpIdentificationReadPrepare(
				&pEndpoint->Session,
				pBuffer,
				&Version
			);

			if ( Code == XSSH_NEED_MORE ) {
				return;
			}
			testRequire((Code == XSSH_OK) &&
				(Version.Size >= 8u) && (memcmp(
					Version.Data,
					"SSH-2.0-",
					8u
			) == 0) && (xrtSshSessionTcpReadCommit(
				&pEndpoint->Session,
				0u,
				&Decision
			) == XSSH_OK) && (Decision == XSSH_REKEY_NONE),
				"ssh TCP session identification read failed");
			(void)xrtAtomic32FetchAdd(
				&pEndpoint->Context->Versions,
				1u,
				XMEMORY_RELEASE
			);
			testSshSessionTcpSendKexInit(pEndpoint);
			continue;
		}
		if ( xrtNetBufEmpty(pBuffer) ) {
			return;
		}
		{
			unsigned char arrPlain[1024];
			xsshrekeydecision Decision;
			xsshsessiontcppacket Packet;
			xsshpacketneed Need;
			xsshcode Code = xrtSshSessionTcpReadInspect(
				&pEndpoint->Session,
				pBuffer,
				&Need
			);

			if ( (Code == XSSH_NEED_MORE) ||
				((Code == XSSH_OK) &&
				 (xrtNetBufSize(pBuffer) < Need.WireSize)) ) {
				return;
			}
			testRequire((Code == XSSH_OK) &&
				(xrtSshSessionTcpReadPrepare(
					&pEndpoint->Session,
					pBuffer,
					arrPlain,
					sizeof(arrPlain),
					NULL,
					0u,
					NULL,
					2u,
					&Packet
				) == XSSH_OK) &&
				(Packet.Session.Kind == XSSH_SESSION_PACKET_KEXINIT) &&
				(Packet.Transport.Payload.Data ==
				 Packet.Session.Payload.Data) &&
				(xrtSshSessionTcpReadCommit(
					&pEndpoint->Session,
					2u,
					&Decision
				) == XSSH_OK), "ssh TCP session packet read failed");
			(void)xrtAtomic32FetchAdd(
				&pEndpoint->Context->Packets,
				1u,
				XMEMORY_RELEASE
			);
		}
	}
}



/* Stream 开放后初始化所属 Worker 的池化会话并发送本端版本。 */
static void testSshSessionTcpOpen(xnetstream* pStream, ptr pData)
{
	testsshsessiontcpendpoint* pEndpoint =
		(testsshsessiontcpendpoint*)pData;
	xsshsessiontcpconfig Config;
	xsshrekeydecision Decision;
	xstrview Version = pEndpoint->Client ?
		XRT_STR_LITERAL("SSH-2.0-xssh_session_client") :
		XRT_STR_LITERAL("SSH-2.0-xssh_session_server");

	pEndpoint->Stream = pStream;
	testRequire(xrtSshSessionTcpConfigInit(
		&Config,
		pEndpoint->Client ? XSSH_ROLE_CLIENT : XSSH_ROLE_SERVER
	) && xrtSshSessionTcpInit(
		&pEndpoint->Session,
		xrtNetWorkerBufPool(xrtNetStreamWorker(pStream)),
		&Config,
		0u
	) && (xrtSshSessionTcpIdentificationWritePrepare(
		&pEndpoint->Session,
		Version
	) == XSSH_OK) && (xrtSshSessionTcpWriteSize(
		&pEndpoint->Session
	) == (Version.Size + 2u)) && (xrtSshSessionTcpWriteAbort(
		&pEndpoint->Session
	) == XSSH_OK) && (xrtSshSessionTcpIdentificationWritePrepare(
		&pEndpoint->Session,
		Version
	) == XSSH_OK) && (xrtSshSessionTcpWriteSubmit(
		&pEndpoint->Session,
		pStream,
		0u,
		&Decision
	) == XNET_RESULT_OK) && (Decision == XSSH_REKEY_NONE),
		"ssh TCP session open transaction failed");
	pEndpoint->Initialized = true;
	(void)xrtAtomic32FetchAdd(
		&pEndpoint->Context->Opened,
		1u,
		XMEMORY_RELEASE
	);
}



/* 会话必须在所属 Worker 上释放池化动态块。 */
static void testSshSessionTcpClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	testsshsessiontcpendpoint* pEndpoint =
		(testsshsessiontcpendpoint*)pData;

	(void)pStream;
	testRequire((Result == XNET_RESULT_OK) && (pError == NULL),
		"ssh TCP session loopback closed with error");
	if ( pEndpoint->Initialized ) {
		xrtSshSessionTcpClear(&pEndpoint->Session);
		pEndpoint->Initialized = false;
	}
	(void)xrtAtomic32FetchAdd(
		&pEndpoint->Context->Closed,
		1u,
		XMEMORY_RELEASE
	);
}



/* Listener 把 accepted Stream 绑定到 server endpoint。 */
static bool testSshSessionTcpAccept(
	xnetlistener* pListener,
	xnetstream* pStream,
	ptr pData
)
{
	testsshsessiontcpcontext* pContext =
		(testsshsessiontcpcontext*)pData;
	static const xnetstreamevents Events = {
		testSshSessionTcpOpen,
		testSshSessionTcpRead,
		NULL,
		NULL,
		NULL,
		NULL,
		testSshSessionTcpClose
	};

	(void)pListener;
	testRequire(xrtNetStreamSetEvents(
		pStream,
		&Events,
		&pContext->Server
	), "ssh TCP session accepted event setup failed");
	pContext->Server.Stream = pStream;
	(void)xrtAtomic32FetchAdd(
		&pContext->Accepted,
		1u,
		XMEMORY_RELEASE
	);
	return true;
}



/* Listener 关闭只发布一次完成事件。 */
static void testSshSessionTcpListenerClose(
	xnetlistener* pListener,
	ptr pData
)
{
	testsshsessiontcpcontext* pContext =
		(testsshsessiontcpcontext*)pData;

	(void)pListener;
	(void)xrtAtomic32FetchAdd(
		&pContext->ListenerClosed,
		1u,
		XMEMORY_RELEASE
	);
}



/* 验证无网络对象的配置、访问器和终止性读回滚。 */
static void testSshSessionTcpState(void)
{
	static const unsigned char arrVersion[] =
		"SSH-2.0-peer\r\n";
	static const unsigned char arrInvalidVersion[] =
		"SSH-3.0-invalid\r\n";
	xsshsessiontcpconfig Config;
	xsshsessiontcp Session;
	xstrview Version;
	xnetbuf Input;

	testRequire(xrtSshSessionTcpConfigInit(
		&Config,
		XSSH_ROLE_CLIENT
	) && xrtSshSessionTcpInit(
		&Session,
		NULL,
		&Config,
		0u
	) && (sizeof(Session) < 3072u) &&
		(xrtSshSessionTcpTransport(&Session) == &Session.Transport) &&
		(xrtSshSessionTcpCore(&Session) == &Session.Session) &&
		(xrtSshSessionTcpPhase(&Session) == XSSH_SESSION_IDENTIFICATION) &&
		(xrtSshSessionTcpAction(&Session) ==
		 XSSH_SESSION_ACTION_WRITE_IDENTIFICATION) &&
		xrtNetBufInit(&Input, NULL) && xrtNetBufAppend(
		&Input,
		arrVersion,
		sizeof(arrVersion) - 1u
	) && (xrtSshSessionTcpIdentificationReadPrepare(
		&Session,
		&Input,
		&Version
	) == XSSH_OK) && (xrtSshSessionTcpAction(&Session) ==
		 XSSH_SESSION_ACTION_READ_PENDING) &&
		(xrtSshSessionTcpReadAbort(
		&Session
	) == XSSH_OK) &&
		(xrtSshSessionTcpPhase(&Session) == XSSH_SESSION_FAILED) &&
		xrtNetBufEmpty(&Input), "ssh TCP session state contract failed");
	xrtNetBufClear(&Input);
	xrtSshSessionTcpClear(&Session);
	testRequire(xrtSshSessionTcpInit(
		&Session,
		NULL,
		&Config,
		0u
	) && xrtNetBufInit(&Input, NULL) && xrtNetBufAppend(
		&Input,
		arrInvalidVersion,
		sizeof(arrInvalidVersion) - 1u
	) && (xrtSshSessionTcpIdentificationReadPrepare(
		&Session,
		&Input,
		&Version
	) == XSSH_ERROR_UNSUPPORTED) &&
		(xrtSshSessionTcpPhase(&Session) == XSSH_SESSION_FAILED),
		"ssh TCP session fatal transport state did not propagate");
	xrtNetBufClear(&Input);
	xrtSshSessionTcpClear(&Session);
}



/* 验证真实 select fallback 上的双端 identification 与 KEXINIT 提交。 */
static void testSshSessionTcpLoopback(void)
{
	static const xnetstreamevents ClientEvents = {
		testSshSessionTcpOpen,
		testSshSessionTcpRead,
		NULL,
		NULL,
		NULL,
		NULL,
		testSshSessionTcpClose
	};
	static const xnetlistenerevents ListenerEvents = {
		testSshSessionTcpAccept,
		NULL,
		testSshSessionTcpListenerClose
	};
	testsshsessiontcpcontext Context;
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetstreamconfig StreamConfig;
	xnetengine* pEngine;
	xnetlistener* pListener;
	xnetaddr Address;

	memset(&Context, 0, sizeof(Context));
	Context.Client.Context = &Context;
	Context.Client.Client = true;
	Context.Server.Context = &Context;
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 1u;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"ssh TCP session select engine start failed");
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0u
	), "ssh TCP session listener address failed");
	ListenConfig.Stream.ReadSize = 32u;
	ListenConfig.Stream.ReadLimit = 4096u;
	pListener = xrtNetListen(
		pEngine,
		&ListenConfig,
		&ListenerEvents,
		&ClientEvents,
		&Context
	);
	testRequire((pListener != NULL) &&
		xrtNetListenerLocal(pListener, &Address),
		"ssh TCP session listener failed");
	xrtNetStreamConfigInit(&StreamConfig);
	StreamConfig.ReadSize = 32u;
	StreamConfig.ReadLimit = 4096u;
	StreamConfig.WriteHighWater = 2048u;
	StreamConfig.WriteLowWater = 1024u;
	StreamConfig.WriteLimit = 4096u;
	Context.Client.Stream = xrtNetStreamConnect(
		pEngine,
		&Address,
		1u,
		&StreamConfig,
		&ClientEvents,
		&Context.Client
	);
	testRequire(Context.Client.Stream != NULL,
		"ssh TCP session connect failed");
	testSshSessionTcpWait(&Context.Accepted, 1u,
		"ssh TCP session accept missing");
	testSshSessionTcpWait(&Context.Opened, 2u,
		"ssh TCP session open missing");
	testSshSessionTcpWait(&Context.Versions, 2u,
		"ssh TCP session versions incomplete");
	testSshSessionTcpWait(&Context.Packets, 1u,
		"ssh TCP session KEXINIT missing");
	testRequire(xrtNetStreamClose(Context.Client.Stream) &&
		xrtNetStreamClose(Context.Server.Stream),
		"ssh TCP session stream close failed");
	testSshSessionTcpWait(&Context.Closed, 2u,
		"ssh TCP session close missing");
	testRequire(xrtNetListenerClose(pListener),
		"ssh TCP session listener close failed");
	testSshSessionTcpWait(&Context.ListenerClosed, 1u,
		"ssh TCP session listener callback missing");
	xrtNetStreamDestroy(Context.Client.Stream);
	xrtNetStreamDestroy(Context.Server.Stream);
	xrtNetListenerDestroy(pListener);
	testRequire(xrtNetEngineDestroy(pEngine),
		"ssh TCP session engine destroy failed");
}



/* 运行组合状态与真实 TCP 所有权回归。 */
int main(void)
{
	testSshSessionTcpState();
	testSshSessionTcpLoopback();
	return 0;
}
