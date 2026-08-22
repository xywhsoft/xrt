#include "../../src/internal/xrt_tls.h"

#include "../test.h"



/* 核对 TLS 1.2、TLS 1.3 共用的序列号 nonce 构造。 */
static void testTlsRecordNonce(void)
{
	static const uint8 Iv[] = {
		0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5,
		0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB
	};
	static const uint8 Expected[] = {
		0xA0, 0xA1, 0xA2, 0xA3, 0xA5, 0xA7,
		0xA5, 0xA3, 0xAD, 0xAF, 0xAD, 0xA3
	};
	uint8 Nonce[12];
	uint8 Encoded[8];

	__xrtTlsRecordNonce(
		Nonce, Iv, UINT64_C(0x0102030405060708)
	);
	testRequire(memcmp(Nonce, Expected, sizeof(Nonce)) == 0,
		"TLS record nonce construction mismatch");
	__xrtTlsWrite64(Encoded, UINT64_C(0x0102030405060708));
	testRequire(memcmp(Encoded, "\x01\x02\x03\x04\x05\x06\x07\x08", 8u) == 0,
		"TLS record sequence encoding mismatch");
}



/* 无 AEAD 后端的记录骨架必须保持可裁剪且明确拒绝密码套件。 */
static void testTlsRecordWithoutBackend(void)
{
	xtlsrecordkey Key;

#if !defined(XRT_FEATURE_TLS_RECORD_AES) || \
	!defined(XRT_FEATURE_TLS_RECORD_CHACHA)
	uint8 KeyData[32] = { 0 };
	uint8 Iv[12] = { 0 };
	xtlsrecordkey Before;
#endif

	memset(&Key, 0xA5, sizeof(Key));

#if !defined(XRT_FEATURE_TLS_RECORD_AES)
	Before = Key;
	testRequire(!__xrtTlsRecordCipherSupported(
		XTLS_VERSION_13, XTLS_AES_128_GCM_SHA256
	), "TLS record skeleton unexpectedly enabled AES-GCM");
	xrtClearError();
	testRequire(!__xrtTlsRecordKeyInit(
		&Key,
		XTLS_VERSION_13,
		XTLS_AES_128_GCM_SHA256,
		(xbytesview) { KeyData, 16u },
		(xbytesview) { Iv, sizeof(Iv) }
	) && (xrtErrorCode(xrtGetError()) == XTLS_ERROR_CIPHER) &&
		(memcmp(&Key, &Before, sizeof(Key)) == 0),
		"unsupported TLS AES-GCM key changed the destination");
#endif

#if !defined(XRT_FEATURE_TLS_RECORD_CHACHA)
	Before = Key;
	testRequire(!__xrtTlsRecordCipherSupported(
		XTLS_VERSION_13, XTLS_CHACHA20_POLY1305_SHA256
	), "TLS record skeleton unexpectedly enabled ChaCha20-Poly1305");
	xrtClearError();
	testRequire(!__xrtTlsRecordKeyInit(
		&Key,
		XTLS_VERSION_13,
		XTLS_CHACHA20_POLY1305_SHA256,
		(xbytesview) { KeyData, sizeof(KeyData) },
		(xbytesview) { Iv, sizeof(Iv) }
	) && (xrtErrorCode(xrtGetError()) == XTLS_ERROR_CIPHER) &&
		(memcmp(&Key, &Before, sizeof(Key)) == 0),
		"unsupported TLS ChaCha20 key changed the destination");
#endif

	__xrtTlsRecordKeyClear(&Key);
	for ( size_t i = 0; i < sizeof(Key); i++ ) {
		testRequire(((const uint8*)&Key)[i] == 0,
			"TLS record key clear left sensitive state");
	}
}



/* 执行不绑定算法的记录保护基础回归。 */
int main(void)
{
	testTlsRecordNonce();
	testTlsRecordWithoutBackend();
	return 0;
}
