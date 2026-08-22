#include "../../src/internal/xrt_tls.h"

#include "../test.h"
#include "tls_schedule_vectors.h"



/* 核对 SHA-384 transcript 的分块与 HelloRetryRequest 重建。 */
static void testTlsScheduleSha384Transcript(void)
{
	xtlstranscript Transcript;
	uint8 Digest[48];

	testRequire(__xrtTlsTranscriptInit(
		&Transcript, XCRYPTO_HASH_SHA384
	) && __xrtTlsTranscriptUpdate(
		&Transcript, XRT_BYTES_LITERAL("client-hello-wire")
	) && __xrtTlsTranscriptUpdate(
		&Transcript, XRT_BYTES_LITERAL("server-hello-wire")
	) && __xrtTlsTranscriptDigest(
		&Transcript, Digest, sizeof(Digest)
	) && (memcmp(
		Digest, TestTlsScheduleSha384Transcript, sizeof(Digest)
	) == 0), "TLS SHA-384 transcript digest mismatch");
	__xrtTlsTranscriptClear(&Transcript);

	testRequire(__xrtTlsTranscriptInit(
		&Transcript, XCRYPTO_HASH_SHA384
	) && __xrtTlsTranscriptUpdate(
		&Transcript, XRT_BYTES_LITERAL("client-hello-wire")
	) && __xrtTlsTranscriptRetry(&Transcript) &&
		__xrtTlsTranscriptUpdate(
			&Transcript, XRT_BYTES_LITERAL("hello-retry-request-wire")
		) && __xrtTlsTranscriptDigest(
			&Transcript, Digest, sizeof(Digest)
		) && (memcmp(
			Digest, TestTlsScheduleSha384Retry, sizeof(Digest)
		) == 0), "TLS SHA-384 message_hash transcript mismatch");
	__xrtTlsTranscriptClear(&Transcript);
}



/* 核对 SHA-384 Extract、Expand-Label 与 Finished 固定向量。 */
static void testTlsScheduleSha384Derive(void)
{
	uint8 Zero[48] = { 0 };
	uint8 Early[48];
	uint8 Secret[48];
	uint8 Context[48];
	uint8 Expanded[48];
	uint8 FinishedKey[48];
	uint8 Finished[48];

	testRequire(__xrtTls13Extract(
		XCRYPTO_HASH_SHA384, (xbytesview) { NULL, 0 },
		(xbytesview) { Zero, sizeof(Zero) }, Early, sizeof(Early)
	) && (memcmp(
		Early, TestTlsScheduleSha384EarlySecret, sizeof(Early)
	) == 0), "TLS SHA-384 early secret mismatch");
	for ( size_t i = 0; i < sizeof(Secret); i++ ) {
		Secret[i] = (uint8)(i + 1u);
	}
	testRequire(xrtSha384(
		"context transcript", 18u, Context
	), "SHA-384 fixture context failed");
	testRequire(__xrtTls13ExpandLabel(
		XCRYPTO_HASH_SHA384,
		(xbytesview) { Secret, sizeof(Secret) },
		XRT_STR_LITERAL("fixture"),
		(xbytesview) { Context, sizeof(Context) },
		Expanded, sizeof(Expanded)
	) && (memcmp(
		Expanded, TestTlsScheduleSha384Expand, sizeof(Expanded)
	) == 0), "TLS SHA-384 Expand-Label fixture mismatch");
	testRequire(__xrtTls13ExpandLabel(
		XCRYPTO_HASH_SHA384,
		(xbytesview) { Secret, sizeof(Secret) },
		XRT_STR_LITERAL("finished"), (xbytesview) { NULL, 0 },
		FinishedKey, sizeof(FinishedKey)
	) && __xrtTls13Finished(
		XCRYPTO_HASH_SHA384,
		(xbytesview) { FinishedKey, sizeof(FinishedKey) },
		(xbytesview) { Context, sizeof(Context) },
		Finished, sizeof(Finished)
	) && (memcmp(
		Finished, TestTlsScheduleSha384Finished, sizeof(Finished)
	) == 0), "TLS SHA-384 Finished fixture mismatch");
}



/* 长 seed 必须通过流式 TLS 1.2 P_SHA384 完整派生。 */
static void testTlsScheduleSha384Prf(void)
{
	uint8 Secret[48];
	uint8 Seed[300];
	uint8 Output[96];

	for ( size_t i = 0; i < sizeof(Secret); i++ ) {
		Secret[i] = (uint8)(i + 1u);
	}
	for ( size_t i = 0; i < sizeof(Seed); i++ ) {
		Seed[i] = (uint8)i;
	}
	testRequire(__xrtTls12Prf(
		XCRYPTO_HASH_SHA384,
		(xbytesview) { Secret, sizeof(Secret) },
		XRT_STR_LITERAL("long regression label"),
		(xbytesview) { Seed, sizeof(Seed) },
		Output, sizeof(Output)
	) && (memcmp(Output, TestTlsScheduleSha384Prf, sizeof(Output)) == 0),
		"TLS 1.2 P_SHA384 long seed vector mismatch");
}



/* 完整 HkdfLabel 边界在 SHA-384 后端也必须可用。 */
static void testTlsScheduleSha384Boundaries(void)
{
	uint8 Secret[48];
	uint8 Context[255];
	char Label[249];
	uint8 Output[48];

	memset(Secret, 0x33, sizeof(Secret));
	memset(Context, 0x44, sizeof(Context));
	memset(Label, 'H', sizeof(Label));
	testRequire(__xrtTls13ExpandLabel(
		XCRYPTO_HASH_SHA384,
		(xbytesview) { Secret, sizeof(Secret) },
		(xstrview) { Label, sizeof(Label) },
		(xbytesview) { Context, sizeof(Context) },
		Output, sizeof(Output)
	), "maximum SHA-384 HkdfLabel was rejected");
	testRequire(!__xrtTls13Finished(
		XCRYPTO_HASH_SHA384,
		(xbytesview) { Secret, sizeof(Secret) - 1u },
		(xbytesview) { Output, sizeof(Output) },
		Output, sizeof(Output)
	), "invalid SHA-384 Finished key size was accepted");
}



/* 执行 TLS SHA-384 transcript 与密钥调度回归。 */
int main(void)
{
	testRequire(__xrtTlsScheduleHashSupported(XCRYPTO_HASH_SHA384) &&
		(__xrtTlsScheduleHashSize(XCRYPTO_HASH_SHA384) == 48u),
		"TLS SHA-384 schedule backend is unavailable");
	testTlsScheduleSha384Transcript();
	testTlsScheduleSha384Derive();
	testTlsScheduleSha384Prf();
	testTlsScheduleSha384Boundaries();
	return 0;
}
