#include "../../src/internal/xrt_tls.h"

#include "../test.h"
#include "tls_record_vectors.h"



/* 初始化测试使用的 AES 记录密钥。 */
static void testTlsAesInit(
	xtlsrecordkey* pKey,
	xtlsversion Version,
	xtlscipher Cipher,
	const uint8* pData,
	size_t iDataSize,
	const uint8* pIv,
	size_t iIvSize
)
{
	memset(pKey, 0, sizeof(*pKey));
	testRequire(__xrtTlsRecordKeyInit(
		pKey, Version, Cipher,
		(xbytesview) { pData, iDataSize },
		(xbytesview) { pIv, iIvSize }
	), "TLS AES record key initialization failed");
}



/* 核对 RFC 8448 给出的完整 TLS 1.3 AES-128-GCM 线路记录。 */
static void testTls13AesVector(void)
{
	uint8 Encoded[sizeof(TestTls13AesRecord)];
	uint8 Plain[64];
	size_t Written = 0;
	size_t Required = 0;
	xtlsrecordtype Type = XTLS_RECORD_ALERT;
	xtlsrecord Record;
	xtlsrecordkey Send;
	xtlsrecordkey Receive;

	testTlsAesInit(
		&Send, XTLS_VERSION_13, XTLS_AES_128_GCM_SHA256,
		TestTls13AesKey, sizeof(TestTls13AesKey),
		TestTls13AesIv, sizeof(TestTls13AesIv)
	);
	testRequire(__xrtTlsRecordSeal(
		&Send,
		XTLS_RECORD_APPLICATION_DATA,
		(xbytesview) { TestTls13AesPlain, sizeof(TestTls13AesPlain) },
		0,
		Encoded,
		sizeof(Encoded),
		&Written
	) && (Written == sizeof(TestTls13AesRecord)) &&
		(memcmp(Encoded, TestTls13AesRecord, Written) == 0) &&
		(Send.Sequence == 1u),
		"TLS 1.3 AES RFC record mismatch");
	testRequire(xrtTlsRecordParse(
		(xbytesview) { TestTls13AesRecord, sizeof(TestTls13AesRecord) },
		&Record,
		&Required
	) == XTLS_OK, "TLS 1.3 AES RFC record parse failed");
	testTlsAesInit(
		&Receive, XTLS_VERSION_13, XTLS_AES_128_GCM_SHA256,
		TestTls13AesKey, sizeof(TestTls13AesKey),
		TestTls13AesIv, sizeof(TestTls13AesIv)
	);
	testRequire(__xrtTlsRecordOpen(
		&Receive, &Record, Plain, sizeof(Plain), &Type, &Written
	) && (Type == XTLS_RECORD_APPLICATION_DATA) &&
		(Written == sizeof(TestTls13AesPlain)) &&
		(memcmp(Plain, TestTls13AesPlain, Written) == 0) &&
		(Receive.Sequence == 1u),
		"TLS 1.3 AES RFC record open failed");
	__xrtTlsRecordKeyClear(&Send);
	__xrtTlsRecordKeyClear(&Receive);
}



/* 核对 TLS 1.2 AES-GCM 的显式 nonce、AAD 与序列号递增。 */
static void testTls12AesVector(void)
{
	uint8 Key[16];
	uint8 Iv[4] = { 0xA0, 0xA1, 0xA2, 0xA3 };
	uint8 Encoded[sizeof(TestTls12AesRecord)];
	uint8 Next[sizeof(TestTls12AesRecord)];
	uint8 Plain[sizeof(TestTlsRecordPlain24)];
	size_t Written = 0;
	xtlsrecordtype Type = XTLS_RECORD_ALERT;
	xtlsrecord Record;
	xtlsrecordkey Send;
	xtlsrecordkey Receive;

	memcpy(Key, TestTlsRecordKey32, sizeof(Key));
	testTlsAesInit(
		&Send, XTLS_VERSION_12, XTLS_ECDHE_RSA_AES_128_GCM_SHA256,
		Key, sizeof(Key), Iv, sizeof(Iv)
	);
	testRequire(__xrtTlsRecordSeal(
		&Send, XTLS_RECORD_APPLICATION_DATA,
		(xbytesview) { TestTlsRecordPlain24, sizeof(TestTlsRecordPlain24) },
		0, Encoded, sizeof(Encoded), &Written
	) && (Written == sizeof(TestTls12AesRecord)) &&
		(memcmp(Encoded, TestTls12AesRecord, Written) == 0),
		"TLS 1.2 AES fixed record mismatch");
	testRequire(__xrtTlsRecordSeal(
		&Send, XTLS_RECORD_APPLICATION_DATA,
		(xbytesview) { TestTlsRecordPlain24, sizeof(TestTlsRecordPlain24) },
		0, Next, sizeof(Next), &Written
	) && (memcmp(Next + XTLS_RECORD_HEADER_SIZE,
		"\x00\x00\x00\x00\x00\x00\x00\x01", 8u) == 0),
		"TLS 1.2 AES explicit nonce did not follow the sequence");
	testRequire(xrtTlsRecordParse(
		(xbytesview) { Encoded, sizeof(Encoded) }, &Record, NULL
	) == XTLS_OK, "TLS 1.2 AES record parse failed");
	testTlsAesInit(
		&Receive, XTLS_VERSION_12, XTLS_ECDHE_RSA_AES_128_GCM_SHA256,
		Key, sizeof(Key), Iv, sizeof(Iv)
	);
	testRequire(__xrtTlsRecordOpen(
		&Receive, &Record, Plain, sizeof(Plain), &Type, &Written
	) && (Type == XTLS_RECORD_APPLICATION_DATA) &&
		(Written == sizeof(Plain)) &&
		(memcmp(Plain, TestTlsRecordPlain24, sizeof(Plain)) == 0),
		"TLS 1.2 AES record open failed");
	__xrtTlsRecordKeyClear(&Send);
	__xrtTlsRecordKeyClear(&Receive);
}



/* 同时覆盖 AES-256 与 TLS 1.3 内层零填充。 */
static void testTlsAes256AndPadding(void)
{
	uint8 Encoded[128];
	uint8 Plain[64];
	uint8 Iv12[12] = { 0 };
	uint8 Iv4[4] = { 0 };
	size_t Written = 0;
	xtlsrecordtype Type = XTLS_RECORD_ALERT;
	xtlsrecord Record;
	xtlsrecordkey Send;
	xtlsrecordkey Receive;

	testTlsAesInit(
		&Send, XTLS_VERSION_13, XTLS_AES_256_GCM_SHA384,
		TestTlsRecordKey32, sizeof(TestTlsRecordKey32), Iv12, sizeof(Iv12)
	);
	testRequire(__xrtTlsRecordSeal(
		&Send, XTLS_RECORD_HANDSHAKE, XRT_BYTES_LITERAL("padded"),
		17u, Encoded, sizeof(Encoded), &Written
	) && (xrtTlsRecordParse(
		(xbytesview) { Encoded, Written }, &Record, NULL
	) == XTLS_OK), "TLS 1.3 AES-256 padded record seal failed");
	testTlsAesInit(
		&Receive, XTLS_VERSION_13, XTLS_AES_256_GCM_SHA384,
		TestTlsRecordKey32, sizeof(TestTlsRecordKey32), Iv12, sizeof(Iv12)
	);
	testRequire(__xrtTlsRecordOpen(
		&Receive, &Record, Plain, sizeof(Plain), &Type, &Written
	) && (Type == XTLS_RECORD_HANDSHAKE) && (Written == 6u) &&
		(memcmp(Plain, "padded", 6u) == 0),
		"TLS 1.3 AES-256 padding removal failed");
	__xrtTlsRecordKeyClear(&Send);
	__xrtTlsRecordKeyClear(&Receive);

	testTlsAesInit(
		&Send, XTLS_VERSION_12,
		XTLS_ECDHE_ECDSA_AES_256_GCM_SHA384,
		TestTlsRecordKey32, sizeof(TestTlsRecordKey32), Iv4, sizeof(Iv4)
	);
	testRequire(__xrtTlsRecordSeal(
		&Send, XTLS_RECORD_HANDSHAKE, XRT_BYTES_LITERAL("aes256"),
		0, Encoded, sizeof(Encoded), &Written
	) && (xrtTlsRecordParse(
		(xbytesview) { Encoded, Written }, &Record, NULL
	) == XTLS_OK), "TLS 1.2 AES-256 record seal failed");
	testTlsAesInit(
		&Receive, XTLS_VERSION_12,
		XTLS_ECDHE_ECDSA_AES_256_GCM_SHA384,
		TestTlsRecordKey32, sizeof(TestTlsRecordKey32), Iv4, sizeof(Iv4)
	);
	testRequire(__xrtTlsRecordOpen(
		&Receive, &Record, Plain, sizeof(Plain), &Type, &Written
	) && (Type == XTLS_RECORD_HANDSHAKE) && (Written == 6u) &&
		(memcmp(Plain, "aes256", 6u) == 0),
		"TLS 1.2 AES-256 record open failed");
	__xrtTlsRecordKeyClear(&Send);
	__xrtTlsRecordKeyClear(&Receive);
}



/* 每个密文字节发生单比特变化时都必须认证失败且保持输出不变。 */
static void testTlsAesMutation(void)
{
	uint8 Changed[sizeof(TestTls13AesRecord)];
	uint8 Plain[64];
	uint8 Before[sizeof(Plain)];
	size_t Written;
	xtlsrecordtype Type;
	xtlsrecord Record;
	xtlsrecordkey Receive;
	size_t Cases = 0;

	memset(Before, 0xA5, sizeof(Before));
	for ( size_t i = XTLS_RECORD_HEADER_SIZE;
		i < sizeof(TestTls13AesRecord); i++ ) {
		for ( unsigned iBit = 0; iBit < 8u; iBit++ ) {
			memcpy(Changed, TestTls13AesRecord, sizeof(Changed));
			Changed[i] ^= (uint8)(1u << iBit);
			testRequire(xrtTlsRecordParse(
				(xbytesview) { Changed, sizeof(Changed) }, &Record, NULL
			) == XTLS_OK, "changed TLS AES record no longer parsed");
			testTlsAesInit(
				&Receive, XTLS_VERSION_13, XTLS_AES_128_GCM_SHA256,
				TestTls13AesKey, sizeof(TestTls13AesKey),
				TestTls13AesIv, sizeof(TestTls13AesIv)
			);
			memcpy(Plain, Before, sizeof(Plain));
			Type = XTLS_RECORD_HANDSHAKE;
			Written = SIZE_MAX;
			xrtClearError();
			testRequire(!__xrtTlsRecordOpen(
				&Receive, &Record, Plain, sizeof(Plain), &Type, &Written
			) && (Written == 0) && (Type == XTLS_RECORD_HANDSHAKE) &&
				(Receive.Sequence == 0) &&
				(memcmp(Plain, Before, sizeof(Plain)) == 0) &&
				(xrtErrorCode(xrtGetError()) == XTLS_ERROR_CIPHER) &&
				(xrtErrorCause(xrtGetError()) != NULL) &&
				(strcmp(xrtErrorDomain(
					xrtErrorCause(xrtGetError())
				), "xrt.crypto") == 0),
				"changed TLS AES record escaped authentication");
			__xrtTlsRecordKeyClear(&Receive);
			Cases++;
		}
	}
	testRequire(Cases == 536u, "TLS AES mutation case count mismatch");
}



/* 验证空记录、精确原位路径和 TLS 1.3 填充硬上限。 */
static void testTlsAesInPlaceAndLimits(void)
{
	uint8 Key16[16] = { 0 };
	uint8 Iv12[12] = { 0 };
	uint8 Iv4[4] = { 0 };
	uint8 Buffer[128];
	size_t Written = 0;
	xtlsrecordtype Type = XTLS_RECORD_HANDSHAKE;
	xtlsrecord Record;
	xtlsrecordkey Send;
	xtlsrecordkey Receive;

	testTlsAesInit(
		&Send, XTLS_VERSION_13, XTLS_AES_128_GCM_SHA256,
		Key16, sizeof(Key16), Iv12, sizeof(Iv12)
	);
	testRequire(__xrtTlsRecordKeyLimit(&Send) == XTLS_AES_GCM_RECORD_LIMIT,
		"TLS AES record usage limit mismatch");
	testRequire(__xrtTlsRecordSealSize(
		&Send, XTLS_RECORD_PLAINTEXT_MAX, 0
	) == (XTLS_RECORD_HEADER_SIZE + XTLS13_INNER_PLAINTEXT_MAX + 16u),
		"TLS 1.3 maximum inner plaintext size mismatch");
	testRequire(__xrtTlsRecordSealSize(
		&Send, XTLS_RECORD_PLAINTEXT_MAX, 1u
	) == 0, "TLS 1.3 accepted padding beyond its inner limit");
	testRequire(__xrtTlsRecordSealSize(
		&Send, 0, XTLS_RECORD_PLAINTEXT_MAX
	) == (XTLS_RECORD_HEADER_SIZE + XTLS13_INNER_PLAINTEXT_MAX + 16u),
		"TLS 1.3 maximum empty-record padding mismatch");
	memcpy(Buffer + XTLS_RECORD_HEADER_SIZE, "inplace", 7u);
	testRequire(__xrtTlsRecordSeal(
		&Send, XTLS_RECORD_APPLICATION_DATA,
		(xbytesview) { Buffer + XTLS_RECORD_HEADER_SIZE, 7u },
		0, Buffer, sizeof(Buffer), &Written
	) && (xrtTlsRecordParse(
		(xbytesview) { Buffer, Written }, &Record, NULL
	) == XTLS_OK), "TLS 1.3 AES in-place seal failed");
	testTlsAesInit(
		&Receive, XTLS_VERSION_13, XTLS_AES_128_GCM_SHA256,
		Key16, sizeof(Key16), Iv12, sizeof(Iv12)
	);
	testRequire(__xrtTlsRecordOpen(
		&Receive, &Record, (void*)Record.Payload.Data,
		Record.Payload.Size, &Type, &Written
	) && (Type == XTLS_RECORD_APPLICATION_DATA) && (Written == 7u) &&
		(memcmp(Record.Payload.Data, "inplace", 7u) == 0),
		"TLS 1.3 AES in-place open failed");
	__xrtTlsRecordKeyClear(&Send);
	__xrtTlsRecordKeyClear(&Receive);

	testTlsAesInit(
		&Send, XTLS_VERSION_13, XTLS_AES_128_GCM_SHA256,
		Key16, sizeof(Key16), Iv12, sizeof(Iv12)
	);
	testRequire(__xrtTlsRecordSeal(
		&Send, XTLS_RECORD_ALERT, (xbytesview) { NULL, 0 },
		0, Buffer, sizeof(Buffer), &Written
	) && (xrtTlsRecordParse(
		(xbytesview) { Buffer, Written }, &Record, NULL
	) == XTLS_OK), "empty TLS 1.3 AES record seal failed");
	testTlsAesInit(
		&Receive, XTLS_VERSION_13, XTLS_AES_128_GCM_SHA256,
		Key16, sizeof(Key16), Iv12, sizeof(Iv12)
	);
	testRequire(__xrtTlsRecordOpen(
		&Receive, &Record, (void*)Record.Payload.Data,
		Record.Payload.Size, &Type, &Written
	) && (Type == XTLS_RECORD_ALERT) && (Written == 0),
		"empty TLS 1.3 AES record open failed");
	__xrtTlsRecordKeyClear(&Send);
	__xrtTlsRecordKeyClear(&Receive);

	testTlsAesInit(
		&Send, XTLS_VERSION_12, XTLS_ECDHE_RSA_AES_128_GCM_SHA256,
		Key16, sizeof(Key16), Iv4, sizeof(Iv4)
	);
	memcpy(Buffer + XTLS_RECORD_HEADER_SIZE + 8u, "tls12", 5u);
	testRequire(__xrtTlsRecordSeal(
		&Send, XTLS_RECORD_HANDSHAKE,
		(xbytesview) { Buffer + XTLS_RECORD_HEADER_SIZE + 8u, 5u },
		0, Buffer, sizeof(Buffer), &Written
	) && (xrtTlsRecordParse(
		(xbytesview) { Buffer, Written }, &Record, NULL
	) == XTLS_OK), "TLS 1.2 AES in-place seal failed");
	testTlsAesInit(
		&Receive, XTLS_VERSION_12, XTLS_ECDHE_RSA_AES_128_GCM_SHA256,
		Key16, sizeof(Key16), Iv4, sizeof(Iv4)
	);
	testRequire(__xrtTlsRecordOpen(
		&Receive, &Record, (void*)(Record.Payload.Data + 8u),
		Record.Payload.Size - 8u, &Type, &Written
	) && (Type == XTLS_RECORD_HANDSHAKE) && (Written == 5u) &&
		(memcmp(Record.Payload.Data + 8u, "tls12", 5u) == 0),
		"TLS 1.2 AES in-place open failed");
	__xrtTlsRecordKeyClear(&Send);
	__xrtTlsRecordKeyClear(&Receive);
}



/* 认证成功但缺少内层类型的 TLS 1.3 记录必须作为协议错误拒绝。 */
static void testTls13AesInnerType(void)
{
	uint8 KeyData[16] = { 0 };
	uint8 Iv[12] = { 0 };
	uint8 Header[XTLS_RECORD_HEADER_SIZE] = {
		XTLS_RECORD_APPLICATION_DATA, 0x03, 0x03, 0x00, 0x11
	};
	uint8 Inner = 0;
	uint8 Encoded[XTLS_RECORD_HEADER_SIZE + 17u];
	uint8 Plain = 0xA5;
	size_t Written = SIZE_MAX;
	xtlsrecordtype Type = XTLS_RECORD_HANDSHAKE;
	xtlsrecord Record;
	xtlsrecordkey Receive;

	testTlsAesInit(
		&Receive, XTLS_VERSION_13, XTLS_AES_128_GCM_SHA256,
		KeyData, sizeof(KeyData), Iv, sizeof(Iv)
	);
	memcpy(Encoded, Header, sizeof(Header));
	testRequire(xrtAesGcmSeal(
		&Receive.Aes, Iv, sizeof(Iv), Header, sizeof(Header),
		&Inner, sizeof(Inner), Encoded + sizeof(Header), 17u
	) && (xrtTlsRecordParse(
		(xbytesview) { Encoded, sizeof(Encoded) }, &Record, NULL
	) == XTLS_OK), "invalid TLS 1.3 inner record setup failed");
	xrtClearError();
	testRequire(!__xrtTlsRecordOpen(
		&Receive, &Record, &Plain, sizeof(Plain), &Type, &Written
	) && (Written == 0) && (Type == XTLS_RECORD_HANDSHAKE) &&
		(Receive.Sequence == 0) &&
		(xrtErrorCode(xrtGetError()) == XTLS_ERROR_RECORD_TYPE),
		"TLS 1.3 record without inner type was accepted");
	__xrtTlsRecordKeyClear(&Receive);
}



/* 检查错误输入、容量和序列号耗尽时的失败原子性。 */
static void testTlsAesEdges(void)
{
	uint8 KeyData[16] = { 0 };
	uint8 Iv[12] = { 0 };
	uint8 Output[80];
	uint8 Before[sizeof(Output)];
	uint8 Plain[50];
	size_t Written = SIZE_MAX;
	xtlsrecordtype Type = XTLS_RECORD_ALERT;
	xtlsrecord Record;
	xtlsrecordkey Key;
	xtlsrecordkey KeyBefore;

	memset(&Key, 0xA5, sizeof(Key));
	KeyBefore = Key;
	testRequire(!__xrtTlsRecordKeyInit(
		&Key, XTLS_VERSION_13, XTLS_AES_128_GCM_SHA256,
		(xbytesview) { KeyData, sizeof(KeyData) - 1u },
		(xbytesview) { Iv, sizeof(Iv) }
	) && (memcmp(&Key, &KeyBefore, sizeof(Key)) == 0),
		"invalid TLS AES key changed the destination");
	testTlsAesInit(
		&Key, XTLS_VERSION_13, XTLS_AES_128_GCM_SHA256,
		KeyData, sizeof(KeyData), Iv, sizeof(Iv)
	);
	memset(Output, 0x5A, sizeof(Output));
	memcpy(Before, Output, sizeof(Before));
	testRequire(!__xrtTlsRecordSeal(
		&Key, XTLS_RECORD_APPLICATION_DATA, XRT_BYTES_LITERAL("short"),
		0, Output, 21u, &Written
	) && (Written == 0) && (Key.Sequence == 0) &&
		(memcmp(Output, Before, sizeof(Output)) == 0),
		"short TLS AES output changed state or bytes");
	Key.Sequence = XTLS_AES_GCM_RECORD_LIMIT;
	testRequire(__xrtTlsRecordKeyExhausted(&Key),
		"TLS AES exhausted key was reported reusable");
	testRequire(!__xrtTlsRecordSeal(
		&Key, XTLS_RECORD_APPLICATION_DATA, XRT_BYTES_LITERAL("x"),
		0, Output, sizeof(Output), &Written
	) && (Written == 0) &&
		(memcmp(Output, Before, sizeof(Output)) == 0),
		"exhausted TLS AES sequence changed output");
	__xrtTlsRecordKeyClear(&Key);

	testRequire(xrtTlsRecordParse(
		(xbytesview) { TestTls13AesRecord, sizeof(TestTls13AesRecord) },
		&Record, NULL
	) == XTLS_OK, "TLS AES edge vector parse failed");
	testTlsAesInit(
		&Key, XTLS_VERSION_13, XTLS_AES_128_GCM_SHA256,
		TestTls13AesKey, sizeof(TestTls13AesKey),
		TestTls13AesIv, sizeof(TestTls13AesIv)
	);
	memset(Plain, 0xA5, sizeof(Plain));
	testRequire(!__xrtTlsRecordOpen(
		&Key, &Record, Plain, sizeof(Plain), &Type, &Written
	) && (Written == 0) && (Key.Sequence == 0),
		"short TLS AES plaintext buffer was accepted");
	for ( size_t i = 0; i < sizeof(Plain); i++ ) {
		testRequire(Plain[i] == 0xA5,
			"short TLS AES plaintext buffer was changed");
	}
	Record.Type = XTLS_RECORD_APPLICATION_DATA;
	Record.LegacyVersion = UINT16_C(0x0303);
	Record.Payload.Data = Output;
	Record.Payload.Size = XTLS13_INNER_PLAINTEXT_MAX + 17u;
	Record.EncodedSize = XTLS_RECORD_HEADER_SIZE + Record.Payload.Size;
	xrtClearError();
	testRequire(!__xrtTlsRecordOpen(
		&Key, &Record, Plain, sizeof(Plain), &Type, &Written
	) && (Written == 0) && (Key.Sequence == 0) &&
		(xrtErrorCode(xrtGetError()) == XTLS_ERROR_RECORD_SIZE),
		"oversized TLS 1.3 inner plaintext was accepted");
	Record.Payload.Data = NULL;
	Record.Payload.Size = 17u;
	xrtClearError();
	testRequire(!__xrtTlsRecordOpen(
		&Key, &Record, Plain, sizeof(Plain), &Type, &Written
	) && (xrtErrorCode(xrtGetError()) == XTLS_ERROR_ARGUMENT),
		"null TLS protected payload was accepted");
	__xrtTlsRecordKeyClear(&Key);
}



/* 执行 AES-GCM TLS 记录保护回归。 */
int main(void)
{
	testTls13AesVector();
	testTls12AesVector();
	testTlsAes256AndPadding();
	testTlsAesMutation();
	testTlsAesInPlaceAndLimits();
	testTls13AesInnerType();
	testTlsAesEdges();
	return 0;
}
