#include "../test.h"



/* 固定临时密钥与签名使动态主机公钥读取测试可重复。 */
static const uint8 g_arrReaderClientPrivate[32] = {
	0x01u, 0x02u, 0x03u, 0x04u, 0x05u, 0x06u, 0x07u, 0x08u,
	0x09u, 0x0au, 0x0bu, 0x0cu, 0x0du, 0x0eu, 0x0fu, 0x10u,
	0x11u, 0x12u, 0x13u, 0x14u, 0x15u, 0x16u, 0x17u, 0x18u,
	0x19u, 0x1au, 0x1bu, 0x1cu, 0x1du, 0x1eu, 0x1fu, 0x20u
};



static const uint8 g_arrReaderServerPrivate[32] = {
	0x21u, 0x22u, 0x23u, 0x24u, 0x25u, 0x26u, 0x27u, 0x28u,
	0x29u, 0x2au, 0x2bu, 0x2cu, 0x2du, 0x2eu, 0x2fu, 0x30u,
	0x31u, 0x32u, 0x33u, 0x34u, 0x35u, 0x36u, 0x37u, 0x38u,
	0x39u, 0x3au, 0x3bu, 0x3cu, 0x3du, 0x3eu, 0x3fu, 0x40u
};



static const uint8 g_arrReaderHostPublic[32] = {
	0xdau, 0x29u, 0xe9u, 0x5bu, 0x02u, 0xe0u, 0x0fu, 0xfau,
	0x15u, 0x64u, 0x57u, 0x75u, 0xfbu, 0x1du, 0x2bu, 0xa2u,
	0x22u, 0xa1u, 0x94u, 0x33u, 0x95u, 0xeeu, 0xa0u, 0x6bu,
	0x94u, 0xe2u, 0xc0u, 0x57u, 0xb7u, 0xbeu, 0x69u, 0xd0u
};



static const uint8 g_arrReaderHostSignature[64] = {
	0x0eu, 0x25u, 0xafu, 0x29u, 0xc6u, 0x7cu, 0xc8u, 0x0du,
	0x3cu, 0xbau, 0x87u, 0x63u, 0x53u, 0x93u, 0x67u, 0x25u,
	0x03u, 0xf7u, 0x55u, 0x9bu, 0x8bu, 0x6bu, 0xb2u, 0x20u,
	0x3cu, 0xb3u, 0x6eu, 0x4au, 0x14u, 0xe9u, 0xbcu, 0x08u,
	0xf5u, 0x9fu, 0x50u, 0x8bu, 0xbcu, 0xb2u, 0x6bu, 0x3cu,
	0xa8u, 0xdfu, 0x60u, 0x35u, 0x0fu, 0x77u, 0xe5u, 0xf6u,
	0x91u, 0xfeu, 0x92u, 0xccu, 0xf4u, 0xedu, 0xf2u, 0x49u,
	0xf4u, 0xe7u, 0xfcu, 0x49u, 0x41u, 0x30u, 0xf8u, 0x05u
};



/* 固定 packet padding 保持双方 packet codec 序列完全可预测。 */
static bool testSshSessionReaderPadding(
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
static xbytesview testSshSessionReaderPayload(
	const xsshwriter* pWriter,
	const void* pData
)
{
	return (xbytesview){ (const unsigned char*)pData, pWriter->Size };
}



/* 构建与固定签名向量一致的初始 KEXINIT。 */
static xbytesview testSshSessionReaderKexInit(
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
		) == XSSH_OK), "ssh reader KEXINIT build failed");
	return testSshSessionReaderPayload(&Writer, pOutput);
}



/* 构建固定 Ed25519 主机公钥和签名 blob。 */
static void testSshSessionReaderHostMaterial(
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
			(xbytesview){
				g_arrReaderHostPublic,
				sizeof(g_arrReaderHostPublic)
			}
		) == XSSH_OK), "ssh reader host key build failed");
	*pHostKeyView = testSshSessionReaderPayload(&Writer, pHostKey);
	testRequire(xrtSshWriterInit(&Writer, pSignature, iSignatureCapacity) &&
		(xrtSshEd25519SignatureWrite(
			&Writer,
			(xbytesview){
				g_arrReaderHostSignature,
				sizeof(g_arrReaderHostSignature)
			}
		) == XSSH_OK), "ssh reader signature build failed");
	*pSignatureView = testSshSessionReaderPayload(&Writer, pSignature);
}



/* 不经过 TCP 线路保存双方版本，用于建立精确 KEX transcript。 */
static void testSshSessionReaderVersions(
	xsshsessiontcp* pSession,
	xstrview Local,
	xstrview Peer
)
{
	xsshtransportcore* pCore = &pSession->Transport.Core;

	testRequire((xrtSshSessionCoreVersionPrepare(
		&pSession->Session,
		pCore,
		XSSH_TRANSPORT_LOCAL,
		Local
	) == XSSH_OK) && (xrtSshTransportCoreIdentificationCommit(
		pCore,
		XSSH_TRANSPORT_LOCAL
	) == XSSH_OK) && (xrtSshSessionCoreVersionCommit(
		&pSession->Session,
		pCore
	) == XSSH_OK) && (xrtSshSessionCoreVersionPrepare(
		&pSession->Session,
		pCore,
		XSSH_TRANSPORT_PEER,
		Peer
	) == XSSH_OK) && (xrtSshTransportCoreIdentificationCommit(
		pCore,
		XSSH_TRANSPORT_PEER
	) == XSSH_OK) && (xrtSshSessionCoreVersionCommit(
		&pSession->Session,
		pCore
	) == XSSH_OK), "ssh reader versions failed");
}



/* 把一个 payload 通过双方无缓冲核心可靠传递。 */
static void testSshSessionReaderTransfer(
	xsshsessiontcp* pSender,
	xsshsessiontcp* pReceiver,
	xbytesview Payload,
	uint8* pPadding
)
{
	unsigned char arrWire[2048];
	unsigned char arrPlain[1024];
	xsshrekeydecision Decision;
	xsshsessionpacket SessionPacket;
	xsshsessionpacketkind Kind;
	xsshpacketview Packet;
	xsshwriter Writer;
	xsshreader Reader;

	testRequire((xrtSshSessionCoreWritePrepare(
		&pSender->Session,
		&pSender->Transport.Core,
		Payload,
		NULL,
		NULL,
		0u,
		0u,
		&Kind
	) == XSSH_OK) && xrtSshWriterInit(
		&Writer,
		arrWire,
		sizeof(arrWire)
	) && (xrtSshTransportCoreWritePrepareWithPadding(
		&pSender->Transport.Core,
		&Writer,
		Payload,
		testSshSessionReaderPadding,
		pPadding,
		0u
	) == XSSH_OK) && (xrtSshSessionCoreWriteBind(
		&pSender->Session,
		&pSender->Transport.Core,
		Payload
	) == XSSH_OK) && (xrtSshTransportCoreWriteCommit(
		&pSender->Transport.Core,
		0u,
		&Decision
	) == XSSH_OK) && (xrtSshSessionCoreWriteCommit(
		&pSender->Session,
		&pSender->Transport.Core,
		0u
	) == XSSH_OK), "ssh reader sender transfer failed");
	testRequire(xrtSshReaderInit(
		&Reader,
		(xbytesview){ arrWire, Writer.Size }
	) && (xrtSshTransportCoreReadPrepare(
		&pReceiver->Transport.Core,
		&Reader,
		&Packet,
		arrPlain,
		sizeof(arrPlain),
		0u
	) == XSSH_OK) && (xrtSshSessionCoreReadPrepare(
		&pReceiver->Session,
		&pReceiver->Transport.Core,
		Packet.Payload,
		NULL,
		0u,
		NULL,
		0u,
		&SessionPacket
	) == XSSH_OK) && (SessionPacket.Kind == Kind) &&
		(xrtSshTransportCoreReadCommit(
			&pReceiver->Transport.Core,
			0u,
			&Decision
		) == XSSH_OK) && (xrtSshSessionCoreReadCommit(
		&pReceiver->Session,
		&pReceiver->Transport.Core,
		0u
	) == XSSH_OK), "ssh reader receiver transfer failed");
}



/* 把任意合法 payload 可靠提交为线路 packet，但不交给对端解析。 */
static size_t testSshSessionReaderPacketWire(
	xsshsessiontcp* pSender,
	xbytesview Payload,
	uint8* pPadding,
	void* pOutput,
	size_t iCapacity
)
{
	xsshrekeydecision Decision;
	xsshsessionpacketkind Kind;
	xsshwriter WireWriter;

	testRequire((xrtSshSessionCoreWritePrepare(
		&pSender->Session,
		&pSender->Transport.Core,
		Payload,
		NULL,
		NULL,
		0u,
		0u,
		&Kind
	) == XSSH_OK) &&
		xrtSshWriterInit(&WireWriter, pOutput, iCapacity) &&
		(xrtSshTransportCoreWritePrepareWithPadding(
			&pSender->Transport.Core,
			&WireWriter,
			Payload,
			testSshSessionReaderPadding,
			pPadding,
			0u
		) == XSSH_OK) && (xrtSshSessionCoreWriteBind(
		&pSender->Session,
		&pSender->Transport.Core,
		Payload
	) == XSSH_OK) && (xrtSshTransportCoreWriteCommit(
		&pSender->Transport.Core,
		0u,
		&Decision
	) == XSSH_OK) && (xrtSshSessionCoreWriteCommit(
		&pSender->Session,
		&pSender->Transport.Core,
		0u
	) == XSSH_OK), "ssh reader packet wire failed");
	return WireWriter.Size;
}



/* 构建并可靠提交服务端 ECDH_REPLY，只把线路字节交给动态读取器。 */
static size_t testSshSessionReaderReplyWire(
	xsshsessiontcp* pServer,
	xbytesview Signature,
	uint8* pPadding,
	void* pOutput,
	size_t iCapacity
)
{
	unsigned char arrPayload[512];
	xsshkexsession* pKex;
	xbytesview Payload;
	xsshwriter Writer;

	pKex = xrtSshKexExchangeSession(
		xrtSshSessionCoreKex(&pServer->Session)
	);
	testRequire((pKex != NULL) && xrtSshWriterInit(
		&Writer,
		arrPayload,
		sizeof(arrPayload)
	) && (xrtSshKexSessionEcdhReplyPrepare(
		pKex,
		&Writer,
		Signature
	) == XSSH_OK), "ssh reader ECDH reply build failed");
	Payload = testSshSessionReaderPayload(&Writer, arrPayload);
	return testSshSessionReaderPacketWire(
		pServer,
		Payload,
		pPadding,
		pOutput,
		iCapacity
	);
}



/* 验证增量线路输入、精确工作区和稳定主机公钥存储。 */
int main(void)
{
	static const char sClientVersion[] = "SSH-2.0-xssh_client";
	static const char sServerVersion[] = "SSH-2.0-xssh_server";
	unsigned char arrClientKex[512];
	unsigned char arrServerKex[512];
	unsigned char arrHostKey[128];
	unsigned char arrSignature[128];
	unsigned char arrPayload[512];
	unsigned char arrWire[2048];
	xnetbufpoolinfo PoolInfo;
	xsshsessiontcpconfig ClientConfig;
	xsshsessiontcpconfig ServerConfig;
	xsshsessiontcppacket Packet;
	xsshsessionreader Reader;
	xsshsessiontcp Client;
	xsshsessiontcp Server;
	xsshrekeydecision Decision;
	xsshkexsession* pClientKex;
	xsshkexsession* pServerKex;
	xbytesview ClientKex;
	xbytesview ServerKex;
	xbytesview HostKey;
	xbytesview Signature;
	xbytesview Payload;
	xbytesview SeenHostKey;
	xnetbufpool* pPool;
	xnetbuf Input;
	xsshcode PrepareCode;
	xsshwriter Writer;
	size_t iWireSize;
	uint8 iPadding = 0x40u;

	pPool = xrtNetBufPoolCreate(NULL);
	testRequire((pPool != NULL) && xrtSshSessionTcpConfigInit(
		&ClientConfig,
		XSSH_ROLE_CLIENT
	) && xrtSshSessionTcpConfigInit(
		&ServerConfig,
		XSSH_ROLE_SERVER
	) && xrtSshSessionTcpInit(
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
		&Reader,
		pPool,
		&Client
	) && xrtNetBufInit(&Input, pPool),
		"ssh reader initialization failed");
	testRequire((sizeof(Reader) < 384u) &&
		(xrtSshSessionReaderSession(&Reader) == &Client) &&
		(xrtSshSessionReaderState(&Reader) == XSSH_SESSION_READER_IDLE) &&
		(xrtSshSessionReaderState(NULL) == XSSH_SESSION_READER_INVALID) &&
		(xrtSshSessionReaderHostKey(&Reader).Size == 0u),
		"ssh reader ownership shape failed");

	testSshSessionReaderVersions(
		&Client,
		XRT_STR_LITERAL(sClientVersion),
		XRT_STR_LITERAL(sServerVersion)
	);
	testSshSessionReaderVersions(
		&Server,
		XRT_STR_LITERAL(sServerVersion),
		XRT_STR_LITERAL(sClientVersion)
	);
	ClientKex = testSshSessionReaderKexInit(
		XSSH_ROLE_CLIENT,
		0x00u,
		arrClientKex,
		sizeof(arrClientKex)
	);
	ServerKex = testSshSessionReaderKexInit(
		XSSH_ROLE_SERVER,
		0x10u,
		arrServerKex,
		sizeof(arrServerKex)
	);
	testSshSessionReaderTransfer(&Client, &Server, ClientKex, &iPadding);
	testSshSessionReaderTransfer(&Server, &Client, ServerKex, &iPadding);
	testSshSessionReaderHostMaterial(
		arrHostKey,
		sizeof(arrHostKey),
		&HostKey,
		arrSignature,
		sizeof(arrSignature),
		&Signature
	);
	testRequire((xrtSshSessionTcpKexBeginWithPrivate(
		&Client,
		(xbytesview){ NULL, 0u },
		(xbytesview){
			g_arrReaderClientPrivate,
			sizeof(g_arrReaderClientPrivate)
		}
	) == XSSH_OK) && (xrtSshSessionTcpKexBeginWithPrivate(
		&Server,
		HostKey,
		(xbytesview){
			g_arrReaderServerPrivate,
			sizeof(g_arrReaderServerPrivate)
		}
	) == XSSH_OK), "ssh reader KEX begin failed");
	pClientKex = xrtSshKexExchangeSession(
		xrtSshSessionCoreKex(&Client.Session)
	);
	pServerKex = xrtSshKexExchangeSession(
		xrtSshSessionCoreKex(&Server.Session)
	);
	testRequire((pClientKex != NULL) && (pServerKex != NULL) && xrtSshWriterInit(
		&Writer,
		arrPayload,
		sizeof(arrPayload)
	) && (xrtSshKexSessionEcdhInitPrepare(
		pClientKex,
		&Writer
	) == XSSH_OK), "ssh reader ECDH init build failed");
	Payload = testSshSessionReaderPayload(&Writer, arrPayload);
	testSshSessionReaderTransfer(&Client, &Server, Payload, &iPadding);
	iWireSize = testSshSessionReaderReplyWire(
		&Server,
		Signature,
		&iPadding,
		arrWire,
		sizeof(arrWire)
	);

	testRequire(xrtNetBufAppend(&Input, arrWire, 3u),
		"ssh reader partial packet append failed");
	PrepareCode = xrtSshSessionReaderPrepare(
		&Reader,
		&Input,
		0u,
		&Packet
	);
	testRequire((PrepareCode == XSSH_NEED_MORE) &&
		(xrtSshSessionReaderState(&Reader) == XSSH_SESSION_READER_IDLE),
		"ssh reader partial packet state failed");
	testRequire(xrtNetBufAppend(
		&Input,
		arrWire + 3u,
		iWireSize - 3u
	), "ssh reader remaining packet append failed");
	PrepareCode = xrtSshSessionReaderPrepare(
		&Reader,
		&Input,
		0u,
		&Packet
	);
	testRequire((PrepareCode == XSSH_OK) &&
		(Packet.Session.Kind == XSSH_SESSION_PACKET_KEX) &&
		(xrtSshSessionReaderState(&Reader) == XSSH_SESSION_READER_READY) &&
		(xrtSshSessionTcpAction(&Client) ==
		 XSSH_SESSION_ACTION_READ_PENDING),
		"ssh reader dynamic packet prepare failed");
	SeenHostKey = xrtSshSessionReaderHostKey(&Reader);
	testRequire(testSshBytesEqual(SeenHostKey, HostKey) &&
		(Reader.PlainSpan.Size >= Reader.Need.PlainSize) &&
		(Reader.HostKeySize == HostKey.Size) &&
		(xrtSshSessionReaderCommit(
			&Reader,
			0u,
			&Decision
		) == XSSH_OK) && (Decision == XSSH_REKEY_NONE) &&
		xrtNetBufEmpty(&Input) &&
		(xrtSshSessionReaderState(&Reader) == XSSH_SESSION_READER_IDLE) &&
		(xrtSshSessionTcpAction(&Client) ==
		 XSSH_SESSION_ACTION_VERIFY_HOST_KEY) &&
		testSshBytesEqual(
			xrtSshSessionReaderHostKey(&Reader),
			HostKey
		), "ssh reader host key commit failed");

	testRequire((xrtSshKexSessionHostKeyAccept(pClientKex) == XSSH_OK) &&
		xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshKexSessionNewKeysPrepare(
			pServerKex,
			&Writer
		) == XSSH_OK), "ssh reader server NEWKEYS build failed");
	Payload = testSshSessionReaderPayload(&Writer, arrPayload);
	testSshSessionReaderTransfer(&Server, &Client, Payload, &iPadding);
	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshKexSessionNewKeysPrepare(
			pClientKex,
			&Writer
		) == XSSH_OK), "ssh reader client NEWKEYS build failed");
	Payload = testSshSessionReaderPayload(&Writer, arrPayload);
	testSshSessionReaderTransfer(&Client, &Server, Payload, &iPadding);
	testRequire((xrtSshSessionCorePhase(
		&Client.Session,
		&Client.Transport.Core
	) == XSSH_SESSION_AUTHENTICATION) && (xrtSshSessionCorePhase(
		&Server.Session,
		&Server.Transport.Core
	) == XSSH_SESSION_AUTHENTICATION),
		"ssh reader NEWKEYS activation failed");

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshIgnoreWrite(
			&Writer,
			XRT_BYTES_LITERAL("dynamic encrypted packet")
		) == XSSH_OK), "ssh reader encrypted control build failed");
	Payload = testSshSessionReaderPayload(&Writer, arrPayload);
	iWireSize = testSshSessionReaderPacketWire(
		&Server,
		Payload,
		&iPadding,
		arrWire,
		sizeof(arrWire)
	);
	testRequire(xrtNetBufAppend(&Input, arrWire, iWireSize) &&
		(xrtSshSessionReaderPrepare(
			&Reader,
			&Input,
			1u,
			&Packet
		) == XSSH_OK) &&
		(Packet.Session.Kind == XSSH_SESSION_PACKET_IGNORE) &&
		(Reader.Need.PlainSize != 0u) &&
		(Reader.Plain.Reserved != NULL) &&
		(Reader.PlainSpan.Size >= Reader.Need.PlainSize) &&
		(xrtSshSessionReaderPrepare(
			&Reader,
			&Input,
			1u,
			&Packet
		) == XSSH_ERROR_STATE) &&
		(xrtSshSessionReaderCommit(
			&Reader,
			1u,
			&Decision
		) == XSSH_OK) && xrtNetBufEmpty(&Input) &&
		(Reader.Plain.Reserved == NULL) &&
		xrtNetBufEmpty(&Reader.Plain) && testSshBytesEqual(
			xrtSshSessionReaderHostKey(&Reader),
			HostKey
		), "ssh reader encrypted packet workspace failed");

	xrtNetBufClear(&Input);
	xrtSshSessionReaderClear(&Reader);
	xrtSshSessionTcpClear(&Client);
	xrtSshSessionTcpClear(&Server);
	xrtNetBufPoolGet(pPool, &PoolInfo);
	testRequire((PoolInfo.LiveBlocks == 0u) &&
		(PoolInfo.LiveBytes == 0u) && xrtNetBufPoolDestroy(pPool),
		"ssh reader dynamic blocks leaked");
	return 0;
}
