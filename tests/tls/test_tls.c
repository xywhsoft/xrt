#include "../test.h"



/* 完整记录必须返回借用负载，并且只消费输入中的第一条记录。 */
static void testTlsRecordParse(void)
{
	static const uint8 Data[] = {
		22, 0x03, 0x03, 0x00, 0x05,
		'h', 'e', 'l', 'l', 'o',
		21, 0x03, 0x03, 0x00, 0x02, 2, 0
	};
	xtlsrecord Record;
	size_t iRequired = 0;

	memset(&Record, 0, sizeof(Record));
	testRequire(xrtTlsRecordParse(
		(xbytesview) { Data, sizeof(Data) }, &Record, &iRequired
	) == XTLS_OK, "complete TLS record parse failed");
	testRequire((Record.Type == XTLS_RECORD_HANDSHAKE) &&
		(Record.LegacyVersion == UINT16_C(0x0303)) &&
		(Record.Payload.Data == Data + XTLS_RECORD_HEADER_SIZE) &&
		(Record.Payload.Size == 5u) &&
		(Record.EncodedSize == 10u) && (iRequired == 10u),
		"parsed TLS record fields mismatch");
}



/* 每一个截断前缀都必须返回精确需求，且不得修改输出或制造错误。 */
static void testTlsRecordFragments(void)
{
	static const uint8 Data[] = {
		23, 0x03, 0x03, 0x00, 0x07,
		'f', 'r', 'a', 'g', 'm', 'e', 'n'
	};

	for ( size_t i = 0; i < sizeof(Data); i++ ) {
		xtlsrecord Record;
		xtlsrecord Before;
		size_t iRequired = 0;

		memset(&Record, 0xA5, sizeof(Record));
		Before = Record;
		xrtClearError();
		testRequire(xrtTlsRecordParse(
			(xbytesview) { Data, i }, &Record, &iRequired
		) == XTLS_AGAIN, "truncated TLS record did not request more input");
		testRequire(memcmp(&Record, &Before, sizeof(Record)) == 0,
			"truncated TLS record changed output");
		testRequire(iRequired == (i < XTLS_RECORD_HEADER_SIZE ?
			XTLS_RECORD_HEADER_SIZE : sizeof(Data)),
			"truncated TLS record reported the wrong requirement");
		testRequire(xrtGetError() == NULL,
			"normal TLS fragmentation set a structured error");
	}
}



/* 编码支持输入输出重叠，并能与解析器无损往返。 */
static void testTlsRecordEncode(void)
{
	uint8 Buffer[32];
	xtlsrecord Record;
	size_t iRequired = 0;

	memset(Buffer, 0, sizeof(Buffer));
	memcpy(Buffer, "overlap", 7);
	testRequire(xrtTlsRecordEncode(
		XTLS_RECORD_APPLICATION_DATA,
		UINT16_C(0x0303),
		(xbytesview) { Buffer, 7 },
		Buffer,
		sizeof(Buffer)
	), "overlapping TLS record encode failed");
	testRequire(xrtTlsRecordParse(
		(xbytesview) { Buffer, 12 }, &Record, &iRequired
	) == XTLS_OK, "encoded TLS record did not parse");
	testRequire((Record.Type == XTLS_RECORD_APPLICATION_DATA) &&
		(Record.Payload.Size == 7u) &&
		(memcmp(Record.Payload.Data, "overlap", 7) == 0),
		"overlapping TLS record changed payload");
	testRequire(xrtTlsRecordSize(7) == 12u,
		"TLS record size calculation mismatch");
}



/* 非法类型、版本、长度和输出容量必须失败并保持输出原子性。 */
static void testTlsRecordErrors(void)
{
	static const uint8 BadType[] = { 24, 0x03, 0x03, 0, 0 };
	static const uint8 BadVersion[] = { 22, 0x03, 0x04, 0, 0 };
	static const uint8 BadSize[] = { 22, 0x03, 0x03, 0x48, 0x01 };
	uint8 Output[8];
	uint8 Before[8];
	xtlsrecord Record;
	xtlsrecord RecordBefore;
	const xerror* pError;

	memset(&Record, 0xA5, sizeof(Record));
	RecordBefore = Record;
	testRequire(xrtTlsRecordParse(
		(xbytesview) { BadType, sizeof(BadType) }, &Record, NULL
	) == XTLS_ERROR, "unknown TLS record type was accepted");
	testRequire(memcmp(&Record, &RecordBefore, sizeof(Record)) == 0,
		"invalid TLS record type changed output");
	pError = xrtGetError();
	testRequire((pError != NULL) &&
		(strcmp(xrtErrorDomain(pError), "xrt.tls") == 0) &&
		(xrtErrorCode(pError) == XTLS_ERROR_RECORD_TYPE),
		"TLS record type error is not structured");

	testRequire(xrtTlsRecordParse(
		(xbytesview) { BadVersion, sizeof(BadVersion) }, &Record, NULL
	) == XTLS_ERROR, "invalid TLS record version was accepted");
	testRequire(xrtErrorCode(xrtGetError()) == XTLS_ERROR_RECORD_VERSION,
		"TLS record version error code mismatch");

	testRequire(xrtTlsRecordParse(
		(xbytesview) { BadSize, sizeof(BadSize) }, &Record, NULL
	) == XTLS_ERROR, "oversized TLS record was accepted");
	testRequire(xrtErrorCode(xrtGetError()) == XTLS_ERROR_RECORD_SIZE,
		"TLS record size error code mismatch");

	memset(Output, 0x5A, sizeof(Output));
	memcpy(Before, Output, sizeof(Output));
	testRequire(!xrtTlsRecordEncode(
		XTLS_RECORD_HANDSHAKE,
		UINT16_C(0x0303),
		XRT_BYTES_LITERAL("data"),
		Output,
		sizeof(Output)
	), "undersized TLS record output was accepted");
	testRequire(memcmp(Output, Before, sizeof(Output)) == 0,
		"failed TLS record encode changed output");
}



/* Alert 编解码必须保留线路数值并拒绝错误长度与级别。 */
static void testTlsAlert(void)
{
	uint8 Payload[2];
	xtlsalertlevel Level = XTLS_ALERT_WARNING;
	xtlsalert Alert = XTLS_ALERT_CLOSE_NOTIFY;

	testRequire(xrtTlsAlertEncode(
		XTLS_ALERT_FATAL, XTLS_ALERT_DECODE_ERROR,
		Payload, sizeof(Payload)
	), "TLS alert encode failed");
	testRequire(xrtTlsAlertParse(
		(xbytesview) { Payload, sizeof(Payload) }, &Level, &Alert
	), "TLS alert parse failed");
	testRequire((Level == XTLS_ALERT_FATAL) &&
		(Alert == XTLS_ALERT_DECODE_ERROR) &&
		(strcmp(xrtTlsAlertName(Alert), "decode_error") == 0),
		"TLS alert round trip mismatch");

	Payload[0] = 3;
	testRequire(!xrtTlsAlertParse(
		(xbytesview) { Payload, sizeof(Payload) }, &Level, &Alert
	), "invalid TLS alert level was accepted");
	testRequire(xrtErrorCode(xrtGetError()) == XTLS_ERROR_ALERT,
		"TLS alert error code mismatch");
	testRequire(!xrtTlsAlertEncode(
		XTLS_ALERT_FATAL, (xtlsalert)256, Payload, sizeof(Payload)
	), "oversized TLS alert description was truncated");
}



/* 密码套件名称必须稳定且保留标准线路命名。 */
static void testTlsCipherNames(void)
{
	testRequire(strcmp(xrtTlsCipherName(
		XTLS_AES_128_GCM_SHA256
	), "TLS_AES_128_GCM_SHA256") == 0,
		"TLS 1.3 cipher name mismatch");
	testRequire(strcmp(xrtTlsCipherName(
		XTLS_ECDHE_RSA_CHACHA20_POLY1305_SHA256
	), "TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256") == 0,
		"TLS 1.2 cipher name mismatch");
	testRequire(strcmp(xrtTlsCipherName((xtlscipher)0), "unknown") == 0,
		"unknown TLS cipher name mismatch");
}



/* 每个公开套件必须只映射到一组完整且自洽的协议参数。 */
static void testTlsCipherInfo(void)
{
	static const xtlscipher Ciphers[] = {
		XTLS_AES_128_GCM_SHA256,
		XTLS_AES_256_GCM_SHA384,
		XTLS_CHACHA20_POLY1305_SHA256,
		XTLS_ECDHE_ECDSA_AES_128_GCM_SHA256,
		XTLS_ECDHE_RSA_AES_128_GCM_SHA256,
		XTLS_ECDHE_ECDSA_AES_256_GCM_SHA384,
		XTLS_ECDHE_RSA_AES_256_GCM_SHA384,
		XTLS_ECDHE_RSA_CHACHA20_POLY1305_SHA256,
		XTLS_ECDHE_ECDSA_CHACHA20_POLY1305_SHA256
	};

	for ( size_t i = 0; i < (sizeof(Ciphers) / sizeof(Ciphers[0])); i++ ) {
		const xtlscipherinfo* pInfo = xrtTlsCipherInfo(Ciphers[i]);

		testRequire((pInfo != NULL) && (pInfo->Cipher == Ciphers[i]),
			"TLS cipher metadata is missing or mismatched");
		testRequire(((pInfo->Version == XTLS_VERSION_12) ||
			(pInfo->Version == XTLS_VERSION_13)) &&
			((pInfo->Hash == XTLS_HASH_SHA256) ||
			 (pInfo->Hash == XTLS_HASH_SHA384)) &&
			(pInfo->HashSize ==
			 (pInfo->Hash == XTLS_HASH_SHA256 ? 32u : 48u)) &&
			((pInfo->KeySize == 16u) || (pInfo->KeySize == 32u)) &&
			((pInfo->IvSize == 4u) || (pInfo->IvSize == 12u)) &&
			(pInfo->TagSize == 16u),
			"TLS cipher metadata has inconsistent sizes");
		if ( pInfo->Version == XTLS_VERSION_13 ) {
			testRequire(
				(pInfo->Authentication ==
				 XTLS_CIPHER_AUTH_INDEPENDENT) &&
				(pInfo->ExplicitNonceSize == 0u),
				"TLS 1.3 cipher metadata leaked TLS 1.2 parameters"
			);
		} else if ( pInfo->Aead == XTLS_AEAD_AES_GCM ) {
			testRequire((pInfo->IvSize == 4u) &&
				(pInfo->ExplicitNonceSize == 8u),
				"TLS 1.2 AES-GCM nonce metadata mismatch");
		} else {
			testRequire((pInfo->IvSize == 12u) &&
				(pInfo->ExplicitNonceSize == 0u),
				"TLS 1.2 ChaCha nonce metadata mismatch");
		}
	}

	xrtClearError();
	testRequire(xrtTlsCipherInfo((xtlscipher)0) == NULL,
		"unknown TLS cipher produced metadata");
	testRequire(xrtGetError() == NULL,
		"unknown TLS cipher metadata lookup set an error");
}



/* 执行 TLS 核心记录与 Alert 回归。 */
int main(void)
{
	testTlsRecordParse();
	testTlsRecordFragments();
	testTlsRecordEncode();
	testTlsRecordErrors();
	testTlsAlert();
	testTlsCipherNames();
	testTlsCipherInfo();
	return 0;
}
