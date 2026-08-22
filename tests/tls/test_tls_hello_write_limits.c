#include "../test.h"



/* writer 的容量、重复和重叠失败必须保持缓冲与 Size 不变。 */
static void testTlsWriterAtomicity(void)
{
	uint8 Buffer[64];
	uint8 Before[sizeof(Buffer)];
	xtlswriter Writer;
	xtlsextension Extension;
	size_t iSize;
	static const uint16 DuplicateIds[] = { 29, 29 };

	memcpy(Buffer, "abc", 3u);
	testRequire(xrtTlsWriterInit(
		&Writer, Buffer, sizeof(Buffer)
	) && xrtTlsWriterExtension(
		&Writer, (xtlsextensiontype)0x1234,
		(xbytesview) { Buffer, 3u }
	), "overlapping raw TLS extension write failed");
	testRequire(xrtTlsExtensionParse(
		(xbytesview) { Buffer, Writer.Size }, &Extension, NULL
	) == XTLS_OK && (Extension.Data.Size == 3u) &&
		(memcmp(Extension.Data.Data, "abc", 3u) == 0),
		"overlapping raw TLS extension changed its payload");

	memset(Buffer, 0x5A, sizeof(Buffer));
	testRequire(xrtTlsWriterReset(&Writer) &&
		xrtTlsWriterExtension(
			&Writer, (xtlsextensiontype)0x1234,
			(xbytesview) { NULL, 0 }
		), "TLS duplicate-extension setup failed");
	iSize = Writer.Size;
	memcpy(Before, Buffer, sizeof(Buffer));
	testRequire(!xrtTlsWriterExtension(
		&Writer, (xtlsextensiontype)0x1234,
		(xbytesview) { NULL, 0 }
	) && (Writer.Size == iSize) &&
		(memcmp(Buffer, Before, sizeof(Buffer)) == 0),
		"duplicate TLS extension write was not atomic");

	testRequire(xrtTlsWriterReset(&Writer),
		"TLS writer reset before overlap test failed");
	memcpy(Buffer, "example.com", 11u);
	memcpy(Before, Buffer, sizeof(Buffer));
	testRequire(!xrtTlsWriterHostName(
		&Writer, (xbytesview) { Buffer, 11u }
	) && (Writer.Size == 0) &&
		(memcmp(Buffer, Before, sizeof(Buffer)) == 0),
		"overlapping TLS SNI write was not rejected atomically");

	testRequire(xrtTlsWriterReset(&Writer),
		"TLS writer reset before duplicate list failed");
	memcpy(Before, Buffer, sizeof(Buffer));
	testRequire(!xrtTlsWriterIds(
		&Writer, XTLS_EXTENSION_SUPPORTED_GROUPS,
		DuplicateIds, 2u
	) && (Writer.Size == 0) &&
		(memcmp(Buffer, Before, sizeof(Buffer)) == 0),
		"duplicate TLS identifier write was not atomic");

	memset(Buffer, 0xA5, sizeof(Buffer));
	testRequire(xrtTlsWriterInit(&Writer, Buffer, 3u),
		"small TLS writer initialization failed");
	memcpy(Before, Buffer, sizeof(Buffer));
	testRequire(!xrtTlsWriterExtension(
		&Writer, XTLS_EXTENSION_PADDING,
		(xbytesview) { NULL, 0 }
	) && (Writer.Size == 0) &&
		(memcmp(Buffer, Before, sizeof(Buffer)) == 0),
		"undersized TLS writer modified output");
}



/* 空 ClientHello key_share 列表必须保留协议允许的 Retry 路径。 */
static void testTlsEmptyKeyShares(void)
{
	uint8 Buffer[16];
	xtlswriter Writer;
	xtlsextension Extension;
	xtlskeysharecursor Shares;
	xtlskeyshare Share;

	testRequire(xrtTlsWriterInit(
		&Writer, Buffer, sizeof(Buffer)
	) && xrtTlsWriterClientKeyShares(&Writer, NULL, 0) &&
		(xrtTlsExtensionParse(
			(xbytesview) { Buffer, Writer.Size }, &Extension, NULL
		) == XTLS_OK) &&
		xrtTlsClientKeyShares(Extension.Data, &Shares) &&
		(xrtTlsKeySharesRead(&Shares, &Share) == XTLS_ITEM_DONE),
		"empty TLS client key-share list did not round trip");
}



/* Hello 编码容量失败和输入输出重叠必须保持输出不变。 */
static void testTlsHelloWriteAtomicity(void)
{
	static const uint8 Ciphers[] = { 0x13, 0x01 };
	static const uint8 Compression[] = { 0 };
	uint8 Random[32];
	uint8 Body[128];
	uint8 Before[sizeof(Body)];
	xtlsclienthello Hello;
	xtlsclienthello Parsed;
	size_t iSize;

	memset(Random, 0x11, sizeof(Random));
	memset(&Hello, 0, sizeof(Hello));
	Hello.LegacyVersion = XTLS_VERSION_12;
	Hello.Random = (xbytesview) { Random, sizeof(Random) };
	Hello.CipherSuites.Data = (xbytesview) {
		Ciphers, sizeof(Ciphers)
	};
	Hello.CompressionMethods = (xbytesview) {
		Compression, sizeof(Compression)
	};
	iSize = xrtTlsClientHelloSize(&Hello);
	testRequire(iSize != 0,
		"minimal TLS ClientHello size calculation failed");
	memset(Body, 0xA5, sizeof(Body));
	memcpy(Before, Body, sizeof(Body));
	testRequire(!xrtTlsClientHelloEncode(
		&Hello, Body, iSize - 1u
	) && (memcmp(Body, Before, sizeof(Body)) == 0),
		"undersized TLS ClientHello write modified output");

	testRequire(xrtTlsClientHelloEncode(
		&Hello, Body, sizeof(Body)
	) && xrtTlsClientHelloParse(
		(xbytesview) { Body, iSize }, &Parsed
	), "TLS ClientHello overlap setup failed");
	memcpy(Before, Body, sizeof(Body));
	testRequire(!xrtTlsClientHelloEncode(
		&Parsed, Body, sizeof(Body)
	) && (memcmp(Body, Before, sizeof(Body)) == 0),
		"overlapping TLS ClientHello write was not atomic");
}



/* Hello writer 必须执行依赖上下文才能判断的扩展位置与 Retry 约束。 */
static void testTlsHelloWriteSemantics(void)
{
	static const uint8 Ciphers[] = { 0x13, 0x01 };
	static const uint8 Compression[] = { 0 };
	uint8 Random[32] = { 0 };
	uint8 Extensions[32];
	xtlswriter Writer;
	xtlsclienthello Client;
	xtlsserverhello Server;

	testRequire(xrtTlsWriterInit(
		&Writer, Extensions, sizeof(Extensions)
	) && xrtTlsWriterExtension(
		&Writer, XTLS_EXTENSION_PRE_SHARED_KEY,
		(xbytesview) { NULL, 0 }
	) && xrtTlsWriterExtension(
		&Writer, XTLS_EXTENSION_PADDING,
		(xbytesview) { NULL, 0 }
	), "TLS pre_shared_key position setup failed");
	memset(&Client, 0, sizeof(Client));
	Client.LegacyVersion = XTLS_VERSION_12;
	Client.Random = (xbytesview) { Random, sizeof(Random) };
	Client.CipherSuites.Data = (xbytesview) {
		Ciphers, sizeof(Ciphers)
	};
	Client.CompressionMethods = (xbytesview) {
		Compression, sizeof(Compression)
	};
	Client.Extensions = xrtTlsWriterData(&Writer);
	testRequire(xrtTlsClientHelloSize(&Client) == 0,
		"non-final TLS pre_shared_key extension was accepted");

	memset(&Server, 0, sizeof(Server));
	Server.LegacyVersion = XTLS_VERSION_12;
	Server.Random = (xbytesview) { Random, sizeof(Random) };
	Server.CipherSuite = XTLS_AES_128_GCM_SHA256;
	Server.Retry = true;
	testRequire(xrtTlsServerHelloSize(&Server) == 0,
		"TLS Retry flag without Retry random was accepted");
}



/* Hello writer 必须支持明显超过旧版 1024 字节栈数组的正文。 */
static void testTlsLargeClientHello(void)
{
	enum { PROTOCOL_COUNT = 255, PROTOCOL_SIZE = 5 };
	static const uint8 Ciphers[] = { 0x13, 0x01 };
	static const uint8 Compression[] = { 0 };
	static const uint16 Versions[] = {
		XTLS_VERSION_13, XTLS_VERSION_12
	};
	uint8 ProtocolData[PROTOCOL_COUNT][PROTOCOL_SIZE];
	xbytesview Protocols[PROTOCOL_COUNT];
	uint8 Random[32];
	uint8 Extensions[2048];
	uint8 Body[2200];
	xtlswriter Writer;
	xtlsclienthello Hello;
	xtlsclienthello Parsed;
	size_t iSize;

	for ( size_t i = 0; i < PROTOCOL_COUNT; i++ ) {
		ProtocolData[i][0] = 'p';
		ProtocolData[i][1] = (uint8)(i >> 8u);
		ProtocolData[i][2] = (uint8)i;
		ProtocolData[i][3] = 'x';
		ProtocolData[i][4] = 'x';
		Protocols[i] = (xbytesview) {
			ProtocolData[i], PROTOCOL_SIZE
		};
	}
	memset(Random, 0x22, sizeof(Random));
	testRequire(xrtTlsWriterInit(
		&Writer, Extensions, sizeof(Extensions)
	) && xrtTlsWriterClientVersions(&Writer, Versions, 2u) &&
		xrtTlsWriterProtocols(&Writer, Protocols, PROTOCOL_COUNT),
		"large TLS ALPN extension write failed");
	testRequire(Writer.Size > 1024u,
		"large TLS extension vector did not cross old fixed limit");

	memset(&Hello, 0, sizeof(Hello));
	Hello.LegacyVersion = XTLS_VERSION_12;
	Hello.Random = (xbytesview) { Random, sizeof(Random) };
	Hello.CipherSuites.Data = (xbytesview) {
		Ciphers, sizeof(Ciphers)
	};
	Hello.CompressionMethods = (xbytesview) {
		Compression, sizeof(Compression)
	};
	Hello.Extensions = xrtTlsWriterData(&Writer);
	iSize = xrtTlsClientHelloSize(&Hello);
	testRequire((iSize > 1024u) && (iSize <= sizeof(Body)) &&
		xrtTlsClientHelloEncode(&Hello, Body, sizeof(Body)) &&
		xrtTlsClientHelloParse(
			(xbytesview) { Body, iSize }, &Parsed
		) && (Parsed.Extensions.Size == Writer.Size),
		"large TLS ClientHello did not round trip");
}



/* 执行 TLS Hello writer 边界回归。 */
int main(void)
{
	testTlsWriterAtomicity();
	testTlsEmptyKeyShares();
	testTlsHelloWriteAtomicity();
	testTlsHelloWriteSemantics();
	testTlsLargeClientHello();
	return 0;
}
