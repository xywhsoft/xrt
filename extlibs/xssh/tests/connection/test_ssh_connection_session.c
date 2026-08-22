#include "../test.h"



#define TEST_SSH_CONNECTION_KEX_INIT 30u
#define TEST_SSH_CONNECTION_KEX_REPLY 31u
#define TEST_SSH_CONNECTION_CHANNELS 4u
#define TEST_SSH_CONNECTION_TOKENS 4u



/* 测试注册表只决定 channel 存储，不参与协议状态推进。 */
typedef struct testsshconnectionregistry {
	xsshchannelcore Channels[TEST_SSH_CONNECTION_CHANNELS];
	xsshreplyqueue Replies[TEST_SSH_CONNECTION_CHANNELS];
	uint64 Tokens[TEST_SSH_CONNECTION_CHANNELS][TEST_SSH_CONNECTION_TOKENS];
	bool Used[TEST_SSH_CONNECTION_CHANNELS];
} testsshconnectionregistry;



/* 线路 packet 使用确定性 padding，保证双端测试结果可重复。 */
static bool testSshConnectionPadding(
	void* pOutput,
	size_t iSize,
	ptr pUserData
)
{
	uint8 iValue = *(const uint8*)pUserData;
	bytes pBytes = (bytes)pOutput;
	size_t i;

	for ( i = 0u; i < iSize; ++i ) {
		pBytes[i] = (uint8)(iValue + (uint8)i);
	}
	return true;
}



/* 构造默认算法视图，仅用于把 transport 推进到首轮 KEX 完成。 */
static void testSshConnectionKexViews(
	xsshrole Role,
	xsshkexinit* pLocal,
	xsshkexinit* pPeer,
	xsshkexnegotiation* pNegotiation
)
{
	xsshkexinit Client;
	xsshkexinit Server;

	memset(&Client, 0, sizeof(Client));
	memset(&Server, 0, sizeof(Server));
	Client.KexAlgorithms = XRT_STR_LITERAL(XSSH_KEX_DEFAULT);
	Server.KexAlgorithms = XRT_STR_LITERAL(XSSH_KEX_DEFAULT);
	Client.ServerHostKeyAlgorithms = XRT_STR_LITERAL(XSSH_HOSTKEY_DEFAULT);
	Server.ServerHostKeyAlgorithms = XRT_STR_LITERAL(XSSH_HOSTKEY_DEFAULT);
	Client.EncryptionClientToServer = XRT_STR_LITERAL(XSSH_CIPHER_DEFAULT);
	Client.EncryptionServerToClient = XRT_STR_LITERAL(XSSH_CIPHER_DEFAULT);
	Server.EncryptionClientToServer = XRT_STR_LITERAL(XSSH_CIPHER_DEFAULT);
	Server.EncryptionServerToClient = XRT_STR_LITERAL(XSSH_CIPHER_DEFAULT);
	Client.MacClientToServer = XRT_STR_LITERAL(XSSH_MAC_DEFAULT);
	Client.MacServerToClient = XRT_STR_LITERAL(XSSH_MAC_DEFAULT);
	Server.MacClientToServer = XRT_STR_LITERAL(XSSH_MAC_DEFAULT);
	Server.MacServerToClient = XRT_STR_LITERAL(XSSH_MAC_DEFAULT);
	Client.CompressionClientToServer =
		XRT_STR_LITERAL(XSSH_COMPRESSION_DEFAULT);
	Client.CompressionServerToClient =
		XRT_STR_LITERAL(XSSH_COMPRESSION_DEFAULT);
	Server.CompressionClientToServer =
		XRT_STR_LITERAL(XSSH_COMPRESSION_DEFAULT);
	Server.CompressionServerToClient =
		XRT_STR_LITERAL(XSSH_COMPRESSION_DEFAULT);
	testRequire(xrtSshKexNegotiate(
		&Client,
		&Server,
		pNegotiation
	) == XSSH_OK, "ssh connection session negotiation failed");
	if ( Role == XSSH_ROLE_CLIENT ) {
		*pLocal = Client;
		*pPeer = Server;
	} else {
		*pLocal = Server;
		*pPeer = Client;
	}
}



/* 只用公开 API 建立已认证、仍使用 plain codec 的 transport。 */
static void testSshConnectionCoreOpen(
	xsshtransportcore* pCore,
	xsshrole Role
)
{
	xsshtransportkexrules Rules;
	xsshkexinit Local;
	xsshkexinit Peer;
	xsshkexnegotiation Negotiation;
	uint32 iActions;
	uint8 iLocalMessage = Role == XSSH_ROLE_CLIENT ?
		TEST_SSH_CONNECTION_KEX_INIT : TEST_SSH_CONNECTION_KEX_REPLY;
	uint8 iPeerMessage = Role == XSSH_ROLE_CLIENT ?
		TEST_SSH_CONNECTION_KEX_REPLY : TEST_SSH_CONNECTION_KEX_INIT;
	xsshtransportdirection AuthDirection = Role == XSSH_ROLE_CLIENT ?
		XSSH_TRANSPORT_PEER : XSSH_TRANSPORT_LOCAL;

	testSshConnectionKexViews(Role, &Local, &Peer, &Negotiation);
	testRequire(xrtSshTransportCoreInit(
		pCore,
		Role,
		0u,
		NULL,
		0u
	) && (xrtSshTransportCoreIdentificationCommit(
		pCore,
		XSSH_TRANSPORT_LOCAL
	) == XSSH_OK) && (xrtSshTransportCoreIdentificationCommit(
		pCore,
		XSSH_TRANSPORT_PEER
	) == XSSH_OK) && (xrtSshTransportKexInitCommit(
		&pCore->State,
		XSSH_TRANSPORT_LOCAL,
		false
	) == XSSH_OK) && (xrtSshTransportKexInitCommit(
		&pCore->State,
		XSSH_TRANSPORT_PEER,
		false
	) == XSSH_OK) && xrtSshTransportKexRulesInit(&Rules) &&
		xrtSshTransportKexRuleSet(
			&Rules,
			XSSH_TRANSPORT_LOCAL,
			iLocalMessage,
			1u
		) && xrtSshTransportKexRuleSet(
			&Rules,
			XSSH_TRANSPORT_PEER,
			iPeerMessage,
			1u
		) && (xrtSshTransportCoreKexConfigure(
			pCore,
			&Local,
			&Peer,
			&Negotiation,
			&Rules
		) == XSSH_OK) && (xrtSshTransportMessageCommit(
		&pCore->State,
		XSSH_TRANSPORT_LOCAL,
		iLocalMessage
	) == XSSH_OK) && (xrtSshTransportMessageCommit(
		&pCore->State,
		XSSH_TRANSPORT_PEER,
		iPeerMessage
	) == XSSH_OK) && (xrtSshTransportNewKeysCommit(
		&pCore->State,
		XSSH_TRANSPORT_LOCAL,
		&iActions
	) == XSSH_OK) && (xrtSshTransportNewKeysCommit(
		&pCore->State,
		XSSH_TRANSPORT_PEER,
		&iActions
	) == XSSH_OK) && (xrtSshTransportAuthSuccessCommit(
		&pCore->State,
		AuthDirection
	) == XSSH_OK) && xrtSshTransportCoreKexComplete(pCore),
		"ssh connection session transport setup failed");
}



/* 初始化调用方 channel 注册表及每个 channel 的独立回复 FIFO。 */
static void testSshConnectionRegistryInit(
	testsshconnectionregistry* pRegistry
)
{
	size_t i;

	memset(pRegistry, 0, sizeof(*pRegistry));
	for ( i = 0u; i < TEST_SSH_CONNECTION_CHANNELS; ++i ) {
		testRequire(xrtSshReplyQueueInit(
			&pRegistry->Replies[i],
			pRegistry->Tokens[i],
			TEST_SSH_CONNECTION_TOKENS
		), "ssh connection channel reply queue init failed");
	}
}



/* 将本地 recipient 映射到调用方选择的 channel 和回复 FIFO。 */
static bool testSshConnectionResolve(
	ptr pUserData,
	uint32 iLocal,
	xsshchannelcore** ppChannel,
	xsshreplyqueue** ppReplies
)
{
	testsshconnectionregistry* pRegistry =
		(testsshconnectionregistry*)pUserData;

	if ( (iLocal >= TEST_SSH_CONNECTION_CHANNELS) ||
		!pRegistry->Used[iLocal] ) {
		return false;
	}
	*ppChannel = &pRegistry->Channels[iLocal];
	*ppReplies = &pRegistry->Replies[iLocal];
	return true;
}



/* 建立角色互补的 transport、全局 FIFO 与 connection 会话。 */
static void testSshConnectionPairOpen(
	xsshtransportcore* pClientCore,
	xsshtransportcore* pServerCore,
	xsshconnectionsession* pClient,
	xsshconnectionsession* pServer,
	testsshconnectionregistry* pClientRegistry,
	testsshconnectionregistry* pServerRegistry,
	xsshreplyqueue* pClientGlobal,
	xsshreplyqueue* pServerGlobal,
	uint64* pClientTokens,
	uint64* pServerTokens
)
{
	testSshConnectionCoreOpen(pClientCore, XSSH_ROLE_CLIENT);
	testSshConnectionCoreOpen(pServerCore, XSSH_ROLE_SERVER);
	testSshConnectionRegistryInit(pClientRegistry);
	testSshConnectionRegistryInit(pServerRegistry);
	testRequire(xrtSshReplyQueueInit(
		pClientGlobal,
		pClientTokens,
		TEST_SSH_CONNECTION_TOKENS
	) && xrtSshReplyQueueInit(
		pServerGlobal,
		pServerTokens,
		TEST_SSH_CONNECTION_TOKENS
	) && xrtSshConnectionSessionInit(
		pClient,
		XSSH_ROLE_CLIENT,
		testSshConnectionResolve,
		pClientRegistry,
		pClientGlobal
	) && xrtSshConnectionSessionInit(
		pServer,
		XSSH_ROLE_SERVER,
		testSshConnectionResolve,
		pServerRegistry,
		pServerGlobal
	) && (xrtSshConnectionSessionBegin(
		pClient,
		pClientCore
	) == XSSH_OK) && (xrtSshConnectionSessionBegin(
		pServer,
		pServerCore
	) == XSSH_OK), "ssh connection session pair init failed");
}



/* 按 connection -> transport -> connection 顺序可靠发送 payload。 */
static size_t testSshConnectionSend(
	xsshconnectionsession* pSession,
	xsshtransportcore* pCore,
	xbytesview Payload,
	xsshchannelcore* pChannel,
	xsshreplyqueue* pReplies,
	uint64 iToken,
	void* pWire,
	size_t iWireCapacity,
	uint64 iNowMs
)
{
	xsshrekeydecision Decision;
	xsshwriter Writer;
	uint8 iPadding = 0x51u;

	testRequire((xrtSshConnectionSessionWritePrepare(
		pSession,
		pCore,
		Payload,
		pChannel,
		pReplies,
		iToken
	) == XSSH_OK) && xrtSshWriterInit(
		&Writer,
		pWire,
		iWireCapacity
	) && (xrtSshTransportCoreWritePrepareWithPadding(
		pCore,
		&Writer,
		Payload,
		testSshConnectionPadding,
		&iPadding,
		iNowMs
	) == XSSH_OK) && (xrtSshTransportCoreWriteCommit(
		pCore,
		iNowMs,
		&Decision
	) == XSSH_OK) && (Decision != XSSH_REKEY_REQUIRED) &&
		(xrtSshConnectionSessionWriteCommit(
			pSession,
			pCore
		) == XSSH_OK), "ssh connection session send failed");
	return Writer.Size;
}



/* 绕过 connection 构建线路包，用于测试未知和畸形消息的路由边界。 */
static size_t testSshConnectionTransportSend(
	xsshtransportcore* pCore,
	xbytesview Payload,
	void* pWire,
	size_t iWireCapacity,
	uint64 iNowMs
)
{
	xsshrekeydecision Decision;
	xsshwriter Writer;
	uint8 iPadding = 0x62u;

	testRequire(xrtSshWriterInit(&Writer, pWire, iWireCapacity) &&
		(xrtSshTransportCoreWritePrepareWithPadding(
			pCore,
			&Writer,
			Payload,
			testSshConnectionPadding,
			&iPadding,
			iNowMs
		) == XSSH_OK) && (xrtSshTransportCoreWriteCommit(
			pCore,
			iNowMs,
			&Decision
		) == XSSH_OK) && (Decision != XSSH_REKEY_REQUIRED),
		"ssh connection raw transport send failed");
	return Writer.Size;
}



/* 准备完整 peer packet，并保持借用视图到显式提交或放弃。 */
static xsshconnectionpacket testSshConnectionReceivePrepare(
	xsshconnectionsession* pSession,
	xsshtransportcore* pCore,
	const void* pWire,
	size_t iWireSize,
	void* pPlain,
	size_t iPlainCapacity,
	uint64 iNowMs
)
{
	xsshconnectionpacket ConnectionPacket;
	xsshpacketview Packet;
	xsshreader Reader;

	testRequire(xrtSshReaderInit(
		&Reader,
		(xbytesview){ (const unsigned char*)pWire, iWireSize }
	), "ssh connection transport reader init failed");
	testRequire(xrtSshTransportCoreReadPrepare(
		pCore,
		&Reader,
		&Packet,
		pPlain,
		iPlainCapacity,
		iNowMs
	) == XSSH_OK, "ssh connection transport receive prepare failed");
	testRequire(Reader.Position == iWireSize,
		"ssh connection transport packet was not fully consumed");
	testRequire(xrtSshConnectionSessionReadPrepare(
		pSession,
		pCore,
		Packet.Payload,
		&ConnectionPacket
	) == XSSH_OK, "ssh connection session receive prepare failed");
	return ConnectionPacket;
}



/* transport 提交后再原子提交 channel 与回复 FIFO。 */
static void testSshConnectionReceiveCommit(
	xsshconnectionsession* pSession,
	xsshtransportcore* pCore,
	uint64 iNowMs
)
{
	xsshrekeydecision Decision;

	testRequire((xrtSshTransportCoreReadCommit(
		pCore,
		iNowMs,
		&Decision
	) == XSSH_OK) && (Decision != XSSH_REKEY_REQUIRED) &&
		(xrtSshConnectionSessionReadCommit(
			pSession,
			pCore
		) == XSSH_OK), "ssh connection receive commit failed");
}



/* 将 writer 当前内容转换为不拥有存储的 payload 视图。 */
static xbytesview testSshConnectionPayload(
	const xsshwriter* pWriter,
	const void* pData
)
{
	return (xbytesview){ (const unsigned char*)pData, pWriter->Size };
}



/* 验证 channel、窗口、回复关联、global 与关闭的完整双端链路。 */
static void testSshConnectionSessionFlow(void)
{
	unsigned char arrPayload[512];
	unsigned char arrWire[1024];
	unsigned char arrPlain[512];
	unsigned char arrData[40] = { 1u };
	unsigned char arrGlobalFields[3] = { 4u, 5u, 6u };
	uint64 arrClientGlobalTokens[TEST_SSH_CONNECTION_TOKENS];
	uint64 arrServerGlobalTokens[TEST_SSH_CONNECTION_TOKENS];
	testsshconnectionregistry ClientRegistry;
	testsshconnectionregistry ServerRegistry;
	xsshtransportcore ClientCore;
	xsshtransportcore ServerCore;
	xsshconnectionsession Client;
	xsshconnectionsession Server;
	xsshreplyqueue ClientGlobal;
	xsshreplyqueue ServerGlobal;
	xsshconnectionpacket Packet;
	xsshchannelopen OpenValues;
	xsshwriter Writer;
	xbytesview Payload;
	size_t iWireSize;
	uint64 iNowMs = 1u;

	testSshConnectionPairOpen(
		&ClientCore,
		&ServerCore,
		&Client,
		&Server,
		&ClientRegistry,
		&ServerRegistry,
		&ClientGlobal,
		&ServerGlobal,
		arrClientGlobalTokens,
		arrServerGlobalTokens
	);
	testRequire(xrtSshChannelCoreOpenInit(
		&ClientRegistry.Channels[1],
		1u,
		128u,
		64u,
		32u
	), "ssh connection outgoing channel init failed");
	ClientRegistry.Used[1] = true;

	/* 客户端发起 channel，服务端在 transport 提交后选择本地编号和存储。 */
	testRequire(xrtSshWriterInit(
		&Writer,
		arrPayload,
		sizeof(arrPayload)
	) && (xrtSshChannelOpenWrite(
		&Writer,
		XRT_STR_LITERAL("session"),
		1u,
		128u,
		64u,
		(xbytesview){ NULL, 0u }
	) == XSSH_OK), "ssh connection channel open build failed");
	Payload = testSshConnectionPayload(&Writer, arrPayload);
	iWireSize = testSshConnectionSend(
		&Client,
		&ClientCore,
		Payload,
		&ClientRegistry.Channels[1],
		NULL,
		0u,
		arrWire,
		sizeof(arrWire),
		iNowMs++
	);
	Packet = testSshConnectionReceivePrepare(
		&Server,
		&ServerCore,
		arrWire,
		iWireSize,
		arrPlain,
		sizeof(arrPlain),
		iNowMs
	);
	testRequire((Packet.Kind == XSSH_CONNECTION_PACKET_CHANNEL_OPEN) &&
		testSshTextEqual(
			Packet.Message.ChannelOpen.Type,
			XRT_STR_LITERAL("session")
		) && (Packet.Message.ChannelOpen.Sender == 1u),
		"ssh connection channel open view mismatch");
	memset(&OpenValues, 0, sizeof(OpenValues));
	OpenValues.Sender = Packet.Message.ChannelOpen.Sender;
	OpenValues.Window = Packet.Message.ChannelOpen.Window;
	OpenValues.MaxPacket = Packet.Message.ChannelOpen.MaxPacket;
	testSshConnectionReceiveCommit(&Server, &ServerCore, iNowMs++);
	testRequire(xrtSshChannelCoreAcceptInit(
		&ServerRegistry.Channels[2],
		2u,
		&OpenValues,
		160u,
		80u,
		32u
	), "ssh connection incoming channel init failed");
	ServerRegistry.Used[2] = true;

	/* 服务端确认 channel 后，两端核心同时进入 OPEN。 */
	testRequire(xrtSshWriterInit(
		&Writer,
		arrPayload,
		sizeof(arrPayload)
	) && (xrtSshChannelOpenConfirmationWrite(
		&Writer,
		1u,
		2u,
		160u,
		80u,
		(xbytesview){ NULL, 0u }
	) == XSSH_OK), "ssh connection confirmation build failed");
	Payload = testSshConnectionPayload(&Writer, arrPayload);
	iWireSize = testSshConnectionSend(
		&Server,
		&ServerCore,
		Payload,
		&ServerRegistry.Channels[2],
		NULL,
		0u,
		arrWire,
		sizeof(arrWire),
		iNowMs++
	);
	Packet = testSshConnectionReceivePrepare(
		&Client,
		&ClientCore,
		arrWire,
		iWireSize,
		arrPlain,
		sizeof(arrPlain),
		iNowMs
	);
	testRequire(Packet.Kind ==
		XSSH_CONNECTION_PACKET_CHANNEL_CONFIRMATION,
		"ssh connection confirmation kind mismatch");
	testSshConnectionReceiveCommit(&Client, &ClientCore, iNowMs++);
	testRequire(xrtSshChannelCoreOpen(&ClientRegistry.Channels[1]) &&
		xrtSshChannelCoreOpen(&ServerRegistry.Channels[2]),
		"ssh connection channel did not open");

	/* want-reply channel request 在可靠发送后入队，在响应提交后出队。 */
	testRequire(xrtSshWriterInit(
		&Writer,
		arrPayload,
		sizeof(arrPayload)
	) && (xrtSshChannelRequestWrite(
		&Writer,
		2u,
		XRT_STR_LITERAL("exec"),
		true,
		(xbytesview){ NULL, 0u }
	) == XSSH_OK), "ssh connection request build failed");
	Payload = testSshConnectionPayload(&Writer, arrPayload);
	iWireSize = testSshConnectionSend(
		&Client,
		&ClientCore,
		Payload,
		&ClientRegistry.Channels[1],
		&ClientRegistry.Replies[1],
		77u,
		arrWire,
		sizeof(arrWire),
		iNowMs++
	);
	testRequire(xrtSshReplyQueueCount(
		&ClientRegistry.Replies[1]
	) == 1u, "ssh connection request token not queued");
	Packet = testSshConnectionReceivePrepare(
		&Server,
		&ServerCore,
		arrWire,
		iWireSize,
		arrPlain,
		sizeof(arrPlain),
		iNowMs
	);
	testRequire((Packet.Kind == XSSH_CONNECTION_PACKET_CHANNEL_REQUEST) &&
		Packet.Message.ChannelRequest.WantReply && testSshTextEqual(
			Packet.Message.ChannelRequest.Type,
			XRT_STR_LITERAL("exec")
		), "ssh connection request view mismatch");
	testSshConnectionReceiveCommit(&Server, &ServerCore, iNowMs++);

	testRequire(xrtSshWriterInit(
		&Writer,
		arrPayload,
		sizeof(arrPayload)
	) && (xrtSshChannelSuccessWrite(
		&Writer,
		1u
	) == XSSH_OK), "ssh connection success build failed");
	Payload = testSshConnectionPayload(&Writer, arrPayload);
	iWireSize = testSshConnectionSend(
		&Server,
		&ServerCore,
		Payload,
		&ServerRegistry.Channels[2],
		NULL,
		0u,
		arrWire,
		sizeof(arrWire),
		iNowMs++
	);
	Packet = testSshConnectionReceivePrepare(
		&Client,
		&ClientCore,
		arrWire,
		iWireSize,
		arrPlain,
		sizeof(arrPlain),
		iNowMs
	);
	testRequire((Packet.Kind == XSSH_CONNECTION_PACKET_CHANNEL_SUCCESS) &&
		Packet.HasReplyToken && (Packet.ReplyToken == 77u),
		"ssh connection response token mismatch");
	testSshConnectionReceiveCommit(&Client, &ClientCore, iNowMs++);
	testRequire(xrtSshReplyQueueCount(
		&ClientRegistry.Replies[1]
	) == 0u, "ssh connection response token not consumed");

	/* 数据与 WINDOW_ADJUST 只在对应 transport commit 后推进窗口。 */
	testRequire(xrtSshWriterInit(
		&Writer,
		arrPayload,
		sizeof(arrPayload)
	) && (xrtSshChannelDataWrite(
		&Writer,
		2u,
		(xbytesview){ arrData, sizeof(arrData) }
	) == XSSH_OK), "ssh connection data build failed");
	Payload = testSshConnectionPayload(&Writer, arrPayload);
	iWireSize = testSshConnectionSend(
		&Client,
		&ClientCore,
		Payload,
		&ClientRegistry.Channels[1],
		NULL,
		0u,
		arrWire,
		sizeof(arrWire),
		iNowMs++
	);
	testRequire(ClientRegistry.Channels[1].Window.SendWindow == 120u,
		"ssh connection send window mismatch");
	Packet = testSshConnectionReceivePrepare(
		&Server,
		&ServerCore,
		arrWire,
		iWireSize,
		arrPlain,
		sizeof(arrPlain),
		iNowMs
	);
	testRequire((Packet.Kind == XSSH_CONNECTION_PACKET_CHANNEL_DATA) &&
		testSshBytesEqual(
			Packet.Message.ChannelData.Data,
			(xbytesview){ arrData, sizeof(arrData) }
		), "ssh connection data view mismatch");
	testSshConnectionReceiveCommit(&Server, &ServerCore, iNowMs++);
	testRequire((ServerRegistry.Channels[2].Window.ReceiveWindow == 120u) &&
		(ServerRegistry.Channels[2].Window.ReceiveBuffered == 40u) &&
		(xrtSshChannelCoreDataConsume(
			&ServerRegistry.Channels[2],
			40u
		) == XSSH_OK) && xrtSshChannelCoreAdjustReady(
			&ServerRegistry.Channels[2]
		), "ssh connection receive window mismatch");

	testRequire(xrtSshWriterInit(
		&Writer,
		arrPayload,
		sizeof(arrPayload)
	) && (xrtSshChannelWindowAdjustWrite(
		&Writer,
		1u,
		40u
	) == XSSH_OK), "ssh connection adjust build failed");
	Payload = testSshConnectionPayload(&Writer, arrPayload);
	iWireSize = testSshConnectionSend(
		&Server,
		&ServerCore,
		Payload,
		&ServerRegistry.Channels[2],
		NULL,
		0u,
		arrWire,
		sizeof(arrWire),
		iNowMs++
	);
	Packet = testSshConnectionReceivePrepare(
		&Client,
		&ClientCore,
		arrWire,
		iWireSize,
		arrPlain,
		sizeof(arrPlain),
		iNowMs
	);
	testRequire(Packet.Kind == XSSH_CONNECTION_PACKET_CHANNEL_ADJUST,
		"ssh connection adjust kind mismatch");
	testSshConnectionReceiveCommit(&Client, &ClientCore, iNowMs++);
	testRequire((ClientRegistry.Channels[1].Window.SendWindow == 160u) &&
		(ServerRegistry.Channels[2].Window.ReceiveWindow == 160u),
		"ssh connection adjust commit mismatch");

	/* 全局请求使用独立 FIFO，并保留 success 的类型专用字段。 */
	testRequire(xrtSshWriterInit(
		&Writer,
		arrPayload,
		sizeof(arrPayload)
	) && (xrtSshGlobalRequestWrite(
		&Writer,
		XRT_STR_LITERAL("keepalive@test"),
		true,
		(xbytesview){ NULL, 0u }
	) == XSSH_OK), "ssh connection global request build failed");
	Payload = testSshConnectionPayload(&Writer, arrPayload);
	iWireSize = testSshConnectionSend(
		&Client,
		&ClientCore,
		Payload,
		NULL,
		NULL,
		99u,
		arrWire,
		sizeof(arrWire),
		iNowMs++
	);
	testRequire(xrtSshReplyQueueCount(&ClientGlobal) == 1u,
		"ssh connection global token not queued");
	Packet = testSshConnectionReceivePrepare(
		&Server,
		&ServerCore,
		arrWire,
		iWireSize,
		arrPlain,
		sizeof(arrPlain),
		iNowMs
	);
	testRequire((Packet.Kind == XSSH_CONNECTION_PACKET_GLOBAL_REQUEST) &&
		Packet.Message.GlobalRequest.WantReply,
		"ssh connection global request mismatch");
	testSshConnectionReceiveCommit(&Server, &ServerCore, iNowMs++);

	testRequire(xrtSshWriterInit(
		&Writer,
		arrPayload,
		sizeof(arrPayload)
	) && (xrtSshGlobalSuccessWrite(
		&Writer,
		(xbytesview){ arrGlobalFields, sizeof(arrGlobalFields) }
	) == XSSH_OK), "ssh connection global success build failed");
	Payload = testSshConnectionPayload(&Writer, arrPayload);
	iWireSize = testSshConnectionSend(
		&Server,
		&ServerCore,
		Payload,
		NULL,
		NULL,
		0u,
		arrWire,
		sizeof(arrWire),
		iNowMs++
	);
	Packet = testSshConnectionReceivePrepare(
		&Client,
		&ClientCore,
		arrWire,
		iWireSize,
		arrPlain,
		sizeof(arrPlain),
		iNowMs
	);
	testRequire((Packet.Kind == XSSH_CONNECTION_PACKET_GLOBAL_SUCCESS) &&
		Packet.HasReplyToken && (Packet.ReplyToken == 99u) &&
		testSshBytesEqual(
			Packet.Message.GlobalSuccess,
			(xbytesview){ arrGlobalFields, sizeof(arrGlobalFields) }
		), "ssh connection global response mismatch");
	testSshConnectionReceiveCommit(&Client, &ClientCore, iNowMs++);
	testRequire(xrtSshReplyQueueCount(&ClientGlobal) == 0u,
		"ssh connection global token not consumed");

	/* EOF 与双向 CLOSE 通过同一事务规则推进，最终允许回收。 */
	testRequire(xrtSshWriterInit(
		&Writer,
		arrPayload,
		sizeof(arrPayload)
	) && (xrtSshChannelEofWrite(
		&Writer,
		2u
	) == XSSH_OK), "ssh connection EOF build failed");
	Payload = testSshConnectionPayload(&Writer, arrPayload);
	iWireSize = testSshConnectionSend(
		&Client,
		&ClientCore,
		Payload,
		&ClientRegistry.Channels[1],
		NULL,
		0u,
		arrWire,
		sizeof(arrWire),
		iNowMs++
	);
	Packet = testSshConnectionReceivePrepare(
		&Server,
		&ServerCore,
		arrWire,
		iWireSize,
		arrPlain,
		sizeof(arrPlain),
		iNowMs
	);
	testRequire(Packet.Kind == XSSH_CONNECTION_PACKET_CHANNEL_EOF,
		"ssh connection EOF kind mismatch");
	testSshConnectionReceiveCommit(&Server, &ServerCore, iNowMs++);

	testRequire(xrtSshWriterInit(
		&Writer,
		arrPayload,
		sizeof(arrPayload)
	) && (xrtSshChannelCloseWrite(
		&Writer,
		2u
	) == XSSH_OK), "ssh connection close build failed");
	Payload = testSshConnectionPayload(&Writer, arrPayload);
	iWireSize = testSshConnectionSend(
		&Client,
		&ClientCore,
		Payload,
		&ClientRegistry.Channels[1],
		NULL,
		0u,
		arrWire,
		sizeof(arrWire),
		iNowMs++
	);
	Packet = testSshConnectionReceivePrepare(
		&Server,
		&ServerCore,
		arrWire,
		iWireSize,
		arrPlain,
		sizeof(arrPlain),
		iNowMs
	);
	testRequire(Packet.Kind == XSSH_CONNECTION_PACKET_CHANNEL_CLOSE,
		"ssh connection close receive mismatch");
	testSshConnectionReceiveCommit(&Server, &ServerCore, iNowMs++);

	testRequire(xrtSshWriterInit(
		&Writer,
		arrPayload,
		sizeof(arrPayload)
	) && (xrtSshChannelCloseWrite(
		&Writer,
		1u
	) == XSSH_OK), "ssh connection close reply build failed");
	Payload = testSshConnectionPayload(&Writer, arrPayload);
	iWireSize = testSshConnectionSend(
		&Server,
		&ServerCore,
		Payload,
		&ServerRegistry.Channels[2],
		NULL,
		0u,
		arrWire,
		sizeof(arrWire),
		iNowMs++
	);
	Packet = testSshConnectionReceivePrepare(
		&Client,
		&ClientCore,
		arrWire,
		iWireSize,
		arrPlain,
		sizeof(arrPlain),
		iNowMs
	);
	testRequire(Packet.Kind == XSSH_CONNECTION_PACKET_CHANNEL_CLOSE,
		"ssh connection close reply mismatch");
	testSshConnectionReceiveCommit(&Client, &ClientCore, iNowMs);
	testRequire(xrtSshChannelCoreClosed(&ClientRegistry.Channels[1]) &&
		xrtSshChannelCoreClosed(&ServerRegistry.Channels[2]),
		"ssh connection channels not reclaimable");

	xrtSshTransportCoreClear(&ClientCore);
	xrtSshTransportCoreClear(&ServerCore);
}



/* 验证写放弃无副作用，读放弃关闭同一不可回滚协议路径。 */
static void testSshConnectionSessionAbort(void)
{
	unsigned char arrPayload[128];
	unsigned char arrWire[256];
	unsigned char arrPlain[128];
	uint64 arrClientGlobalTokens[TEST_SSH_CONNECTION_TOKENS];
	uint64 arrServerGlobalTokens[TEST_SSH_CONNECTION_TOKENS];
	testsshconnectionregistry ClientRegistry;
	testsshconnectionregistry ServerRegistry;
	xsshtransportcore ClientCore;
	xsshtransportcore ServerCore;
	xsshconnectionsession Client;
	xsshconnectionsession Server;
	xsshreplyqueue ClientGlobal;
	xsshreplyqueue ServerGlobal;
	xsshchannelcore Before;
	xsshconnectionpacket Packet;
	xsshwriter Writer;
	xbytesview Payload;
	size_t iWireSize;

	testSshConnectionPairOpen(
		&ClientCore,
		&ServerCore,
		&Client,
		&Server,
		&ClientRegistry,
		&ServerRegistry,
		&ClientGlobal,
		&ServerGlobal,
		arrClientGlobalTokens,
		arrServerGlobalTokens
	);
	testRequire(xrtSshChannelCoreOpenInit(
		&ClientRegistry.Channels[1],
		1u,
		64u,
		32u,
		16u
	), "ssh connection abort channel init failed");
	ClientRegistry.Used[1] = true;
	Before = ClientRegistry.Channels[1];
	testRequire(xrtSshWriterInit(
		&Writer,
		arrPayload,
		sizeof(arrPayload)
	) && (xrtSshChannelOpenWrite(
		&Writer,
		XRT_STR_LITERAL("session"),
		1u,
		64u,
		32u,
		(xbytesview){ NULL, 0u }
	) == XSSH_OK), "ssh connection abort open build failed");
	Payload = testSshConnectionPayload(&Writer, arrPayload);
	testRequire((xrtSshConnectionSessionWritePrepare(
		&Client,
		&ClientCore,
		Payload,
		&ClientRegistry.Channels[1],
		NULL,
		0u
	) == XSSH_OK) && (xrtSshConnectionSessionWriteAbort(
		&Client
	) == XSSH_OK) && (memcmp(
		&Before,
		&ClientRegistry.Channels[1],
		sizeof(Before)
	) == 0) && xrtSshConnectionSessionActive(&Client),
		"ssh connection write abort changed state");

	testRequire(xrtSshWriterInit(
		&Writer,
		arrPayload,
		sizeof(arrPayload)
	) && (xrtSshGlobalRequestWrite(
		&Writer,
		XRT_STR_LITERAL("ping"),
		false,
		(xbytesview){ NULL, 0u }
	) == XSSH_OK), "ssh connection abort global build failed");
	Payload = testSshConnectionPayload(&Writer, arrPayload);
	iWireSize = testSshConnectionSend(
		&Client,
		&ClientCore,
		Payload,
		NULL,
		NULL,
		0u,
		arrWire,
		sizeof(arrWire),
		1u
	);
	Packet = testSshConnectionReceivePrepare(
		&Server,
		&ServerCore,
		arrWire,
		iWireSize,
		arrPlain,
		sizeof(arrPlain),
		1u
	);
	testRequire((Packet.Kind == XSSH_CONNECTION_PACKET_GLOBAL_REQUEST) &&
		(xrtSshConnectionSessionReadAbort(&Server) == XSSH_OK) &&
		(xrtSshTransportCoreReadAbort(&ServerCore) == XSSH_OK) &&
		!xrtSshConnectionSessionActive(&Server) &&
		(ServerCore.State.Phase == XSSH_TRANSPORT_CLOSED),
		"ssh connection read abort remained usable");
	xrtSshTransportCoreClear(&ClientCore);
	xrtSshTransportCoreClear(&ServerCore);
}



/* 验证未知应用消息可转交外层，已识别畸形消息则终止会话。 */
static void testSshConnectionSessionRouting(void)
{
	unsigned char arrWire[256];
	unsigned char arrPlain[128];
	unsigned char arrUnknown[1] = { 101u };
	unsigned char arrMalformed[2] = { XSSH_MSG_CHANNEL_DATA, 0u };
	uint64 arrClientGlobalTokens[TEST_SSH_CONNECTION_TOKENS];
	uint64 arrServerGlobalTokens[TEST_SSH_CONNECTION_TOKENS];
	testsshconnectionregistry ClientRegistry;
	testsshconnectionregistry ServerRegistry;
	xsshtransportcore ClientCore;
	xsshtransportcore ServerCore;
	xsshconnectionsession Client;
	xsshconnectionsession Server;
	xsshreplyqueue ClientGlobal;
	xsshreplyqueue ServerGlobal;
	xsshconnectionpacket ConnectionPacket;
	xsshpacketview Packet;
	xsshreader Reader;
	xsshrekeydecision Decision;
	size_t iWireSize;

	testSshConnectionPairOpen(
		&ClientCore,
		&ServerCore,
		&Client,
		&Server,
		&ClientRegistry,
		&ServerRegistry,
		&ClientGlobal,
		&ServerGlobal,
		arrClientGlobalTokens,
		arrServerGlobalTokens
	);
	iWireSize = testSshConnectionTransportSend(
		&ClientCore,
		(xbytesview){ arrUnknown, sizeof(arrUnknown) },
		arrWire,
		sizeof(arrWire),
		1u
	);
	testRequire(xrtSshReaderInit(
		&Reader,
		(xbytesview){ arrWire, iWireSize }
	) && (xrtSshTransportCoreReadPrepare(
		&ServerCore,
		&Reader,
		&Packet,
		arrPlain,
		sizeof(arrPlain),
		1u
	) == XSSH_OK) && (xrtSshConnectionSessionReadPrepare(
		&Server,
		&ServerCore,
		Packet.Payload,
		&ConnectionPacket
	) == XSSH_ERROR_UNSUPPORTED) && xrtSshConnectionSessionActive(
		&Server
	) && (xrtSshTransportCoreReadCommit(
		&ServerCore,
		1u,
		&Decision
	) == XSSH_OK), "ssh connection unknown message was not routable");

	iWireSize = testSshConnectionTransportSend(
		&ClientCore,
		(xbytesview){ arrMalformed, sizeof(arrMalformed) },
		arrWire,
		sizeof(arrWire),
		2u
	);
	testRequire(xrtSshReaderInit(
		&Reader,
		(xbytesview){ arrWire, iWireSize }
	) && (xrtSshTransportCoreReadPrepare(
		&ServerCore,
		&Reader,
		&Packet,
		arrPlain,
		sizeof(arrPlain),
		2u
	) == XSSH_OK) && (xrtSshConnectionSessionReadPrepare(
		&Server,
		&ServerCore,
		Packet.Payload,
		&ConnectionPacket
	) == XSSH_ERROR_PROTOCOL) && !xrtSshConnectionSessionActive(
		&Server
	) && (xrtSshTransportCoreReadAbort(
		&ServerCore
	) == XSSH_OK), "ssh connection malformed message remained usable");
	xrtSshTransportCoreClear(&ClientCore);
	xrtSshTransportCoreClear(&ServerCore);
}



/* 验证对象边界、队列歧义和初始化重叠保护。 */
static void testSshConnectionSessionBoundaries(void)
{
	unsigned char arrPayload[64];
	uint64 arrTokens[2];
	xsshtransportcore Core;
	xsshconnectionsession Session;
	xsshreplyqueue Queue;
	xsshreplyqueue OverlapQueue;
	xsshwriter Writer;
	xbytesview Payload;

	testSshConnectionCoreOpen(&Core, XSSH_ROLE_CLIENT);
	testRequire(xrtSshReplyQueueInit(&Queue, arrTokens, 2u) &&
		!xrtSshConnectionSessionInit(
			NULL,
			XSSH_ROLE_CLIENT,
			NULL,
			NULL,
			&Queue
		) && xrtSshConnectionSessionInit(
			&Session,
			XSSH_ROLE_CLIENT,
			NULL,
			NULL,
			&Queue
		) && (xrtSshConnectionSessionBegin(
			&Session,
			&Core
		) == XSSH_OK), "ssh connection boundary init failed");
	testRequire(xrtSshWriterInit(
		&Writer,
		arrPayload,
		sizeof(arrPayload)
	) && (xrtSshGlobalRequestWrite(
		&Writer,
		XRT_STR_LITERAL("ping"),
		true,
		(xbytesview){ NULL, 0u }
	) == XSSH_OK), "ssh connection boundary request build failed");
	Payload = testSshConnectionPayload(&Writer, arrPayload);
	testRequire((xrtSshConnectionSessionWritePrepare(
		&Session,
		&Core,
		Payload,
		NULL,
		&Queue,
		1u
	) == XSSH_ERROR_ARGUMENT) &&
		xrtSshConnectionSessionActive(&Session) &&
		(xrtSshReplyQueueCount(&Queue) == 0u),
		"ssh connection global queue ambiguity accepted");
	xrtSshConnectionSessionClear(&Session);
	testRequire(!xrtSshConnectionSessionActive(&Session) &&
		(xrtSshReplyQueuePush(&Queue, 7u) == XSSH_OK) &&
		!xrtSshConnectionSessionInit(
			&Session,
			XSSH_ROLE_CLIENT,
			NULL,
			NULL,
			&Queue
		) && xrtSshReplyQueueInit(
			&OverlapQueue,
			(uint64*)(void*)&Session,
			1u
		) && !xrtSshConnectionSessionInit(
			&Session,
			XSSH_ROLE_CLIENT,
			NULL,
			NULL,
			&OverlapQueue
		), "ssh connection queue lifecycle boundary failed");
	xrtSshTransportCoreClear(&Core);
}



/* 运行无网络、无固定 channel 数量的 connection 编排测试。 */
int main(void)
{
	testSshConnectionSessionFlow();
	testSshConnectionSessionAbort();
	testSshConnectionSessionRouting();
	testSshConnectionSessionBoundaries();
	return 0;
}
