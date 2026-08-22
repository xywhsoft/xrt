#include "../test.h"



typedef struct testsshtransporttcpcontext testsshtransporttcpcontext;



typedef struct testsshtransporttcpendpoint {
	testsshtransporttcpcontext* Context;
	xnetstream* Stream;
	bool Client;
} testsshtransporttcpendpoint;



struct testsshtransporttcpcontext {
	testsshtransporttcpendpoint Client;
	testsshtransporttcpendpoint Server;
	xsshtransporttcp Transport;
	xatomic32 Accepted;
	xatomic32 ClientOpen;
	xatomic32 ServerOpen;
	xatomic32 ClientReady;
	xatomic32 ServerBytes;
	xatomic32 ClientClose;
	xatomic32 ServerClose;
	xatomic32 ListenerClose;
	xatomic32 TransportCleared;
	uint32 ExpectedBytes;
};



/* 测试使用确定性 padding，便于检查 AGAIN 前后 sequence 不变。 */
static bool testSshTransportTcpPadding(
	void* pOutput,
	size_t iSize,
	ptr pUserData
)
{
	uint8 iStart = *(const uint8*)pUserData;
	bytes pBytes = (bytes)pOutput;

	for ( size_t i = 0u; i < iSize; ++i ) {
		pBytes[i] = (uint8)(iStart + (uint8)i);
	}
	return true;
}



/* 构建正常或通过 language name 扩大的有效 client KEXINIT。 */
static bool testSshTransportTcpKexInit(
	void* pOutput,
	size_t iCapacity,
	bool bLarge,
	xbytesview* pPayload
)
{
	unsigned char arrCookie[XSSH_KEX_COOKIE_SIZE] = { 0u };
	char arrLanguage[700];
	xsshkexinitconfig Config;
	xsshwriter Writer;

	memset(arrLanguage, 'a', sizeof(arrLanguage));
	if ( !xrtSshKexInitConfigInit(
		&Config,
		XSSH_ROLE_CLIENT,
		true
	) || !xrtSshWriterInit(&Writer, pOutput, iCapacity) ) {
		return false;
	}
	if ( bLarge ) {
		Config.LanguagesClientToServer = (xstrview){
			arrLanguage,
			sizeof(arrLanguage)
		};
	}
	if ( xrtSshKexInitWrite(
		&Writer,
		(xbytesview){ arrCookie, sizeof(arrCookie) },
		&Config
	) != XSSH_OK ) {
		return false;
	}
	pPayload->Data = (const unsigned char*)pOutput;
	pPayload->Size = Writer.Size;
	return true;
}



/* 等待异步 loopback 状态达到指定值。 */
static void testSshTransportTcpWait(
	xatomic32* pValue,
	uint32 iExpected,
	cstr sMessage
)
{
	xdeadline Deadline = xrtDeadlineAfter(5000000u);

	while ( xrtAtomic32Load(pValue, XMEMORY_ACQUIRE) < iExpected ) {
		testRequire(!xrtDeadlineExpired(Deadline), sMessage);
		xrtThreadYield();
	}
}



/* 验证分块 identification 和 packet 只消费当前完整输入。 */
static void testSshTransportTcpBufferRead(void)
{
	unsigned char arrPayload[1024];
	unsigned char arrWire[2048];
	uint8 iPadding = 0x30u;
	xsshtransporttcpconfig Config;
	xsshtransporttcp Transport;
	xsshpacketcodec Peer;
	xsshpacketneed Need;
	xsshpacketview Packet;
	xsshrekeydecision Decision;
	xsshwriter Writer;
	xstrview Banner;
	xbytesview Payload;
	xnetbuf Input;
	size_t iFirst;

	testRequire(xrtSshTransportTcpConfigInit(
		&Config,
		XSSH_ROLE_CLIENT
	) && xrtSshTransportTcpInit(
		&Transport,
		NULL,
		&Config,
		0u
	) && xrtNetBufInit(&Input, NULL),
		"ssh TCP buffer-read setup failed");
	testRequire(xrtSshTransportCoreIdentificationCommit(
		xrtSshTransportTcpCore(&Transport),
		XSSH_TRANSPORT_LOCAL
	) == XSSH_OK, "ssh TCP local identification setup failed");
	testRequire(xrtNetBufAppend(
		&Input,
		"notice\r\nSSH-2.",
		sizeof("notice\r\nSSH-2.") - 1u
	) &&
		(xrtSshTransportTcpIdentificationReadPrepare(
			&Transport,
			&Input,
			&Banner
		) == XSSH_NEED_MORE) && xrtNetBufAppend(
			&Input,
			"0-peer\r\ntail",
			sizeof("0-peer\r\ntail") - 1u
		) && (xrtSshTransportTcpIdentificationReadPrepare(
			&Transport,
			&Input,
			&Banner
		) == XSSH_OK) && testSshTextEqual(
			Banner,
			XRT_STR_LITERAL("SSH-2.0-peer")
		) && (xrtSshTransportTcpReadCommit(
			&Transport,
			0u,
			&Decision
		) == XSSH_OK) && (Decision == XSSH_REKEY_NONE) &&
		(xrtNetBufSize(&Input) == 4u),
		"ssh TCP fragmented identification failed");
	xrtNetBufClear(&Input);
	testRequire(testSshTransportTcpKexInit(
		arrPayload,
		sizeof(arrPayload),
		false,
		&Payload
	) && (xrtSshPacketCodecInit(&Peer, 0u) == XSSH_OK) &&
		xrtSshWriterInit(&Writer, arrWire, sizeof(arrWire)) &&
		(xrtSshPacketCodecWriteWithPadding(
			&Peer,
			&Writer,
			Payload,
			testSshTransportTcpPadding,
			&iPadding
		) == XSSH_OK) && xrtNetBufInit(&Input, NULL),
		"ssh TCP fragmented packet setup failed");
	iFirst = 2u;
	testRequire(xrtNetBufAppend(&Input, arrWire, iFirst) &&
		(xrtSshTransportTcpReadInspect(
			&Transport,
			&Input,
			&Need
		) == XSSH_NEED_MORE) && xrtNetBufAppend(
			&Input,
			arrWire + iFirst,
			5u
		) && xrtNetBufAppend(
			&Input,
			arrWire + iFirst + 5u,
			Writer.Size - iFirst - 5u
		) && xrtNetBufAppend(&Input, "tail", 4u) &&
		(xrtSshTransportTcpReadInspect(
			&Transport,
			&Input,
			&Need
		) == XSSH_OK) && (Need.WireSize == Writer.Size) &&
		(Need.PlainSize == 0u) &&
		(xrtSshTransportTcpReadPrepare(
			&Transport,
			&Input,
			&Packet,
			NULL,
			0u,
			1u
		) == XSSH_OK) && testSshBytesEqual(Packet.Payload, Payload) &&
		(xrtSshTransportTcpReadCommit(
			&Transport,
			1u,
			&Decision
		) == XSSH_OK) && (xrtNetBufSize(&Input) == 4u),
		"ssh TCP fragmented packet read failed");
	xrtNetBufClear(&Input);
	xrtSshPacketCodecClear(&Peer);
	xrtSshTransportTcpClear(&Transport);
}



/* 验证非破坏 Inspect 与会话级 Prepare 的致命错误边界。 */
static void testSshTransportTcpInvalid(void)
{
	static const unsigned char arrBadPacket[8] = {
		0u, 0u, 0u, 5u, 4u, XSSH_MSG_KEXINIT, 0u, 0u
	};
	static const unsigned char arrBadBanner[] = {
		'b', 'a', 'd', 0u, '\r', '\n',
		'S', 'S', 'H', '-', '2', '.', '0', '-', 'p', '\r', '\n'
	};
	xsshtransporttcpconfig Config;
	xsshtransporttcp Transport;
	xsshpacketneed Need;
	xsshpacketview Packet;
	xstrview Banner;
	xnetbuf Input;

	testRequire(xrtSshTransportTcpConfigInit(
		&Config,
		XSSH_ROLE_CLIENT
	) && xrtSshTransportTcpInit(
		&Transport,
		NULL,
		&Config,
		0u
	) && xrtNetBufInit(&Input, NULL) && xrtNetBufAppend(
		&Input,
		arrBadBanner,
		sizeof(arrBadBanner)
	), "ssh TCP invalid banner setup failed");
	testRequire((xrtSshTransportTcpIdentificationReadPrepare(
		&Transport,
		&Input,
		&Banner
	) == XSSH_ERROR_PROTOCOL) &&
		(Transport.Core.State.Phase == XSSH_TRANSPORT_CLOSED),
		"ssh TCP invalid banner did not close transport");
	xrtNetBufClear(&Input);
	xrtSshTransportTcpClear(&Transport);
	testRequire(xrtSshTransportTcpInit(
		&Transport,
		NULL,
		&Config,
		0u
	) && (xrtSshTransportCoreIdentificationCommit(
		&Transport.Core,
		XSSH_TRANSPORT_LOCAL
	) == XSSH_OK) && (xrtSshTransportCoreIdentificationCommit(
		&Transport.Core,
		XSSH_TRANSPORT_PEER
	) == XSSH_OK) && xrtNetBufInit(&Input, NULL) && xrtNetBufAppend(
		&Input,
		arrBadPacket,
		sizeof(arrBadPacket)
	), "ssh TCP invalid packet setup failed");
	testRequire((xrtSshTransportTcpReadInspect(
		&Transport,
		&Input,
		&Need
	) == XSSH_ERROR_PROTOCOL) &&
		(Transport.Core.State.Phase == XSSH_TRANSPORT_KEY_EXCHANGE) &&
		(xrtSshTransportTcpReadPrepare(
			&Transport,
			&Input,
			&Packet,
			NULL,
			0u,
			0u
		) == XSSH_ERROR_PROTOCOL) &&
		(Transport.Core.State.Phase == XSSH_TRANSPORT_CLOSED) &&
		(xrtSshTransportTcpReadInspect(
			&Transport,
			&Input,
			(xsshpacketneed*)&Transport.Output
		) == XSSH_ERROR_ARGUMENT),
		"ssh TCP invalid packet boundary failed");
	xrtNetBufClear(&Input);
	xrtSshTransportTcpClear(&Transport);
}



/* Server 只统计并消费线路字节，避免测试引入额外协议状态。 */
static void testSshTransportTcpRead(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	ptr pData
)
{
	testsshtransporttcpendpoint* pEndpoint =
		(testsshtransporttcpendpoint*)pData;
	size_t iSize = xrtNetBufSize(pBuffer);

	(void)pStream;
	testRequire(!pEndpoint->Client,
		"ssh TCP client received unexpected data");
	testRequire(xrtNetBufConsume(pBuffer, iSize) == iSize,
		"ssh TCP server could not consume input");
	(void)xrtAtomic32FetchAdd(
		&pEndpoint->Context->ServerBytes,
		(uint32)iSize,
		XMEMORY_RELEASE
	);
}



/* Client Open 回调验证真实 TCP 队列的接管、AGAIN 与重试边界。 */
static void testSshTransportTcpOpen(xnetstream* pStream, ptr pData)
{
	testsshtransporttcpendpoint* pEndpoint =
		(testsshtransporttcpendpoint*)pData;
	testsshtransporttcpcontext* pContext = pEndpoint->Context;

	pEndpoint->Stream = pStream;
	if ( !pEndpoint->Client ) {
		(void)xrtAtomic32FetchAdd(
			&pContext->ServerOpen,
			1,
			XMEMORY_RELEASE
		);
		return;
	}
	{
		unsigned char arrLarge[2048];
		unsigned char arrNormal[1024];
		uint8 iPadding = 0x50u;
		xsshtransporttcpconfig Config;
		xsshrekeydecision Decision;
		xbytesview Large;
		xbytesview Normal;
		size_t iBannerSize;
		size_t iPacketSize;

		testRequire(xrtSshTransportTcpConfigInit(
			&Config,
			XSSH_ROLE_CLIENT
		) && xrtSshTransportTcpInit(
			&pContext->Transport,
			xrtNetWorkerBufPool(xrtNetStreamWorker(pStream)),
			&Config,
			0u
		), "ssh TCP worker transport setup failed");
		testRequire(xrtSshTransportTcpIdentificationPrepare(
			&pContext->Transport,
			XRT_STR_LITERAL("SSH-2.0-xssh_tcp")
		) == XSSH_OK, "ssh TCP identification prepare failed");
		iBannerSize = xrtSshTransportTcpWriteSize(
			&pContext->Transport
		);
		testRequire((xrtSshTransportTcpWriteSubmit(
			&pContext->Transport,
			pStream,
			0u,
			&Decision
		) == XNET_RESULT_OK) && (Decision == XSSH_REKEY_NONE) &&
			xrtNetBufEmpty(&pContext->Transport.Output) &&
			(xrtSshTransportCoreIdentificationCommit(
				xrtSshTransportTcpCore(&pContext->Transport),
				XSSH_TRANSPORT_PEER
			) == XSSH_OK),
			"ssh TCP identification submit failed");
		testRequire(testSshTransportTcpKexInit(
			arrLarge,
			sizeof(arrLarge),
			true,
			&Large
		) && (xrtSshTransportTcpWritePrepareWithPadding(
			&pContext->Transport,
			Large,
			testSshTransportTcpPadding,
			&iPadding,
			1u
		) == XSSH_OK) &&
		(xrtSshTransportTcpWriteSize(&pContext->Transport) > 512u) &&
		(xrtSshTransportTcpWriteSubmit(
			&pContext->Transport,
			pStream,
			1u,
			&Decision
		) == XNET_RESULT_AGAIN) &&
		(pContext->Transport.Core.Codec.WriteSequence == 0u) &&
		(pContext->Transport.WritePending ==
		 XSSH_TRANSPORT_TCP_PENDING_PACKET) &&
		(xrtSshTransportTcpWriteAbort(
			&pContext->Transport
		) == XSSH_OK) &&
		(pContext->Transport.Core.Codec.WriteSequence == 0u),
			"ssh TCP hard backpressure changed packet state");
		testRequire(testSshTransportTcpKexInit(
			arrNormal,
			sizeof(arrNormal),
			false,
			&Normal
		) && (xrtSshTransportTcpWritePrepareWithPadding(
			&pContext->Transport,
			Normal,
			testSshTransportTcpPadding,
			&iPadding,
			2u
		) == XSSH_OK), "ssh TCP normal packet prepare failed");
		iPacketSize = xrtSshTransportTcpWriteSize(&pContext->Transport);
		testRequire((iPacketSize < 512u) &&
			(xrtSshTransportTcpWriteSubmit(
				&pContext->Transport,
				pStream,
				2u,
				&Decision
			) == XNET_RESULT_OK) &&
			(pContext->Transport.Core.Codec.WriteSequence == 1u) &&
			xrtNetBufEmpty(&pContext->Transport.Output),
			"ssh TCP normal packet submit failed");
		pContext->ExpectedBytes = (uint32)(iBannerSize + iPacketSize);
	}
	(void)xrtAtomic32FetchAdd(
		&pContext->ClientOpen,
		1,
		XMEMORY_RELEASE
	);
	(void)xrtAtomic32FetchAdd(
		&pContext->ClientReady,
		1,
		XMEMORY_RELEASE
	);
}



/* Stream 关闭回调在所属 Worker 清理绑定该池的 transport。 */
static void testSshTransportTcpClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	testsshtransporttcpendpoint* pEndpoint =
		(testsshtransporttcpendpoint*)pData;
	testsshtransporttcpcontext* pContext = pEndpoint->Context;

	(void)pStream;
	testRequire((Result == XNET_RESULT_OK) && (pError == NULL),
		"ssh TCP loopback closed with error");
	if ( pEndpoint->Client ) {
		xrtSshTransportTcpClear(&pContext->Transport);
		(void)xrtAtomic32FetchAdd(
			&pContext->TransportCleared,
			1,
			XMEMORY_RELEASE
		);
		(void)xrtAtomic32FetchAdd(
			&pContext->ClientClose,
			1,
			XMEMORY_RELEASE
		);
	} else {
		(void)xrtAtomic32FetchAdd(
			&pContext->ServerClose,
			1,
			XMEMORY_RELEASE
		);
	}
}



/* Listener 接管 accepted Stream，并换成 server endpoint 数据。 */
static bool testSshTransportTcpAccept(
	xnetlistener* pListener,
	xnetstream* pStream,
	ptr pData
)
{
	testsshtransporttcpcontext* pContext =
		(testsshtransporttcpcontext*)pData;
	static const xnetstreamevents Events = {
		testSshTransportTcpOpen,
		testSshTransportTcpRead,
		NULL,
		NULL,
		NULL,
		NULL,
		testSshTransportTcpClose
	};

	(void)pListener;
	testRequire(xrtNetStreamSetEvents(
		pStream,
		&Events,
		&pContext->Server
	), "ssh TCP accepted stream event setup failed");
	pContext->Server.Stream = pStream;
	(void)xrtAtomic32FetchAdd(
		&pContext->Accepted,
		1,
		XMEMORY_RELEASE
	);
	return true;
}



/* Listener 正常关闭只发布一次完成通知。 */
static void testSshTransportTcpListenerClose(
	xnetlistener* pListener,
	ptr pData
)
{
	testsshtransporttcpcontext* pContext =
		(testsshtransporttcpcontext*)pData;

	(void)pListener;
	(void)xrtAtomic32FetchAdd(
		&pContext->ListenerClose,
		1,
		XMEMORY_RELEASE
	);
}



/* 验证 select fallback 上的真实缓冲池所有权和 TCP 受理边界。 */
static void testSshTransportTcpLoopback(void)
{
	static const xnetstreamevents ClientEvents = {
		testSshTransportTcpOpen,
		testSshTransportTcpRead,
		NULL,
		NULL,
		NULL,
		NULL,
		testSshTransportTcpClose
	};
	static const xnetlistenerevents ListenerEvents = {
		testSshTransportTcpAccept,
		NULL,
		testSshTransportTcpListenerClose
	};
	testsshtransporttcpcontext Context;
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
		"ssh TCP select engine start failed");
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0u
	), "ssh TCP listener address failed");
	ListenConfig.Stream.ReadSize = 64u;
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
		"ssh TCP loopback listener failed");
	xrtNetStreamConfigInit(&StreamConfig);
	StreamConfig.ReadSize = 64u;
	StreamConfig.ReadLimit = 4096u;
	StreamConfig.WriteHighWater = 256u;
	StreamConfig.WriteLowWater = 128u;
	StreamConfig.WriteLimit = 512u;
	Context.Client.Stream = xrtNetStreamConnect(
		pEngine,
		&Address,
		1u,
		&StreamConfig,
		&ClientEvents,
		&Context.Client
	);
	testRequire(Context.Client.Stream != NULL,
		"ssh TCP loopback connect failed");
	testSshTransportTcpWait(&Context.Accepted, 1u,
		"ssh TCP accept callback missing");
	testSshTransportTcpWait(&Context.ClientReady, 1u,
		"ssh TCP client transport did not finish");
	testSshTransportTcpWait(&Context.ServerOpen, 1u,
		"ssh TCP server open callback missing");
	testSshTransportTcpWait(
		&Context.ServerBytes,
		Context.ExpectedBytes,
		"ssh TCP server did not receive accepted outputs"
	);
	testRequire(xrtNetStreamClose(Context.Client.Stream) &&
		xrtNetStreamClose(Context.Server.Stream),
		"ssh TCP stream close failed");
	testSshTransportTcpWait(&Context.ClientClose, 1u,
		"ssh TCP client close callback missing");
	testSshTransportTcpWait(&Context.ServerClose, 1u,
		"ssh TCP server close callback missing");
	testSshTransportTcpWait(&Context.TransportCleared, 1u,
		"ssh TCP transport was not cleared on Worker");
	testRequire(xrtNetListenerClose(pListener),
		"ssh TCP listener close failed");
	testSshTransportTcpWait(&Context.ListenerClose, 1u,
		"ssh TCP listener close callback missing");
	xrtNetStreamDestroy(Context.Client.Stream);
	xrtNetStreamDestroy(Context.Server.Stream);
	xrtNetListenerDestroy(pListener);
	testRequire(xrtNetEngineDestroy(pEngine),
		"ssh TCP select engine destroy failed");
}



/* 运行动态缓冲与真实 XRT TCP 所有权边界测试。 */
int main(void)
{
	testSshTransportTcpBufferRead();
	testSshTransportTcpInvalid();
	testSshTransportTcpLoopback();
	return 0;
}
