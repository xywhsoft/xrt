#include "../fixtures/tls_server.h"
#include "../../src/internal/xrt_tls_client.h"
#include "../../src/internal/xrt_tls_server.h"



/* TLS 1.2 与 TLS 1.3 使用同一个选择器 Cookie 契约。 */
static bool testTlsServer12Select(
	ptr pContext,
	const xtlsserverrequest* pRequest,
	xtlsserverchoice* pChoice
)
{
	(void)pContext;
	if ( (pRequest == NULL) || (pChoice == NULL) ||
		(pChoice->Identity == NULL) ) {
		return false;
	}
	pChoice->Cookie = UINT64_C(0x12C0011E12C0011E);
	return true;
}



/* 创建只开放 TLS 1.2 ECDHE-RSA-AES-GCM 的测试上下文。 */
static xtlscontext* testTlsServer12Context(void)
{
	static const xtlsversion Versions[] = { XTLS_VERSION_12 };
	static const xtlscipher Ciphers[] = {
		XTLS_ECDHE_RSA_AES_128_GCM_SHA256
	};
	static const uint16 Groups[] = { XTLS_GROUP_X25519 };
	static const xtlssignature Signatures[] = {
		XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256,
		XTLS_SIGNATURE_RSA_PKCS1_SHA256
	};
	xtlspolicy Policy;
	xtlscontextconfig Config;

	xrtTlsPolicyInit(&Policy);
	Policy.Versions = Versions;
	Policy.VersionCount = sizeof(Versions) / sizeof(Versions[0]);
	Policy.Ciphers = Ciphers;
	Policy.CipherCount = sizeof(Ciphers) / sizeof(Ciphers[0]);
	Policy.Groups = Groups;
	Policy.GroupCount = sizeof(Groups) / sizeof(Groups[0]);
	Policy.Signatures = Signatures;
	Policy.SignatureCount = sizeof(Signatures) / sizeof(Signatures[0]);
	xrtTlsContextConfigInit(&Config);
	Config.Policy = &Policy;
	Config.Limits.RecordBudget = 4u;
	Config.Limits.HandshakeBudget = 4u;
	return xrtTlsContextCreate(&Config);
}



/* Give the client a handshake split independently from the peer's records. */
static xtlsresult testTlsClient12RequestFeed(
	xtlssession* pClient,
	xbytesview Message
)
{
	uint8 Wire[4096];
	size_t iWire = xrtTlsRecordSize(Message.Size);
	xtlsresult Result = XTLS_AGAIN;

	testRequire((iWire <= sizeof(Wire)) && xrtTlsRecordEncode(
		XTLS_RECORD_HANDSHAKE, XTLS_VERSION_12, Message, Wire, sizeof(Wire)
	), "TLS 1.2 request fixture record encoding failed");
	for ( size_t i = 0; i < iWire; i++ ) {
		testRequire(xrtTlsSessionFeed(pClient, Wire + i, 1u) == XTLS_OK,
			"TLS 1.2 request fixture feed failed");
		Result = xrtTlsClientDrive(pClient);
		if ( Result == XTLS_ERROR ) {
			break;
		}
	}
	return Result;
}



/* The native server does not implement client authentication. This wire
 * fixture inserts CertificateRequest before ServerHelloDone, then checks and
 * consumes only the empty client Certificate on the server's behalf. Both
 * Finished messages and the EMS schedule still use the complete transcript. */
static void testTlsClient12RequestAlert(xtlssession* pClient, xtlsalert Expected)
{
	xnetspan Span;
	xtlsrecord Record;
	xtlsalertlevel Level;
	xtlsalert Alert;

	testRequire(xrtTlsSessionSendFront(pClient, &Span) &&
		(xrtTlsRecordParse((xbytesview) { Span.Data, Span.Size }, &Record, NULL) == XTLS_OK) &&
		(Record.EncodedSize == Span.Size) && (Record.Type == XTLS_RECORD_ALERT) &&
		xrtTlsAlertParse(Record.Payload, &Level, &Alert) &&
		(Level == XTLS_ALERT_FATAL) && (Alert == Expected),
		"TLS 1.2 rejected request fatal alert code differs");
}



static void testTlsClient12CertificateRequest(
	const xtlsclientconfig* pClientConfig,
	const xtlsserverconfig* pServerConfig,
	size_t iMode
)
{
	static const uint8 EmptyCertificate[] = { 11u, 0, 0, 3u, 0, 0, 0 };
	uint8 Body[] = { 1u, 1u, 0, 2u, 8u, 4u, 0, 0 };
	uint8 Request[32];
	uint8 Wire[8192];
	uint8 Forward[256];
	uint8 Before[32];
	uint8 After[32];
	xtlssession* pClient = xrtTlsClientCreate(pClientConfig, NULL);
	xtlssession* pServer = xrtTlsServerCreate(pServerConfig, NULL);
	xtlsclientstate* pState;
	xtlsserverstate* pServerState;
	xtlstranscript Transcript;
	test_tls_server_rng Rng = { UINT32_C(0xCE2712) };
	xnetspan Span;
	xtlsrecord Record;
	xtlshandshake Message;
	size_t iRequest = xrtTlsHandshakeSize(sizeof(Body));
	bool bInserted = false;
	bool bCertificate = false;

	testRequire((pClient != NULL) && (pServer != NULL),
		"TLS 1.2 request fixture creation failed");
	pState = (xtlsclientstate*)__xrtTlsSessionRoleData(pClient);
	pServerState = (xtlsserverstate*)__xrtTlsSessionRoleData(pServer);
	if ( iMode == 2u ) {
		Body[1] = 99u;
		Body[4] = 0xFEu;
		Body[5] = 0xFEu;
	} else if ( iMode == 4u ) {
		Body[0] = 0;
	} else if ( iMode == 5u ) {
		Body[3] = 1u;
	} else if ( iMode == 6u ) {
		Body[7] = 1u;
	}
	testRequire(xrtTlsHandshakeEncode(XTLS_HANDSHAKE_CERTIFICATE_REQUEST,
		(xbytesview) { Body, sizeof(Body) }, Request, sizeof(Request)) &&
		__xrtTlsTranscriptInit(&Transcript, XCRYPTO_HASH_SHA256) &&
		xrtTlsSessionSendFront(pClient, &Span) &&
		(xrtTlsRecordParse((xbytesview) { Span.Data, Span.Size },
			&Record, NULL) == XTLS_OK) &&
		__xrtTlsTranscriptUpdate(&Transcript, Record.Payload) &&
		(xrtTlsSessionFeed(pServer, Span.Data, Span.Size) == XTLS_OK) &&
		xrtTlsSessionSendConsume(pClient, Span.Size) &&
		(xrtTlsServerDrive(pServer) == XTLS_OK),
		"TLS 1.2 request fixture ClientHello failed");
	while ( xrtTlsSessionSendSize(pServer) != 0 ) {
		size_t iWire;
		size_t iOffset = 0;

		testRequire(xrtTlsSessionSendFront(pServer, &Span) &&
			(Span.Size <= sizeof(Wire)), "TLS 1.2 server flight is too large");
		iWire = Span.Size;
		memcpy(Wire, Span.Data, iWire);
		testRequire(xrtTlsSessionSendConsume(pServer, iWire),
			"TLS 1.2 server flight consume failed");
		while ( iOffset < iWire ) {
			size_t iMessage = 0;

			testRequire(xrtTlsRecordParse((xbytesview) { Wire + iOffset,
				iWire - iOffset }, &Record, NULL) == XTLS_OK,
				"TLS 1.2 server record parse failed");
			iOffset += Record.EncodedSize;
			while ( iMessage < Record.Payload.Size ) {
				xbytesview Encoded;

				testRequire(xrtTlsHandshakeParse((xbytesview) {
					Record.Payload.Data + iMessage, Record.Payload.Size - iMessage
				}, &Message, NULL) == XTLS_OK,
					"TLS 1.2 server message parse failed");
				Encoded = (xbytesview) { Record.Payload.Data + iMessage,
					Message.EncodedSize };
				iMessage += Message.EncodedSize;
				if ( (Message.Type == XTLS_HANDSHAKE_SERVER_HELLO_DONE) ||
					((iMode == 7u) && (Message.Type == XTLS_HANDSHAKE_SERVER_KEY_EXCHANGE)) ) {
					xtlsresult Result = testTlsClient12RequestFeed(pClient,
						(xbytesview) { Request, iRequest });

					if ( (iMode >= 4u) && (iMode <= 7u) ) {
						testRequire((Result == XTLS_ERROR) &&
							(xrtTlsSessionState(pClient) == XTLS_STATE_FAILED) &&
							!pState->CertificateRequested &&
							(xrtErrorKind(xrtGetError()) == XERR_PROTOCOL),
							"TLS 1.2 accepted a malformed or early request");
						testTlsClient12RequestAlert(pClient, iMode == 7u ?
							XTLS_ALERT_UNEXPECTED_MESSAGE : XTLS_ALERT_DECODE_ERROR);
						goto cleanup;
					}
					testRequire((Result == XTLS_OK) && pState->CertificateRequested,
						"TLS 1.2 optional request failed");
					if ( iMode == 3u ) {
						testRequire(testTlsClient12RequestFeed(pClient,
							(xbytesview) { Request, iRequest }) == XTLS_ERROR,
							"TLS 1.2 accepted a duplicate request");
						testTlsClient12RequestAlert(pClient, XTLS_ALERT_UNEXPECTED_MESSAGE);
						goto cleanup;
					}
					testRequire(__xrtTlsTranscriptUpdate(&Transcript,
						(xbytesview) { Request, iRequest }),
						"TLS 1.2 request transcript failed");
					bInserted = true;
					if ( iMode == 8u ) {
						size_t iForward = xrtTlsRecordSize(Encoded.Size);

						testRequire(xrtTlsRecordEncode(XTLS_RECORD_HANDSHAKE, XTLS_VERSION_12,
							Encoded, Forward, sizeof(Forward)) &&
							(xrtTlsSessionFeed(pClient, Forward, iForward) == XTLS_OK) &&
							__xrtTlsTranscriptDigest(&pState->Transcript, Before, sizeof(Before)) &&
							xrtMemDebugFailAfter(0), "TLS 1.2 request output OOM setup failed");
						Result = xrtTlsClientDrive(pClient);
						testRequire(xrtMemDebugFailTriggered(),
							"TLS 1.2 request output OOM did not reach an allocation");
						xrtMemDebugFailClear();
						testRequire((Result == XTLS_ERROR) &&
							(xrtTlsSessionState(pClient) == XTLS_STATE_FAILED) &&
							!pClient->WriteKey.Ready && !pState->PendingReadKey.Ready &&
							(xrtErrorIs(xrtGetError(), XERR_MEMORY) != NULL) &&
							__xrtTlsTranscriptDigest(&pState->Transcript, After, sizeof(After)) &&
							(memcmp(Before, After, sizeof(Before)) == 0),
							"TLS 1.2 request output OOM committed transcript or epoch");
						goto cleanup;
					} else if ( iMode == 1u ) {
						const xtlslimits* pLimits = xrtTlsContextLimits(pClientConfig->Context);
						/* X25519 + AES-128-GCM: 100 wire bytes including Certificate. */
						size_t iFill = pLimits->SendLimit - 99u;
						bytes pFill = (bytes)xrtMalloc(iFill);

						testRequire(pFill != NULL, "TLS 1.2 filler allocation failed");
						memset(pFill, 0xA5, iFill);
						testRequire((__xrtTlsSessionSend(pClient, pFill, iFill) == XTLS_OK) &&
							__xrtTlsTranscriptDigest(&pState->Transcript, Before, sizeof(Before)),
							"TLS 1.2 output backpressure setup failed");
						xrtFree(pFill);
						testRequire((testTlsClient12RequestFeed(pClient, Encoded) == XTLS_AGAIN) &&
							(pState->Step == XTLS_CLIENT_WAIT_SERVER_HELLO_DONE) &&
							(pState->RecordOffset == 0) && !pClient->WriteKey.Ready &&
							(xrtTlsSessionSendSize(pClient) == iFill) &&
							__xrtTlsTranscriptDigest(&pState->Transcript, After, sizeof(After)) &&
							(memcmp(Before, After, sizeof(Before)) == 0) &&
							xrtTlsSessionSendConsume(pClient, iFill) &&
							(xrtTlsClientDrive(pClient) == XTLS_OK),
							"TLS 1.2 output backpressure changed transcript or epoch");
					} else {
						testRequire(testTlsClient12RequestFeed(pClient, Encoded) == XTLS_OK,
							"TLS 1.2 ServerHelloDone after request failed");
					}
				} else {
					testRequire(testTlsClient12RequestFeed(pClient, Encoded) == XTLS_OK,
						"TLS 1.2 server flight before request failed");
				}
				testRequire(__xrtTlsTranscriptUpdate(&Transcript, Encoded),
					"TLS 1.2 server transcript rebuild failed");
			}
		}
	}
	testRequire(bInserted, "TLS 1.2 fixture omitted the request");
	__xrtTlsTranscriptClear(&pServerState->Transcript);
	pServerState->Transcript = Transcript;
	memset(&Transcript, 0, sizeof(Transcript));
	while ( xrtTlsSessionSendSize(pClient) != 0 ) {
		size_t iWire;
		size_t iOffset = 0;

		testRequire(xrtTlsSessionSendFront(pClient, &Span) &&
			(Span.Size <= sizeof(Wire)), "TLS 1.2 client flight is too large");
		iWire = Span.Size;
		memcpy(Wire, Span.Data, iWire);
		testRequire(xrtTlsSessionSendConsume(pClient, iWire),
			"TLS 1.2 client flight consume failed");
		while ( iOffset < iWire ) {
			xbytesview Forwarded;

			testRequire(xrtTlsRecordParse((xbytesview) { Wire + iOffset,
				iWire - iOffset }, &Record, NULL) == XTLS_OK,
				"TLS 1.2 client record parse failed");
			Forwarded = (xbytesview) { Wire + iOffset, Record.EncodedSize };
			iOffset += Record.EncodedSize;
			if ( !bCertificate ) {
				testRequire((Record.Type == XTLS_RECORD_HANDSHAKE) &&
					(Record.Payload.Size > sizeof(EmptyCertificate)) &&
					(memcmp(Record.Payload.Data, EmptyCertificate, sizeof(EmptyCertificate)) == 0) &&
					(xrtTlsHandshakeParse((xbytesview) {
						Record.Payload.Data + sizeof(EmptyCertificate),
						Record.Payload.Size - sizeof(EmptyCertificate)
					}, &Message, NULL) == XTLS_OK) &&
					(Message.Type == XTLS_HANDSHAKE_CLIENT_KEY_EXCHANGE) &&
					(Message.EncodedSize + sizeof(EmptyCertificate) == Record.Payload.Size) &&
					__xrtTlsTranscriptUpdate(&pServerState->Transcript,
						(xbytesview) { EmptyCertificate, sizeof(EmptyCertificate) }) &&
					xrtTlsRecordEncode(XTLS_RECORD_HANDSHAKE, XTLS_VERSION_12,
						(xbytesview) { Record.Payload.Data + sizeof(EmptyCertificate),
							Message.EncodedSize }, Forward, sizeof(Forward)),
					"TLS 1.2 did not send exactly one empty Certificate before ClientKeyExchange");
				Forwarded = (xbytesview) { Forward, xrtTlsRecordSize(Message.EncodedSize) };
				bCertificate = true;
			}
			testRequire((xrtTlsSessionFeed(pServer, Forwarded.Data, Forwarded.Size) == XTLS_OK) &&
				(xrtTlsServerDrive(pServer) == XTLS_OK),
				"TLS 1.2 peer rejected EMS or client Finished after empty Certificate");
		}
	}
	testRequire(bCertificate && testTlsServerHandshake(pClient, pServer, &Rng) &&
		testTlsServerTransfer(pClient, pServer, true, "request client", 14u, &Rng) &&
		testTlsServerTransfer(pServer, pClient, false, "request server", 14u, &Rng),
		"TLS 1.2 request handshake or application keys differ");

cleanup:
	__xrtTlsTranscriptClear(&Transcript);
	xrtTlsSessionDestroy(pServer);
	xrtTlsSessionDestroy(pClient);
	xrtClearError();
}



/* 修改首条 Hello 的扩展，但保持外层长度正确，验证角色层的初始绑定检查。 */
static void testTlsServer12Renegotiation(
	const xtlsclientconfig* pClientConfig,
	const xtlsserverconfig* pServerConfig
)
{
	for ( size_t i = 0; i < 3u; i++ ) {
		xtlssession* pClient = xrtTlsClientCreate(pClientConfig, NULL);
		xtlssession* pServer = xrtTlsServerCreate(pServerConfig, NULL);
		xtlssession* pSource = i == 0 ? pClient : pServer;
		xtlssession* pTarget = i == 0 ? pServer : pClient;
		xnetspan Span;
		xtlsrecord Record;
		xtlshandshake Message;
		xtlsclienthello ClientHello;
		xtlsserverhello ServerHello;
		xtlsextensioncursor Cursor;
		xtlsextension Extension;
		xtlsitemresult Result;
		xtlswriter Writer;
		xbytesview Extensions;
		uint8 Storage[1024];
		uint8 Body[2048];
		uint8 Wire[2057];
		static const uint8 Binding[] = { 1u, 0xA5u };
		size_t iRequired;
		size_t iSize;
		bool bFound = false;

		testRequire((pClient != NULL) && (pServer != NULL),
			"TLS 1.2 binding roles creation failed");
		if ( i != 0 ) {
			testRequire(xrtTlsSessionSendFront(pClient, &Span) &&
				(xrtTlsSessionFeed(pServer, Span.Data, Span.Size) == XTLS_OK) &&
				xrtTlsSessionSendConsume(pClient, Span.Size) &&
				(xrtTlsServerDrive(pServer) == XTLS_OK),
				"TLS 1.2 binding ServerHello setup failed");
		}
		testRequire(xrtTlsSessionSendFront(pSource, &Span) &&
			(xrtTlsRecordParse((xbytesview) { Span.Data, Span.Size },
				&Record, &iRequired) == XTLS_OK) &&
			(xrtTlsHandshakeParse(Record.Payload, &Message, &iRequired) == XTLS_OK),
			"TLS 1.2 binding Hello parsing failed");
		if ( i == 0 ) {
			testRequire(xrtTlsClientHelloParse(Message.Body, &ClientHello),
				"TLS 1.2 binding ClientHello parsing failed");
			Extensions = ClientHello.Extensions;
		} else {
			testRequire(xrtTlsServerHelloParse(Message.Body, &ServerHello),
				"TLS 1.2 binding ServerHello parsing failed");
			Extensions = ServerHello.Extensions;
		}
		testRequire(xrtTlsExtensionsInit(&Cursor, Extensions) &&
			xrtTlsWriterInit(&Writer, Storage, sizeof(Storage)),
			"TLS 1.2 binding extension setup failed");
		while ( (Result = xrtTlsExtensionsRead(&Cursor, &Extension)) == XTLS_ITEM_VALUE ) {
			if ( Extension.Type == XTLS_EXTENSION_RENEGOTIATION_INFO ) {
				bFound = true;
				testRequire((Extension.Data.Size == 1u) && (Extension.Data.Data[0] == 0),
					"TLS 1.2 did not send an empty initial binding");
				if ( i == 2u ) {
					continue;
				}
				Extension.Data = (xbytesview) { Binding, sizeof(Binding) };
			}
			testRequire(xrtTlsWriterExtension(&Writer, Extension.Type, Extension.Data),
				"TLS 1.2 binding extension encoding failed");
		}
		testRequire(bFound && (Result == XTLS_ITEM_DONE),
			"TLS 1.2 initial binding extension is missing");
		Extensions = (xbytesview) { Storage, Writer.Size };
		if ( i == 0 ) {
			ClientHello.Extensions = Extensions;
			iSize = xrtTlsClientHelloSize(&ClientHello);
			testRequire(xrtTlsClientHelloEncode(&ClientHello, Body, sizeof(Body)),
				"TLS 1.2 binding ClientHello encoding failed");
		} else {
			ServerHello.Extensions = Extensions;
			iSize = xrtTlsServerHelloSize(&ServerHello);
			testRequire(xrtTlsServerHelloEncode(&ServerHello, Body, sizeof(Body)),
				"TLS 1.2 binding ServerHello encoding failed");
		}
		testRequire(xrtTlsHandshakeEncode(Message.Type,
			(xbytesview) { Body, iSize }, Wire + XTLS_RECORD_HEADER_SIZE,
			sizeof(Wire) - XTLS_RECORD_HEADER_SIZE) &&
			xrtTlsRecordEncode(XTLS_RECORD_HANDSHAKE, XTLS_VERSION_12,
				(xbytesview) { Wire + XTLS_RECORD_HEADER_SIZE, xrtTlsHandshakeSize(iSize) },
				Wire, sizeof(Wire)) &&
			(xrtTlsSessionFeed(pTarget, Wire,
				xrtTlsRecordSize(xrtTlsHandshakeSize(iSize))) == XTLS_OK) &&
			((i == 0 ? xrtTlsServerDrive(pTarget) : xrtTlsClientDrive(pTarget)) == XTLS_ERROR) &&
			(xrtTlsSessionState(pTarget) == XTLS_STATE_FAILED) &&
			(xrtErrorCode(xrtGetError()) == XTLS_ERROR_EXTENSION),
			"TLS 1.2 accepted a nonempty or missing initial binding");
		xrtTlsSessionDestroy(pServer);
		xrtTlsSessionDestroy(pClient);
		xrtClearError();
	}
}



/* 验证 TLS 1.2 完整握手、协商快照、数据、版本边界和认证关闭。 */
int main(void)
{
	testRequire(xrtMemDebugEnable(true), "TLS 1.2 memory fault instrumentation failed");
	static const xstrview Protocols[] = {
		XRT_STR_INIT("h2"),
		XRT_STR_INIT("http/1.1")
	};
	static const char ClientData[] =
		"TLS 1.2 client fragmented application payload";
	static const char ServerData[] =
		"TLS 1.2 server fragmented application payload";
	static const uint8 TicketData[] = { 0x12u, 0x34u, 0x56u, 0x78u };
	xtlscontext* pContext = testTlsServer12Context();
	xtlsidentity* pIdentity = testTlsServerIdentity();
	xtlsverifierconfig VerifierConfig;
	xtlsverifier* pVerifier;
	xtlsclientconfig ClientConfig;
	xtlsserverconfig ServerConfig;
	test_tls_server_rng Rng = { UINT32_C(0xD4C3B2A1) };
	xtlssession* pClient;
	xtlssession* pServer;
	xtlsresume* pResume = NULL;
	xbytesview Name;
	xbytesview ClientProtocol;
	xbytesview ServerProtocol;
	uint64 iCookie = 0;

	testRequire((pContext != NULL) && (pIdentity != NULL),
		"TLS 1.2 fixture creation failed");
	xrtTlsVerifierConfigInit(&VerifierConfig);
	VerifierConfig.Verify = testTlsServerAccept;
	pVerifier = xrtTlsVerifierCreate(&VerifierConfig);
	testRequire(pVerifier != NULL, "TLS 1.2 verifier creation failed");

	xrtTlsClientConfigInit(&ClientConfig);
	ClientConfig.Context = pContext;
	ClientConfig.ServerName = XRT_STR_LITERAL("example.com");
	ClientConfig.Protocols = Protocols;
	ClientConfig.ProtocolCount = sizeof(Protocols) / sizeof(Protocols[0]);
	ClientConfig.Verifier = pVerifier;
	xrtTlsServerConfigInit(&ServerConfig);
	ServerConfig.Context = pContext;
	ServerConfig.Identity = pIdentity;
	ServerConfig.Protocols = Protocols;
	ServerConfig.ProtocolCount = sizeof(Protocols) / sizeof(Protocols[0]);
	ServerConfig.Select = testTlsServer12Select;
	ServerConfig.RequireProtocol = true;
	for ( size_t i = 0; i < 9u; i++ ) {
		testTlsClient12CertificateRequest(&ClientConfig, &ServerConfig, i);
	}
	testTlsServer12Renegotiation(&ClientConfig, &ServerConfig);
	pClient = xrtTlsClientCreate(&ClientConfig, NULL);
	pServer = xrtTlsServerCreate(&ServerConfig, NULL);
	testRequire((pClient != NULL) && (pServer != NULL),
		"TLS 1.2 client or server creation failed");
	testRequire(testTlsServerHandshake(pClient, pServer, &Rng),
		"TLS 1.2 fragmented handshake failed");

	testRequire((xrtTlsSessionVersion(pClient) == XTLS_VERSION_12) &&
		(xrtTlsSessionVersion(pServer) == XTLS_VERSION_12) &&
		(xrtTlsSessionCipher(pClient) ==
			XTLS_ECDHE_RSA_AES_128_GCM_SHA256) &&
		(xrtTlsSessionCipher(pServer) ==
			XTLS_ECDHE_RSA_AES_128_GCM_SHA256),
		"TLS 1.2 negotiated snapshot differs");
	testRequire((xrtTlsClientCertificateCount(pClient) == 1u) &&
		xrtTlsServerName(pServer, &Name) && (Name.Size == 11u) &&
		(memcmp(Name.Data, "example.com", 11u) == 0),
		"TLS 1.2 certificate or SNI state differs");
	testRequire(xrtTlsServerCookie(pServer, &iCookie) &&
		(iCookie == UINT64_C(0x12C0011E12C0011E)),
		"TLS 1.2 server did not retain the selector Cookie");
	testRequire(xrtTlsSessionProtocol(pClient, &ClientProtocol) &&
		xrtTlsSessionProtocol(pServer, &ServerProtocol) &&
		testTlsServerViewEqual(ClientProtocol, ServerProtocol) &&
		(ClientProtocol.Size == 2u) &&
		(memcmp(ClientProtocol.Data, "h2", 2u) == 0),
		"TLS 1.2 ALPN selection differs");

	testRequire(testTlsServerTransfer(
		pClient, pServer, true,
		ClientData, sizeof(ClientData) - 1u, &Rng
	) && testTlsServerTransfer(
		pServer, pClient, false,
		ServerData, sizeof(ServerData) - 1u, &Rng
	), "TLS 1.2 bidirectional transfer failed");
	testRequire((xrtTlsClientKeyUpdate(
		pClient, XTLS_KEY_UPDATE_NOT_REQUESTED
	) == XTLS_ERROR) && (xrtErrorKind(xrtGetError()) == XERR_UNSUPPORTED) &&
		(xrtTlsSessionState(pClient) == XTLS_STATE_READY),
		"TLS 1.2 client accepted KeyUpdate or damaged the session");
	testRequire((xrtTlsServerKeyUpdate(
		pServer, XTLS_KEY_UPDATE_NOT_REQUESTED
	) == XTLS_ERROR) && (xrtErrorKind(xrtGetError()) == XERR_UNSUPPORTED) &&
		(xrtTlsSessionState(pServer) == XTLS_STATE_READY),
		"TLS 1.2 server accepted KeyUpdate or damaged the session");
	testRequire((xrtTlsServerTicket(
		pServer,
		(xbytesview) { TicketData, sizeof(TicketData) }, 60u, &pResume
	) == XTLS_ERROR) && (pResume == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_UNSUPPORTED) &&
		(xrtTlsSessionState(pServer) == XTLS_STATE_READY),
		"TLS 1.2 server accepted a TLS 1.3 ticket");
	testRequire(testTlsServerTransfer(
		pClient, pServer, true,
		ClientData, sizeof(ClientData) - 1u, &Rng
	) && testTlsServerTransfer(
		pServer, pClient, false,
		ServerData, sizeof(ServerData) - 1u, &Rng
	), "TLS 1.2 transfer failed after rejected TLS 1.3 operations");
	testRequire(testTlsServerClose(pClient, pServer, &Rng),
		"TLS 1.2 close_notify exchange failed");

	xrtTlsSessionDestroy(pServer);
	xrtTlsSessionDestroy(pClient);
	/* 新增 RI 应答不能挤掉最大 255 字节 ALPN 的 ServerHello 容量。 */
	{
		char LongName[255];
		xstrview LongProtocol = { LongName, sizeof(LongName) };

		memset(LongName, 'x', sizeof(LongName));
		ClientConfig.Protocols = &LongProtocol;
		ClientConfig.ProtocolCount = 1;
		ServerConfig.Protocols = &LongProtocol;
		ServerConfig.ProtocolCount = 1;
		pClient = xrtTlsClientCreate(&ClientConfig, NULL);
		pServer = xrtTlsServerCreate(&ServerConfig, NULL);
		testRequire((pClient != NULL) && (pServer != NULL) &&
			testTlsServerHandshake(pClient, pServer, &Rng) &&
			xrtTlsSessionProtocol(pClient, &ClientProtocol) &&
			(ClientProtocol.Size == sizeof(LongName)) &&
			(memcmp(ClientProtocol.Data, LongName, sizeof(LongName)) == 0),
			"TLS 1.2 initial binding broke maximum ALPN size");
		xrtTlsSessionDestroy(pServer);
		xrtTlsSessionDestroy(pClient);
	}
	xrtTlsVerifierRelease(pVerifier);
	xrtTlsIdentityRelease(pIdentity);
	xrtTlsContextRelease(pContext);
	return 0;
}
