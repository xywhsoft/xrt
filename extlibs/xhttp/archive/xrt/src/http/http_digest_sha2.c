#include "../internal/xrt_http.h"

#include <xrt/crypto.h>
#include <xrt/http_digest.h>



#if defined(XRT_FEATURE_HTTP_DIGEST_SHA2)

/* 判断 Structured key 是否与固定算法名相同。 */
static bool __xrtHttpDigestAlgorithmEqual(
	xstrview Algorithm,
	const char* sExpected,
	size_t iSize
)
{
	return (Algorithm.Size == iSize) &&
		(memcmp(Algorithm.Data, sExpected, iSize) == 0);
}



/* 在计算正文摘要前验证输入、目标和长度输出。 */
static bool __xrtHttpDigestSha2WriteArguments(
	const void* pData,
	size_t iSize,
	void* pOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	return __xrtRangeValid(pData, iSize) &&
		__xrtRangeValid(pOutputSize, sizeof(*pOutputSize)) &&
		((pOutput == NULL) ? (iCapacity == 0) :
			(__xrtRangeValid(pOutput, iCapacity) &&
			 !__xrtRangesOverlap(
				pOutput, iCapacity,
				pOutputSize, sizeof(*pOutputSize)
			 )));
}



/* 计算 SHA-256 并写出单成员 Dictionary。 */
XRT_API bool xrtHttpDigestSha256Write(
	const void* pData,
	size_t iSize,
	void* pOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	uint8 Digest[XRT_SHA256_SIZE];
	bool bResult;

	if ( !__xrtHttpDigestSha2WriteArguments(
		pData, iSize, pOutput, iCapacity, pOutputSize
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtSha256(pData, iSize, Digest) ) {
		return false;
	}
	bResult = xrtHttpDigestWrite(
		XRT_STR_LITERAL("sha-256"),
		(xbytesview){ Digest, sizeof(Digest) },
		pOutput, iCapacity, pOutputSize
	);
	xrtSecureZero(Digest, sizeof(Digest));
	return bResult;
}



/* 计算 SHA-512 并写出单成员 Dictionary。 */
XRT_API bool xrtHttpDigestSha512Write(
	const void* pData,
	size_t iSize,
	void* pOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	uint8 Digest[XRT_SHA512_SIZE];
	bool bResult;

	if ( !__xrtHttpDigestSha2WriteArguments(
		pData, iSize, pOutput, iCapacity, pOutputSize
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtSha512(pData, iSize, Digest) ) {
		return false;
	}
	bResult = xrtHttpDigestWrite(
		XRT_STR_LITERAL("sha-512"),
		(xbytesview){ Digest, sizeof(Digest) },
		pOutput, iCapacity, pOutputSize
	);
	xrtSecureZero(Digest, sizeof(Digest));
	return bResult;
}



/* 读取摘要后以常量时间比较 SHA-2 计算结果。 */
XRT_API xhttpdigestmatch xrtHttpDigestSha2Verify(
	const xhttpdigest* pDigest,
	const void* pData,
	size_t iSize
)
{
	xhttpdigest Digest;
	uint8 Expected[XRT_SHA512_SIZE];
	uint8 Received[XRT_SHA512_SIZE];
	size_t iExpected;
	size_t iReceived;
	bool bEqual;

	if ( !__xrtRangeValid(pDigest, sizeof(Digest)) ||
		!__xrtRangeValid(pData, iSize) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_DIGEST_MATCH_ERROR;
	}
	memcpy(&Digest, pDigest, sizeof(Digest));
	if ( !__xrtHttpViewValid(Digest.Algorithm) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_DIGEST_MATCH_ERROR;
	}
	if ( !xrtHttpStructuredKeyValid(Digest.Algorithm) ) {
		if ( xrtGetError() == NULL ) {
			__xrtErrorSetValue();
		}
		return XHTTP_DIGEST_MATCH_ERROR;
	}
	if ( __xrtHttpDigestAlgorithmEqual(
		Digest.Algorithm, "sha-256", 7u
	) ) {
		iExpected = XRT_SHA256_SIZE;
	} else if ( __xrtHttpDigestAlgorithmEqual(
		Digest.Algorithm, "sha-512", 7u
	) ) {
		iExpected = XRT_SHA512_SIZE;
	} else {
		return XHTTP_DIGEST_MATCH_UNSUPPORTED;
	}
	if ( !xrtHttpDigestRead(
		&Digest, NULL, 0, &iReceived
	) ) {
		return XHTTP_DIGEST_MATCH_ERROR;
	}
	if ( iReceived != iExpected ) {
		return XHTTP_DIGEST_MATCH_MISMATCH;
	}
	if ( !xrtHttpDigestRead(
		&Digest, Received, sizeof(Received), &iReceived
	) ) {
		xrtSecureZero(Received, sizeof(Received));
		return XHTTP_DIGEST_MATCH_ERROR;
	}
	if ( iExpected == XRT_SHA256_SIZE ) {
		if ( !xrtSha256(pData, iSize, Expected) ) {
			xrtSecureZero(Expected, sizeof(Expected));
			xrtSecureZero(Received, sizeof(Received));
			return XHTTP_DIGEST_MATCH_ERROR;
		}
	} else if ( !xrtSha512(pData, iSize, Expected) ) {
		xrtSecureZero(Expected, sizeof(Expected));
		xrtSecureZero(Received, sizeof(Received));
		return XHTTP_DIGEST_MATCH_ERROR;
	}
	bEqual = xrtConstTimeEqual(Expected, Received, iExpected);
	xrtSecureZero(Expected, sizeof(Expected));
	xrtSecureZero(Received, sizeof(Received));
	return bEqual ?
		XHTTP_DIGEST_MATCH_OK : XHTTP_DIGEST_MATCH_MISMATCH;
}

#endif
