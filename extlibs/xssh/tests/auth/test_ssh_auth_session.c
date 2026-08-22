#include "../test.h"



#define TEST_SSH_AUTH_KEX_INIT 30u
#define TEST_SSH_AUTH_KEX_REPLY 31u



/* 测试 packet 使用确定性 padding，保证两端线路行为可重复。 */
static bool testSshAuthSessionPadding(
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



/* 构造无扩展标记的默认 KEX 视图，供认证层测试建立 OPEN transport。 */
static void testSshAuthSessionKexViews(
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
	) == XSSH_OK, "ssh auth session negotiation failed");
	if ( Role == XSSH_ROLE_CLIENT ) {
		*pLocal = Client;
		*pPeer = Server;
	} else {
		*pLocal = Server;
		*pPeer = Client;
	}
}



/* 只使用公开状态 API 完成首轮 KEX，保留 plain codec 便于隔离测试认证层。 */
static void testSshAuthSessionCoreOpen(
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
		TEST_SSH_AUTH_KEX_INIT : TEST_SSH_AUTH_KEX_REPLY;
	uint8 iPeerMessage = Role == XSSH_ROLE_CLIENT ?
		TEST_SSH_AUTH_KEX_REPLY : TEST_SSH_AUTH_KEX_INIT;

	testSshAuthSessionKexViews(
		Role,
		&Local,
		&Peer,
		&Negotiation
	);
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
	) == XSSH_OK) && xrtSshTransportCoreKexComplete(pCore),
		"ssh auth session transport setup failed");
}



/* 建立角色互补且使用同一预算策略的认证会话。 */
static void testSshAuthSessionPairOpen(
	xsshtransportcore* pClientCore,
	xsshtransportcore* pServerCore,
	xsshauthsession* pClient,
	xsshauthsession* pServer,
	const xsshauthguardpolicy* pPolicy,
	uint64 iNowMs
)
{
	testSshAuthSessionCoreOpen(pClientCore, XSSH_ROLE_CLIENT);
	testSshAuthSessionCoreOpen(pServerCore, XSSH_ROLE_SERVER);
	testRequire(xrtSshAuthSessionInit(
		pClient,
		XSSH_ROLE_CLIENT
	) && xrtSshAuthSessionInit(
		pServer,
		XSSH_ROLE_SERVER
	) && (xrtSshAuthSessionBegin(
		pClient,
		pClientCore,
		pPolicy,
		iNowMs
	) == XSSH_OK) && (xrtSshAuthSessionBegin(
		pServer,
		pServerCore,
		pPolicy,
		iNowMs
	) == XSSH_OK), "ssh auth session begin failed");
}



/* 按 session -> transport -> session 顺序可靠发送一个认证 payload。 */
static size_t testSshAuthSessionSend(
	xsshauthsession* pSession,
	xsshtransportcore* pCore,
	xbytesview Payload,
	void* pWire,
	size_t iWireCapacity,
	uint64 iNowMs
)
{
	xsshrekeydecision Decision;
	xsshwriter Writer;
	uint8 iPadding = 0x40u;

	testRequire((xrtSshAuthSessionWritePrepare(
		pSession,
		pCore,
		Payload,
		iNowMs
	) == XSSH_OK) && xrtSshWriterInit(
		&Writer,
		pWire,
		iWireCapacity
	) && (xrtSshTransportCoreWritePrepareWithPadding(
		pCore,
		&Writer,
		Payload,
		testSshAuthSessionPadding,
		&iPadding,
		iNowMs
	) == XSSH_OK) && (xrtSshTransportCoreWriteCommit(
		pCore,
		iNowMs,
		&Decision
	) == XSSH_OK) && (Decision != XSSH_REKEY_REQUIRED) &&
		(xrtSshAuthSessionWriteCommit(
			pSession,
			pCore
		) == XSSH_OK), "ssh auth session send failed");
	return Writer.Size;
}



/* 准备对端完整 packet，并保留认证借用视图直到显式提交。 */
static xsshauthsessionpacket testSshAuthSessionReceivePrepare(
	xsshauthsession* pSession,
	xsshtransportcore* pCore,
	const void* pWire,
	size_t iWireSize,
	void* pPlain,
	size_t iPlainCapacity,
	uint64 iNowMs
)
{
	xsshauthsessionpacket Kind = XSSH_AUTH_SESSION_PACKET_NONE;
	xsshpacketview Packet;
	xsshreader Reader;

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
	) == XSSH_OK) && (Reader.Position == iWireSize) &&
		(xrtSshAuthSessionReadPrepare(
			pSession,
			pCore,
			Packet.Payload,
			iNowMs,
			&Kind
		) == XSSH_OK), "ssh auth session receive prepare failed");
	return Kind;
}



/* transport 提交后再提交认证状态，借用视图在此边界失效。 */
static void testSshAuthSessionReceiveCommit(
	xsshauthsession* pSession,
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
		(xrtSshAuthSessionReadCommit(
			pSession,
			pCore
		) == XSSH_OK), "ssh auth session receive commit failed");
}



/* 构建器成功后返回调用方 payload 缓冲视图。 */
static xbytesview testSshAuthSessionPayload(
	const xsshwriter* pWriter,
	const void* pData
)
{
	return (xbytesview){ (const unsigned char*)pData, pWriter->Size };
}



/* 验证 service、banner、多方法交互和成功方向的完整双端编排。 */
static void testSshAuthSessionFlow(void)
{
	unsigned char arrPayload[512];
	unsigned char arrWire[1024];
	unsigned char arrPlain[512];
	unsigned char arrMethodChallenge[] = { 60u, 1u, 2u };
	unsigned char arrMethodResponse[] = { 61u, 3u };
	xsshtransportcore ClientCore;
	xsshtransportcore ServerCore;
	xsshauthsession Client;
	xsshauthsession Server;
	xsshauthrequest Request;
	xsshauthfailure Failure;
	xsshauthbanner Banner;
	xsshauthguard ClientBudget;
	xsshauthguard ServerBudget;
	xbytesview Method;
	xbytesview Payload;
	xsshwriter Writer;
	size_t iWireSize;
	uint64 iNowMs = 100u;

	testSshAuthSessionPairOpen(
		&ClientCore,
		&ServerCore,
		&Client,
		&Server,
		NULL,
		iNowMs
	);
	testRequire((xrtSshAuthSessionEvent(&Client) ==
		XSSH_AUTH_SESSION_EVENT_WRITE_SERVICE_REQUEST) &&
		(xrtSshAuthSessionEvent(&Server) ==
		 XSSH_AUTH_SESSION_EVENT_READ_SERVICE_REQUEST) &&
		xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshServiceRequestWrite(
			&Writer,
			XRT_STR_LITERAL(XSSH_SERVICE_USERAUTH)
		) == XSSH_OK), "ssh auth session service request build failed");
	Payload = testSshAuthSessionPayload(&Writer, arrPayload);
	iWireSize = testSshAuthSessionSend(
		&Client,
		&ClientCore,
		Payload,
		arrWire,
		sizeof(arrWire),
		++iNowMs
	);
	testRequire(testSshAuthSessionReceivePrepare(
		&Server,
		&ServerCore,
		arrWire,
		iWireSize,
		arrPlain,
		sizeof(arrPlain),
		iNowMs
	) == XSSH_AUTH_SESSION_PACKET_SERVICE_REQUEST,
		"ssh auth session service request classify failed");
	testSshAuthSessionReceiveCommit(&Server, &ServerCore, iNowMs);

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshServiceAcceptWrite(
			&Writer,
			XRT_STR_LITERAL(XSSH_SERVICE_USERAUTH)
		) == XSSH_OK), "ssh auth session service accept build failed");
	Payload = testSshAuthSessionPayload(&Writer, arrPayload);
	iWireSize = testSshAuthSessionSend(
		&Server,
		&ServerCore,
		Payload,
		arrWire,
		sizeof(arrWire),
		++iNowMs
	);
	testRequire(testSshAuthSessionReceivePrepare(
		&Client,
		&ClientCore,
		arrWire,
		iWireSize,
		arrPlain,
		sizeof(arrPlain),
		iNowMs
	) == XSSH_AUTH_SESSION_PACKET_SERVICE_ACCEPT,
		"ssh auth session service accept classify failed");
	testSshAuthSessionReceiveCommit(&Client, &ClientCore, iNowMs);

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshAuthBannerWrite(
			&Writer,
			XRT_STR_LITERAL("authorized access"),
			XRT_STR_LITERAL("en")
		) == XSSH_OK), "ssh auth session banner build failed");
	Payload = testSshAuthSessionPayload(&Writer, arrPayload);
	iWireSize = testSshAuthSessionSend(
		&Server,
		&ServerCore,
		Payload,
		arrWire,
		sizeof(arrWire),
		++iNowMs
	);
	testRequire(testSshAuthSessionReceivePrepare(
		&Client,
		&ClientCore,
		arrWire,
		iWireSize,
		arrPlain,
		sizeof(arrPlain),
		iNowMs
	) == XSSH_AUTH_SESSION_PACKET_BANNER &&
		(xrtSshAuthSessionBanner(&Client, &Banner) == XSSH_OK) &&
		testSshTextEqual(
			Banner.Message,
			XRT_STR_LITERAL("authorized access")
		) && testSshTextEqual(Banner.Language, XRT_STR_LITERAL("en")),
		"ssh auth session banner view failed");
	testSshAuthSessionReceiveCommit(&Client, &ClientCore, iNowMs);
	testRequire(xrtSshAuthSessionEvent(&Client) ==
		XSSH_AUTH_SESSION_EVENT_WRITE_REQUEST,
		"ssh auth session banner changed primary event");

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshAuthNoneWrite(
			&Writer,
			XRT_STR_LITERAL("alice")
		) == XSSH_OK), "ssh auth session none build failed");
	Payload = testSshAuthSessionPayload(&Writer, arrPayload);
	iWireSize = testSshAuthSessionSend(
		&Client,
		&ClientCore,
		Payload,
		arrWire,
		sizeof(arrWire),
		++iNowMs
	);
	testRequire(testSshAuthSessionReceivePrepare(
		&Server,
		&ServerCore,
		arrWire,
		iWireSize,
		arrPlain,
		sizeof(arrPlain),
		iNowMs
	) == XSSH_AUTH_SESSION_PACKET_REQUEST &&
		(xrtSshAuthSessionRequest(&Server, &Request) == XSSH_OK) &&
		testSshTextEqual(Request.User, XRT_STR_LITERAL("alice")) &&
		testSshTextEqual(
			Request.Service,
			XRT_STR_LITERAL(XSSH_SERVICE_CONNECTION)
		) && testSshTextEqual(
			Request.Method,
			XRT_STR_LITERAL(XSSH_AUTH_METHOD_NONE)
		), "ssh auth session request view failed");
	testSshAuthSessionReceiveCommit(&Server, &ServerCore, iNowMs);

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshAuthFailureWrite(
			&Writer,
			XRT_STR_LITERAL("password,keyboard-interactive"),
			false
		) == XSSH_OK), "ssh auth session failure build failed");
	Payload = testSshAuthSessionPayload(&Writer, arrPayload);
	iWireSize = testSshAuthSessionSend(
		&Server,
		&ServerCore,
		Payload,
		arrWire,
		sizeof(arrWire),
		++iNowMs
	);
	testRequire(testSshAuthSessionReceivePrepare(
		&Client,
		&ClientCore,
		arrWire,
		iWireSize,
		arrPlain,
		sizeof(arrPlain),
		iNowMs
	) == XSSH_AUTH_SESSION_PACKET_FAILURE &&
		(xrtSshAuthSessionFailure(&Client, &Failure) == XSSH_OK) &&
		!Failure.PartialSuccess && testSshTextEqual(
			Failure.Methods,
			XRT_STR_LITERAL("password,keyboard-interactive")
		), "ssh auth session failure view failed");
	testSshAuthSessionReceiveCommit(&Client, &ClientCore, iNowMs);

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshAuthRequestWrite(
			&Writer,
			XRT_STR_LITERAL("alice"),
			XRT_STR_LITERAL(XSSH_SERVICE_CONNECTION),
			XRT_STR_LITERAL(XSSH_AUTH_METHOD_KEYBOARD_INTERACTIVE),
			(xbytesview){ NULL, 0u }
		) == XSSH_OK), "ssh auth session keyboard request build failed");
	Payload = testSshAuthSessionPayload(&Writer, arrPayload);
	iWireSize = testSshAuthSessionSend(
		&Client,
		&ClientCore,
		Payload,
		arrWire,
		sizeof(arrWire),
		++iNowMs
	);
	testRequire(testSshAuthSessionReceivePrepare(
		&Server,
		&ServerCore,
		arrWire,
		iWireSize,
		arrPlain,
		sizeof(arrPlain),
		iNowMs
	) == XSSH_AUTH_SESSION_PACKET_REQUEST,
		"ssh auth session second request failed");
	testSshAuthSessionReceiveCommit(&Server, &ServerCore, iNowMs);

	Payload = (xbytesview){ arrMethodChallenge, sizeof(arrMethodChallenge) };
	iWireSize = testSshAuthSessionSend(
		&Server,
		&ServerCore,
		Payload,
		arrWire,
		sizeof(arrWire),
		++iNowMs
	);
	testRequire(testSshAuthSessionReceivePrepare(
		&Client,
		&ClientCore,
		arrWire,
		iWireSize,
		arrPlain,
		sizeof(arrPlain),
		iNowMs
	) == XSSH_AUTH_SESSION_PACKET_METHOD &&
		(xrtSshAuthSessionMethod(&Client, &Method) == XSSH_OK) &&
		testSshBytesEqual(Method, Payload),
		"ssh auth session method challenge failed");
	testSshAuthSessionReceiveCommit(&Client, &ClientCore, iNowMs);

	Payload = (xbytesview){ arrMethodResponse, sizeof(arrMethodResponse) };
	iWireSize = testSshAuthSessionSend(
		&Client,
		&ClientCore,
		Payload,
		arrWire,
		sizeof(arrWire),
		++iNowMs
	);
	testRequire(testSshAuthSessionReceivePrepare(
		&Server,
		&ServerCore,
		arrWire,
		iWireSize,
		arrPlain,
		sizeof(arrPlain),
		iNowMs
	) == XSSH_AUTH_SESSION_PACKET_METHOD &&
		(xrtSshAuthSessionMethod(&Server, &Method) == XSSH_OK) &&
		testSshBytesEqual(Method, Payload),
		"ssh auth session method response failed");
	testSshAuthSessionReceiveCommit(&Server, &ServerCore, iNowMs);

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshAuthSuccessWrite(&Writer) == XSSH_OK),
		"ssh auth session success build failed");
	Payload = testSshAuthSessionPayload(&Writer, arrPayload);
	iWireSize = testSshAuthSessionSend(
		&Server,
		&ServerCore,
		Payload,
		arrWire,
		sizeof(arrWire),
		++iNowMs
	);
	testRequire(testSshAuthSessionReceivePrepare(
		&Client,
		&ClientCore,
		arrWire,
		iWireSize,
		arrPlain,
		sizeof(arrPlain),
		iNowMs
	) == XSSH_AUTH_SESSION_PACKET_SUCCESS,
		"ssh auth session success classify failed");
	testSshAuthSessionReceiveCommit(&Client, &ClientCore, iNowMs);

	testRequire(xrtSshAuthSessionComplete(&Client, &ClientCore) &&
		xrtSshAuthSessionComplete(&Server, &ServerCore) &&
		(xrtSshAuthSessionBudget(&Client, &ClientBudget) == XSSH_OK) &&
		(xrtSshAuthSessionBudget(&Server, &ServerBudget) == XSSH_OK) &&
		(ClientBudget.Attempts == 2u) && (ServerBudget.Attempts == 2u) &&
		(ClientBudget.Rounds == 1u) && (ServerBudget.Rounds == 1u) &&
		(ClientBudget.Messages == 7u) && (ServerBudget.Messages == 7u) &&
		(ClientBudget.Bytes == ServerBudget.Bytes),
		"ssh auth session completion or budget failed");
	printf(
		"auth-session=%zu messages=%u bytes=%llu\n",
		sizeof(Client),
		ClientBudget.Messages,
		(unsigned long long)ClientBudget.Bytes
	);
	xrtSshTransportCoreClear(&ClientCore);
	xrtSshTransportCoreClear(&ServerCore);
}



/* 验证写取消无副作用，读取消关闭不可回滚的接收事务。 */
static void testSshAuthSessionAbort(void)
{
	unsigned char arrPayload[128];
	unsigned char arrWire[256];
	unsigned char arrPlain[128];
	xsshtransportcore ClientCore;
	xsshtransportcore ServerCore;
	xsshauthsession Client;
	xsshauthsession Server;
	xsshauthguard Before;
	xsshauthguard After;
	xbytesview Payload;
	xsshwriter Writer;
	size_t iWireSize;

	testSshAuthSessionPairOpen(
		&ClientCore,
		&ServerCore,
		&Client,
		&Server,
		NULL,
		0u
	);
	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshServiceRequestWrite(
			&Writer,
			XRT_STR_LITERAL(XSSH_SERVICE_USERAUTH)
		) == XSSH_OK), "ssh auth session abort payload build failed");
	Payload = testSshAuthSessionPayload(&Writer, arrPayload);
	testRequire((xrtSshAuthSessionBudget(&Client, &Before) == XSSH_OK) &&
		(xrtSshAuthSessionWritePrepare(
			&Client,
			&ClientCore,
			Payload,
			1u
		) == XSSH_OK) && (xrtSshAuthSessionWriteAbort(&Client) == XSSH_OK) &&
		(xrtSshAuthSessionBudget(&Client, &After) == XSSH_OK) &&
		(memcmp(&Before, &After, sizeof(Before)) == 0) &&
		(xrtSshAuthSessionEvent(&Client) ==
		 XSSH_AUTH_SESSION_EVENT_WRITE_SERVICE_REQUEST),
		"ssh auth session write abort consumed state");
	iWireSize = testSshAuthSessionSend(
		&Client,
		&ClientCore,
		Payload,
		arrWire,
		sizeof(arrWire),
		2u
	);
	testRequire(testSshAuthSessionReceivePrepare(
		&Server,
		&ServerCore,
		arrWire,
		iWireSize,
		arrPlain,
		sizeof(arrPlain),
		2u
	) == XSSH_AUTH_SESSION_PACKET_SERVICE_REQUEST &&
		(xrtSshAuthSessionReadAbort(&Server) == XSSH_OK) &&
		(xrtSshTransportCoreReadAbort(&ServerCore) == XSSH_OK) &&
		(xrtSshAuthSessionEvent(&Server) ==
		 XSSH_AUTH_SESSION_EVENT_FAILED) &&
		(ServerCore.State.Phase == XSSH_TRANSPORT_CLOSED),
		"ssh auth session read abort remained usable");
	xrtSshTransportCoreClear(&ClientCore);
	xrtSshTransportCoreClear(&ServerCore);
}



/* 验证尝试预算在第二次请求准备时原子拒绝并终止会话。 */
static void testSshAuthSessionAttemptLimit(void)
{
	unsigned char arrPayload[256];
	unsigned char arrWire[512];
	unsigned char arrPlain[256];
	xsshtransportcore ClientCore;
	xsshtransportcore ServerCore;
	xsshauthsession Client;
	xsshauthsession Server;
	xsshauthguard Budget;
	xsshauthguardpolicy Policy = { 0u };
	xbytesview Payload;
	xsshwriter Writer;
	size_t iWireSize;

	Policy.AttemptLimit = 1u;
	testSshAuthSessionPairOpen(
		&ClientCore,
		&ServerCore,
		&Client,
		&Server,
		&Policy,
		0u
	);
	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshServiceRequestWrite(
			&Writer,
			XRT_STR_LITERAL(XSSH_SERVICE_USERAUTH)
		) == XSSH_OK), "ssh auth session limit service build failed");
	Payload = testSshAuthSessionPayload(&Writer, arrPayload);
	iWireSize = testSshAuthSessionSend(
		&Client,
		&ClientCore,
		Payload,
		arrWire,
		sizeof(arrWire),
		1u
	);
	testRequire(testSshAuthSessionReceivePrepare(
		&Server,
		&ServerCore,
		arrWire,
		iWireSize,
		arrPlain,
		sizeof(arrPlain),
		1u
	) == XSSH_AUTH_SESSION_PACKET_SERVICE_REQUEST,
		"ssh auth session limit service receive failed");
	testSshAuthSessionReceiveCommit(&Server, &ServerCore, 1u);
	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshServiceAcceptWrite(
			&Writer,
			XRT_STR_LITERAL(XSSH_SERVICE_USERAUTH)
		) == XSSH_OK), "ssh auth session limit accept build failed");
	Payload = testSshAuthSessionPayload(&Writer, arrPayload);
	iWireSize = testSshAuthSessionSend(
		&Server,
		&ServerCore,
		Payload,
		arrWire,
		sizeof(arrWire),
		2u
	);
	testRequire(testSshAuthSessionReceivePrepare(
		&Client,
		&ClientCore,
		arrWire,
		iWireSize,
		arrPlain,
		sizeof(arrPlain),
		2u
	) == XSSH_AUTH_SESSION_PACKET_SERVICE_ACCEPT,
		"ssh auth session limit accept receive failed");
	testSshAuthSessionReceiveCommit(&Client, &ClientCore, 2u);

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshAuthNoneWrite(
			&Writer,
			XRT_STR_LITERAL("alice")
		) == XSSH_OK), "ssh auth session first attempt build failed");
	Payload = testSshAuthSessionPayload(&Writer, arrPayload);
	iWireSize = testSshAuthSessionSend(
		&Client,
		&ClientCore,
		Payload,
		arrWire,
		sizeof(arrWire),
		3u
	);
	testRequire(testSshAuthSessionReceivePrepare(
		&Server,
		&ServerCore,
		arrWire,
		iWireSize,
		arrPlain,
		sizeof(arrPlain),
		3u
	) == XSSH_AUTH_SESSION_PACKET_REQUEST,
		"ssh auth session first attempt receive failed");
	testSshAuthSessionReceiveCommit(&Server, &ServerCore, 3u);
	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshAuthFailureWrite(
			&Writer,
			XRT_STR_LITERAL("password"),
			false
		) == XSSH_OK), "ssh auth session limit failure build failed");
	Payload = testSshAuthSessionPayload(&Writer, arrPayload);
	iWireSize = testSshAuthSessionSend(
		&Server,
		&ServerCore,
		Payload,
		arrWire,
		sizeof(arrWire),
		4u
	);
	testRequire(testSshAuthSessionReceivePrepare(
		&Client,
		&ClientCore,
		arrWire,
		iWireSize,
		arrPlain,
		sizeof(arrPlain),
		4u
	) == XSSH_AUTH_SESSION_PACKET_FAILURE,
		"ssh auth session limit failure receive failed");
	testSshAuthSessionReceiveCommit(&Client, &ClientCore, 4u);

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshAuthNoneWrite(
			&Writer,
			XRT_STR_LITERAL("alice")
		) == XSSH_OK), "ssh auth session second attempt build failed");
	Payload = testSshAuthSessionPayload(&Writer, arrPayload);
	testRequire((xrtSshAuthSessionWritePrepare(
		&Client,
		&ClientCore,
		Payload,
		5u
	) == XSSH_ERROR_AUTHENTICATION) &&
		(xrtSshAuthSessionEvent(&Client) ==
		 XSSH_AUTH_SESSION_EVENT_FAILED) && !ClientCore.Write.Active &&
		(xrtSshAuthSessionBudget(&Client, &Budget) == XSSH_OK) &&
		(Budget.Attempts == 2u) &&
		(Budget.Exhaustion == XSSH_AUTH_EXHAUST_ATTEMPTS),
		"ssh auth session attempt limit did not fail atomically");
	xrtSshTransportCoreClear(&ClientCore);
	xrtSshTransportCoreClear(&ServerCore);
}



/* 验证单调时间边界、显式终止和对象清理。 */
static void testSshAuthSessionLifecycleEdges(void)
{
	xsshtransportcore Core;
	xsshauthsession Session;
	xsshauthguardpolicy Policy = { 0u };
	xsshauthguarddecision Decision = XSSH_AUTH_GUARD_IGNORE;

	Policy.TimeoutMs = 10u;
	testSshAuthSessionCoreOpen(&Core, XSSH_ROLE_CLIENT);
	testRequire(xrtSshAuthSessionInit(
		&Session,
		XSSH_ROLE_CLIENT
	) && (xrtSshAuthSessionBegin(
		&Session,
		&Core,
		&Policy,
		100u
	) == XSSH_OK) && (xrtSshAuthSessionCheck(
		&Session,
		109u,
		&Decision
	) == XSSH_OK) && (Decision == XSSH_AUTH_GUARD_ALLOW) &&
		(xrtSshAuthSessionCheck(
			&Session,
			110u,
			&Decision
		) == XSSH_OK) && (Decision == XSSH_AUTH_GUARD_DISCONNECT) &&
		(xrtSshAuthSessionEvent(&Session) ==
		 XSSH_AUTH_SESSION_EVENT_FAILED) &&
		(xrtSshAuthSessionCheck(
			&Session,
			111u,
			&Decision
		) == XSSH_ERROR_STATE) &&
		!xrtSshAuthSessionComplete(&Session, &Core),
		"ssh auth session timeout boundary failed");
	xrtSshAuthSessionClear(&Session);
	testRequire((xrtSshAuthSessionEvent(&Session) ==
		XSSH_AUTH_SESSION_EVENT_NONE) &&
		!xrtSshAuthSessionComplete(&Session, &Core),
		"ssh auth session clear failed");

	testRequire(xrtSshAuthSessionInit(
		&Session,
		XSSH_ROLE_CLIENT
	) && (xrtSshAuthSessionBegin(
		&Session,
		&Core,
		NULL,
		200u
	) == XSSH_OK), "ssh auth session restart setup failed");
	xrtSshAuthSessionFail(&Session);
	testRequire(xrtSshAuthSessionEvent(&Session) ==
		XSSH_AUTH_SESSION_EVENT_FAILED,
		"ssh auth session explicit fail failed");
	xrtSshTransportCoreClear(&Core);
}



/* 运行无网络认证会话的事务、资源和双端状态测试。 */
int main(void)
{
	testSshAuthSessionFlow();
	testSshAuthSessionAbort();
	testSshAuthSessionAttemptLimit();
	testSshAuthSessionLifecycleEdges();
	return 0;
}
