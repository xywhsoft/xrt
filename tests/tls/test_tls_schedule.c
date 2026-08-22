#include "../../src/internal/xrt_tls.h"

#include "../test.h"



/* 未选择摘要后端时，调度骨架必须保持可裁剪且不修改目标。 */
static void testTlsScheduleWithoutBackend(void)
{
	#if defined(XRT_FEATURE_TLS_SCHEDULE_SHA256) || \
		defined(XRT_FEATURE_TLS_SCHEDULE_SHA384)
		return;
	#else
		xtlstranscript Transcript;
		xtlstranscript Before;

		memset(&Transcript, 0xA5, sizeof(Transcript));
		Before = Transcript;
		testRequire(!__xrtTlsScheduleHashSupported(XCRYPTO_HASH_SHA256) &&
			!__xrtTlsScheduleHashSupported(XCRYPTO_HASH_SHA384) &&
			(__xrtTlsScheduleHashSize(XCRYPTO_HASH_SHA256) == 0) &&
			(__xrtTlsScheduleHashSize(XCRYPTO_HASH_SHA384) == 0),
			"TLS schedule skeleton unexpectedly enabled a hash backend");
		testRequire(!__xrtTlsTranscriptInit(
			&Transcript, XCRYPTO_HASH_SHA256
		) && (memcmp(&Transcript, &Before, sizeof(Transcript)) == 0) &&
			(xrtErrorCode(xrtGetError()) == XTLS_ERROR_KEY_DERIVATION),
			"unsupported TLS transcript changed the destination");
		__xrtTlsTranscriptClear(&Transcript);
		for ( size_t i = 0; i < sizeof(Transcript); i++ ) {
			testRequire(((const uint8*)&Transcript)[i] == 0,
				"TLS transcript clear left state bytes");
		}
	#endif
}



/* 摘要能力查询必须与实际裁剪宏完全一致。 */
static void testTlsScheduleFeatureMatrix(void)
{
	#if defined(XRT_FEATURE_TLS_SCHEDULE_SHA256)
		testRequire(__xrtTlsScheduleHashSupported(XCRYPTO_HASH_SHA256),
			"enabled TLS SHA-256 schedule was not reported");
	#else
		testRequire(!__xrtTlsScheduleHashSupported(XCRYPTO_HASH_SHA256),
			"disabled TLS SHA-256 schedule was reported");
	#endif
	#if defined(XRT_FEATURE_TLS_SCHEDULE_SHA384)
		testRequire(__xrtTlsScheduleHashSupported(XCRYPTO_HASH_SHA384),
			"enabled TLS SHA-384 schedule was not reported");
	#else
		testRequire(!__xrtTlsScheduleHashSupported(XCRYPTO_HASH_SHA384),
			"disabled TLS SHA-384 schedule was reported");
	#endif
}



/* 未初始化 transcript 的操作必须报告稳定的状态错误。 */
static void testTlsScheduleStateError(void)
{
	xtlstranscript Transcript;
	uint8 Digest[32];

	memset(&Transcript, 0, sizeof(Transcript));
	testRequire(!__xrtTlsTranscriptUpdate(
		&Transcript, XRT_BYTES_LITERAL("message")
	) && (xrtErrorCode(xrtGetError()) == XTLS_ERROR_TRANSCRIPT),
		"uninitialized TLS transcript update was accepted");
	testRequire(!__xrtTlsTranscriptDigest(
		&Transcript, Digest, sizeof(Digest)
	) && (xrtErrorCode(xrtGetError()) == XTLS_ERROR_TRANSCRIPT),
		"uninitialized TLS transcript digest was accepted");
}



/* 执行不绑定摘要算法的 TLS 调度基础回归。 */
int main(void)
{
	testTlsScheduleFeatureMatrix();
	testTlsScheduleWithoutBackend();
	testTlsScheduleStateError();
	return 0;
}
