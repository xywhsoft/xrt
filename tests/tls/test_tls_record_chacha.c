#include "../../src/internal/xrt_tls.h"

#include "../test.h"
#include "tls_record_vectors.h"



/* 初始化测试使用的 ChaCha20-Poly1305 记录密钥。 */
static void testTlsChaChaInit(
	xtlsrecordkey* pKey,
	xtlsversion Version,
	xtlscipher Cipher
)
{
	memset(pKey, 0, sizeof(*pKey));
	testRequire(__xrtTlsRecordKeyInit(
		pKey, Version, Cipher,
		(xbytesview) { TestTlsRecordKey32, sizeof(TestTlsRecordKey32) },
		(xbytesview) { TestTlsRecordIv12, sizeof(TestTlsRecordIv12) }
	), "TLS ChaCha record key initialization failed");
}



/* 核对 TLS 1.2 ChaCha20-Poly1305 的无显式 nonce 线路布局。 */
static void testTls12ChaChaVector(void)
{
	uint8 Encoded[sizeof(TestTls12ChaChaRecord)];
	uint8 Plain[sizeof(TestTlsRecordPlain24)];
	size_t Written = 0;
	xtlsrecordtype Type = XTLS_RECORD_ALERT;
	xtlsrecord Record;
	xtlsrecordkey Send;
	xtlsrecordkey Receive;

	testTlsChaChaInit(
		&Send, XTLS_VERSION_12,
		XTLS_ECDHE_RSA_CHACHA20_POLY1305_SHA256
	);
	testRequire(__xrtTlsRecordSeal(
		&Send, XTLS_RECORD_APPLICATION_DATA,
		(xbytesview) { TestTlsRecordPlain24, sizeof(TestTlsRecordPlain24) },
		0, Encoded, sizeof(Encoded), &Written
	) && (Written == sizeof(TestTls12ChaChaRecord)) &&
		(memcmp(Encoded, TestTls12ChaChaRecord, Written) == 0),
		"TLS 1.2 ChaCha fixed record mismatch");
	testRequire(xrtTlsRecordParse(
		(xbytesview) { Encoded, Written }, &Record, NULL
	) == XTLS_OK, "TLS 1.2 ChaCha record parse failed");
	testTlsChaChaInit(
		&Receive, XTLS_VERSION_12,
		XTLS_ECDHE_RSA_CHACHA20_POLY1305_SHA256
	);
	testRequire(__xrtTlsRecordOpen(
		&Receive, &Record, Plain, sizeof(Plain), &Type, &Written
	) && (Type == XTLS_RECORD_APPLICATION_DATA) &&
		(Written == sizeof(Plain)) &&
		(memcmp(Plain, TestTlsRecordPlain24, sizeof(Plain)) == 0),
		"TLS 1.2 ChaCha fixed record open failed");
	__xrtTlsRecordKeyClear(&Send);
	__xrtTlsRecordKeyClear(&Receive);
}



/* 核对 TLS 1.3 ChaCha 记录、内层类型和零填充。 */
static void testTls13ChaChaVector(void)
{
	uint8 Plain32[32];
	uint8 Encoded[sizeof(TestTls13ChaChaRecord)];
	uint8 Plain[40];
	size_t Written = 0;
	xtlsrecordtype Type = XTLS_RECORD_ALERT;
	xtlsrecord Record;
	xtlsrecordkey Send;
	xtlsrecordkey Receive;

	for ( size_t i = 0; i < sizeof(Plain32); i++ ) {
		Plain32[i] = (uint8)i;
	}
	testTlsChaChaInit(
		&Send, XTLS_VERSION_13, XTLS_CHACHA20_POLY1305_SHA256
	);
	testRequire(__xrtTlsRecordSeal(
		&Send, XTLS_RECORD_APPLICATION_DATA,
		(xbytesview) { Plain32, sizeof(Plain32) },
		3u, Encoded, sizeof(Encoded), &Written
	) && (Written == sizeof(TestTls13ChaChaRecord)) &&
		(memcmp(Encoded, TestTls13ChaChaRecord, Written) == 0),
		"TLS 1.3 ChaCha fixed record mismatch");
	testRequire(xrtTlsRecordParse(
		(xbytesview) { Encoded, Written }, &Record, NULL
	) == XTLS_OK, "TLS 1.3 ChaCha record parse failed");
	testTlsChaChaInit(
		&Receive, XTLS_VERSION_13, XTLS_CHACHA20_POLY1305_SHA256
	);
	testRequire(__xrtTlsRecordOpen(
		&Receive, &Record, Plain, sizeof(Plain), &Type, &Written
	) && (Type == XTLS_RECORD_APPLICATION_DATA) &&
		(Written == sizeof(Plain32)) &&
		(memcmp(Plain, Plain32, sizeof(Plain32)) == 0),
		"TLS 1.3 ChaCha fixed record open failed");
	__xrtTlsRecordKeyClear(&Send);
	__xrtTlsRecordKeyClear(&Receive);
}



/* 验证 ChaCha 的 TLS 1.2 与 TLS 1.3 精确原位路径。 */
static void testTlsChaChaInPlace(void)
{
	uint8 Buffer[96];
	size_t Written = 0;
	xtlsrecordtype Type = XTLS_RECORD_ALERT;
	xtlsrecord Record;
	xtlsrecordkey Send;
	xtlsrecordkey Receive;

	testTlsChaChaInit(
		&Send, XTLS_VERSION_13, XTLS_CHACHA20_POLY1305_SHA256
	);
	memcpy(Buffer + XTLS_RECORD_HEADER_SIZE, "tls13", 5u);
	testRequire(__xrtTlsRecordSeal(
		&Send, XTLS_RECORD_APPLICATION_DATA,
		(xbytesview) { Buffer + XTLS_RECORD_HEADER_SIZE, 5u },
		2u, Buffer, sizeof(Buffer), &Written
	) && (xrtTlsRecordParse(
		(xbytesview) { Buffer, Written }, &Record, NULL
	) == XTLS_OK), "TLS 1.3 ChaCha in-place seal failed");
	testTlsChaChaInit(
		&Receive, XTLS_VERSION_13, XTLS_CHACHA20_POLY1305_SHA256
	);
	testRequire(__xrtTlsRecordOpen(
		&Receive, &Record, (void*)Record.Payload.Data,
		Record.Payload.Size, &Type, &Written
	) && (Type == XTLS_RECORD_APPLICATION_DATA) && (Written == 5u) &&
		(memcmp(Record.Payload.Data, "tls13", 5u) == 0),
		"TLS 1.3 ChaCha in-place open failed");
	__xrtTlsRecordKeyClear(&Send);
	__xrtTlsRecordKeyClear(&Receive);

	testTlsChaChaInit(
		&Send, XTLS_VERSION_12,
		XTLS_ECDHE_RSA_CHACHA20_POLY1305_SHA256
	);
	memcpy(Buffer + XTLS_RECORD_HEADER_SIZE, "tls12", 5u);
	testRequire(__xrtTlsRecordSeal(
		&Send, XTLS_RECORD_HANDSHAKE,
		(xbytesview) { Buffer + XTLS_RECORD_HEADER_SIZE, 5u },
		0, Buffer, sizeof(Buffer), &Written
	) && (xrtTlsRecordParse(
		(xbytesview) { Buffer, Written }, &Record, NULL
	) == XTLS_OK), "TLS 1.2 ChaCha in-place seal failed");
	testTlsChaChaInit(
		&Receive, XTLS_VERSION_12,
		XTLS_ECDHE_RSA_CHACHA20_POLY1305_SHA256
	);
	testRequire(__xrtTlsRecordOpen(
		&Receive, &Record, (void*)Record.Payload.Data,
		Record.Payload.Size, &Type, &Written
	) && (Type == XTLS_RECORD_HANDSHAKE) && (Written == 5u) &&
		(memcmp(Record.Payload.Data, "tls12", 5u) == 0),
		"TLS 1.2 ChaCha in-place open failed");
	__xrtTlsRecordKeyClear(&Send);
	__xrtTlsRecordKeyClear(&Receive);
}



/* 覆盖 ECDSA 套件、认证失败和序列号耗尽。 */
static void testTlsChaChaEdges(void)
{
	uint8 Encoded[96];
	uint8 Changed[96];
	uint8 Plain[64];
	uint8 Before[sizeof(Plain)];
	size_t Written = 0;
	xtlsrecordtype Type = XTLS_RECORD_ALERT;
	xtlsrecord Record;
	xtlsrecordkey Send;
	xtlsrecordkey Receive;

	testTlsChaChaInit(
		&Send, XTLS_VERSION_12,
		XTLS_ECDHE_ECDSA_CHACHA20_POLY1305_SHA256
	);
	testRequire(__xrtTlsRecordSeal(
		&Send, XTLS_RECORD_HANDSHAKE, XRT_BYTES_LITERAL("ecdsa"),
		0, Encoded, sizeof(Encoded), &Written
	) && (xrtTlsRecordParse(
		(xbytesview) { Encoded, Written }, &Record, NULL
	) == XTLS_OK), "TLS 1.2 ECDSA ChaCha record failed");
	testTlsChaChaInit(
		&Receive, XTLS_VERSION_12,
		XTLS_ECDHE_ECDSA_CHACHA20_POLY1305_SHA256
	);
	testRequire(__xrtTlsRecordOpen(
		&Receive, &Record, Plain, sizeof(Plain), &Type, &Written
	) && (Type == XTLS_RECORD_HANDSHAKE) && (Written == 5u) &&
		(memcmp(Plain, "ecdsa", 5u) == 0),
		"TLS 1.2 ECDSA ChaCha record open failed");
	__xrtTlsRecordKeyClear(&Send);
	__xrtTlsRecordKeyClear(&Receive);

	memcpy(Changed, TestTls13ChaChaRecord, sizeof(TestTls13ChaChaRecord));
	Changed[20] ^= 1u;
	testRequire(xrtTlsRecordParse(
		(xbytesview) { Changed, sizeof(TestTls13ChaChaRecord) },
		&Record, NULL
	) == XTLS_OK, "changed TLS ChaCha record parse failed");
	testTlsChaChaInit(
		&Receive, XTLS_VERSION_13, XTLS_CHACHA20_POLY1305_SHA256
	);
	memset(Plain, 0xA5, sizeof(Plain));
	memcpy(Before, Plain, sizeof(Before));
	xrtClearError();
	testRequire(!__xrtTlsRecordOpen(
		&Receive, &Record, Plain, sizeof(Plain), &Type, &Written
	) && (Written == 0) && (Receive.Sequence == 0) &&
		(memcmp(Plain, Before, sizeof(Plain)) == 0) &&
		(xrtErrorCode(xrtGetError()) == XTLS_ERROR_CIPHER) &&
		(xrtErrorCause(xrtGetError()) != NULL),
		"changed TLS ChaCha record escaped authentication");
	__xrtTlsRecordKeyClear(&Receive);

	testTlsChaChaInit(
		&Send, XTLS_VERSION_13, XTLS_CHACHA20_POLY1305_SHA256
	);
	Send.Sequence = UINT64_MAX;
	memset(Encoded, 0x5A, sizeof(Encoded));
	memcpy(Changed, Encoded, sizeof(Encoded));
	testRequire(!__xrtTlsRecordSeal(
		&Send, XTLS_RECORD_APPLICATION_DATA, XRT_BYTES_LITERAL("x"),
		0, Encoded, sizeof(Encoded), &Written
	) && (Written == 0) &&
		(memcmp(Encoded, Changed, sizeof(Encoded)) == 0),
		"exhausted TLS ChaCha sequence changed output");
	__xrtTlsRecordKeyClear(&Send);
}



/* 执行 ChaCha20-Poly1305 TLS 记录保护回归。 */
int main(void)
{
	testTls12ChaChaVector();
	testTls13ChaChaVector();
	testTlsChaChaInPlace();
	testTlsChaChaEdges();
	return 0;
}
