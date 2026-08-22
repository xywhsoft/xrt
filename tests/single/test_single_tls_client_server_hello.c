#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件客户端能从首航推进到 TLS 1.3 握手密钥 epoch。 */
int main(void)
{
	static const xstrview Protocols[] = {
		{ "h2", sizeof("h2") - 1u }
	};
	uint8 PrivateKey[56];
	uint8 PublicKey[97];
	uint8 Extensions[128];
	uint8 Body[256];
	uint8 ServerHello[320];
	uint8 RecordBytes[384];
	xtlsclientconfig Config;
	xtlssession* pSession;
	xtlssession* pServer = NULL;
	xnetspan Span;
	xtlsrecord Record;
	xtlshandshake Handshake;
	xtlsclienthello ClientHello;
	xtlsextension Extension;
	xtlskeysharecursor Cursor;
	xtlskeyshare ClientShare;
	xtlskeyshare ServerShare;
	xtlswriter Writer;
	xtlsserverhello Hello;
	const xtlsgroupinfo* pGroup;
	xbytesview Selected = XRT_BYTES_LITERAL("h2");
	xbytesview Protocol;
	xbytesview ExtensionData;
	uint16 iCipher;
	size_t iBodySize;
	size_t iHelloSize;
	size_t iRecordSize;
	bool bResult;

	xrtTlsClientConfigInit(&Config);
	Config.ServerName = XRT_STR_LITERAL("example.com");
	Config.Protocols = Protocols;
	Config.ProtocolCount = sizeof(Protocols) / sizeof(Protocols[0]);
	pSession = xrtTlsClientCreate(&Config, NULL);
	if ( pSession == NULL ) {
		return 1;
	}
	bResult = xrtTlsSessionSendFront(pSession, &Span) &&
		(xrtTlsRecordParse(
			(xbytesview) { Span.Data, Span.Size }, &Record, NULL
		) == XTLS_OK) &&
		(xrtTlsHandshakeParse(
			Record.Payload, &Handshake, NULL
		) == XTLS_OK) &&
		xrtTlsClientHelloParse(Handshake.Body, &ClientHello) &&
		xrtTlsIdsGet(&ClientHello.CipherSuites, 0, &iCipher) &&
		(xrtTlsExtensionsFind(
			ClientHello.Extensions, XTLS_EXTENSION_KEY_SHARE, &Extension
		) == XTLS_ITEM_VALUE) &&
		xrtTlsClientKeyShares(Extension.Data, &Cursor) &&
		(xrtTlsKeySharesRead(&Cursor, &ClientShare) == XTLS_ITEM_VALUE);
	pGroup = bResult ? xrtTlsGroupInfo(ClientShare.Group) : NULL;
	bResult = bResult && (pGroup != NULL) &&
		xrtTlsKeyShareGenerate(
			ClientShare.Group,
			PrivateKey, sizeof(PrivateKey),
			PublicKey, sizeof(PublicKey)
		);
	if ( bResult ) {
		ServerShare.Group = ClientShare.Group;
		ServerShare.Key = (xbytesview) {
			PublicKey, pGroup->PublicSize
		};
		bResult = xrtTlsWriterInit(
			&Writer, Extensions, sizeof(Extensions)
		) && xrtTlsWriterServerVersion(
			&Writer, XTLS_VERSION_13
		) && xrtTlsWriterServerKeyShare(&Writer, &ServerShare);
	}
	if ( bResult ) {
		memset(&Hello, 0, sizeof(Hello));
		Hello.LegacyVersion = XTLS_VERSION_12;
		Hello.Random = XRT_BYTES_LITERAL(
			"0123456789abcdef0123456789abcdef"
		);
		Hello.SessionId = ClientHello.SessionId;
		Hello.CipherSuite = iCipher;
		Hello.Extensions = xrtTlsWriterData(&Writer);
		iBodySize = xrtTlsServerHelloSize(&Hello);
		iHelloSize = xrtTlsHandshakeSize(iBodySize);
		iRecordSize = xrtTlsRecordSize(iHelloSize);
		bResult = (iBodySize <= sizeof(Body)) &&
			(iHelloSize <= sizeof(ServerHello)) &&
			(iRecordSize <= sizeof(RecordBytes)) &&
			xrtTlsServerHelloEncode(&Hello, Body, sizeof(Body)) &&
			xrtTlsHandshakeEncode(
				XTLS_HANDSHAKE_SERVER_HELLO,
				(xbytesview) { Body, iBodySize },
				ServerHello, sizeof(ServerHello)
			) && xrtTlsRecordEncode(
				XTLS_RECORD_HANDSHAKE, XTLS_VERSION_12,
				(xbytesview) { ServerHello, iHelloSize },
				RecordBytes, sizeof(RecordBytes)
			);
	}
	if ( bResult ) {
		bResult = xrtTlsSessionSendConsume(
			pSession, xrtTlsSessionSendSize(pSession)
		) && (xrtTlsSessionFeed(
			pSession, RecordBytes, iRecordSize
		) == XTLS_OK) && (xrtTlsClientDrive(
			pSession
		) == XTLS_OK) &&
			(xrtTlsSessionState(pSession) == XTLS_STATE_HANDSHAKE) &&
			(xrtTlsSessionFeedSize(pSession) == 0) &&
			(xrtTlsClientDrive(pSession) == XTLS_AGAIN);
	}
	if ( bResult ) {
		pServer = __xrtTlsSessionCreate(
			pSession->Context, NULL, XTLS_SERVER
		);
		if ( pServer != NULL ) {
			pServer->WriteKey = pSession->ReadKey;
		}
		bResult = (pServer != NULL) && xrtTlsWriterInit(
			&Writer, Extensions, sizeof(Extensions)
		) && xrtTlsWriterProtocols(&Writer, &Selected, 1u);
	}
	if ( bResult ) {
		ExtensionData = xrtTlsWriterData(&Writer);
		iBodySize = xrtTlsEncryptedExtensionsSize(ExtensionData);
		iHelloSize = xrtTlsHandshakeSize(iBodySize);
		bResult = (iBodySize <= sizeof(Body)) &&
			(iHelloSize <= sizeof(ServerHello)) &&
			xrtTlsEncryptedExtensionsEncode(
				ExtensionData, Body, sizeof(Body)
			) && xrtTlsHandshakeEncode(
				XTLS_HANDSHAKE_ENCRYPTED_EXTENSIONS,
				(xbytesview) { Body, iBodySize },
				ServerHello, sizeof(ServerHello)
			) && (__xrtTlsSessionRecordProtect(
				pServer, XTLS_RECORD_HANDSHAKE,
				(xbytesview) { ServerHello, iHelloSize }, 0
			) == XTLS_OK);
	}
	if ( bResult ) {
		bResult = xrtTlsSessionSendFront(pServer, &Span) &&
			(xrtTlsSessionFeed(
				pSession, Span.Data, Span.Size
			) == XTLS_OK) && xrtTlsSessionSendConsume(
				pServer, Span.Size
			) && (xrtTlsClientDrive(pSession) == XTLS_OK) &&
			xrtTlsSessionProtocol(pSession, &Protocol) &&
			(Protocol.Size == Selected.Size) &&
			(memcmp(Protocol.Data, Selected.Data, Selected.Size) == 0) &&
			(xrtTlsSessionFeedSize(pSession) == 0);
	}
	xrtTlsSessionDestroy(pServer);
	xrtTlsSessionDestroy(pSession);
	xrtSecureZero(PrivateKey, sizeof(PrivateKey));
	return bResult ? 0 : 1;
}
