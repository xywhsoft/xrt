#include "../../src/internal/xrt_tls.h"

#include "../test.h"



/* 完整握手消息必须借用正文并只消费输入中的第一条消息。 */
static void testTlsHandshakeParse(void)
{
	static const uint8 Data[] = {
		1, 0, 0, 5, 'h', 'e', 'l', 'l', 'o',
		20, 0, 0, 0
	};
	xtlshandshake Handshake;
	size_t iRequired = 0;

	testRequire(xrtTlsHandshakeParse(
		(xbytesview) { Data, sizeof(Data) }, &Handshake, &iRequired
	) == XTLS_OK, "complete TLS handshake parse failed");
	testRequire((Handshake.Type == XTLS_HANDSHAKE_CLIENT_HELLO) &&
		(Handshake.Body.Data == Data + XTLS_HANDSHAKE_HEADER_SIZE) &&
		(Handshake.Body.Size == 5u) &&
		(Handshake.EncodedSize == 9u) && (iRequired == 9u) &&
		(memcmp(Handshake.Body.Data, "hello", 5u) == 0),
		"parsed TLS handshake fields mismatch");
}



/* 每个握手截断前缀必须报告精确需求且保持输出不变。 */
static void testTlsHandshakeFragments(void)
{
	static const uint8 Data[] = {
		2, 0, 0, 7, 'f', 'r', 'a', 'g', 'm', 'e', 'n'
	};

	for ( size_t i = 0; i < sizeof(Data); i++ ) {
		xtlshandshake Handshake;
		xtlshandshake Before;
		size_t iRequired = 0;

		memset(&Handshake, 0xA5, sizeof(Handshake));
		Before = Handshake;
		xrtClearError();
		testRequire(xrtTlsHandshakeParse(
			(xbytesview) { Data, i }, &Handshake, &iRequired
		) == XTLS_AGAIN, "truncated TLS handshake did not request input");
		testRequire(memcmp(&Handshake, &Before, sizeof(Handshake)) == 0,
			"truncated TLS handshake changed output");
		testRequire(iRequired == (i < XTLS_HANDSHAKE_HEADER_SIZE ?
			XTLS_HANDSHAKE_HEADER_SIZE : sizeof(Data)),
			"truncated TLS handshake reported the wrong requirement");
		testRequire(xrtGetError() == NULL,
			"normal TLS handshake fragmentation set an error");
	}
}



/* 握手编码必须支持重叠、空正文和未知扩展消息类型。 */
static void testTlsHandshakeEncode(void)
{
	uint8 Buffer[32];
	xtlshandshake Handshake;

	memcpy(Buffer, "overlap", 7u);
	testRequire(xrtTlsHandshakeEncode(
		XTLS_HANDSHAKE_CERTIFICATE,
		(xbytesview) { Buffer, 7u }, Buffer, sizeof(Buffer)
	), "overlapping TLS handshake encode failed");
	testRequire(xrtTlsHandshakeParse(
		(xbytesview) { Buffer, 11u }, &Handshake, NULL
	) == XTLS_OK, "encoded TLS handshake did not parse");
	testRequire((Handshake.Type == XTLS_HANDSHAKE_CERTIFICATE) &&
		(Handshake.Body.Size == 7u) &&
		(memcmp(Handshake.Body.Data, "overlap", 7u) == 0),
		"overlapping TLS handshake changed the body");

	testRequire(xrtTlsHandshakeEncode(
		(xtlshandshaketype)200, (xbytesview) { NULL, 0 },
		Buffer, sizeof(Buffer)
	) && (xrtTlsHandshakeParse(
		(xbytesview) { Buffer, XTLS_HANDSHAKE_HEADER_SIZE },
		&Handshake, NULL
	) == XTLS_OK) && ((uint32)Handshake.Type == 200u) &&
		(strcmp(xrtTlsHandshakeName(Handshake.Type), "unknown_handshake") == 0),
		"unknown TLS handshake framing was not preserved");
}



/* 扩展 framing 必须支持分片、未知类型和重叠编码。 */
static void testTlsExtensionFraming(void)
{
	static const uint8 Data[] = {
		0, 43, 0, 2, 3, 4,
		0x12, 0x34, 0, 0
	};
	uint8 Buffer[32];
	xtlsextension Extension;
	size_t iRequired = 0;

	testRequire(xrtTlsExtensionParse(
		(xbytesview) { Data, sizeof(Data) }, &Extension, &iRequired
	) == XTLS_OK, "complete TLS extension parse failed");
	testRequire((Extension.Type == XTLS_EXTENSION_SUPPORTED_VERSIONS) &&
		(Extension.Data.Data == Data + XTLS_EXTENSION_HEADER_SIZE) &&
		(Extension.Data.Size == 2u) && (Extension.EncodedSize == 6u) &&
		(iRequired == 6u) && (memcmp(Extension.Data.Data, "\x03\x04", 2u) == 0),
		"parsed TLS extension fields mismatch");

	for ( size_t i = 0; i < 6u; i++ ) {
		xtlsextension Fragment;
		xtlsextension Before;

		memset(&Fragment, 0xA5, sizeof(Fragment));
		Before = Fragment;
		testRequire(xrtTlsExtensionParse(
			(xbytesview) { Data, i }, &Fragment, &iRequired
		) == XTLS_AGAIN, "truncated TLS extension did not request input");
		testRequire(memcmp(&Fragment, &Before, sizeof(Fragment)) == 0,
			"truncated TLS extension changed output");
		testRequire(iRequired == (i < XTLS_EXTENSION_HEADER_SIZE ?
			XTLS_EXTENSION_HEADER_SIZE : 6u),
			"truncated TLS extension reported the wrong requirement");
	}

	memcpy(Buffer, "h2", 2u);
	testRequire(xrtTlsExtensionEncode(
		XTLS_EXTENSION_ALPN,
		(xbytesview) { Buffer, 2u }, Buffer, sizeof(Buffer)
	) && (xrtTlsExtensionParse(
		(xbytesview) { Buffer, 6u }, &Extension, NULL
	) == XTLS_OK) && (Extension.Type == XTLS_EXTENSION_ALPN) &&
		(Extension.Data.Size == 2u) &&
		(memcmp(Extension.Data.Data, "h2", 2u) == 0),
		"overlapping TLS extension round trip failed");
	testRequire(strcmp(xrtTlsExtensionName(
		(xtlsextensiontype)0x1234
	), "unknown_extension") == 0,
		"unknown TLS extension name mismatch");
}



/* 线路上限和容量失败不得改写调用方输出。 */
static void testTlsHandshakeLimits(void)
{
	static const uint8 MaxHandshake[] = { 1, 0xFF, 0xFF, 0xFF };
	static const uint8 MaxExtension[] = { 0xFF, 0xFE, 0xFF, 0xFF };
	uint8 Byte = 0;
	uint8 Output[8];
	uint8 Before[sizeof(Output)];
	xtlshandshake Handshake;
	xtlsextension Extension;
	size_t iRequired = 0;

	testRequire(xrtTlsHandshakeParse(
		(xbytesview) { MaxHandshake, sizeof(MaxHandshake) },
		&Handshake, &iRequired
	) == XTLS_AGAIN &&
		(iRequired == XTLS_HANDSHAKE_HEADER_SIZE + XTLS_HANDSHAKE_BODY_MAX),
		"maximum TLS handshake length requirement mismatch");
	testRequire(xrtTlsExtensionParse(
		(xbytesview) { MaxExtension, sizeof(MaxExtension) },
		&Extension, &iRequired
	) == XTLS_AGAIN &&
		(iRequired == XTLS_EXTENSION_HEADER_SIZE + XTLS_EXTENSION_DATA_MAX),
		"maximum TLS extension length requirement mismatch");
	testRequire((xrtTlsHandshakeSize(XTLS_HANDSHAKE_BODY_MAX) ==
		XTLS_HANDSHAKE_HEADER_SIZE + XTLS_HANDSHAKE_BODY_MAX) &&
		(xrtTlsExtensionSize(XTLS_EXTENSION_DATA_MAX) ==
		XTLS_EXTENSION_HEADER_SIZE + XTLS_EXTENSION_DATA_MAX),
		"TLS framing maximum size calculation mismatch");
	testRequire((xrtTlsHandshakeSize(XTLS_HANDSHAKE_BODY_MAX + 1u) == 0) &&
		(xrtTlsExtensionSize(XTLS_EXTENSION_DATA_MAX + 1u) == 0),
		"TLS framing accepted an unencodable size");

	memset(Output, 0x5A, sizeof(Output));
	memcpy(Before, Output, sizeof(Before));
	testRequire(!xrtTlsHandshakeEncode(
		XTLS_HANDSHAKE_FINISHED, XRT_BYTES_LITERAL("12345"),
		Output, sizeof(Output)
	) && (memcmp(Output, Before, sizeof(Output)) == 0),
		"undersized TLS handshake output was modified");
	testRequire(!xrtTlsExtensionEncode(
		XTLS_EXTENSION_KEY_SHARE, XRT_BYTES_LITERAL("12345"),
		Output, sizeof(Output)
	) && (memcmp(Output, Before, sizeof(Output)) == 0),
		"undersized TLS extension output was modified");
	testRequire(!xrtTlsHandshakeEncode(
		(xtlshandshaketype)256,
		(xbytesview) { &Byte, 1u }, Output, sizeof(Output)
	) && (xrtErrorCode(xrtGetError()) == XTLS_ERROR_HANDSHAKE),
		"oversized TLS handshake type was truncated");
	testRequire(!xrtTlsExtensionEncode(
		(xtlsextensiontype)65536,
		(xbytesview) { &Byte, 1u }, Output, sizeof(Output)
	) && (xrtErrorCode(xrtGetError()) == XTLS_ERROR_EXTENSION),
		"oversized TLS extension type was truncated");
}



/* 执行 TLS 握手与扩展 framing 回归。 */
int main(void)
{
	testTlsHandshakeParse();
	testTlsHandshakeFragments();
	testTlsHandshakeEncode();
	testTlsExtensionFraming();
	testTlsHandshakeLimits();
	return 0;
}
