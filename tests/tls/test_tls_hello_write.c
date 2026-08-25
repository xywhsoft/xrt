#include "../test.h"



/* 增量 writer 必须构造可由严格解析器读取的完整 ClientHello。 */
static void testTlsClientHelloWrite(void)
{
	static const uint8 CipherSuites[] = {
		0x13, 0x01, 0x13, 0x02, 0xC0, 0x2F
	};
	static const uint8 Compression[] = { 0 };
	static const uint8 PskModes[] = { 1, 1 };
	static const uint8 PointFormats[] = { 1, 0 };
	static const uint16 Versions[] = {
		XTLS_VERSION_13, XTLS_VERSION_12
	};
	static const uint16 Groups[] = {
		XTLS_GROUP_X25519, XTLS_GROUP_SECP256R1
	};
	static const uint16 Signatures[] = {
		XTLS_SIGNATURE_ED25519,
		XTLS_SIGNATURE_ECDSA_SECP256R1_SHA256
	};
	static const xbytesview Protocols[] = {
		XRT_BYTES_INIT("h2"),
		XRT_BYTES_INIT("http/1.1")
	};
	uint8 Key[32];
	uint8 Random[32];
	uint8 Extensions[512];
	uint8 Body[640];
	xtlskeyshare Share;
	xtlswriter Writer;
	xtlsclienthello Hello;
	xtlsclienthello Parsed;
	xbytesview Built;
	size_t iRequired;

	for ( uint8 i = 0; i < 32u; i++ ) {
		Key[i] = (uint8)(0x80u + i);
		Random[i] = i;
	}
	Share.Group = XTLS_GROUP_X25519;
	Share.Key.Data = Key;
	Share.Key.Size = sizeof(Key);
	testRequire(xrtTlsWriterInit(
		&Writer, Extensions, sizeof(Extensions)
	), "TLS extension writer initialization failed");
	testRequire(xrtTlsWriterHostName(
		&Writer, XRT_BYTES_LITERAL("example.com")
	), "TLS SNI writer failed");
	testRequire(xrtTlsWriterProtocols(
		&Writer, Protocols, sizeof(Protocols) / sizeof(Protocols[0])
	), "TLS ALPN writer failed");
	testRequire(xrtTlsWriterClientVersions(
		&Writer, Versions, sizeof(Versions) / sizeof(Versions[0])
	), "TLS client version writer failed");
	testRequire(xrtTlsWriterIds(
		&Writer, XTLS_EXTENSION_SUPPORTED_GROUPS,
		Groups, sizeof(Groups) / sizeof(Groups[0])
	), "TLS named-group writer failed");
	testRequire(xrtTlsWriterIds(
		&Writer, XTLS_EXTENSION_SIGNATURE_ALGORITHMS,
		Signatures, sizeof(Signatures) / sizeof(Signatures[0])
	), "TLS signature writer failed");
	testRequire(xrtTlsWriterClientKeyShares(
		&Writer, &Share, 1u
	), "TLS client key-share writer failed");
	testRequire(xrtTlsWriterExtension(
		&Writer, XTLS_EXTENSION_PSK_KEY_EXCHANGE_MODES,
		(xbytesview) { PskModes, sizeof(PskModes) }
	), "TLS PSK-mode raw extension writer failed");
	testRequire(xrtTlsWriterExtension(
		&Writer, XTLS_EXTENSION_EC_POINT_FORMATS,
		(xbytesview) { PointFormats, sizeof(PointFormats) }
	), "TLS point-format raw extension writer failed");
	Built = xrtTlsWriterData(&Writer);
	testRequire((Built.Data == Extensions) && (Built.Size == Writer.Size) &&
		xrtTlsExtensionsValidate(Built),
		"TLS extension writer produced an invalid vector");

	memset(&Hello, 0, sizeof(Hello));
	Hello.LegacyVersion = XTLS_VERSION_12;
	Hello.Random = (xbytesview) { Random, sizeof(Random) };
	Hello.CipherSuites.Data = (xbytesview) {
		CipherSuites, sizeof(CipherSuites)
	};
	Hello.CompressionMethods = (xbytesview) {
		Compression, sizeof(Compression)
	};
	Hello.Extensions = Built;
	iRequired = xrtTlsClientHelloSize(&Hello);
	testRequire((iRequired != 0) && xrtTlsClientHelloEncode(
		&Hello, Body, sizeof(Body)
	), "TLS ClientHello writer failed");
	testRequire(xrtTlsClientHelloParse(
		(xbytesview) { Body, iRequired }, &Parsed
	) && (Parsed.CipherSuites.Data.Size == sizeof(CipherSuites)) &&
		(Parsed.Extensions.Size == Built.Size),
		"written TLS ClientHello did not round trip");
}



/* ServerHello 与 Retry writer 必须匹配各自不同的 key_share 形态。 */
static void testTlsServerHelloWrite(void)
{
	static const uint8 RetryRandom[32] = {
		0xCF, 0x21, 0xAD, 0x74, 0xE5, 0x9A, 0x61, 0x11,
		0xBE, 0x1D, 0x8C, 0x02, 0x1E, 0x65, 0xB8, 0x91,
		0xC2, 0xA2, 0x11, 0x16, 0x7A, 0xBB, 0x8C, 0x5E,
		0x07, 0x9E, 0x09, 0xE2, 0xC8, 0xA8, 0x33, 0x9C
	};
	static const xbytesview Selected[] = {
		XRT_BYTES_INIT("h2")
	};
	static const uint8 Cookie[] = {
		0x19, 0xA4, 0x72, 0xE1, 0x3C
	};
	uint8 Key[32];
	uint8 Random[32];
	uint8 Extensions[128];
	uint8 Body[256];
	xtlskeyshare Share;
	xtlswriter Writer;
	xtlsserverhello Hello;
	xtlsserverhello Parsed;
	xtlsextension Extension;
	xbytesview ParsedCookie;
	size_t iRequired;

	for ( uint8 i = 0; i < 32u; i++ ) {
		Key[i] = (uint8)(0xC0u + i);
		Random[i] = (uint8)(0x40u + i);
	}
	Share.Group = XTLS_GROUP_X25519;
	Share.Key = (xbytesview) { Key, sizeof(Key) };
	testRequire(xrtTlsWriterInit(
		&Writer, Extensions, sizeof(Extensions)
	) && xrtTlsWriterServerVersion(&Writer, XTLS_VERSION_13) &&
		xrtTlsWriterServerKeyShare(&Writer, &Share) &&
		xrtTlsWriterProtocols(&Writer, Selected, 1u),
		"TLS ServerHello extension writer failed");

	memset(&Hello, 0, sizeof(Hello));
	Hello.LegacyVersion = XTLS_VERSION_12;
	Hello.Random = (xbytesview) { Random, sizeof(Random) };
	Hello.CipherSuite = XTLS_AES_128_GCM_SHA256;
	Hello.Extensions = xrtTlsWriterData(&Writer);
	iRequired = xrtTlsServerHelloSize(&Hello);
	testRequire((iRequired != 0) && xrtTlsServerHelloEncode(
		&Hello, Body, sizeof(Body)
	) && xrtTlsServerHelloParse(
		(xbytesview) { Body, iRequired }, &Parsed
	) && !Parsed.Retry,
		"TLS ServerHello writer did not round trip");

	testRequire(xrtTlsWriterReset(&Writer) &&
		xrtTlsWriterServerVersion(&Writer, XTLS_VERSION_13) &&
		xrtTlsWriterRetryGroup(&Writer, XTLS_GROUP_X25519) &&
		xrtTlsWriterRetryCookie(
			&Writer, (xbytesview) { Cookie, sizeof(Cookie) }
		),
		"TLS HelloRetryRequest extension writer failed");
	Hello.Random = (xbytesview) { RetryRandom, sizeof(RetryRandom) };
	Hello.Extensions = xrtTlsWriterData(&Writer);
	Hello.Retry = true;
	iRequired = xrtTlsServerHelloSize(&Hello);
	testRequire((iRequired != 0) && xrtTlsServerHelloEncode(
		&Hello, Body, sizeof(Body)
	) && xrtTlsServerHelloParse(
		(xbytesview) { Body, iRequired }, &Parsed
	) && Parsed.Retry && (xrtTlsExtensionsFind(
		Parsed.Extensions, XTLS_EXTENSION_COOKIE, &Extension
	) == XTLS_ITEM_VALUE) && xrtTlsRetryCookie(
		Extension.Data, &ParsedCookie
	) && (ParsedCookie.Size == sizeof(Cookie)) &&
		(memcmp(ParsedCookie.Data, Cookie, sizeof(Cookie)) == 0),
		"TLS HelloRetryRequest writer did not round trip");
}



/* 执行 TLS Hello writer 正向回归。 */
int main(void)
{
	testTlsClientHelloWrite();
	testTlsServerHelloWrite();
	return 0;
}
