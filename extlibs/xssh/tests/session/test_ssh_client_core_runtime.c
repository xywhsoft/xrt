#include "../test.h"



/* 固定临时密钥和签名，使完整客户端状态机测试可重复。 */
static const uint8 g_arrClientPrivate[32] = {
	0x01u, 0x02u, 0x03u, 0x04u, 0x05u, 0x06u, 0x07u, 0x08u,
	0x09u, 0x0au, 0x0bu, 0x0cu, 0x0du, 0x0eu, 0x0fu, 0x10u,
	0x11u, 0x12u, 0x13u, 0x14u, 0x15u, 0x16u, 0x17u, 0x18u,
	0x19u, 0x1au, 0x1bu, 0x1cu, 0x1du, 0x1eu, 0x1fu, 0x20u
};



static const uint8 g_arrServerPrivate[32] = {
	0x21u, 0x22u, 0x23u, 0x24u, 0x25u, 0x26u, 0x27u, 0x28u,
	0x29u, 0x2au, 0x2bu, 0x2cu, 0x2du, 0x2eu, 0x2fu, 0x30u,
	0x31u, 0x32u, 0x33u, 0x34u, 0x35u, 0x36u, 0x37u, 0x38u,
	0x39u, 0x3au, 0x3bu, 0x3cu, 0x3du, 0x3eu, 0x3fu, 0x40u
};



static const uint8 g_arrHostPublic[32] = {
	0xdau, 0x29u, 0xe9u, 0x5bu, 0x02u, 0xe0u, 0x0fu, 0xfau,
	0x15u, 0x64u, 0x57u, 0x75u, 0xfbu, 0x1du, 0x2bu, 0xa2u,
	0x22u, 0xa1u, 0x94u, 0x33u, 0x95u, 0xeeu, 0xa0u, 0x6bu,
	0x94u, 0xe2u, 0xc0u, 0x57u, 0xb7u, 0xbeu, 0x69u, 0xd0u
};



static const uint8 g_arrHostSignature[64] = {
	0x0eu, 0x25u, 0xafu, 0x29u, 0xc6u, 0x7cu, 0xc8u, 0x0du,
	0x3cu, 0xbau, 0x87u, 0x63u, 0x53u, 0x93u, 0x67u, 0x25u,
	0x03u, 0xf7u, 0x55u, 0x9bu, 0x8bu, 0x6bu, 0xb2u, 0x20u,
	0x3cu, 0xb3u, 0x6eu, 0x4au, 0x14u, 0xe9u, 0xbcu, 0x08u,
	0xf5u, 0x9fu, 0x50u, 0x8bu, 0xbcu, 0xb2u, 0x6bu, 0x3cu,
	0xa8u, 0xdfu, 0x60u, 0x35u, 0x0fu, 0x77u, 0xe5u, 0xf6u,
	0x91u, 0xfeu, 0x92u, 0xccu, 0xf4u, 0xedu, 0xf2u, 0x49u,
	0xf4u, 0xe7u, 0xfcu, 0x49u, 0x41u, 0x30u, 0xf8u, 0x05u
};



typedef struct testsshclienthost {
	xbytesview Expected;
	uint32 Calls;
} testsshclienthost;



/* 固定 packet padding，避免测试结果受随机数影响。 */
static bool testSshClientRuntimePadding(
	void* pOutput,
	size_t iSize,
	ptr pUserData
)
{
	bytes pBytes = (bytes)pOutput;
	uint8 iSeed = *(const uint8*)pUserData;
	size_t i;

	for ( i = 0u; i < iSize; ++i ) {
		pBytes[i] = (uint8)(iSeed + (uint8)i);
	}
	return true;
}



/* 返回 writer 当前完整 payload。 */
static xbytesview testSshClientRuntimePayload(
	const xsshwriter* pWriter,
	const void* pData
)
{
	return (xbytesview){ (const unsigned char*)pData, pWriter->Size };
}



/* 构建与固定签名向量一致的初始 KEXINIT。 */
static xbytesview testSshClientRuntimeKexInit(
	xsshrole Role,
	uint8 iCookieBase,
	void* pOutput,
	size_t iCapacity
)
{
	uint8 arrCookie[XSSH_KEX_COOKIE_SIZE];
	xsshkexinitconfig Config;
	xsshwriter Writer;
	size_t i;

	for ( i = 0u; i < sizeof(arrCookie); ++i ) {
		arrCookie[i] = (uint8)(iCookieBase + (uint8)i);
	}
	testRequire(xrtSshKexInitConfigInit(&Config, Role, true) &&
		xrtSshWriterInit(&Writer, pOutput, iCapacity) &&
		(xrtSshKexInitWrite(
			&Writer,
			(xbytesview){ arrCookie, sizeof(arrCookie) },
			&Config
		) == XSSH_OK), "ssh client runtime KEXINIT build failed");
	return testSshClientRuntimePayload(&Writer, pOutput);
}



/* 构建固定 Ed25519 主机公钥和签名 blob。 */
static void testSshClientRuntimeHostMaterial(
	void* pHostKey,
	size_t iHostKeyCapacity,
	xbytesview* pHostKeyView,
	void* pSignature,
	size_t iSignatureCapacity,
	xbytesview* pSignatureView
)
{
	xsshwriter Writer;

	testRequire(xrtSshWriterInit(&Writer, pHostKey, iHostKeyCapacity) &&
		(xrtSshEd25519PublicKeyWrite(
			&Writer,
			(xbytesview){ g_arrHostPublic, sizeof(g_arrHostPublic) }
		) == XSSH_OK), "ssh client runtime host key build failed");
	*pHostKeyView = testSshClientRuntimePayload(&Writer, pHostKey);
	testRequire(xrtSshWriterInit(&Writer, pSignature, iSignatureCapacity) &&
		(xrtSshEd25519SignatureWrite(
			&Writer,
			(xbytesview){ g_arrHostSignature, sizeof(g_arrHostSignature) }
		) == XSSH_OK), "ssh client runtime signature build failed");
	*pSignatureView = testSshClientRuntimePayload(&Writer, pSignature);
}



/* 不经过 TCP 线路提交双方 identification transcript。 */
static void testSshClientRuntimeVersions(
	xsshsessiontcp* pSession,
	xstrview Local,
	xstrview Peer
)
{
	xsshtransportcore* pTransport = xrtSshTransportTcpCore(
		xrtSshSessionTcpTransport(pSession)
	);
	xsshsessioncore* pCore = xrtSshSessionTcpCore(pSession);

	testRequire((pCore != NULL) && (pTransport != NULL) &&
		(xrtSshSessionCoreVersionPrepare(
			pCore,
			pTransport,
			XSSH_TRANSPORT_LOCAL,
			Local
		) == XSSH_OK) && (xrtSshTransportCoreIdentificationCommit(
			pTransport,
			XSSH_TRANSPORT_LOCAL
		) == XSSH_OK) && (xrtSshSessionCoreVersionCommit(
			pCore,
			pTransport
		) == XSSH_OK) && (xrtSshSessionCoreVersionPrepare(
			pCore,
			pTransport,
			XSSH_TRANSPORT_PEER,
			Peer
		) == XSSH_OK) && (xrtSshTransportCoreIdentificationCommit(
			pTransport,
			XSSH_TRANSPORT_PEER
		) == XSSH_OK) && (xrtSshSessionCoreVersionCommit(
			pCore,
			pTransport
		) == XSSH_OK), "ssh client runtime versions failed");
}



/* 把一个 payload 封装为真实线包并可靠提交发送端事务。 */
static size_t testSshClientRuntimeWire(
	xsshsessiontcp* pSender,
	xbytesview Payload,
	uint64 iNowMs,
	uint8* pPadding,
	void* pOutput,
	size_t iCapacity,
	xsshsessionpacketkind* pKind
)
{
	xsshtransportcore* pTransport = xrtSshTransportTcpCore(
		xrtSshSessionTcpTransport(pSender)
	);
	xsshsessioncore* pCore = xrtSshSessionTcpCore(pSender);
	xsshrekeydecision Decision;
	xsshwriter Writer;

	testRequire((pCore != NULL) && (pTransport != NULL) &&
		(xrtSshSessionCoreWritePrepare(
			pCore,
			pTransport,
			Payload,
			NULL,
			NULL,
			0u,
			iNowMs,
			pKind
		) == XSSH_OK) && xrtSshWriterInit(
			&Writer,
			pOutput,
			iCapacity
		) && (xrtSshTransportCoreWritePrepareWithPadding(
			pTransport,
			&Writer,
			Payload,
			testSshClientRuntimePadding,
			pPadding,
			iNowMs
		) == XSSH_OK) && (xrtSshSessionCoreWriteBind(
			pCore,
			pTransport,
			Payload
		) == XSSH_OK) && (xrtSshTransportCoreWriteCommit(
			pTransport,
			iNowMs,
			&Decision
		) == XSSH_OK) && (Decision == XSSH_REKEY_NONE) &&
		(xrtSshSessionCoreWriteCommit(
			pCore,
			pTransport,
			iNowMs
		) == XSSH_OK), "ssh client runtime wire build failed");
	return Writer.Size;
}



/* 通过无额外缓冲的底层事务把客户端 payload 交给模拟服务器。 */
static void testSshClientRuntimeTransfer(
	xsshsessiontcp* pSender,
	xsshsessiontcp* pReceiver,
	xbytesview Payload,
	uint64 iNowMs,
	uint8* pPadding
)
{
	unsigned char arrWire[2048];
	unsigned char arrPlain[1024];
	xsshtransportcore* pTransport = xrtSshTransportTcpCore(
		xrtSshSessionTcpTransport(pReceiver)
	);
	xsshsessioncore* pCore = xrtSshSessionTcpCore(pReceiver);
	xsshrekeydecision Decision;
	xsshsessionpacket SessionPacket;
	xsshsessionpacketkind Kind;
	xsshpacketview Packet;
	xsshreader Reader;
	size_t iWireSize;

	iWireSize = testSshClientRuntimeWire(
		pSender,
		Payload,
		iNowMs,
		pPadding,
		arrWire,
		sizeof(arrWire),
		&Kind
	);
	testRequire((pCore != NULL) && (pTransport != NULL) && xrtSshReaderInit(
		&Reader,
		(xbytesview){ arrWire, iWireSize }
	) && (xrtSshTransportCoreReadPrepare(
		pTransport,
		&Reader,
		&Packet,
		arrPlain,
		sizeof(arrPlain),
		iNowMs
	) == XSSH_OK) && (xrtSshSessionCoreReadPrepare(
		pCore,
		pTransport,
		Packet.Payload,
		NULL,
		0u,
		NULL,
		iNowMs,
		&SessionPacket
	) == XSSH_OK) && (SessionPacket.Kind == Kind) &&
		(xrtSshTransportCoreReadCommit(
			pTransport,
			iNowMs,
			&Decision
		) == XSSH_OK) && (Decision == XSSH_REKEY_NONE) &&
		(xrtSshSessionCoreReadCommit(
			pCore,
			pTransport,
			iNowMs
		) == XSSH_OK), "ssh client runtime transfer failed");
}



/* 用动态 Reader 接收服务器线包，并在提交前交给客户端观察器。 */
static void testSshClientRuntimeReceive(
	xsshsessiontcp* pServer,
	xsshclientcore* pClient,
	xsshsessionreader* pReader,
	xnetbuf* pInput,
	xbytesview Payload,
	uint64 iNowMs,
	uint8* pPadding
)
{
	unsigned char arrWire[2048];
	xsshrekeydecision Decision;
	xsshsessionpacketkind Kind;
	xsshsessiontcppacket Packet;
	size_t iWireSize;

	iWireSize = testSshClientRuntimeWire(
		pServer,
		Payload,
		iNowMs,
		pPadding,
		arrWire,
		sizeof(arrWire),
		&Kind
	);
	testRequire(xrtNetBufAppend(pInput, arrWire, iWireSize) &&
		(xrtSshSessionReaderPrepare(
			pReader,
			pInput,
			iNowMs,
			&Packet
		) == XSSH_OK) && (Packet.Session.Kind == Kind) &&
		(xrtSshClientCoreObserve(
			pClient,
			xrtSshSessionReaderSessionConst(pReader),
			&Packet
		) == XSSH_OK) && (xrtSshSessionReaderCommit(
			pReader,
			iNowMs,
			&Decision
		) == XSSH_OK) && (Decision == XSSH_REKEY_NONE) &&
		xrtNetBufEmpty(pInput), "ssh client runtime receive failed");
}



/* 只接受测试指定的主机密钥和默认现代算法。 */
static xsshclienthostdecision testSshClientRuntimeHostKey(
	xsshclientcore* pClient,
	const xsshclienthost* pHost,
	ptr pUserData
)
{
	testsshclienthost* pState = (testsshclienthost*)pUserData;

	(void)pClient;
	pState->Calls += 1u;
	if ( !testSshBytesEqual(pHost->Key, pState->Expected) ||
		!testSshTextEqual(
			pHost->Negotiation.KexAlgorithm,
			XRT_STR_LITERAL("curve25519-sha256")
		) || !testSshTextEqual(
			pHost->Negotiation.ServerHostKeyAlgorithm,
			XRT_STR_LITERAL("ssh-ed25519")
		) ) {
		return XSSH_CLIENT_HOST_REJECT;
	}
	return XSSH_CLIENT_HOST_ACCEPT;
}



/* 完成真实 KEX、主机认证、password 认证和 connection 切换。 */
int main(void)
{
	static const char sClientVersion[] = "SSH-2.0-xssh_client";
	static const char sServerVersion[] = "SSH-2.0-xssh_server";
	unsigned char arrClientKex[512];
	unsigned char arrServerKex[512];
	unsigned char arrHostKey[128];
	unsigned char arrSignature[128];
	unsigned char arrPayload[512];
	testsshclienthost HostState;
	xsshclientcoreconfig ClientCoreConfig;
	xsshsessiontcpconfig ClientConfig;
	xsshsessiontcpconfig ServerConfig;
	xsshauthpassword PasswordMessage;
	xnetbufpoolinfo PoolInfo;
	xsshsessionreader ClientReader;
	xsshclientnext Next;
	xsshclientcore ClientCore;
	xsshsessiontcp Client;
	xsshsessiontcp Server;
	xsshkexsession* pServerKex;
	xbytesview ClientKex;
	xbytesview ServerKex;
	xbytesview HostKey;
	xbytesview Signature;
	xbytesview Payload;
	xstrview Methods;
	xstrview Password = XRT_STR_LITERAL("correct horse battery staple");
	xnetbufpool* pPool;
	xnetbuf Input;
	xsshwriter Writer;
	bool bPartialSuccess = true;
	uint8 iPadding = 0x40u;

	memset(&HostState, 0, sizeof(HostState));
	testRequire(xrtSshClientCoreConfigInit(&ClientCoreConfig) &&
		xrtSshSessionTcpConfigInit(&ClientConfig, XSSH_ROLE_CLIENT) &&
		xrtSshSessionTcpConfigInit(&ServerConfig, XSSH_ROLE_SERVER),
		"ssh client runtime configuration failed");
	ClientCoreConfig.Version = XRT_STR_LITERAL(sClientVersion);
	ClientCoreConfig.User = XRT_STR_LITERAL("alice");
	ClientCoreConfig.HostKey = testSshClientRuntimeHostKey;
	ClientCoreConfig.HostKeyData = &HostState;
	ClientCoreConfig.Authenticate = xrtSshClientPasswordAuth;
	ClientCoreConfig.AuthenticateData = &Password;
	ClientCoreConfig.OutputInitial = 64u;
	ClientCoreConfig.OutputLimit = 4096u;
	pPool = xrtNetBufPoolCreate(NULL);
	testRequire((pPool != NULL) && xrtSshSessionTcpInit(
		&Client,
		pPool,
		&ClientConfig,
		0u
	) && xrtSshSessionTcpInit(
		&Server,
		pPool,
		&ServerConfig,
		0u
	) && xrtSshSessionReaderInit(
		&ClientReader,
		pPool,
		&Client
	) && xrtNetBufInit(&Input, pPool) && xrtSshClientCoreInit(
		&ClientCore,
		&ClientCoreConfig
	), "ssh client runtime initialization failed");

	testSshClientRuntimeVersions(
		&Client,
		ClientCoreConfig.Version,
		XRT_STR_LITERAL(sServerVersion)
	);
	testSshClientRuntimeVersions(
		&Server,
		XRT_STR_LITERAL(sServerVersion),
		ClientCoreConfig.Version
	);
	ClientKex = testSshClientRuntimeKexInit(
		XSSH_ROLE_CLIENT,
		0x00u,
		arrClientKex,
		sizeof(arrClientKex)
	);
	ServerKex = testSshClientRuntimeKexInit(
		XSSH_ROLE_SERVER,
		0x10u,
		arrServerKex,
		sizeof(arrServerKex)
	);
	testSshClientRuntimeTransfer(
		&Client,
		&Server,
		ClientKex,
		0u,
		&iPadding
	);
	testSshClientRuntimeTransfer(
		&Server,
		&Client,
		ServerKex,
		0u,
		&iPadding
	);
	testSshClientRuntimeHostMaterial(
		arrHostKey,
		sizeof(arrHostKey),
		&HostKey,
		arrSignature,
		sizeof(arrSignature),
		&Signature
	);
	HostState.Expected = HostKey;
	testRequire((xrtSshSessionTcpKexBeginWithPrivate(
		&Client,
		(xbytesview){ NULL, 0u },
		(xbytesview){ g_arrClientPrivate, sizeof(g_arrClientPrivate) }
	) == XSSH_OK) && (xrtSshSessionTcpKexBeginWithPrivate(
		&Server,
		HostKey,
		(xbytesview){ g_arrServerPrivate, sizeof(g_arrServerPrivate) }
	) == XSSH_OK), "ssh client runtime KEX begin failed");

	testRequire((xrtSshClientCoreNext(
		&ClientCore,
		&Client,
		&ClientReader,
		1u,
		&Next
	) == XSSH_OK) && (Next.Kind == XSSH_CLIENT_NEXT_PAYLOAD),
		"ssh client runtime ECDH action failed");
	testSshClientRuntimeTransfer(
		&Client,
		&Server,
		Next.Data,
		1u,
		&iPadding
	);
	pServerKex = xrtSshKexExchangeSession(
		xrtSshSessionCoreKex(xrtSshSessionTcpCore(&Server))
	);
	testRequire((pServerKex != NULL) && xrtSshWriterInit(
		&Writer,
		arrPayload,
		sizeof(arrPayload)
	) && (xrtSshKexSessionEcdhReplyPrepare(
		pServerKex,
		&Writer,
		Signature
	) == XSSH_OK), "ssh client runtime ECDH reply failed");
	Payload = testSshClientRuntimePayload(&Writer, arrPayload);
	testSshClientRuntimeReceive(
		&Server,
		&ClientCore,
		&ClientReader,
		&Input,
		Payload,
		2u,
		&iPadding
	);
	testRequire((xrtSshClientCoreNext(
		&ClientCore,
		&Client,
		&ClientReader,
		2u,
		&Next
	) == XSSH_OK) && (Next.Kind == XSSH_CLIENT_NEXT_PAYLOAD) &&
		(HostState.Calls == 1u), "ssh client runtime host trust failed");
	testSshClientRuntimeTransfer(
		&Client,
		&Server,
		Next.Data,
		3u,
		&iPadding
	);
	testRequire(xrtSshWriterInit(
		&Writer,
		arrPayload,
		sizeof(arrPayload)
	) && (xrtSshKexSessionNewKeysPrepare(
		pServerKex,
		&Writer
	) == XSSH_OK), "ssh client runtime server NEWKEYS failed");
	Payload = testSshClientRuntimePayload(&Writer, arrPayload);
	testSshClientRuntimeReceive(
		&Server,
		&ClientCore,
		&ClientReader,
		&Input,
		Payload,
		4u,
		&iPadding
	);

	testRequire((xrtSshSessionTcpAuthBegin(
		&Server,
		NULL,
		5u
	) == XSSH_OK) && (xrtSshClientCoreNext(
		&ClientCore,
		&Client,
		&ClientReader,
		5u,
		&Next
	) == XSSH_OK) && (Next.Kind == XSSH_CLIENT_NEXT_PAYLOAD),
		"ssh client runtime service action failed");
	testSshClientRuntimeTransfer(
		&Client,
		&Server,
		Next.Data,
		5u,
		&iPadding
	);
	testRequire(xrtSshWriterInit(
		&Writer,
		arrPayload,
		sizeof(arrPayload)
	) && (xrtSshServiceAcceptWrite(
		&Writer,
		XRT_STR_LITERAL(XSSH_SERVICE_USERAUTH)
	) == XSSH_OK), "ssh client runtime service accept failed");
	Payload = testSshClientRuntimePayload(&Writer, arrPayload);
	testSshClientRuntimeReceive(
		&Server,
		&ClientCore,
		&ClientReader,
		&Input,
		Payload,
		6u,
		&iPadding
	);
	testRequire((xrtSshClientCoreNext(
		&ClientCore,
		&Client,
		&ClientReader,
		7u,
		&Next
	) == XSSH_OK) && (Next.Kind == XSSH_CLIENT_NEXT_PAYLOAD),
		"ssh client runtime none probe failed");
	testSshClientRuntimeTransfer(
		&Client,
		&Server,
		Next.Data,
		7u,
		&iPadding
	);
	testRequire(xrtSshWriterInit(
		&Writer,
		arrPayload,
		sizeof(arrPayload)
	) && (xrtSshAuthFailureWrite(
		&Writer,
		XRT_STR_LITERAL("publickey,password"),
		false
	) == XSSH_OK), "ssh client runtime auth failure failed");
	Payload = testSshClientRuntimePayload(&Writer, arrPayload);
	testSshClientRuntimeReceive(
		&Server,
		&ClientCore,
		&ClientReader,
		&Input,
		Payload,
		8u,
		&iPadding
	);
	Methods = xrtSshClientCoreAuthMethods(
		&ClientCore,
		&bPartialSuccess
	);
	testRequire(!bPartialSuccess && testSshTextEqual(
		Methods,
		XRT_STR_LITERAL("publickey,password")
	), "ssh client runtime auth methods copy failed");
	testRequire((xrtSshClientCoreNext(
		&ClientCore,
		&Client,
		&ClientReader,
		9u,
		&Next
	) == XSSH_OK) && (Next.Kind == XSSH_CLIENT_NEXT_PAYLOAD) &&
		(xrtSshAuthPasswordRead(
			Next.Data,
			&PasswordMessage
		) == XSSH_OK) && testSshTextEqual(
			PasswordMessage.User,
			ClientCoreConfig.User
		) && testSshTextEqual(
			PasswordMessage.Password,
			Password
		), "ssh client runtime password action failed");
	testSshClientRuntimeTransfer(
		&Client,
		&Server,
		Next.Data,
		9u,
		&iPadding
	);
	testRequire(xrtSshWriterInit(
		&Writer,
		arrPayload,
		sizeof(arrPayload)
	) && (xrtSshAuthSuccessWrite(&Writer) == XSSH_OK),
		"ssh client runtime auth success failed");
	Payload = testSshClientRuntimePayload(&Writer, arrPayload);
	testSshClientRuntimeReceive(
		&Server,
		&ClientCore,
		&ClientReader,
		&Input,
		Payload,
		10u,
		&iPadding
	);
	testRequire((xrtSshClientCoreNext(
		&ClientCore,
		&Client,
		&ClientReader,
		10u,
		&Next
	) == XSSH_OK) && (Next.Kind == XSSH_CLIENT_NEXT_READY) &&
		(xrtSshSessionTcpPhase(&Client) == XSSH_SESSION_CONNECTION),
		"ssh client runtime did not enter connection phase");

	xrtSshClientCoreClear(&ClientCore);
	xrtNetBufClear(&Input);
	xrtSshSessionReaderClear(&ClientReader);
	xrtSshSessionTcpClear(&Client);
	xrtSshSessionTcpClear(&Server);
	xrtNetBufPoolGet(pPool, &PoolInfo);
	testRequire((PoolInfo.LiveBlocks == 0u) &&
		(PoolInfo.LiveBytes == 0u) && xrtNetBufPoolDestroy(pPool),
		"ssh client runtime dynamic blocks leaked");
	return 0;
}
