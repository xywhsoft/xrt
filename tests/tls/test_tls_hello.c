#include "tls_hello_vectors.h"



/* ClientHello 视图必须完整暴露线路字段和常用扩展。 */
static void testTlsClientHello(void)
{
	uint8 Data[256];
	size_t iSize = testTlsClientHelloVector(
		Data, sizeof(Data), NULL
	);
	xtlsclienthello Hello;
	xtlsextension Extension;
	xtlsservernamecursor Names;
	xtlsservername Name;
	xtlsprotocolcursor Protocols;
	xbytesview Protocol;
	xtlsids Groups;
	xtlskeysharecursor Shares;
	xtlskeyshare Share;
	uint16 iCipher = 0;

	testRequire(xrtTlsClientHelloParse(
		(xbytesview) { Data, iSize }, &Hello
	), "valid TLS ClientHello did not parse");
	testRequire((Hello.LegacyVersion == XTLS_VERSION_12) &&
		(Hello.Random.Data == Data + 2u) &&
		(Hello.Random.Size == XTLS_RANDOM_SIZE) &&
		(Hello.SessionId.Size == 0) &&
		(xrtTlsIdsCount(&Hello.CipherSuites) == 3u) &&
		xrtTlsIdsGet(&Hello.CipherSuites, 2u, &iCipher) &&
		(iCipher == XTLS_ECDHE_RSA_AES_128_GCM_SHA256) &&
		(Hello.CompressionMethods.Size == 1u) &&
		(Hello.CompressionMethods.Data[0] == 0),
		"TLS ClientHello fixed field views mismatch");

	testRequire(xrtTlsExtensionsFind(
		Hello.Extensions, XTLS_EXTENSION_SERVER_NAME, &Extension
	) == XTLS_ITEM_VALUE, "TLS ClientHello SNI extension was not found");
	testRequire(xrtTlsServerNames(Extension.Data, &Names) &&
		(xrtTlsServerNamesRead(&Names, &Name) == XTLS_ITEM_VALUE) &&
		(Name.Type == 0) && (Name.Name.Size == 11u) &&
		(memcmp(Name.Name.Data, "example.com", 11u) == 0) &&
		(xrtTlsServerNamesRead(&Names, &Name) == XTLS_ITEM_DONE),
		"TLS SNI name list view mismatch");

	testRequire(xrtTlsExtensionsFind(
		Hello.Extensions, XTLS_EXTENSION_ALPN, &Extension
	) == XTLS_ITEM_VALUE, "TLS ClientHello ALPN extension was not found");
	testRequire(xrtTlsProtocols(Extension.Data, &Protocols) &&
		(xrtTlsProtocolsRead(&Protocols, &Protocol) == XTLS_ITEM_VALUE) &&
		(Protocol.Size == 2u) &&
		(memcmp(Protocol.Data, "h2", 2u) == 0) &&
		(xrtTlsProtocolsRead(&Protocols, &Protocol) == XTLS_ITEM_VALUE) &&
		(Protocol.Size == 8u) &&
		(memcmp(Protocol.Data, "http/1.1", 8u) == 0) &&
		(xrtTlsProtocolsRead(&Protocols, &Protocol) == XTLS_ITEM_DONE),
		"TLS ALPN protocol cursor mismatch");

	testRequire(xrtTlsExtensionsFind(
		Hello.Extensions, XTLS_EXTENSION_SUPPORTED_GROUPS, &Extension
	) == XTLS_ITEM_VALUE, "TLS supported_groups extension was not found");
	testRequire(xrtTlsGroups(Extension.Data, &Groups) &&
		(xrtTlsIdsCount(&Groups) == 2u) &&
		xrtTlsIdsContain(&Groups, XTLS_GROUP_X25519) &&
		xrtTlsIdsContain(&Groups, XTLS_GROUP_SECP256R1),
		"TLS supported group list mismatch");

	testRequire(xrtTlsExtensionsFind(
		Hello.Extensions, XTLS_EXTENSION_KEY_SHARE, &Extension
	) == XTLS_ITEM_VALUE, "TLS client key_share extension was not found");
	testRequire(xrtTlsClientKeyShares(Extension.Data, &Shares) &&
		(xrtTlsKeySharesRead(&Shares, &Share) == XTLS_ITEM_VALUE) &&
		(Share.Group == XTLS_GROUP_X25519) &&
		(Share.Key.Size == 32u) && (Share.Key.Data[0] == 0x80u) &&
		(xrtTlsKeySharesRead(&Shares, &Share) == XTLS_ITEM_DONE),
		"TLS client key-share cursor mismatch");

	testRequire(xrtTlsExtensionsFind(
		Hello.Extensions, (xtlsextensiontype)0x1234, &Extension
	) == XTLS_ITEM_VALUE && (Extension.Data.Size == 1u) &&
		(Extension.Data.Data[0] == 0xA5),
		"unknown TLS extension was not preserved");
}



/* ALPN 协议名称是非空不透明字节，不得套用文本字符限制。 */
static void testTlsAlpnOpaque(void)
{
	static const uint8 Data[] = { 0, 4, 3, 0, 0xFF, 'x' };
	xtlsprotocolcursor Cursor;
	xbytesview Protocol;

	testRequire(xrtTlsProtocols(
		(xbytesview) { Data, sizeof(Data) }, &Cursor
	) && (xrtTlsProtocolsRead(&Cursor, &Protocol) == XTLS_ITEM_VALUE) &&
		(Protocol.Size == 3u) && (Protocol.Data[0] == 0) &&
		(Protocol.Data[1] == 0xFF) && (Protocol.Data[2] == 'x'),
		"TLS ALPN parser rejected an opaque protocol name");
}



/* ALPN 选择必须遵循服务端偏好并区分没有交集。 */
static void testTlsAlpnSelect(void)
{
	static const uint8 Offered[] = {
		0, 12, 2, 'h', '2', 8,
		'h', 't', 't', 'p', '/', '1', '.', '1'
	};
	static const uint8 Preferred[] = {
		0, 12, 8, 'h', 't', 't', 'p', '/', '1', '.', '1',
		2, 'h', '2'
	};
	static const uint8 Missing[] = { 0, 3, 2, 'h', '3' };
	xbytesview Protocol;
	xbytesview Before;

	testRequire(xrtTlsProtocolFind(
		(xbytesview) { Offered, sizeof(Offered) },
		XRT_BYTES_LITERAL("h2")
	) == XTLS_ITEM_VALUE, "TLS ALPN protocol lookup failed");
	testRequire(xrtTlsProtocolSelect(
		(xbytesview) { Offered, sizeof(Offered) },
		(xbytesview) { Preferred, sizeof(Preferred) }, &Protocol
	) == XTLS_ITEM_VALUE && (Protocol.Size == 8u) &&
		(memcmp(Protocol.Data, "http/1.1", 8u) == 0),
		"TLS ALPN selection ignored server preference");
	memset(&Protocol, 0xA5, sizeof(Protocol));
	Before = Protocol;
	testRequire(xrtTlsProtocolSelect(
		(xbytesview) { Offered, sizeof(Offered) },
		(xbytesview) { Missing, sizeof(Missing) }, &Protocol
	) == XTLS_ITEM_DONE &&
		(memcmp(&Protocol, &Before, sizeof(Protocol)) == 0),
		"TLS ALPN no-match result changed output");
}



/* ServerHello 与 HelloRetryRequest 必须使用不同的 key_share 语义。 */
static void testTlsServerHello(void)
{
	uint8 Data[128];
	size_t iSize = testTlsServerHelloVector(
		Data, sizeof(Data), false, NULL
	);
	xtlsserverhello Hello;
	xtlsextension Extension;
	xtlskeyshare Share;
	xbytesview Protocol;
	uint16 iVersion = 0;

	testRequire(xrtTlsServerHelloParse(
		(xbytesview) { Data, iSize }, &Hello
	), "valid TLS ServerHello did not parse");
	testRequire(!Hello.Retry &&
		(Hello.LegacyVersion == XTLS_VERSION_12) &&
		(Hello.CipherSuite == XTLS_AES_128_GCM_SHA256) &&
		(Hello.CompressionMethod == 0),
		"TLS ServerHello fixed field views mismatch");
	testRequire(xrtTlsExtensionsFind(
		Hello.Extensions, XTLS_EXTENSION_SUPPORTED_VERSIONS, &Extension
	) == XTLS_ITEM_VALUE &&
		xrtTlsServerVersion(Extension.Data, &iVersion) &&
		(iVersion == XTLS_VERSION_13),
		"TLS ServerHello selected version mismatch");
	testRequire(xrtTlsExtensionsFind(
		Hello.Extensions, XTLS_EXTENSION_KEY_SHARE, &Extension
	) == XTLS_ITEM_VALUE &&
		xrtTlsServerKeyShare(Extension.Data, &Share) &&
		(Share.Group == XTLS_GROUP_X25519) &&
		(Share.Key.Size == 32u) && (Share.Key.Data[0] == 0xC0u),
		"TLS ServerHello key share mismatch");
	testRequire(xrtTlsExtensionsFind(
		Hello.Extensions, XTLS_EXTENSION_ALPN, &Extension
	) == XTLS_ITEM_VALUE &&
		xrtTlsProtocolSelected(Extension.Data, &Protocol) &&
		(Protocol.Size == 2u) &&
		(memcmp(Protocol.Data, "h2", 2u) == 0),
		"TLS ServerHello ALPN selection mismatch");

	iSize = testTlsServerHelloVector(
		Data, sizeof(Data), true, NULL
	);
	testRequire(xrtTlsServerHelloParse(
		(xbytesview) { Data, iSize }, &Hello
	) && Hello.Retry,
		"valid TLS HelloRetryRequest did not parse");
	testRequire(xrtTlsExtensionsFind(
		Hello.Extensions, XTLS_EXTENSION_KEY_SHARE, &Extension
	) == XTLS_ITEM_VALUE &&
		xrtTlsRetryGroup(Extension.Data, &iVersion) &&
		(iVersion == XTLS_GROUP_X25519),
		"TLS HelloRetryRequest selected group mismatch");
}



/* 扩展查找必须区分未找到与协议错误。 */
static void testTlsExtensionCursor(void)
{
	static const uint8 Data[] = {
		0, 10, 0, 2, 0, 29,
		0x12, 0x34, 0, 1, 0xA5
	};
	xtlsextensioncursor Cursor;
	xtlsextension Extension;

	testRequire(xrtTlsExtensionsInit(
		&Cursor, (xbytesview) { Data, sizeof(Data) }
	), "TLS extension cursor initialization failed");
	testRequire((xrtTlsExtensionsRead(
		&Cursor, &Extension
	) == XTLS_ITEM_VALUE) &&
		(Extension.Type == XTLS_EXTENSION_SUPPORTED_GROUPS),
		"TLS extension cursor first item mismatch");
	testRequire((xrtTlsExtensionsRead(
		&Cursor, &Extension
	) == XTLS_ITEM_VALUE) && ((uint16)Extension.Type == 0x1234u),
		"TLS extension cursor unknown item mismatch");
	testRequire(xrtTlsExtensionsRead(
		&Cursor, &Extension
	) == XTLS_ITEM_DONE, "TLS extension cursor did not finish exactly");
	testRequire(xrtTlsExtensionsFind(
		(xbytesview) { Data, sizeof(Data) },
		XTLS_EXTENSION_ALPN, &Extension
	) == XTLS_ITEM_DONE, "missing TLS extension did not report done");
}



/* 执行 TLS Hello 与核心扩展正向回归。 */
int main(void)
{
	testTlsClientHello();
	testTlsAlpnOpaque();
	testTlsAlpnSelect();
	testTlsServerHello();
	testTlsExtensionCursor();
	return 0;
}
