#include "../../src/internal/xrt_tls.h"

#include "../test.h"
#include "tls_schedule_vectors.h"



/* 核对 SHA-256 transcript 的分块、快照和 HelloRetryRequest 重建。 */
static void testTlsScheduleSha256Transcript(void)
{
	xtlstranscript Transcript;
	uint8 Digest[32];
	uint8 Before[sizeof(Digest)];

	memset(&Transcript, 0xA5, sizeof(Transcript));
	testRequire(__xrtTlsTranscriptInit(
		&Transcript, XCRYPTO_HASH_SHA256
	), "TLS SHA-256 transcript initialization failed");
	testRequire(__xrtTlsTranscriptUpdate(
		&Transcript, XRT_BYTES_LITERAL("client-hello-wire")
	) && __xrtTlsTranscriptUpdate(
		&Transcript, XRT_BYTES_LITERAL("server-hello-wire")
	) && __xrtTlsTranscriptDigest(
		&Transcript, Digest, sizeof(Digest)
	) && (memcmp(
		Digest, TestTlsScheduleSha256Transcript, sizeof(Digest)
	) == 0), "TLS SHA-256 transcript digest mismatch");

	memset(Digest, 0x5A, sizeof(Digest));
	memcpy(Before, Digest, sizeof(Before));
	testRequire(!__xrtTlsTranscriptDigest(
		&Transcript, Digest, sizeof(Digest) - 1u
	) && (memcmp(Digest, Before, sizeof(Digest)) == 0),
		"invalid transcript digest size changed output");
	__xrtTlsTranscriptClear(&Transcript);

	testRequire(__xrtTlsTranscriptInit(
		&Transcript, XCRYPTO_HASH_SHA256
	) && __xrtTlsTranscriptUpdate(
		&Transcript, XRT_BYTES_LITERAL("client-hello-wire")
	) && __xrtTlsTranscriptRetry(&Transcript) &&
		__xrtTlsTranscriptUpdate(
			&Transcript, XRT_BYTES_LITERAL("hello-retry-request-wire")
		) && __xrtTlsTranscriptDigest(
			&Transcript, Digest, sizeof(Digest)
		) && (memcmp(
			Digest, TestTlsScheduleSha256Retry, sizeof(Digest)
		) == 0), "TLS SHA-256 message_hash transcript mismatch");
	__xrtTlsTranscriptClear(&Transcript);
	for ( size_t i = 0; i < sizeof(Transcript); i++ ) {
		testRequire(((const uint8*)&Transcript)[i] == 0,
			"TLS SHA-256 transcript clear left state bytes");
	}
}



/* 核对 RFC 8448 的 TLS 1.3 SHA-256 密钥调度中间值。 */
static void testTlsScheduleSha256Rfc8448(void)
{
	uint8 Zero[32] = { 0 };
	uint8 Early[32];
	uint8 Derived[32];
	uint8 Handshake[32];
	uint8 Traffic[32];
	uint8 Master[32];
	uint8 Key[16];
	uint8 Iv[12];
	uint8 FinishedKey[32];

	testRequire(__xrtTls13Extract(
		XCRYPTO_HASH_SHA256,
		(xbytesview) { NULL, 0 },
		(xbytesview) { Zero, sizeof(Zero) },
		Early, sizeof(Early)
	) && (memcmp(Early, TestTls13RfcEarlySecret, sizeof(Early)) == 0),
		"RFC 8448 early secret mismatch");
	testRequire(__xrtTls13DeriveSecret(
		XCRYPTO_HASH_SHA256,
		(xbytesview) { Early, sizeof(Early) },
		XRT_STR_LITERAL("derived"),
		(xbytesview) {
			TestTls13RfcEmptyHash, sizeof(TestTls13RfcEmptyHash) - 1u
		},
		Derived, sizeof(Derived)
	) && (memcmp(
		Derived, TestTls13RfcEarlyDerived, sizeof(Derived)
	) == 0), "RFC 8448 early derived secret mismatch");
	testRequire(__xrtTls13Extract(
		XCRYPTO_HASH_SHA256,
		(xbytesview) { Derived, sizeof(Derived) },
		(xbytesview) { TestTls13RfcEcdhe, sizeof(TestTls13RfcEcdhe) - 1u },
		Handshake, sizeof(Handshake)
	) && (memcmp(
		Handshake, TestTls13RfcHandshakeSecret, sizeof(Handshake)
	) == 0), "RFC 8448 handshake secret mismatch");
	testRequire(__xrtTls13DeriveSecret(
		XCRYPTO_HASH_SHA256,
		(xbytesview) { Handshake, sizeof(Handshake) },
		XRT_STR_LITERAL("c hs traffic"),
		(xbytesview) {
			TestTls13RfcHandshakeHash, sizeof(TestTls13RfcHandshakeHash) - 1u
		}, Traffic, sizeof(Traffic)
	) && (memcmp(
		Traffic, TestTls13RfcClientHandshakeTraffic, sizeof(Traffic)
	) == 0), "RFC 8448 client handshake traffic secret mismatch");
	testRequire(__xrtTls13DeriveSecret(
		XCRYPTO_HASH_SHA256,
		(xbytesview) { Handshake, sizeof(Handshake) },
		XRT_STR_LITERAL("s hs traffic"),
		(xbytesview) {
			TestTls13RfcHandshakeHash, sizeof(TestTls13RfcHandshakeHash) - 1u
		}, Traffic, sizeof(Traffic)
	) && (memcmp(
		Traffic, TestTls13RfcServerHandshakeTraffic, sizeof(Traffic)
	) == 0), "RFC 8448 server handshake traffic secret mismatch");
	testRequire(__xrtTls13ExpandLabel(
		XCRYPTO_HASH_SHA256,
		(xbytesview) { Traffic, sizeof(Traffic) },
		XRT_STR_LITERAL("key"), (xbytesview) { NULL, 0 },
		Key, sizeof(Key)
	) && (memcmp(
		Key, TestTls13RfcServerHandshakeKey, sizeof(Key)
	) == 0), "RFC 8448 server handshake key mismatch");
	testRequire(__xrtTls13ExpandLabel(
		XCRYPTO_HASH_SHA256,
		(xbytesview) { Traffic, sizeof(Traffic) },
		XRT_STR_LITERAL("iv"), (xbytesview) { NULL, 0 },
		Iv, sizeof(Iv)
	) && (memcmp(
		Iv, TestTls13RfcServerHandshakeIv, sizeof(Iv)
	) == 0), "RFC 8448 server handshake IV mismatch");
	testRequire(__xrtTls13ExpandLabel(
		XCRYPTO_HASH_SHA256,
		(xbytesview) { Traffic, sizeof(Traffic) },
		XRT_STR_LITERAL("finished"), (xbytesview) { NULL, 0 },
		FinishedKey, sizeof(FinishedKey)
	) && (memcmp(
		FinishedKey, TestTls13RfcServerFinishedKey, sizeof(FinishedKey)
	) == 0), "RFC 8448 server Finished key mismatch");

	testRequire(__xrtTls13DeriveSecret(
		XCRYPTO_HASH_SHA256,
		(xbytesview) { Handshake, sizeof(Handshake) },
		XRT_STR_LITERAL("derived"),
		(xbytesview) {
			TestTls13RfcEmptyHash, sizeof(TestTls13RfcEmptyHash) - 1u
		},
		Derived, sizeof(Derived)
	) && (memcmp(
		Derived, TestTls13RfcMasterDerived, sizeof(Derived)
	) == 0), "RFC 8448 master derived secret mismatch");
	testRequire(__xrtTls13Extract(
		XCRYPTO_HASH_SHA256,
		(xbytesview) { Derived, sizeof(Derived) },
		(xbytesview) { Zero, sizeof(Zero) },
		Master, sizeof(Master)
	) && (memcmp(
		Master, TestTls13RfcMasterSecret, sizeof(Master)
	) == 0), "RFC 8448 master secret mismatch");
}



/* 核对独立生成的 Expand-Label 与 Finished 向量。 */
static void testTlsScheduleSha256Finished(void)
{
	uint8 Secret[32];
	uint8 Context[32];
	uint8 Expanded[32];
	uint8 FinishedKey[32];
	uint8 Finished[32];

	for ( size_t i = 0; i < sizeof(Secret); i++ ) {
		Secret[i] = (uint8)(i + 1u);
	}
	testRequire(xrtSha256(
		"context transcript", 18u, Context
	), "SHA-256 fixture context failed");
	testRequire(__xrtTls13ExpandLabel(
		XCRYPTO_HASH_SHA256,
		(xbytesview) { Secret, sizeof(Secret) },
		XRT_STR_LITERAL("fixture"),
		(xbytesview) { Context, sizeof(Context) },
		Expanded, sizeof(Expanded)
	) && (memcmp(
		Expanded, TestTlsScheduleSha256Expand, sizeof(Expanded)
	) == 0), "TLS SHA-256 Expand-Label fixture mismatch");
	testRequire(__xrtTls13ExpandLabel(
		XCRYPTO_HASH_SHA256,
		(xbytesview) { Secret, sizeof(Secret) },
		XRT_STR_LITERAL("finished"), (xbytesview) { NULL, 0 },
		FinishedKey, sizeof(FinishedKey)
	) && __xrtTls13Finished(
		XCRYPTO_HASH_SHA256,
		(xbytesview) { FinishedKey, sizeof(FinishedKey) },
		(xbytesview) { Context, sizeof(Context) },
		Finished, sizeof(Finished)
	) && (memcmp(
		Finished, TestTlsScheduleSha256Finished, sizeof(Finished)
	) == 0), "TLS SHA-256 Finished fixture mismatch");
}



/* 长 seed 必须通过流式 TLS 1.2 P_SHA256 完整派生。 */
static void testTlsScheduleSha256Prf(void)
{
	uint8 Secret[48];
	uint8 Seed[300];
	uint8 Output[80];

	for ( size_t i = 0; i < sizeof(Secret); i++ ) {
		Secret[i] = (uint8)(i + 1u);
	}
	for ( size_t i = 0; i < sizeof(Seed); i++ ) {
		Seed[i] = (uint8)i;
	}
	testRequire(__xrtTls12Prf(
		XCRYPTO_HASH_SHA256,
		(xbytesview) { Secret, sizeof(Secret) },
		XRT_STR_LITERAL("long regression label"),
		(xbytesview) { Seed, sizeof(Seed) },
		Output, sizeof(Output)
	) && (memcmp(Output, TestTlsScheduleSha256Prf, sizeof(Output)) == 0),
		"TLS 1.2 P_SHA256 long seed vector mismatch");
}



/* HkdfLabel 完整边界、重叠和错误原子性必须明确。 */
static void testTlsScheduleSha256Boundaries(void)
{
	xtlstranscript Transcript;
	uint8 Secret[32];
	uint8 Context[255];
	char Label[250];
	uint8 Output[32];
	uint8 Before[sizeof(Output)];

	memset(Secret, 0x11, sizeof(Secret));
	memset(Context, 0x22, sizeof(Context));
	memset(Label, 'L', sizeof(Label));
	testRequire(__xrtTls13ExpandLabel(
		XCRYPTO_HASH_SHA256,
		(xbytesview) { Secret, sizeof(Secret) },
		(xstrview) { Label, 249u },
		(xbytesview) { Context, sizeof(Context) },
		Output, sizeof(Output)
	), "maximum TLS 1.3 HkdfLabel was rejected");
	memcpy(Before, Output, sizeof(Before));
	testRequire(!__xrtTls13ExpandLabel(
		XCRYPTO_HASH_SHA256,
		(xbytesview) { Secret, sizeof(Secret) },
		(xstrview) { Label, sizeof(Label) },
		(xbytesview) { Context, sizeof(Context) },
		Output, sizeof(Output)
	) && (memcmp(Output, Before, sizeof(Output)) == 0),
		"oversized TLS 1.3 label changed output");
	testRequire(!__xrtTls13ExpandLabel(
		XCRYPTO_HASH_SHA256,
		(xbytesview) { Secret, sizeof(Secret) },
		XRT_STR_LITERAL("key"), (xbytesview) { NULL, 0 },
		Secret, sizeof(Secret)
	), "overlapping TLS 1.3 secret and output was accepted");
	testRequire(__xrtTls13ExpandLabel(
		XCRYPTO_HASH_SHA256,
		(xbytesview) { Secret, sizeof(Secret) },
		XRT_STR_LITERAL("empty"), (xbytesview) { NULL, 0 },
		NULL, 0
	), "zero-length TLS 1.3 expansion failed");
	testRequire(!__xrtTls13Extract(
		XCRYPTO_HASH_SHA256, (xbytesview) { NULL, 0 },
		(xbytesview) { NULL, 0 }, Output, sizeof(Output) - 1u
	), "undersized TLS 1.3 extract output was accepted");
	#if !defined(XRT_FEATURE_TLS_SCHEDULE_SHA384)
		testRequire(!__xrtTlsTranscriptInit(
			&Transcript, XCRYPTO_HASH_SHA384
		) && (xrtErrorCode(xrtGetError()) == XTLS_ERROR_KEY_DERIVATION),
			"disabled SHA-384 schedule was accepted");
	#else
		(void)Transcript;
	#endif
}



/* 双摘要构建必须允许两个独立 transcript 交错工作。 */
static void testTlsScheduleDualBackend(void)
{
	#if defined(XRT_FEATURE_TLS_SCHEDULE_SHA384)
		xtlstranscript Sha256;
		xtlstranscript Sha384;
		uint8 Digest256[32];
		uint8 Digest384[48];

		testRequire(__xrtTlsTranscriptInit(
			&Sha256, XCRYPTO_HASH_SHA256
		) && __xrtTlsTranscriptInit(
			&Sha384, XCRYPTO_HASH_SHA384
		) && __xrtTlsTranscriptUpdate(
			&Sha256, XRT_BYTES_LITERAL("client-hello-wire")
		) && __xrtTlsTranscriptUpdate(
			&Sha384, XRT_BYTES_LITERAL("client-hello-wire")
		) && __xrtTlsTranscriptUpdate(
			&Sha256, XRT_BYTES_LITERAL("server-hello-wire")
		) && __xrtTlsTranscriptUpdate(
			&Sha384, XRT_BYTES_LITERAL("server-hello-wire")
		) && __xrtTlsTranscriptDigest(
			&Sha256, Digest256, sizeof(Digest256)
		) && __xrtTlsTranscriptDigest(
			&Sha384, Digest384, sizeof(Digest384)
		) && (memcmp(
			Digest256, TestTlsScheduleSha256Transcript, sizeof(Digest256)
		) == 0) && (memcmp(
			Digest384, TestTlsScheduleSha384Transcript, sizeof(Digest384)
		) == 0), "dual TLS transcript backend mismatch");
		__xrtTlsTranscriptClear(&Sha256);
		__xrtTlsTranscriptClear(&Sha384);
	#endif
}



/* 执行 TLS SHA-256 transcript 与密钥调度回归。 */
int main(void)
{
	testRequire(__xrtTlsScheduleHashSupported(XCRYPTO_HASH_SHA256) &&
		(__xrtTlsScheduleHashSize(XCRYPTO_HASH_SHA256) == 32u),
		"TLS SHA-256 schedule backend is unavailable");
	testTlsScheduleSha256Transcript();
	testTlsScheduleSha256Rfc8448();
	testTlsScheduleSha256Finished();
	testTlsScheduleSha256Prf();
	testTlsScheduleSha256Boundaries();
	testTlsScheduleDualBackend();
	return 0;
}
