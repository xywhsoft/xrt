#include "../test.h"

#include <xrt/http_digest.h>



/* 验证写出器的未对齐、容量、回绕和别名边界。 */
static void testHttpDigestWriteMemory(void)
{
	static const uint8 Digest[] = { 0u, 1u, 2u };
	union {
		uint64 Align;
		uint8 Bytes[sizeof(size_t) + 1u];
	} SizeStorage;
	size_t* pUnalignedSize = (size_t*)(SizeStorage.Bytes + 1u);
	char arrAlgorithm[32] = "sha-256";
	char arrOutput[32];
	char arrBefore[32];
	size_t iSize;

	testRequire(
		xrtHttpDigestWrite(
			XRT_STR_LITERAL("sha-256"),
			(xbytesview){ Digest, sizeof(Digest) },
			arrOutput, sizeof(arrOutput), pUnalignedSize
		),
		"HTTP digest writer rejected unaligned length output"
	);
	memcpy(&iSize, pUnalignedSize, sizeof(iSize));
	testRequire(
		(iSize == 14u) &&
		(memcmp(arrOutput, "sha-256=:AAEC:", iSize) == 0),
		"HTTP digest unaligned length result mismatch"
	);

	memset(arrOutput, 0xA5, sizeof(arrOutput));
	memcpy(arrBefore, arrOutput, sizeof(arrOutput));
	iSize = 77u;
	xrtClearError();
	testRequire(
		!xrtHttpDigestWrite(
			XRT_STR_LITERAL("sha-256"),
			(xbytesview){ Digest, sizeof(Digest) },
			arrOutput, 4u, &iSize
		) && (iSize == 14u) &&
		(memcmp(arrOutput, arrBefore, sizeof(arrOutput)) == 0),
		"HTTP digest capacity failure was not atomic"
	);
	xrtClearError();

	iSize = 77u;
	testRequire(
		!xrtHttpDigestWrite(
			(xstrview){
				(cstr)(uintptr_t)(UINTPTR_MAX - 1u), 4u
			},
			(xbytesview){ Digest, sizeof(Digest) },
			arrOutput, sizeof(arrOutput), &iSize
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(iSize == 77u),
		"HTTP digest writer accepted wrapped algorithm"
	);
	xrtClearError();
	testRequire(
		!xrtHttpDigestWrite(
			XRT_STR_LITERAL("sha-256"),
			(xbytesview){
				(cbytes)(uintptr_t)(UINTPTR_MAX - 1u), 4u
			}, arrOutput, sizeof(arrOutput), &iSize
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP digest writer accepted wrapped digest bytes"
	);
	xrtClearError();
	testRequire(
		!xrtHttpDigestWrite(
			XRT_STR_LITERAL("sha-256"),
			(xbytesview){ Digest, sizeof(Digest) },
			(void*)(uintptr_t)(UINTPTR_MAX - 1u), 4u, &iSize
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP digest writer accepted wrapped output"
	);
	xrtClearError();
	testRequire(
		!xrtHttpDigestPreferenceWrite(
			XRT_STR_LITERAL("sha-256"), 11u,
			arrOutput, sizeof(arrOutput),
			(size_t*)(uintptr_t)(UINTPTR_MAX - 1u)
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP digest preference checked value before memory"
	);
	xrtClearError();

	testRequire(
		!xrtHttpDigestPreferenceWrite(
			(xstrview){ arrAlgorithm, 7u }, 10u,
			arrOutput, sizeof(arrOutput),
			(size_t*)(arrAlgorithm + 1u)
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP digest writer accepted algorithm/size overlap"
	);
	xrtClearError();
	iSize = 77u;
	testRequire(
		!xrtHttpDigestPreferenceWrite(
			(xstrview){ arrAlgorithm, 7u }, 10u,
			arrAlgorithm, 10u, &iSize
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(iSize == 77u),
		"HTTP digest writer accepted algorithm/output overlap"
	);
	xrtClearError();
}



/* 验证单算法摘要和偏好的规范写出。 */
int main(void)
{
	static const uint8 Digest[] = { 0u, 1u, 2u };
	char arrOutput[64];
	size_t iSize;

	testRequire(
		xrtHttpDigestWrite(
			XRT_STR_LITERAL("sha-256"),
			(xbytesview){ Digest, sizeof(Digest) },
			NULL, 0, &iSize
		) && (iSize == 14u) &&
		xrtHttpDigestWrite(
			XRT_STR_LITERAL("sha-256"),
			(xbytesview){ Digest, sizeof(Digest) },
			arrOutput, sizeof(arrOutput), &iSize
		) && (memcmp(arrOutput, "sha-256=:AAEC:", iSize) == 0),
		"HTTP digest writer mismatch"
	);
	testRequire(
		xrtHttpDigestPreferenceWrite(
			XRT_STR_LITERAL("sha-256"), 10u,
			arrOutput, sizeof(arrOutput), &iSize
		) && (iSize == 10u) &&
		(memcmp(arrOutput, "sha-256=10", iSize) == 0),
		"HTTP digest preference writer mismatch"
	);
	iSize = 77u;
	testRequire(
		!xrtHttpDigestPreferenceWrite(
			XRT_STR_LITERAL("sha-256"), 11u,
			arrOutput, sizeof(arrOutput), &iSize
		) && (iSize == 77u),
		"HTTP digest writer accepted an invalid preference"
	);
	testHttpDigestWriteMemory();
	printf("[PASS] http_digest_write\n");
	return 0;
}
