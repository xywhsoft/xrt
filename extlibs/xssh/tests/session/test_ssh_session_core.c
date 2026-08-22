#include "../test.h"



/* 固定密钥和签名使连接级状态机测试完全可重复。 */
static const uint8 g_arrSessionClientPrivate[32] = {
	0x01u, 0x02u, 0x03u, 0x04u, 0x05u, 0x06u, 0x07u, 0x08u,
	0x09u, 0x0au, 0x0bu, 0x0cu, 0x0du, 0x0eu, 0x0fu, 0x10u,
	0x11u, 0x12u, 0x13u, 0x14u, 0x15u, 0x16u, 0x17u, 0x18u,
	0x19u, 0x1au, 0x1bu, 0x1cu, 0x1du, 0x1eu, 0x1fu, 0x20u
};



static const uint8 g_arrSessionServerPrivate[32] = {
	0x21u, 0x22u, 0x23u, 0x24u, 0x25u, 0x26u, 0x27u, 0x28u,
	0x29u, 0x2au, 0x2bu, 0x2cu, 0x2du, 0x2eu, 0x2fu, 0x30u,
	0x31u, 0x32u, 0x33u, 0x34u, 0x35u, 0x36u, 0x37u, 0x38u,
	0x39u, 0x3au, 0x3bu, 0x3cu, 0x3du, 0x3eu, 0x3fu, 0x40u
};



static const uint8 g_arrSessionClientRekeyPrivate[32] = {
	0x41u, 0x42u, 0x43u, 0x44u, 0x45u, 0x46u, 0x47u, 0x48u,
	0x49u, 0x4au, 0x4bu, 0x4cu, 0x4du, 0x4eu, 0x4fu, 0x50u,
	0x51u, 0x52u, 0x53u, 0x54u, 0x55u, 0x56u, 0x57u, 0x58u,
	0x59u, 0x5au, 0x5bu, 0x5cu, 0x5du, 0x5eu, 0x5fu, 0x60u
};



static const uint8 g_arrSessionServerRekeyPrivate[32] = {
	0x61u, 0x62u, 0x63u, 0x64u, 0x65u, 0x66u, 0x67u, 0x68u,
	0x69u, 0x6au, 0x6bu, 0x6cu, 0x6du, 0x6eu, 0x6fu, 0x70u,
	0x71u, 0x72u, 0x73u, 0x74u, 0x75u, 0x76u, 0x77u, 0x78u,
	0x79u, 0x7au, 0x7bu, 0x7cu, 0x7du, 0x7eu, 0x7fu, 0x80u
};



static const uint8 g_arrSessionHostPublic[32] = {
	0xdau, 0x29u, 0xe9u, 0x5bu, 0x02u, 0xe0u, 0x0fu, 0xfau,
	0x15u, 0x64u, 0x57u, 0x75u, 0xfbu, 0x1du, 0x2bu, 0xa2u,
	0x22u, 0xa1u, 0x94u, 0x33u, 0x95u, 0xeeu, 0xa0u, 0x6bu,
	0x94u, 0xe2u, 0xc0u, 0x57u, 0xb7u, 0xbeu, 0x69u, 0xd0u
};



static const uint8 g_arrSessionHostRekeySignature[64] = {
	0xc5u, 0x0cu, 0x39u, 0x36u, 0x15u, 0xe2u, 0xbau, 0xd2u,
	0x51u, 0x2du, 0xb4u, 0x6du, 0x1cu, 0x18u, 0xfbu, 0x1du,
	0x05u, 0x7bu, 0x91u, 0x4au, 0x23u, 0x8fu, 0x88u, 0x92u,
	0xe3u, 0x76u, 0xb9u, 0xd1u, 0x21u, 0xc4u, 0x85u, 0xa9u,
	0x67u, 0xd0u, 0x84u, 0x24u, 0xb2u, 0xd3u, 0x60u, 0xd6u,
	0x21u, 0xddu, 0x73u, 0x96u, 0x9du, 0x32u, 0xdeu, 0x8du,
	0x0du, 0xe2u, 0x05u, 0x75u, 0xd4u, 0x3eu, 0x23u, 0x72u,
	0x17u, 0x99u, 0xf1u, 0x8fu, 0x9au, 0x05u, 0xeau, 0x06u
};



static const uint8 g_arrSessionHostSignature[64] = {
	0x0eu, 0x25u, 0xafu, 0x29u, 0xc6u, 0x7cu, 0xc8u, 0x0du,
	0x3cu, 0xbau, 0x87u, 0x63u, 0x53u, 0x93u, 0x67u, 0x25u,
	0x03u, 0xf7u, 0x55u, 0x9bu, 0x8bu, 0x6bu, 0xb2u, 0x20u,
	0x3cu, 0xb3u, 0x6eu, 0x4au, 0x14u, 0xe9u, 0xbcu, 0x08u,
	0xf5u, 0x9fu, 0x50u, 0x8bu, 0xbcu, 0xb2u, 0x6bu, 0x3cu,
	0xa8u, 0xdfu, 0x60u, 0x35u, 0x0fu, 0x77u, 0xe5u, 0xf6u,
	0x91u, 0xfeu, 0x92u, 0xccu, 0xf4u, 0xedu, 0xf2u, 0x49u,
	0xf4u, 0xe7u, 0xfcu, 0x49u, 0x41u, 0x30u, 0xf8u, 0x05u
};



/* 固定 packet padding，确保线路输出稳定。 */
static bool testSshSessionCorePadding(
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
static xbytesview testSshSessionCorePayload(
	const xsshwriter* pWriter,
	const void* pData
)
{
	return (xbytesview){ (const unsigned char*)pData, pWriter->Size };
}



/* 构建固定 cookie 的初始 KEXINIT。 */
static xbytesview testSshSessionCoreKexInit(
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
		) == XSSH_OK), "ssh session KEXINIT build failed");
	return testSshSessionCorePayload(&Writer, pOutput);
}



/* 构建固定 Ed25519 主机公钥和签名 blob。 */
static void testSshSessionCoreHostMaterial(
	void* pHostKey,
	size_t iHostKeyCapacity,
	xbytesview* pHostKeyView,
	void* pSignature,
	size_t iSignatureCapacity,
	xbytesview* pSignatureView,
	xbytesview RawSignature
)
{
	xsshwriter Writer;

	testRequire(xrtSshWriterInit(&Writer, pHostKey, iHostKeyCapacity) &&
		(xrtSshEd25519PublicKeyWrite(
			&Writer,
			(xbytesview){
				g_arrSessionHostPublic,
				sizeof(g_arrSessionHostPublic)
			}
		) == XSSH_OK), "ssh session host key build failed");
	*pHostKeyView = testSshSessionCorePayload(&Writer, pHostKey);
	testRequire(xrtSshWriterInit(&Writer, pSignature, iSignatureCapacity) &&
		(xrtSshEd25519SignatureWrite(
			&Writer,
			RawSignature
		) == XSSH_OK), "ssh session signature build failed");
	*pSignatureView = testSshSessionCorePayload(&Writer, pSignature);
}



/* 在双方会话中可靠提交一条完整 payload。 */
static xsshsessionpacketkind testSshSessionCoreTransfer(
	xsshsessioncore* pSender,
	xsshtransportcore* pSenderCore,
	xsshsessioncore* pReceiver,
	xsshtransportcore* pReceiverCore,
	xbytesview Payload,
	xsshchannelcore* pChannel,
	xsshreplyqueue* pReplies,
	uint64 iReplyToken,
	void* pHostKeyStorage,
	size_t iHostKeyCapacity,
	size_t* pHostKeySize,
	uint64 iNowMs,
	bool bAbortFirst,
	uint8* pPadding
)
{
	unsigned char arrWire[2048];
	unsigned char arrPlain[1024];
	xsshrekeydecision Decision;
	xsshsessionpacket Packet;
	xsshsessionpacketkind WriteKind;
	xsshpacketview CorePacket;
	xsshwriter Writer;
	xsshreader Reader;
	xbytesview Wrong;

	testRequire((xrtSshSessionCoreWritePrepare(
		pSender,
		pSenderCore,
		Payload,
		pChannel,
		pReplies,
		iReplyToken,
		iNowMs,
		&WriteKind
	) == XSSH_OK) && xrtSshWriterInit(
		&Writer,
		arrWire,
		sizeof(arrWire)
	) && (xrtSshTransportCoreWritePrepareWithPadding(
		pSenderCore,
		&Writer,
		Payload,
		testSshSessionCorePadding,
		pPadding,
		iNowMs
	) == XSSH_OK) && (xrtSshSessionCoreAction(
		pSender,
		pSenderCore
	) == XSSH_SESSION_ACTION_WRITE_PENDING),
		"ssh session sender prepare failed");
	Wrong = (xbytesview){ arrPlain, Payload.Size };
	testRequire((xrtSshSessionCoreWriteBind(
		pSender,
		pSenderCore,
		Wrong
	) == XSSH_ERROR_ARGUMENT) && (xrtSshSessionCoreWriteBind(
		pSender,
		pSenderCore,
		Payload
	) == XSSH_OK) && (xrtSshSessionCoreWriteCommit(
		pSender,
		pSenderCore,
		iNowMs
	) == XSSH_ERROR_STATE), "ssh session write binding was not strict");
	if ( bAbortFirst ) {
		testRequire((xrtSshSessionCoreWriteAbort(
			pSender,
			pSenderCore
		) == XSSH_OK) && (xrtSshTransportCoreWriteAbort(
			pSenderCore
		) == XSSH_OK) && (xrtSshSessionCoreWritePrepare(
			pSender,
			pSenderCore,
			Payload,
			pChannel,
			pReplies,
			iReplyToken,
			iNowMs,
			&WriteKind
		) == XSSH_OK) && xrtSshWriterInit(
			&Writer,
			arrWire,
			sizeof(arrWire)
		) && (xrtSshTransportCoreWritePrepareWithPadding(
			pSenderCore,
			&Writer,
			Payload,
			testSshSessionCorePadding,
			pPadding,
			iNowMs
		) == XSSH_OK) && (xrtSshSessionCoreWriteBind(
			pSender,
			pSenderCore,
			Payload
		) == XSSH_OK), "ssh session write abort retry failed");
	}
	testRequire((xrtSshTransportCoreWriteCommit(
		pSenderCore,
		iNowMs,
		&Decision
	) == XSSH_OK) && (Decision != XSSH_REKEY_REQUIRED) &&
		(xrtSshSessionCoreWriteCommit(
			pSender,
			pSenderCore,
			iNowMs
		) == XSSH_OK), "ssh session sender commit failed");

	testRequire(xrtSshReaderInit(
		&Reader,
		(xbytesview){ arrWire, Writer.Size }
	) && (xrtSshTransportCoreReadPrepare(
		pReceiverCore,
		&Reader,
		&CorePacket,
		arrPlain,
		sizeof(arrPlain),
		iNowMs
	) == XSSH_OK) && (xrtSshSessionCoreReadPrepare(
		pReceiver,
		pReceiverCore,
		CorePacket.Payload,
		pHostKeyStorage,
		iHostKeyCapacity,
		pHostKeySize,
		iNowMs,
		&Packet
	) == XSSH_OK) && (xrtSshSessionCoreAction(
		pReceiver,
		pReceiverCore
	) == XSSH_SESSION_ACTION_READ_PENDING) &&
		(Packet.Kind == WriteKind) &&
		(Packet.Number == Payload.Data[0]) && (xrtSshTransportCoreReadCommit(
			pReceiverCore,
			iNowMs,
			&Decision
		) == XSSH_OK) && (Decision != XSSH_REKEY_REQUIRED) &&
		(xrtSshSessionCoreReadCommit(
			pReceiver,
			pReceiverCore,
			iNowMs
		) == XSSH_OK), "ssh session receiver transaction failed");
	return Packet.Kind;
}



/* 保存双方 identification，并验证版本事务边界。 */
static void testSshSessionCoreVersions(
	xsshsessioncore* pSession,
	xsshtransportcore* pCore,
	xstrview Local,
	xstrview Peer
)
{
	testRequire((xrtSshSessionCoreVersionPrepare(
		pSession,
		pCore,
		XSSH_TRANSPORT_LOCAL,
		Local
	) == XSSH_OK) && (xrtSshSessionCoreAction(
		pSession,
		pCore
	) == XSSH_SESSION_ACTION_WRITE_PENDING) &&
		(xrtSshSessionCoreVersionAbort(
		pSession,
		pCore
	) == XSSH_OK) && (xrtSshSessionCoreVersionPrepare(
		pSession,
		pCore,
		XSSH_TRANSPORT_LOCAL,
		Local
	) == XSSH_OK) && (xrtSshTransportCoreIdentificationCommit(
		pCore,
		XSSH_TRANSPORT_LOCAL
	) == XSSH_OK) && (xrtSshSessionCoreVersionCommit(
		pSession,
		pCore
	) == XSSH_OK) && (xrtSshSessionCoreAction(
		pSession,
		pCore
	) == XSSH_SESSION_ACTION_READ_IDENTIFICATION),
		"ssh session local version failed");
	testRequire((xrtSshSessionCoreVersionPrepare(
		pSession,
		pCore,
		XSSH_TRANSPORT_PEER,
		Peer
	) == XSSH_OK) && (xrtSshSessionCoreAction(
		pSession,
		pCore
	) == XSSH_SESSION_ACTION_READ_PENDING) &&
		(xrtSshTransportCoreIdentificationCommit(
		pCore,
		XSSH_TRANSPORT_PEER
	) == XSSH_OK) && (xrtSshSessionCoreVersionCommit(
		pSession,
		pCore
	) == XSSH_OK) && (xrtSshSessionCoreAction(
		pSession,
		pCore
	) == XSSH_SESSION_ACTION_WRITE_KEXINIT),
		"ssh session peer version failed");
}



/* 完成真实双端 KEX，并由会话核心自动激活两个 cipher 方向。 */
static void testSshSessionCoreKex(
	xsshsessioncore* pClient,
	xsshtransportcore* pClientCore,
	xsshsessioncore* pServer,
	xsshtransportcore* pServerCore,
	xbytesview HostKey,
	xbytesview Signature,
	xbytesview ClientPrivate,
	xbytesview ServerPrivate,
	bool bInitial,
	uint8 iClientCookie,
	uint8 iServerCookie,
	uint64 iNowMs,
	uint8* pPadding
)
{
	unsigned char arrClientKex[512];
	unsigned char arrServerKex[512];
	unsigned char arrPayload[512];
	unsigned char arrClientHostKey[128];
	xbytesview ClientKex;
	xbytesview ServerKex;
	xbytesview Payload;
	xbytesview SeenHostKey;
	xsshkexsession* pClientKex;
	xsshkexsession* pServerKex;
	xsshwriter Writer;
	size_t iHostKeySize = 0u;

	ClientKex = testSshSessionCoreKexInit(
		XSSH_ROLE_CLIENT,
		bInitial,
		iClientCookie,
		arrClientKex,
		sizeof(arrClientKex)
	);
	ServerKex = testSshSessionCoreKexInit(
		XSSH_ROLE_SERVER,
		bInitial,
		iServerCookie,
		arrServerKex,
		sizeof(arrServerKex)
	);
	testRequire(testSshSessionCoreTransfer(
		pClient,
		pClientCore,
		pServer,
		pServerCore,
		ClientKex,
		NULL,
		NULL,
		0u,
		NULL,
		0u,
		NULL,
		iNowMs,
		true,
		pPadding
	) == XSSH_SESSION_PACKET_KEXINIT, "ssh session client KEXINIT failed");
	testRequire(testSshSessionCoreTransfer(
		pServer,
		pServerCore,
		pClient,
		pClientCore,
		ServerKex,
		NULL,
		NULL,
		0u,
		NULL,
		0u,
		NULL,
		iNowMs,
		false,
		pPadding
	) == XSSH_SESSION_PACKET_KEXINIT, "ssh session server KEXINIT failed");
	testRequire((xrtSshSessionCoreAction(
		pClient,
		pClientCore
	) == XSSH_SESSION_ACTION_BEGIN_KEX) &&
		(xrtSshSessionCoreAction(
			pServer,
			pServerCore
		) == XSSH_SESSION_ACTION_BEGIN_KEX) &&
		(xrtSshSessionCoreKexBeginWithPrivate(
		pClient,
		pClientCore,
		(xbytesview){ NULL, 0u },
		ClientPrivate
	) == XSSH_OK) && (xrtSshSessionCoreKexBeginWithPrivate(
		pServer,
		pServerCore,
		HostKey,
		ServerPrivate
	) == XSSH_OK) && (xrtSshSessionCoreAction(
		pClient,
		pClientCore
	) == XSSH_SESSION_ACTION_WRITE_ECDH_INIT) &&
		(xrtSshSessionCoreAction(
			pServer,
			pServerCore
		) == XSSH_SESSION_ACTION_READ_ECDH_INIT),
		"ssh session KEX begin failed");
	pClientKex = xrtSshKexExchangeSession(xrtSshSessionCoreKex(pClient));
	pServerKex = xrtSshKexExchangeSession(xrtSshSessionCoreKex(pServer));
	testRequire((pClientKex != NULL) && (pServerKex != NULL),
		"ssh session KEX accessors failed");

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshKexSessionEcdhInitPrepare(
			pClientKex,
			&Writer
		) == XSSH_OK), "ssh session ECDH init build failed");
	Payload = testSshSessionCorePayload(&Writer, arrPayload);
	testRequire(testSshSessionCoreTransfer(
		pClient,
		pClientCore,
		pServer,
		pServerCore,
		Payload,
		NULL,
		NULL,
		0u,
		NULL,
		0u,
		NULL,
		iNowMs + 1u,
		false,
		pPadding
	) == XSSH_SESSION_PACKET_KEX && (xrtSshSessionCoreAction(
		pClient,
		pClientCore
	) == XSSH_SESSION_ACTION_READ_ECDH_REPLY) &&
		(xrtSshSessionCoreAction(
			pServer,
			pServerCore
		) == XSSH_SESSION_ACTION_WRITE_ECDH_REPLY),
		"ssh session ECDH init transfer failed");

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshKexSessionEcdhReplyPrepare(
			pServerKex,
			&Writer,
			Signature
		) == XSSH_OK), "ssh session ECDH reply build failed");
	Payload = testSshSessionCorePayload(&Writer, arrPayload);
	testRequire(testSshSessionCoreTransfer(
		pServer,
		pServerCore,
		pClient,
		pClientCore,
		Payload,
		NULL,
		NULL,
		0u,
		arrClientHostKey,
		sizeof(arrClientHostKey),
		&iHostKeySize,
		iNowMs + 2u,
		false,
		pPadding
	) == XSSH_SESSION_PACKET_KEX && (xrtSshSessionCoreAction(
		pClient,
		pClientCore
	) == XSSH_SESSION_ACTION_VERIFY_HOST_KEY),
		"ssh session ECDH reply transfer failed");
	SeenHostKey = (xbytesview){ arrClientHostKey, iHostKeySize };
	testRequire(testSshBytesEqual(SeenHostKey, HostKey) &&
		(xrtSshKexSessionHostKeyAccept(pClientKex) == XSSH_OK) &&
		(xrtSshSessionCoreAction(
			pClient,
			pClientCore
		) == XSSH_SESSION_ACTION_WRITE_NEWKEYS),
		"ssh session host trust failed");

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshKexSessionNewKeysPrepare(
			pServerKex,
			&Writer
		) == XSSH_OK), "ssh session server NEWKEYS build failed");
	Payload = testSshSessionCorePayload(&Writer, arrPayload);
	testRequire(testSshSessionCoreTransfer(
		pServer,
		pServerCore,
		pClient,
		pClientCore,
		Payload,
		NULL,
		NULL,
		0u,
		NULL,
		0u,
		NULL,
		iNowMs + 3u,
		false,
		pPadding
	) == XSSH_SESSION_PACKET_KEX, "ssh session server NEWKEYS failed");
	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshKexSessionNewKeysPrepare(
			pClientKex,
			&Writer
		) == XSSH_OK), "ssh session client NEWKEYS build failed");
	Payload = testSshSessionCorePayload(&Writer, arrPayload);
	testRequire(testSshSessionCoreTransfer(
		pClient,
		pClientCore,
		pServer,
		pServerCore,
		Payload,
		NULL,
		NULL,
		0u,
		NULL,
		0u,
		NULL,
		iNowMs + 4u,
		false,
		pPadding
	) == XSSH_SESSION_PACKET_KEX, "ssh session client NEWKEYS failed");
	testRequire((xrtSshSessionCorePhase(pClient, pClientCore) ==
		 (bInitial ? XSSH_SESSION_AUTHENTICATION :
		  XSSH_SESSION_CONNECTION)) &&
		(xrtSshSessionCorePhase(pServer, pServerCore) ==
		 (bInitial ? XSSH_SESSION_AUTHENTICATION :
		  XSSH_SESSION_CONNECTION)) && (xrtSshSessionCoreAction(
			pClient,
			pClientCore
		) == (bInitial ? XSSH_SESSION_ACTION_BEGIN_AUTH :
			XSSH_SESSION_ACTION_CONNECTION)),
		"ssh session KEX did not complete");
}



/* 完成认证并验证 connection、transport 和扩展路由。 */
static void testSshSessionCoreApplication(
	xsshsessioncore* pClient,
	xsshtransportcore* pClientCore,
	xsshsessioncore* pServer,
	xsshtransportcore* pServerCore,
	uint8* pPadding
)
{
	unsigned char arrPayload[512];
	xbytesview Payload;
	xsshwriter Writer;

	testRequire((xrtSshSessionCoreAuthBegin(
		pClient,
		pClientCore,
		NULL,
		5u
	) == XSSH_OK) && (xrtSshSessionCoreAuthBegin(
		pServer,
		pServerCore,
		NULL,
		5u
	) == XSSH_OK) && (xrtSshSessionCoreAction(
		pClient,
		pClientCore
	) == XSSH_SESSION_ACTION_WRITE_SERVICE_REQUEST) &&
		(xrtSshSessionCoreAction(
			pServer,
			pServerCore
		) == XSSH_SESSION_ACTION_READ_SERVICE_REQUEST),
		"ssh session auth begin failed");
	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshServiceRequestWrite(
			&Writer,
			XRT_STR_LITERAL(XSSH_SERVICE_USERAUTH)
		) == XSSH_OK), "ssh session service request build failed");
	Payload = testSshSessionCorePayload(&Writer, arrPayload);
	testRequire(testSshSessionCoreTransfer(
		pClient,
		pClientCore,
		pServer,
		pServerCore,
		Payload,
		NULL,
		NULL,
		0u,
		NULL,
		0u,
		NULL,
		5u,
		true,
		pPadding
	) == XSSH_SESSION_PACKET_AUTH, "ssh session service request failed");
	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshServiceAcceptWrite(
			&Writer,
			XRT_STR_LITERAL(XSSH_SERVICE_USERAUTH)
		) == XSSH_OK), "ssh session service accept build failed");
	Payload = testSshSessionCorePayload(&Writer, arrPayload);
	(void)testSshSessionCoreTransfer(
		pServer, pServerCore, pClient, pClientCore, Payload,
		NULL, NULL, 0u, NULL, 0u, NULL, 6u, false, pPadding
	);
	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshAuthNoneWrite(
			&Writer,
			XRT_STR_LITERAL("alice")
		) == XSSH_OK), "ssh session auth request build failed");
	Payload = testSshSessionCorePayload(&Writer, arrPayload);
	(void)testSshSessionCoreTransfer(
		pClient, pClientCore, pServer, pServerCore, Payload,
		NULL, NULL, 0u, NULL, 0u, NULL, 7u, false, pPadding
	);
	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshAuthSuccessWrite(&Writer) == XSSH_OK),
		"ssh session auth success build failed");
	Payload = testSshSessionCorePayload(&Writer, arrPayload);
	testRequire(testSshSessionCoreTransfer(
		pServer,
		pServerCore,
		pClient,
		pClientCore,
		Payload,
		NULL,
		NULL,
		0u,
		NULL,
		0u,
		NULL,
		8u,
		false,
		pPadding
	) == XSSH_SESSION_PACKET_AUTH, "ssh session auth success failed");
	testRequire((xrtSshSessionCorePhase(pClient, pClientCore) ==
		 XSSH_SESSION_CONNECTION) &&
		(xrtSshSessionCorePhase(pServer, pServerCore) ==
		 XSSH_SESSION_CONNECTION) && xrtSshConnectionSessionActive(
		xrtSshSessionCoreConnection(pClient)
	) && (xrtSshSessionCoreAction(
		pClient,
		pClientCore
	) == XSSH_SESSION_ACTION_CONNECTION),
		"ssh session connection did not open automatically");

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshGlobalRequestWrite(
			&Writer,
			XRT_STR_LITERAL("keepalive@xssh"),
			false,
			(xbytesview){ NULL, 0u }
		) == XSSH_OK), "ssh session global request build failed");
	Payload = testSshSessionCorePayload(&Writer, arrPayload);
	testRequire(testSshSessionCoreTransfer(
		pClient,
		pClientCore,
		pServer,
		pServerCore,
		Payload,
		NULL,
		NULL,
		0u,
		NULL,
		0u,
		NULL,
		9u,
		false,
		pPadding
	) == XSSH_SESSION_PACKET_CONNECTION,
		"ssh session connection routing failed");
	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshIgnoreWrite(
			&Writer,
			XRT_BYTES_LITERAL("padding")
		) == XSSH_OK), "ssh session ignore build failed");
	Payload = testSshSessionCorePayload(&Writer, arrPayload);
	testRequire(testSshSessionCoreTransfer(
		pServer,
		pServerCore,
		pClient,
		pClientCore,
		Payload,
		NULL,
		NULL,
		0u,
		NULL,
		0u,
		NULL,
		10u,
		false,
		pPadding
	) == XSSH_SESSION_PACKET_IGNORE, "ssh session control routing failed");
	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshWriteByte(&Writer, 200u) == XSSH_OK),
		"ssh session extension build failed");
	Payload = testSshSessionCorePayload(&Writer, arrPayload);
	testRequire(testSshSessionCoreTransfer(
		pClient,
		pClientCore,
		pServer,
		pServerCore,
		Payload,
		NULL,
		NULL,
		0u,
		NULL,
		0u,
		NULL,
		11u,
		false,
		pPadding
	) == XSSH_SESSION_PACKET_EXTENSION,
		"ssh session extension escape hatch failed");
}



/* 验证完整双端生命周期、事务关联和动态内存释放。 */
int main(void)
{
	static const char sClientVersion[] = "SSH-2.0-xssh_client";
	static const char sServerVersion[] = "SSH-2.0-xssh_server";
	unsigned char arrHostKey[128];
	unsigned char arrSignature[128];
	xnetbufpoolinfo PoolInfo;
	xnetbufpool* pPool;
	xbytesview HostKey;
	xbytesview Signature;
	xsshtransportcore ClientCore;
	xsshtransportcore ServerCore;
	xsshsessioncore Client;
	xsshsessioncore Server;
	uint8 iPadding = 0x40u;

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
	) && xrtSshSessionCoreInit(
		&Client,
		pPool,
		XSSH_ROLE_CLIENT,
		NULL,
		NULL,
		NULL
	) && xrtSshSessionCoreInit(
		&Server,
		pPool,
		XSSH_ROLE_SERVER,
		NULL,
		NULL,
		NULL
	), "ssh session core initialization failed");
	testRequire((sizeof(xsshsessioncore) < 2048u) &&
		(xrtSshSessionCorePhase(&Client, &ClientCore) ==
		 XSSH_SESSION_IDENTIFICATION) &&
		(xrtSshSessionCoreAction(&Client, &ClientCore) ==
		 XSSH_SESSION_ACTION_WRITE_IDENTIFICATION) &&
		(xrtSshSessionCoreKex(&Client) != NULL) &&
		(xrtSshSessionCoreAuthConst(&Client) != NULL) &&
		(xrtSshSessionCoreConnection(&Client) != NULL),
		"ssh session core ownership shape failed");
	testSshSessionCoreVersions(
		&Client,
		&ClientCore,
		XRT_STR_LITERAL(sClientVersion),
		XRT_STR_LITERAL(sServerVersion)
	);
	testSshSessionCoreVersions(
		&Server,
		&ServerCore,
		XRT_STR_LITERAL(sServerVersion),
		XRT_STR_LITERAL(sClientVersion)
	);
	testSshSessionCoreHostMaterial(
		arrHostKey,
		sizeof(arrHostKey),
		&HostKey,
		arrSignature,
		sizeof(arrSignature),
		&Signature,
		(xbytesview){
			g_arrSessionHostSignature,
			sizeof(g_arrSessionHostSignature)
		}
	);
	testSshSessionCoreKex(
		&Client,
		&ClientCore,
		&Server,
		&ServerCore,
		HostKey,
		Signature,
		(xbytesview){
			g_arrSessionClientPrivate,
			sizeof(g_arrSessionClientPrivate)
		},
		(xbytesview){
			g_arrSessionServerPrivate,
			sizeof(g_arrSessionServerPrivate)
		},
		true,
		0x00u,
		0x10u,
		0u,
		&iPadding
	);
	testSshSessionCoreApplication(
		&Client,
		&ClientCore,
		&Server,
		&ServerCore,
		&iPadding
	);
	testSshSessionCoreHostMaterial(
		arrHostKey,
		sizeof(arrHostKey),
		&HostKey,
		arrSignature,
		sizeof(arrSignature),
		&Signature,
		(xbytesview){
			g_arrSessionHostRekeySignature,
			sizeof(g_arrSessionHostRekeySignature)
		}
	);
	testSshSessionCoreKex(
		&Client,
		&ClientCore,
		&Server,
		&ServerCore,
		HostKey,
		Signature,
		(xbytesview){
			g_arrSessionClientRekeyPrivate,
			sizeof(g_arrSessionClientRekeyPrivate)
		},
		(xbytesview){
			g_arrSessionServerRekeyPrivate,
			sizeof(g_arrSessionServerRekeyPrivate)
		},
		false,
		0x40u,
		0x50u,
		20u,
		&iPadding
	);
	testRequire(xrtSshConnectionSessionActive(
		xrtSshSessionCoreConnection(&Client)
	) && xrtSshConnectionSessionActive(
		xrtSshSessionCoreConnection(&Server)
	), "ssh session rekey lost connection state");
	xrtSshSessionCoreFail(&Client);
	testRequire((xrtSshSessionCorePhase(&Client, &ClientCore) ==
		 XSSH_SESSION_FAILED) && (xrtSshSessionCoreKex(&Client) == NULL),
		"ssh session fail state remained usable");
	xrtSshSessionCoreClear(&Client);
	xrtSshSessionCoreClear(&Server);
	xrtSshTransportCoreClear(&ClientCore);
	xrtSshTransportCoreClear(&ServerCore);
	xrtNetBufPoolGet(pPool, &PoolInfo);
	testRequire((PoolInfo.LiveBlocks == 0u) && xrtNetBufPoolDestroy(pPool),
		"ssh session core leaked dynamic blocks");
	puts("ssh connection-level session core tests passed");
	return 0;
}
