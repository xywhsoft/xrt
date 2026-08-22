#include "../test.h"



static const uint8 g_arrExchangeClientPrivate[32] = {
	0x01u, 0x02u, 0x03u, 0x04u, 0x05u, 0x06u, 0x07u, 0x08u,
	0x09u, 0x0au, 0x0bu, 0x0cu, 0x0du, 0x0eu, 0x0fu, 0x10u,
	0x11u, 0x12u, 0x13u, 0x14u, 0x15u, 0x16u, 0x17u, 0x18u,
	0x19u, 0x1au, 0x1bu, 0x1cu, 0x1du, 0x1eu, 0x1fu, 0x20u
};



static const uint8 g_arrExchangeServerPrivate[32] = {
	0x21u, 0x22u, 0x23u, 0x24u, 0x25u, 0x26u, 0x27u, 0x28u,
	0x29u, 0x2au, 0x2bu, 0x2cu, 0x2du, 0x2eu, 0x2fu, 0x30u,
	0x31u, 0x32u, 0x33u, 0x34u, 0x35u, 0x36u, 0x37u, 0x38u,
	0x39u, 0x3au, 0x3bu, 0x3cu, 0x3du, 0x3eu, 0x3fu, 0x40u
};



static const uint8 g_arrExchangeHostPublic[32] = {
	0xdau, 0x29u, 0xe9u, 0x5bu, 0x02u, 0xe0u, 0x0fu, 0xfau,
	0x15u, 0x64u, 0x57u, 0x75u, 0xfbu, 0x1du, 0x2bu, 0xa2u,
	0x22u, 0xa1u, 0x94u, 0x33u, 0x95u, 0xeeu, 0xa0u, 0x6bu,
	0x94u, 0xe2u, 0xc0u, 0x57u, 0xb7u, 0xbeu, 0x69u, 0xd0u
};



/* 固定 padding 使 packet 事务可重复。 */
static bool testSshKexExchangePadding(
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



/* 构建固定 cookie 的初始 KEXINIT。 */
static xbytesview testSshKexExchangeKexInit(
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
		) == XSSH_OK), "ssh KEX exchange KEXINIT build failed");
	return (xbytesview){ (const unsigned char*)pOutput, Writer.Size };
}



/* 分别保存 local/peer 版本，并验证中止和提交先后约束。 */
static void testSshKexExchangeVersions(
	xsshkexexchange* pExchange,
	xsshtransportcore* pCore,
	xstrview Local,
	xstrview Peer
)
{
	testRequire((xrtSshKexExchangeVersionPrepare(
		pExchange,
		pCore,
		XSSH_TRANSPORT_LOCAL,
		Local
	) == XSSH_OK) && (xrtSshKexExchangeVersionAbort(
		pExchange,
		pCore
	) == XSSH_OK) && (xrtSshKexExchangeVersionPrepare(
		pExchange,
		pCore,
		XSSH_TRANSPORT_LOCAL,
		Local
	) == XSSH_OK) && (xrtSshKexExchangeVersionCommit(
		pExchange,
		pCore
	) == XSSH_ERROR_STATE) && (xrtSshTransportCoreIdentificationCommit(
		pCore,
		XSSH_TRANSPORT_LOCAL
	) == XSSH_OK) && (xrtSshKexExchangeVersionCommit(
		pExchange,
		pCore
	) == XSSH_OK), "ssh KEX exchange local version transaction failed");
	testRequire((xrtSshKexExchangeVersionPrepare(
		pExchange,
		pCore,
		XSSH_TRANSPORT_PEER,
		Peer
	) == XSSH_OK) && (xrtSshTransportCoreIdentificationCommit(
		pCore,
		XSSH_TRANSPORT_PEER
	) == XSSH_OK) && (xrtSshKexExchangeVersionCommit(
		pExchange,
		pCore
	) == XSSH_OK) &&
		(pExchange->Phase == XSSH_KEX_EXCHANGE_KEXINIT),
		"ssh KEX exchange peer version transaction failed");
}



/* 通过真实双端 packet core 保存同一条 KEXINIT。 */
static void testSshKexExchangeTransfer(
	xsshkexexchange* pSenderExchange,
	xsshtransportcore* pSenderCore,
	xsshkexexchange* pReceiverExchange,
	xsshtransportcore* pReceiverCore,
	xbytesview Payload,
	bool bTestAbort,
	uint8* pPadding
)
{
	unsigned char arrWire[1024];
	unsigned char arrPlain[1024];
	xsshrekeydecision Decision;
	xsshpacketview Packet;
	xsshwriter Writer;
	xsshreader Reader;

	testRequire((xrtSshKexExchangeKexInitPrepare(
			pSenderExchange,
			pSenderCore,
			XSSH_TRANSPORT_LOCAL,
			Payload
		) == XSSH_OK) && xrtSshWriterInit(
		&Writer,
		arrWire,
		sizeof(arrWire)
	) && (xrtSshTransportCoreWritePrepareWithPadding(
		pSenderCore,
		&Writer,
		Payload,
		testSshKexExchangePadding,
		pPadding,
		0u
	) == XSSH_OK), "ssh KEX exchange sender prepare failed");
	if ( bTestAbort ) {
		testRequire((xrtSshKexExchangeKexInitAbort(
			pSenderExchange,
			pSenderCore
		) == XSSH_OK) && (xrtSshTransportCoreWriteAbort(
			pSenderCore
		) == XSSH_OK) && xrtSshWriterInit(
			&Writer,
			arrWire,
			sizeof(arrWire)
		) && (xrtSshKexExchangeKexInitPrepare(
			pSenderExchange,
			pSenderCore,
			XSSH_TRANSPORT_LOCAL,
			Payload
		) == XSSH_OK) && (xrtSshTransportCoreWritePrepareWithPadding(
			pSenderCore,
			&Writer,
			Payload,
			testSshKexExchangePadding,
			pPadding,
			0u
		) == XSSH_OK), "ssh KEX exchange sender retry failed");
	}
	testRequire((xrtSshKexExchangeKexInitCommit(
		pSenderExchange,
		pSenderCore
	) == XSSH_ERROR_STATE) && (xrtSshTransportCoreWriteCommit(
		pSenderCore,
		0u,
		&Decision
	) == XSSH_OK) && (xrtSshKexExchangeKexInitCommit(
		pSenderExchange,
		pSenderCore
	) == XSSH_OK), "ssh KEX exchange sender commit failed");
	testRequire(xrtSshReaderInit(
		&Reader,
		(xbytesview){ arrWire, Writer.Size }
	) && (xrtSshTransportCoreReadPrepare(
		pReceiverCore,
		&Reader,
		&Packet,
		arrPlain,
		sizeof(arrPlain),
		0u
	) == XSSH_OK) && (xrtSshKexExchangeKexInitPrepare(
		pReceiverExchange,
		pReceiverCore,
		XSSH_TRANSPORT_PEER,
		Packet.Payload
	) == XSSH_OK) && (xrtSshTransportCoreReadCommit(
		pReceiverCore,
		0u,
		&Decision
	) == XSSH_OK) && (xrtSshKexExchangeKexInitCommit(
		pReceiverExchange,
		pReceiverCore
	) == XSSH_OK), "ssh KEX exchange receiver commit failed");
}



/* 验证 wire 层兼容的最长 LF identification 也能进入 transcript。 */
static void testSshKexExchangeLongVersion(void)
{
	unsigned char arrLong[XSSH_IDENTIFICATION_MAX - 1u];
	unsigned char iKex = XSSH_MSG_KEXINIT;
	xsshkextranscript Transcript;

	memset(arrLong, 'x', sizeof(arrLong));
	memcpy(arrLong, "SSH-2.0-", 8u);
	testRequire(xrtSshKexTranscriptInit(
		&Transcript,
		(xbytesview){ arrLong, sizeof(arrLong) },
		XRT_BYTES_LITERAL("SSH-2.0-xssh_server"),
		(xbytesview){ &iKex, 1u },
		(xbytesview){ &iKex, 1u }
	) == XSSH_OK, "ssh KEX transcript rejected accepted LF banner limit");
}



/* 验证动态保存、双端提交关联、晋升和释放。 */
int main(void)
{
	static const char sClientVersion[] = "SSH-2.0-xssh_client";
	static const char sServerVersion[] = "SSH-2.0-xssh_server";
	unsigned char arrClientKex[512];
	unsigned char arrServerKex[512];
	unsigned char arrHostKey[128];
	xnetbufpoolinfo PoolInfo;
	xnetbufpool* pPool;
	xbytesview ClientKex;
	xbytesview ServerKex;
	xbytesview HostKey;
	xsshkextranscript ClientTranscript;
	xsshkextranscript ServerTranscript;
	xsshtransportcore ClientCore;
	xsshtransportcore ServerCore;
	xsshkexexchange Client;
	xsshkexexchange Server;
	xsshwriter Writer;
	uint8 iPadding = 0x40u;

	testSshKexExchangeLongVersion();
	pPool = xrtNetBufPoolCreate(NULL);
	testRequire((pPool != NULL) && xrtSshTransportCoreInit(
		&ClientCore,
		XSSH_ROLE_CLIENT,
		0u,
		NULL,
		0u
	) && xrtSshTransportCoreInit(
		&ServerCore,
		XSSH_ROLE_SERVER,
		0u,
		NULL,
		0u
	) && xrtSshKexExchangeInit(
		&Client,
		pPool,
		XSSH_ROLE_CLIENT
	) && xrtSshKexExchangeInit(
		&Server,
		pPool,
		XSSH_ROLE_SERVER
	), "ssh KEX exchange initialization failed");
	testSshKexExchangeVersions(
		&Client,
		&ClientCore,
		XRT_STR_LITERAL(sClientVersion),
		XRT_STR_LITERAL(sServerVersion)
	);
	testSshKexExchangeVersions(
		&Server,
		&ServerCore,
		XRT_STR_LITERAL(sServerVersion),
		XRT_STR_LITERAL(sClientVersion)
	);
	ClientKex = testSshKexExchangeKexInit(
		XSSH_ROLE_CLIENT,
		0x00u,
		arrClientKex,
		sizeof(arrClientKex)
	);
	ServerKex = testSshKexExchangeKexInit(
		XSSH_ROLE_SERVER,
		0x10u,
		arrServerKex,
		sizeof(arrServerKex)
	);
	testSshKexExchangeTransfer(
		&Client,
		&ClientCore,
		&Server,
		&ServerCore,
		ClientKex,
		true,
		&iPadding
	);
	testSshKexExchangeTransfer(
		&Server,
		&ServerCore,
		&Client,
		&ClientCore,
		ServerKex,
		false,
		&iPadding
	);
	testRequire(xrtSshKexExchangeReady(&Client, &ClientCore) &&
		xrtSshKexExchangeReady(&Server, &ServerCore) &&
		(xrtSshKexExchangeTranscript(
			&Client,
			&ClientTranscript
		) == XSSH_OK) && (xrtSshKexExchangeTranscript(
			&Server,
			&ServerTranscript
		) == XSSH_OK) && testSshBytesEqual(
		ClientTranscript.ClientKexInit,
		ClientKex
	) && testSshBytesEqual(
		ClientTranscript.ServerKexInit,
		ServerKex
	) && testSshBytesEqual(
		ClientTranscript.ClientVersion,
		XRT_BYTES_LITERAL(sClientVersion)
	) && testSshBytesEqual(
		ServerTranscript.ServerVersion,
		XRT_BYTES_LITERAL(sServerVersion)
	), "ssh KEX exchange transcript mismatch");
	testRequire(xrtSshWriterInit(
		&Writer,
		arrHostKey,
		sizeof(arrHostKey)
	) && (xrtSshEd25519PublicKeyWrite(
		&Writer,
		(xbytesview){
			g_arrExchangeHostPublic,
			sizeof(g_arrExchangeHostPublic)
		}
	) == XSSH_OK), "ssh KEX exchange host key build failed");
	HostKey = (xbytesview){ arrHostKey, Writer.Size };
	testRequire((xrtSshKexExchangeBeginWithPrivate(
		&Client,
		&ClientCore,
		(xbytesview){ NULL, 0u },
		(xbytesview){
			g_arrExchangeClientPrivate,
			sizeof(g_arrExchangeClientPrivate)
		}
	) == XSSH_OK) && (xrtSshKexExchangeBeginWithPrivate(
		&Server,
		&ServerCore,
		HostKey,
		(xbytesview){
			g_arrExchangeServerPrivate,
			sizeof(g_arrExchangeServerPrivate)
		}
	) == XSSH_OK) &&
		(Client.Phase == XSSH_KEX_EXCHANGE_METHOD) &&
		(Server.Phase == XSSH_KEX_EXCHANGE_METHOD) &&
		(xrtSshKexExchangeSession(&Client) != NULL) &&
		(xrtSshKexExchangeSessionConst(&Server) != NULL) &&
		xrtNetBufEmpty(&Client.NextClientKexInit) &&
		xrtNetBufEmpty(&Client.NextServerKexInit) &&
		!xrtNetBufEmpty(&Client.ClientKexInit) &&
		!xrtNetBufEmpty(&Client.ServerKexInit) &&
		(xrtSshKexExchangeTranscript(
			&Client,
			&ClientTranscript
		) == XSSH_OK) && testSshBytesEqual(
		ClientTranscript.ClientKexInit,
		ClientKex
	), "ssh KEX exchange generation promotion failed");

	xrtSshKexExchangeClear(&Client);
	xrtSshKexExchangeClear(&Server);
	xrtSshTransportCoreClear(&ClientCore);
	xrtSshTransportCoreClear(&ServerCore);
	xrtNetBufPoolGet(pPool, &PoolInfo);
	testRequire((PoolInfo.LiveBlocks == 0u) &&
		xrtNetBufPoolDestroy(pPool),
		"ssh KEX exchange leaked dynamic transcript blocks");
	puts("ssh dynamic KEX exchange tests passed");
	return 0;
}
