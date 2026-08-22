#include "../test.h"



/* 固定私钥和签名把双端 KEX 测试变成完全可重复的协议向量。 */
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



static const uint8 g_arrClientRekeyPrivate[32] = {
	0x41u, 0x42u, 0x43u, 0x44u, 0x45u, 0x46u, 0x47u, 0x48u,
	0x49u, 0x4au, 0x4bu, 0x4cu, 0x4du, 0x4eu, 0x4fu, 0x50u,
	0x51u, 0x52u, 0x53u, 0x54u, 0x55u, 0x56u, 0x57u, 0x58u,
	0x59u, 0x5au, 0x5bu, 0x5cu, 0x5du, 0x5eu, 0x5fu, 0x60u
};



static const uint8 g_arrServerRekeyPrivate[32] = {
	0x61u, 0x62u, 0x63u, 0x64u, 0x65u, 0x66u, 0x67u, 0x68u,
	0x69u, 0x6au, 0x6bu, 0x6cu, 0x6du, 0x6eu, 0x6fu, 0x70u,
	0x71u, 0x72u, 0x73u, 0x74u, 0x75u, 0x76u, 0x77u, 0x78u,
	0x79u, 0x7au, 0x7bu, 0x7cu, 0x7du, 0x7eu, 0x7fu, 0x80u
};



static const uint8 g_arrHostPublic[32] = {
	0xdau, 0x29u, 0xe9u, 0x5bu, 0x02u, 0xe0u, 0x0fu, 0xfau,
	0x15u, 0x64u, 0x57u, 0x75u, 0xfbu, 0x1du, 0x2bu, 0xa2u,
	0x22u, 0xa1u, 0x94u, 0x33u, 0x95u, 0xeeu, 0xa0u, 0x6bu,
	0x94u, 0xe2u, 0xc0u, 0x57u, 0xb7u, 0xbeu, 0x69u, 0xd0u
};



static const uint8 g_arrExpectedHash[32] = {
	0x9du, 0x5eu, 0x8au, 0xa3u, 0xbeu, 0xedu, 0x73u, 0x23u,
	0x3du, 0x50u, 0xafu, 0xb4u, 0x7au, 0x03u, 0x0bu, 0x7eu,
	0xe8u, 0x01u, 0x6fu, 0xadu, 0xd6u, 0x26u, 0xeau, 0x6du,
	0xedu, 0x1fu, 0xdbu, 0x5au, 0xafu, 0x7au, 0xc8u, 0x70u
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



static const uint8 g_arrExpectedRekeyHash[32] = {
	0xaeu, 0x77u, 0xf7u, 0xefu, 0x66u, 0x57u, 0x0du, 0x05u,
	0xaeu, 0x51u, 0x0cu, 0x57u, 0x16u, 0x28u, 0x7au, 0xb2u,
	0xbeu, 0x60u, 0x94u, 0xcdu, 0xf2u, 0xbcu, 0x64u, 0x9eu,
	0xcfu, 0x17u, 0x95u, 0xb6u, 0x94u, 0x36u, 0xcfu, 0x5fu
};



static const uint8 g_arrHostRekeySignature[64] = {
	0xc5u, 0x0cu, 0x39u, 0x36u, 0x15u, 0xe2u, 0xbau, 0xd2u,
	0x51u, 0x2du, 0xb4u, 0x6du, 0x1cu, 0x18u, 0xfbu, 0x1du,
	0x05u, 0x7bu, 0x91u, 0x4au, 0x23u, 0x8fu, 0x88u, 0x92u,
	0xe3u, 0x76u, 0xb9u, 0xd1u, 0x21u, 0xc4u, 0x85u, 0xa9u,
	0x67u, 0xd0u, 0x84u, 0x24u, 0xb2u, 0xd3u, 0x60u, 0xd6u,
	0x21u, 0xddu, 0x73u, 0x96u, 0x9du, 0x32u, 0xdeu, 0x8du,
	0x0du, 0xe2u, 0x05u, 0x75u, 0xd4u, 0x3eu, 0x23u, 0x72u,
	0x17u, 0x99u, 0xf1u, 0x8fu, 0x9au, 0x05u, 0xeau, 0x06u
};



/* packet padding 固定递增字节，不给协议测试引入外部随机性。 */
static bool testSshKexSessionPadding(
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



/* 构建固定 cookie 的 client 或 server KEXINIT。 */
static xbytesview testSshKexSessionKexInit(
	xsshrole Role,
	bool bInitial,
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
	testRequire(xrtSshKexInitConfigInit(&Config, Role, bInitial) &&
		xrtSshWriterInit(&Writer, pOutput, iCapacity) &&
		(xrtSshKexInitWrite(
			&Writer,
			(xbytesview){ arrCookie, sizeof(arrCookie) },
			&Config
		) == XSSH_OK), "ssh KEX session KEXINIT build failed");
	return (xbytesview){ (const unsigned char*)pOutput, Writer.Size };
}



/* 初始化 transport core，并直接提交测试已经保存的双方 KEXINIT。 */
static void testSshKexSessionCoreInit(
	xsshtransportcore* pCore,
	xsshrole Role
)
{
	testRequire(xrtSshTransportCoreInit(pCore, Role, 0u, NULL, 0u) &&
		(xrtSshTransportCoreIdentificationCommit(
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
		) == XSSH_OK), "ssh KEX session core setup failed");
}



/* 将一个 payload 通过真实 core packet 写事务编码到线路缓冲。 */
static size_t testSshKexSessionCoreWrite(
	xsshtransportcore* pCore,
	xbytesview Payload,
	void* pWire,
	size_t iCapacity,
	uint64 iNowMs,
	uint8* pPadding
)
{
	xsshrekeydecision Decision;
	xsshwriter Writer;

	testRequire(xrtSshWriterInit(&Writer, pWire, iCapacity) &&
		(xrtSshTransportCoreWritePrepareWithPadding(
			pCore,
			&Writer,
			Payload,
			testSshKexSessionPadding,
			pPadding,
			iNowMs
		) == XSSH_OK) && (xrtSshTransportCoreWriteCommit(
			pCore,
			iNowMs,
			&Decision
		) == XSSH_OK) && (Decision != XSSH_REKEY_REQUIRED),
		"ssh KEX session core write failed");
	return Writer.Size;
}



/* 用真实 core packet 读事务认证线路包，但把提交留给会话解析之后。 */
static xsshpacketview testSshKexSessionCoreReadPrepare(
	xsshtransportcore* pCore,
	const void* pWire,
	size_t iWireSize,
	void* pPlain,
	size_t iPlainCapacity,
	uint64 iNowMs
)
{
	xsshreader Reader;
	xsshpacketview Packet;

	memset(&Packet, 0, sizeof(Packet));
	testRequire(xrtSshReaderInit(
		&Reader,
		(xbytesview){ (const unsigned char*)pWire, iWireSize }
	) && (xrtSshTransportCoreReadPrepare(
		pCore,
		&Reader,
		&Packet,
		pPlain,
		iPlainCapacity,
		iNowMs
	) == XSSH_OK) && (Reader.Position == iWireSize),
		"ssh KEX session core read prepare failed");
	return Packet;
}



/* 提交已经由 KEX session 接受的 core 输入。 */
static void testSshKexSessionCoreReadCommit(
	xsshtransportcore* pCore,
	uint64 iNowMs
)
{
	xsshrekeydecision Decision;

	testRequire((xrtSshTransportCoreReadCommit(
		pCore,
		iNowMs,
		&Decision
	) == XSSH_OK) && (Decision != XSSH_REKEY_REQUIRED),
		"ssh KEX session core read commit failed");
}



/* 在已加密或明文状态下完整转交一个普通 core payload。 */
static void testSshKexSessionCoreTransfer(
	xsshtransportcore* pSender,
	xsshtransportcore* pReceiver,
	xbytesview Payload,
	uint64 iNowMs,
	uint8* pPadding
)
{
	unsigned char arrWire[2048];
	unsigned char arrPlain[1024];
	xsshpacketview Packet;
	size_t iWireSize;

	iWireSize = testSshKexSessionCoreWrite(
		pSender,
		Payload,
		arrWire,
		sizeof(arrWire),
		iNowMs,
		pPadding
	);
	Packet = testSshKexSessionCoreReadPrepare(
		pReceiver,
		arrWire,
		iWireSize,
		arrPlain,
		sizeof(arrPlain),
		iNowMs
	);
	testRequire(testSshBytesEqual(Packet.Payload, Payload),
		"ssh KEX core transfer payload mismatch");
	testSshKexSessionCoreReadCommit(pReceiver, iNowMs);
}



/* 完整提交一个已经由 sender session 准备的 KEX payload。 */
static void testSshKexSessionTransfer(
	xsshkexsession* pSenderSession,
	xsshtransportcore* pSenderCore,
	xsshkexsession* pReceiverSession,
	xsshtransportcore* pReceiverCore,
	xbytesview Payload,
	void* pHostKeyStorage,
	size_t iHostKeyCapacity,
	uint64 iNowMs,
	uint8* pPadding
)
{
	unsigned char arrWire[2048];
	unsigned char arrPlain[1024];
	xsshpacketview Packet;
	size_t iWireSize;

	iWireSize = testSshKexSessionCoreWrite(
		pSenderCore,
		Payload,
		arrWire,
		sizeof(arrWire),
		iNowMs,
		pPadding
	);
	testRequire((xrtSshKexSessionWriteCommit(
		pSenderSession,
		pSenderCore
	) == XSSH_OK), "ssh KEX sender session commit failed");
	Packet = testSshKexSessionCoreReadPrepare(
		pReceiverCore,
		arrWire,
		iWireSize,
		arrPlain,
		sizeof(arrPlain),
		iNowMs
	);
	testRequire((xrtSshKexSessionReadPrepare(
		pReceiverSession,
		pReceiverCore,
		Packet.Payload,
		pHostKeyStorage,
		iHostKeyCapacity,
		NULL
	) == XSSH_OK), "ssh KEX receiver session prepare failed");
	testSshKexSessionCoreReadCommit(pReceiverCore, iNowMs);
	testRequire((xrtSshKexSessionReadCommit(
		pReceiverSession,
		pReceiverCore
	) == XSSH_OK), "ssh KEX receiver session commit failed");
}



/* 构建固定 Ed25519 主机公钥和对应 exchange hash 签名 blob。 */
static void testSshKexSessionHostMaterial(
	void* pHostKey,
	size_t iHostKeyCapacity,
	xbytesview* pHostKeyView,
	xbytesview RawSignature,
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
		) == XSSH_OK), "ssh KEX session host key build failed");
	*pHostKeyView = (xbytesview){
		(const unsigned char*)pHostKey,
		Writer.Size
	};
	testRequire(xrtSshWriterInit(&Writer, pSignature, iSignatureCapacity) &&
		(xrtSshEd25519SignatureWrite(
			&Writer,
			RawSignature
		) == XSSH_OK), "ssh KEX session signature build failed");
	*pSignatureView = (xbytesview){
		(const unsigned char*)pSignature,
		Writer.Size
	};
}



/* 验证确定性 client/server 初始 KEX 和新密钥数据面。 */
static void testSshKexSessionInitial(void)
{
	static const unsigned char arrClientVersion[] =
		"SSH-2.0-xssh_client";
	static const unsigned char arrServerVersion[] =
		"SSH-2.0-xssh_server";
	unsigned char arrClientKex[512];
	unsigned char arrServerKex[512];
	unsigned char arrTranscript[1024];
	unsigned char arrHostKey[128];
	unsigned char arrSignature[128];
	unsigned char arrClientHostKey[128];
	unsigned char arrInitialSessionId[XSSH_SHA256_SIZE];
	unsigned char arrPayload[512];
	unsigned char arrWire[1024];
	unsigned char arrPlain[512];
	unsigned char arrApplication[] = { 94u, 'o', 'k' };
	uint8 iPadding = 0x40u;
	xbytesview ClientKex;
	xbytesview ServerKex;
	xbytesview HostKey;
	xbytesview Signature;
	xbytesview Hash;
	xbytesview ClientSessionId;
	xbytesview ServerSessionId;
	xsshkextranscript Borrowed;
	xsshkextranscript Transcript;
	xsshtransportcore ClientCore;
	xsshtransportcore ServerCore;
	xsshkexsession Client;
	xsshkexsession Server;
	xsshpacketview Packet;
	xsshwriter Writer;
	xsshreader Reader;
	xsshrekeydecision Decision;
	size_t iTranscriptSize;
	size_t iHostKeySize;
	size_t iWireSize;

	ClientKex = testSshKexSessionKexInit(
		XSSH_ROLE_CLIENT,
		true,
		0x00u,
		arrClientKex,
		sizeof(arrClientKex)
	);
	ServerKex = testSshKexSessionKexInit(
		XSSH_ROLE_SERVER,
		true,
		0x10u,
		arrServerKex,
		sizeof(arrServerKex)
	);
	testRequire((xrtSshKexTranscriptInit(
		&Borrowed,
		(xbytesview){ arrClientVersion, sizeof(arrClientVersion) - 1u },
		(xbytesview){ arrServerVersion, sizeof(arrServerVersion) - 1u },
		ClientKex,
		ServerKex
	) == XSSH_OK) && (xrtSshKexTranscriptMeasure(
		&Borrowed,
		&iTranscriptSize
	) == XSSH_OK) && (iTranscriptSize == 686u) &&
		xrtSshWriterInit(&Writer, arrTranscript, sizeof(arrTranscript)) &&
		(xrtSshKexTranscriptWrite(
			&Writer,
			&Borrowed,
			&Transcript
		) == XSSH_OK) && (Writer.Size == iTranscriptSize),
		"ssh KEX transcript copy failed");
	testSshKexSessionHostMaterial(
		arrHostKey,
		sizeof(arrHostKey),
		&HostKey,
		(xbytesview){ g_arrHostSignature, sizeof(g_arrHostSignature) },
		arrSignature,
		sizeof(arrSignature),
		&Signature
	);
	testSshKexSessionCoreInit(&ClientCore, XSSH_ROLE_CLIENT);
	testSshKexSessionCoreInit(&ServerCore, XSSH_ROLE_SERVER);
	testRequire(xrtSshKexSessionInit(&Client, XSSH_ROLE_CLIENT) &&
		xrtSshKexSessionInit(&Server, XSSH_ROLE_SERVER) &&
		(xrtSshKexSessionBeginWithPrivate(
			&Client,
			&ClientCore,
			&Transcript,
			(xbytesview){ NULL, 0u },
			(xbytesview){ g_arrClientPrivate, sizeof(g_arrClientPrivate) }
		) == XSSH_OK) && (xrtSshKexSessionBeginWithPrivate(
			&Server,
			&ServerCore,
			&Transcript,
			HostKey,
			(xbytesview){ g_arrServerPrivate, sizeof(g_arrServerPrivate) }
		) == XSSH_OK) &&
		(xrtSshKexSessionEvent(&Client) ==
		 XSSH_KEX_EVENT_WRITE_ECDH_INIT) &&
		(xrtSshKexSessionEvent(&Server) ==
		 XSSH_KEX_EVENT_READ_ECDH_INIT),
		"ssh KEX session begin failed");

	/* client ECDH_INIT 经过真实 packet core 送入 server。 */
	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshKexSessionEcdhInitPrepare(
			&Client,
			&Writer
		) == XSSH_OK), "ssh KEX client init prepare failed");
	iWireSize = testSshKexSessionCoreWrite(
		&ClientCore,
		(xbytesview){ arrPayload, Writer.Size },
		arrWire,
		sizeof(arrWire),
		1u,
		&iPadding
	);
	testRequire((xrtSshKexSessionWriteCommit(
		&Client,
		&ClientCore
	) == XSSH_OK), "ssh KEX client init commit failed");
	Packet = testSshKexSessionCoreReadPrepare(
		&ServerCore,
		arrWire,
		iWireSize,
		arrPlain,
		sizeof(arrPlain),
		1u
	);
	testRequire((xrtSshKexSessionReadPrepare(
		&Server,
		&ServerCore,
		Packet.Payload,
		NULL,
		0u,
		NULL
	) == XSSH_OK), "ssh KEX server init parse failed");
	testSshKexSessionCoreReadCommit(&ServerCore, 1u);
	testRequire(xrtSshKexSessionReadCommit(
		&Server,
		&ServerCore
	) == XSSH_OK, "ssh KEX server init commit failed");
	testRequire(xrtSshKexSessionExchangeHash(
		&Server,
		&Hash
	) == XSSH_OK, "ssh KEX server hash calculation failed");
	testRequire(testSshBytesEqual(
		Hash,
		(xbytesview){ g_arrExpectedHash, sizeof(g_arrExpectedHash) }
	), "ssh KEX server hash mismatch");

	/* 错误签名不得建立写事务，正确签名随后仍可重试。 */
	arrSignature[sizeof(uint32) + sizeof("ssh-ed25519") - 1u +
		sizeof(uint32)] ^= 1u;
	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshKexSessionEcdhReplyPrepare(
			&Server,
			&Writer,
			Signature
		) == XSSH_ERROR_AUTHENTICATION) &&
		(Server.WritePending == XSSH_KEX_PACKET_NONE),
		"ssh KEX accepted invalid server signature");
	arrSignature[sizeof(uint32) + sizeof("ssh-ed25519") - 1u +
		sizeof(uint32)] ^= 1u;
	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshKexSessionEcdhReplyPrepare(
			&Server,
			&Writer,
			Signature
		) == XSSH_OK), "ssh KEX server reply prepare failed");
	iWireSize = testSshKexSessionCoreWrite(
		&ServerCore,
		(xbytesview){ arrPayload, Writer.Size },
		arrWire,
		sizeof(arrWire),
		2u,
		&iPadding
	);
	testRequire((xrtSshKexSessionWriteCommit(
		&Server,
		&ServerCore
	) == XSSH_OK), "ssh KEX server reply commit failed");
	Packet = testSshKexSessionCoreReadPrepare(
		&ClientCore,
		arrWire,
		iWireSize,
		arrPlain,
		sizeof(arrPlain),
		2u
	);
	iHostKeySize = 0u;
	testRequire((xrtSshKexSessionReadPrepare(
		&Client,
		&ClientCore,
		Packet.Payload,
		arrClientHostKey,
		1u,
		&iHostKeySize
	) == XSSH_ERROR_SPACE) && (iHostKeySize == HostKey.Size) &&
		(Client.ReadPending == XSSH_KEX_PACKET_NONE),
		"ssh KEX host key space probe failed");
	testRequire((xrtSshKexSessionReadPrepare(
		&Client,
		&ClientCore,
		Packet.Payload,
		arrClientHostKey,
		sizeof(arrClientHostKey),
		&iHostKeySize
	) == XSSH_OK), "ssh KEX client reply parse failed");
	testSshKexSessionCoreReadCommit(&ClientCore, 2u);
	testRequire((xrtSshKexSessionReadCommit(
		&Client,
		&ClientCore
	) == XSSH_OK) && (xrtSshKexSessionHostKey(
		&Client,
		&Hash
	) == XSSH_OK) && testSshBytesEqual(Hash, HostKey) &&
		(xrtSshKexSessionExchangeHash(&Client, &Hash) == XSSH_OK) &&
		testSshBytesEqual(
			Hash,
			(xbytesview){ g_arrExpectedHash, sizeof(g_arrExpectedHash) }
		) && (xrtSshKexSessionEvent(&Client) ==
		 XSSH_KEX_EVENT_VERIFY_HOST_KEY) &&
		(xrtSshKexSessionHostKeyAccept(&Client) == XSSH_OK),
		"ssh KEX client host trust failed");

	/* server 先发送 NEWKEYS，验证两个方向可以独立切换。 */
	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshKexSessionNewKeysPrepare(
			&Server,
			&Writer
		) == XSSH_OK), "ssh KEX server NEWKEYS prepare failed");
	iWireSize = testSshKexSessionCoreWrite(
		&ServerCore,
		(xbytesview){ arrPayload, Writer.Size },
		arrWire,
		sizeof(arrWire),
		3u,
		&iPadding
	);
	testRequire((xrtSshKexSessionWriteCommit(
		&Server,
		&ServerCore
	) == XSSH_OK) && (xrtSshKexSessionActivateWrite(
		&Server,
		&ServerCore,
		3u
	) == XSSH_OK), "ssh KEX server write key activation failed");
	Packet = testSshKexSessionCoreReadPrepare(
		&ClientCore,
		arrWire,
		iWireSize,
		arrPlain,
		sizeof(arrPlain),
		3u
	);
	testRequire((xrtSshKexSessionReadPrepare(
		&Client,
		&ClientCore,
		Packet.Payload,
		NULL,
		0u,
		NULL
	) == XSSH_OK), "ssh KEX client NEWKEYS parse failed");
	testSshKexSessionCoreReadCommit(&ClientCore, 3u);
	testRequire((xrtSshKexSessionReadCommit(
		&Client,
		&ClientCore
	) == XSSH_OK) && (xrtSshKexSessionActivateRead(
		&Client,
		&ClientCore,
		3u
	) == XSSH_OK), "ssh KEX client read key activation failed");

	/* client NEWKEYS 仍由旧写方向编码，提交后再切换新写密钥。 */
	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshKexSessionNewKeysPrepare(
			&Client,
			&Writer
		) == XSSH_OK), "ssh KEX client NEWKEYS prepare failed");
	iWireSize = testSshKexSessionCoreWrite(
		&ClientCore,
		(xbytesview){ arrPayload, Writer.Size },
		arrWire,
		sizeof(arrWire),
		4u,
		&iPadding
	);
	testRequire((xrtSshKexSessionWriteCommit(
		&Client,
		&ClientCore
	) == XSSH_OK) && (xrtSshKexSessionActivateWrite(
		&Client,
		&ClientCore,
		4u
	) == XSSH_OK), "ssh KEX client write key activation failed");
	Packet = testSshKexSessionCoreReadPrepare(
		&ServerCore,
		arrWire,
		iWireSize,
		arrPlain,
		sizeof(arrPlain),
		4u
	);
	testRequire((xrtSshKexSessionReadPrepare(
		&Server,
		&ServerCore,
		Packet.Payload,
		NULL,
		0u,
		NULL
	) == XSSH_OK), "ssh KEX server NEWKEYS parse failed");
	testSshKexSessionCoreReadCommit(&ServerCore, 4u);
	testRequire((xrtSshKexSessionReadCommit(
		&Server,
		&ServerCore
	) == XSSH_OK) && (xrtSshKexSessionActivateRead(
		&Server,
		&ServerCore,
		4u
	) == XSSH_OK) && xrtSshKexSessionComplete(
		&Client,
		&ClientCore
	) && xrtSshKexSessionComplete(
		&Server,
		&ServerCore
	) && (xrtSshKexSessionId(
		&Client,
		&ClientSessionId
	) == XSSH_OK) && (xrtSshKexSessionId(
		&Server,
		&ServerSessionId
	) == XSSH_OK) && testSshBytesEqual(
		ClientSessionId,
		ServerSessionId
	), "ssh KEX completion mismatch");
	memcpy(
		arrInitialSessionId,
		ClientSessionId.Data,
		sizeof(arrInitialSessionId)
	);

	/* 新 AES-GCM 数据面必须能在双端直接互通。 */
	iWireSize = testSshKexSessionCoreWrite(
		&ClientCore,
		(xbytesview){ arrApplication, sizeof(arrApplication) },
		arrWire,
		sizeof(arrWire),
		5u,
		&iPadding
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
		5u
	) == XSSH_OK) && testSshBytesEqual(
		Packet.Payload,
		(xbytesview){ arrApplication, sizeof(arrApplication) }
	) && (xrtSshTransportCoreReadCommit(
		&ServerCore,
		5u,
		&Decision
	) == XSSH_OK), "ssh KEX encrypted data path failed");

	/* KEXINIT 本身必须由旧密钥保护，并把 core 重新推进到交换阶段。 */
	ClientKex = testSshKexSessionKexInit(
		XSSH_ROLE_CLIENT,
		false,
		0x40u,
		arrClientKex,
		sizeof(arrClientKex)
	);
	ServerKex = testSshKexSessionKexInit(
		XSSH_ROLE_SERVER,
		false,
		0x50u,
		arrServerKex,
		sizeof(arrServerKex)
	);
	testRequire((xrtSshKexTranscriptInit(
		&Borrowed,
		(xbytesview){ arrClientVersion, sizeof(arrClientVersion) - 1u },
		(xbytesview){ arrServerVersion, sizeof(arrServerVersion) - 1u },
		ClientKex,
		ServerKex
	) == XSSH_OK) && (xrtSshKexTranscriptMeasure(
		&Borrowed,
		&iTranscriptSize
	) == XSSH_OK) && (iTranscriptSize == 580u) &&
		xrtSshWriterInit(&Writer, arrTranscript, sizeof(arrTranscript)) &&
		(xrtSshKexTranscriptWrite(
			&Writer,
			&Borrowed,
			&Transcript
		) == XSSH_OK) && (Writer.Size == iTranscriptSize),
		"ssh rekey transcript copy failed");
	testSshKexSessionCoreTransfer(
		&ClientCore,
		&ServerCore,
		ClientKex,
		6u,
		&iPadding
	);
	testSshKexSessionCoreTransfer(
		&ServerCore,
		&ClientCore,
		ServerKex,
		7u,
		&iPadding
	);
	testRequire((xrtSshKexSessionBeginWithPrivate(
		&Client,
		&ClientCore,
		&Transcript,
		(xbytesview){ NULL, 0u },
		(xbytesview){
			g_arrClientRekeyPrivate,
			sizeof(g_arrClientRekeyPrivate)
		}
	) == XSSH_OK) && (xrtSshKexSessionBeginWithPrivate(
		&Server,
		&ServerCore,
		&Transcript,
		HostKey,
		(xbytesview){
			g_arrServerRekeyPrivate,
			sizeof(g_arrServerRekeyPrivate)
		}
	) == XSSH_OK), "ssh rekey session begin failed");

	/* 第二轮方法交换使用新的临时密钥，并验证固定 exchange hash。 */
	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshKexSessionEcdhInitPrepare(
			&Client,
			&Writer
		) == XSSH_OK), "ssh rekey client init prepare failed");
	testSshKexSessionTransfer(
		&Client,
		&ClientCore,
		&Server,
		&ServerCore,
		(xbytesview){ arrPayload, Writer.Size },
		NULL,
		0u,
		8u,
		&iPadding
	);
	testRequire((xrtSshKexSessionExchangeHash(
		&Server,
		&Hash
	) == XSSH_OK), "ssh rekey server hash calculation failed");
	testRequire(testSshBytesEqual(
		Hash,
		(xbytesview){
			g_arrExpectedRekeyHash,
			sizeof(g_arrExpectedRekeyHash)
		}
	), "ssh rekey server hash mismatch");
	testSshKexSessionHostMaterial(
		arrHostKey,
		sizeof(arrHostKey),
		&HostKey,
		(xbytesview){
			g_arrHostRekeySignature,
			sizeof(g_arrHostRekeySignature)
		},
		arrSignature,
		sizeof(arrSignature),
		&Signature
	);
	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshKexSessionEcdhReplyPrepare(
			&Server,
			&Writer,
			Signature
		) == XSSH_OK), "ssh rekey server reply prepare failed");
	testSshKexSessionTransfer(
		&Server,
		&ServerCore,
		&Client,
		&ClientCore,
		(xbytesview){ arrPayload, Writer.Size },
		arrClientHostKey,
		sizeof(arrClientHostKey),
		9u,
		&iPadding
	);
	testRequire((xrtSshKexSessionEvent(&Client) ==
		 XSSH_KEX_EVENT_VERIFY_HOST_KEY) &&
		(xrtSshKexSessionHostKeyAccept(&Client) == XSSH_OK),
		"ssh rekey client host trust failed");

	/* 双向 NEWKEYS 再次独立切换，避免单向切换掩盖状态错误。 */
	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshKexSessionNewKeysPrepare(
			&Server,
			&Writer
		) == XSSH_OK), "ssh rekey server NEWKEYS prepare failed");
	testSshKexSessionTransfer(
		&Server,
		&ServerCore,
		&Client,
		&ClientCore,
		(xbytesview){ arrPayload, Writer.Size },
		NULL,
		0u,
		10u,
		&iPadding
	);
	testRequire((xrtSshKexSessionActivateWrite(
		&Server,
		&ServerCore,
		10u
	) == XSSH_OK) && (xrtSshKexSessionActivateRead(
		&Client,
		&ClientCore,
		10u
	) == XSSH_OK), "ssh rekey server direction activation failed");
	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshKexSessionNewKeysPrepare(
			&Client,
			&Writer
		) == XSSH_OK), "ssh rekey client NEWKEYS prepare failed");
	testSshKexSessionTransfer(
		&Client,
		&ClientCore,
		&Server,
		&ServerCore,
		(xbytesview){ arrPayload, Writer.Size },
		NULL,
		0u,
		11u,
		&iPadding
	);
	testRequire((xrtSshKexSessionActivateWrite(
		&Client,
		&ClientCore,
		11u
	) == XSSH_OK) && (xrtSshKexSessionActivateRead(
		&Server,
		&ServerCore,
		11u
	) == XSSH_OK), "ssh rekey client direction activation failed");

	/* SessionId 保留首轮哈希，当前 exchange hash 和数据面使用第二轮结果。 */
	testRequire(xrtSshKexSessionComplete(&Client, &ClientCore) &&
		xrtSshKexSessionComplete(&Server, &ServerCore) &&
		(ClientCore.State.KexCount == 2u) &&
		(ServerCore.State.KexCount == 2u) &&
		(xrtSshKexSessionId(&Client, &ClientSessionId) == XSSH_OK) &&
		testSshBytesEqual(
			ClientSessionId,
			(xbytesview){
				arrInitialSessionId,
				sizeof(arrInitialSessionId)
			}
		) && (xrtSshKexSessionExchangeHash(
			&Client,
			&Hash
		) == XSSH_OK) && testSshBytesEqual(
			Hash,
			(xbytesview){
				g_arrExpectedRekeyHash,
				sizeof(g_arrExpectedRekeyHash)
			}
		), "ssh rekey completion mismatch");
	testSshKexSessionCoreTransfer(
		&ServerCore,
		&ClientCore,
		(xbytesview){ arrApplication, sizeof(arrApplication) },
		12u,
		&iPadding
	);

	printf("kex-session=%zu transcript=%zu session-id=%zu\n",
		sizeof(xsshkexsession), iTranscriptSize, ClientSessionId.Size);
	xrtSshKexSessionClear(&Client);
	xrtSshKexSessionClear(&Server);
	xrtSshTransportCoreClear(&ClientCore);
	xrtSshTransportCoreClear(&ServerCore);
}



/* 运行确定性双端 KEX 会话测试。 */
int main(void)
{
	testSshKexSessionInitial();
	return 0;
}
