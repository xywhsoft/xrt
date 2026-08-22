#include "../internal/xrt_http.h"

#include <xrt/http_auth.h>



#if defined(XRT_FEATURE_HTTP_AUTH_DIGEST_NONCE)

#define XRT_HTTP_DIGEST_NONCE_VERSION 1u
#define XRT_HTTP_DIGEST_NONCE_BODY_SIZE \
	(1u + 8u + XRT_HTTP_DIGEST_NONCE_SALT_SIZE)
#define XRT_HTTP_DIGEST_NONCE_BINARY_SIZE \
	(XRT_HTTP_DIGEST_NONCE_BODY_SIZE + XRT_SHA256_SIZE)
#define XRT_HTTP_DIGEST_NONCE_MAX_MESSAGE (UINT64_MAX >> 3u)



static const uint8 __xrtHttpDigestNonceDomain[] =
	"xrt-http-digest-nonce-v1";



/* 写出无符号 64 位大端时间戳。 */
static void __xrtHttpDigestNonceStore64(uint8* pOutput, uint64 iValue)
{
	for ( size_t i = 0; i < 8u; i++ ) {
		pOutput[7u - i] = (uint8)(iValue >> (i * 8u));
	}
}



/* 读取无符号 64 位大端时间戳。 */
static uint64 __xrtHttpDigestNonceLoad64(const uint8* pInput)
{
	uint64 iValue = 0;

	for ( size_t i = 0; i < 8u; i++ ) {
		iValue = (iValue << 8u) | (uint64)pInput[i];
	}
	return iValue;
}



/* 验证密钥、上下文和 HMAC 可表示的累计消息长度。 */
static bool __xrtHttpDigestNonceInputValid(
	xbytesview Key,
	xbytesview Context,
	int64 iIssuedSeconds
)
{
	uint64 iPrefix = (uint64)(
		sizeof(__xrtHttpDigestNonceDomain) - 1u +
		XRT_HTTP_DIGEST_NONCE_BODY_SIZE
	);

	if ( !__xrtRangeValid(Key.Data, Key.Size) ||
		!__xrtRangeValid(Context.Data, Context.Size) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (Key.Size < XRT_HTTP_DIGEST_NONCE_KEY_MIN) ||
		(iIssuedSeconds < 0) ) {
		__xrtErrorSetValue();
		return false;
	}
	if ( ((uint64)Key.Size > XRT_HTTP_DIGEST_NONCE_MAX_MESSAGE) ||
		((uint64)Context.Size >
		 (XRT_HTTP_DIGEST_NONCE_MAX_MESSAGE - iPrefix)) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	return true;
}



/* 验证写出描述符和所有输入范围互不覆盖。 */
static bool __xrtHttpDigestNonceOutputValid(
	xbytesview Key,
	xbytesview Context,
	const void* pSalt,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	if ( !__xrtRangeValid(pSize, sizeof(size_t)) ||
		((pOutput == NULL) && (iCapacity != 0)) ||
		((pOutput != NULL) && !__xrtRangeValid(
			pOutput, iCapacity
		)) || ((pSalt != NULL) && !__xrtRangeValid(
			pSalt, XRT_HTTP_DIGEST_NONCE_SALT_SIZE
		)) || __xrtRangesOverlap(
			pSize, sizeof(size_t), Key.Data, Key.Size
		) || __xrtRangesOverlap(
			pSize, sizeof(size_t), Context.Data, Context.Size
		) || ((pSalt != NULL) && __xrtRangesOverlap(
			pSize, sizeof(size_t),
			pSalt, XRT_HTTP_DIGEST_NONCE_SALT_SIZE
		)) || ((pOutput != NULL) && (
			__xrtRangesOverlap(
				pOutput, iCapacity, pSize, sizeof(size_t)
			) || __xrtRangesOverlap(
				pOutput, iCapacity, Key.Data, Key.Size
			) || __xrtRangesOverlap(
				pOutput, iCapacity, Context.Data, Context.Size
			) || ((pSalt != NULL) && __xrtRangesOverlap(
				pOutput, iCapacity,
				pSalt, XRT_HTTP_DIGEST_NONCE_SALT_SIZE
			))
		)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return true;
}



/* 发布固定长度 nonce 的查询、短缓冲或实际写入决策。 */
static bool __xrtHttpDigestNonceOutputPrepare(
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	bool* pWrite
)
{
	size_t iRequired = XRT_HTTP_DIGEST_NONCE_TEXT_SIZE;

	*pWrite = false;
	if ( pOutput == NULL ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		return true;
	}
	if ( iCapacity < iRequired ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		__xrtErrorSetRange();
		return false;
	}
	*pWrite = true;
	return true;
}



/* 计算 HMAC 并把固定二进制 nonce 编码为 Base64URL。 */
static bool __xrtHttpDigestNonceEncode(
	xbytesview Key,
	xbytesview Context,
	int64 iIssuedSeconds,
	const uint8* pSalt,
	char* sOutput
)
{
	static const xbase64config Config = {
		NULL,
		(uint32)XBASE64_URL | (uint32)XBASE64_NO_PADDING
	};
	xhmacsha256 State;
	uint8 Binary[XRT_HTTP_DIGEST_NONCE_BINARY_SIZE];
	char Encoded[XRT_HTTP_DIGEST_NONCE_TEXT_SIZE + 1u];
	size_t iSize;
	bool bResult = false;

	memset(&State, 0, sizeof(State));
	memset(Binary, 0, sizeof(Binary));
	memset(Encoded, 0, sizeof(Encoded));
	Binary[0] = XRT_HTTP_DIGEST_NONCE_VERSION;
	__xrtHttpDigestNonceStore64(
		Binary + 1u, (uint64)iIssuedSeconds
	);
	memcpy(
		Binary + 9u,
		pSalt,
		XRT_HTTP_DIGEST_NONCE_SALT_SIZE
	);
	if ( !xrtHmacSha256Init(&State, Key.Data, Key.Size) ||
		!xrtHmacSha256Update(
			&State,
			__xrtHttpDigestNonceDomain,
			sizeof(__xrtHttpDigestNonceDomain) - 1u
		) || !xrtHmacSha256Update(
			&State, Binary, XRT_HTTP_DIGEST_NONCE_BODY_SIZE
		) || !xrtHmacSha256Update(
			&State, Context.Data, Context.Size
		) || !xrtHmacSha256Final(
			&State, Binary + XRT_HTTP_DIGEST_NONCE_BODY_SIZE
		) || !xrtBase64Encode(
			Binary,
			sizeof(Binary),
			Encoded,
			sizeof(Encoded),
			&iSize,
			&Config
		) || (iSize != XRT_HTTP_DIGEST_NONCE_TEXT_SIZE) ) {
		goto cleanup;
	}
	memcpy(sOutput, Encoded, XRT_HTTP_DIGEST_NONCE_TEXT_SIZE);
	bResult = true;

cleanup:
	xrtSecureZero(&State, sizeof(State));
	xrtSecureZero(Binary, sizeof(Binary));
	xrtSecureZero(Encoded, sizeof(Encoded));
	return bResult;
}



/* 使用调用方 salt 构建确定性的无状态 nonce。 */
XRT_API bool xrtHttpDigestNonceWrite(
	xbytesview Key,
	xbytesview Context,
	int64 iIssuedSeconds,
	const void* pSalt,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	char Nonce[XRT_HTTP_DIGEST_NONCE_TEXT_SIZE];
	bool bWrite;

	if ( (pSalt == NULL) || !__xrtRangeValid(
		pSalt, XRT_HTTP_DIGEST_NONCE_SALT_SIZE
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtHttpDigestNonceInputValid(
		Key, Context, iIssuedSeconds
	) || !__xrtHttpDigestNonceOutputValid(
		Key, Context, pSalt, pOutput, iCapacity, pSize
	) ) {
		return false;
	}
	if ( !__xrtHttpDigestNonceOutputPrepare(
		pOutput, iCapacity, pSize, &bWrite
	) || !bWrite ) {
		return pOutput == NULL;
	}
	if ( !__xrtHttpDigestNonceEncode(
		Key, Context, iIssuedSeconds, (const uint8*)pSalt, Nonce
	) ) {
		xrtSecureZero(Nonce, sizeof(Nonce));
		return false;
	}
	memcpy(pOutput, Nonce, sizeof(Nonce));
	{
		size_t iRequired = sizeof(Nonce);

		memcpy(pSize, &iRequired, sizeof(iRequired));
	}
	xrtSecureZero(Nonce, sizeof(Nonce));
	return true;
}



/* 判断固定长度输入是否只包含无填充 Base64URL 字符。 */
static bool __xrtHttpDigestNonceTextValid(xstrview Nonce)
{
	if ( Nonce.Size != XRT_HTTP_DIGEST_NONCE_TEXT_SIZE ) {
		return false;
	}
	for ( size_t i = 0; i < Nonce.Size; i++ ) {
		uint8 c = (uint8)Nonce.Data[i];

		if ( !(((c >= (uint8)'A') && (c <= (uint8)'Z')) ||
			  ((c >= (uint8)'a') && (c <= (uint8)'z')) ||
			  ((c >= (uint8)'0') && (c <= (uint8)'9')) ||
			  (c == (uint8)'-') || (c == (uint8)'_')) ) {
			return false;
		}
	}
	return true;
}



/* 重新计算固定二进制 nonce 的 HMAC。 */
static bool __xrtHttpDigestNonceMac(
	xbytesview Key,
	xbytesview Context,
	const uint8* pBinary,
	uint8* pMac
)
{
	xhmacsha256 State;
	bool bResult;

	memset(&State, 0, sizeof(State));
	bResult = xrtHmacSha256Init(&State, Key.Data, Key.Size) &&
		xrtHmacSha256Update(
			&State,
			__xrtHttpDigestNonceDomain,
			sizeof(__xrtHttpDigestNonceDomain) - 1u
		) && xrtHmacSha256Update(
			&State, pBinary, XRT_HTTP_DIGEST_NONCE_BODY_SIZE
		) && xrtHmacSha256Update(
			&State, Context.Data, Context.Size
		) && xrtHmacSha256Final(&State, pMac);
	xrtSecureZero(&State, sizeof(State));
	return bResult;
}



/* 验证无状态 nonce 的结构、签名和时间窗口。 */
XRT_API xhttpdigestnoncecheck xrtHttpDigestNonceVerify(
	xstrview Nonce,
	xbytesview Key,
	xbytesview Context,
	int64 iNowSeconds,
	int64 iLifetimeSeconds,
	int64 iFutureSkewSeconds,
	int64* pIssuedSeconds
)
{
	static const xbase64config Config = {
		NULL,
		(uint32)XBASE64_URL | (uint32)XBASE64_NO_PADDING
	};
	uint8 Binary[XRT_HTTP_DIGEST_NONCE_BINARY_SIZE];
	uint8 Mac[XRT_SHA256_SIZE];
	uint64 iUnsignedIssued;
	int64 iIssued;
	size_t iSize;
	xhttpdigestnoncecheck Check = XHTTP_DIGEST_NONCE_ERROR;

	if ( !__xrtHttpViewValid(Nonce) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_DIGEST_NONCE_ERROR;
	}
	if ( !__xrtHttpDigestNonceInputValid(Key, Context, 0) ) {
		return XHTTP_DIGEST_NONCE_ERROR;
	}
	if ( (pIssuedSeconds != NULL) && (
			!__xrtRangeValid(pIssuedSeconds, sizeof(int64)) ||
			__xrtRangesOverlap(
				pIssuedSeconds, sizeof(int64), Nonce.Data, Nonce.Size
			) || __xrtRangesOverlap(
				pIssuedSeconds, sizeof(int64), Key.Data, Key.Size
			) || __xrtRangesOverlap(
				pIssuedSeconds, sizeof(int64),
				Context.Data, Context.Size
			)
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_DIGEST_NONCE_ERROR;
	}
	if ( (iNowSeconds < 0) || (iLifetimeSeconds <= 0) ||
		(iFutureSkewSeconds < 0) ) {
		__xrtErrorSetValue();
		return XHTTP_DIGEST_NONCE_ERROR;
	}
	if ( !__xrtHttpDigestNonceTextValid(Nonce) ) {
		return XHTTP_DIGEST_NONCE_INVALID;
	}
	memset(Binary, 0, sizeof(Binary));
	memset(Mac, 0, sizeof(Mac));
	if ( !xrtBase64Decode(
		Nonce.Data,
		Nonce.Size,
		Binary,
		sizeof(Binary),
		&iSize,
		&Config
	) || (iSize != sizeof(Binary)) ) {
		goto cleanup;
	}
	if ( Binary[0] != XRT_HTTP_DIGEST_NONCE_VERSION ) {
		Check = XHTTP_DIGEST_NONCE_INVALID;
		goto cleanup;
	}
	if ( !__xrtHttpDigestNonceMac(Key, Context, Binary, Mac) ) {
		goto cleanup;
	}
	if ( !xrtConstTimeEqual(
		Mac,
		Binary + XRT_HTTP_DIGEST_NONCE_BODY_SIZE,
		sizeof(Mac)
	) ) {
		Check = XHTTP_DIGEST_NONCE_INVALID;
		goto cleanup;
	}
	iUnsignedIssued = __xrtHttpDigestNonceLoad64(Binary + 1u);
	if ( iUnsignedIssued > (uint64)INT64_MAX ) {
		Check = XHTTP_DIGEST_NONCE_INVALID;
		goto cleanup;
	}
	iIssued = (int64)iUnsignedIssued;
	if ( (iIssued > iNowSeconds) &&
		((iIssued - iNowSeconds) > iFutureSkewSeconds) ) {
		Check = XHTTP_DIGEST_NONCE_INVALID;
		goto cleanup;
	}
	Check = ((iNowSeconds > iIssued) &&
		((iNowSeconds - iIssued) > iLifetimeSeconds)) ?
		XHTTP_DIGEST_NONCE_STALE : XHTTP_DIGEST_NONCE_VALID;
	if ( pIssuedSeconds != NULL ) {
		memcpy(pIssuedSeconds, &iIssued, sizeof(iIssued));
	}

cleanup:
	xrtSecureZero(Binary, sizeof(Binary));
	xrtSecureZero(Mac, sizeof(Mac));
	return Check;
}



#if defined(XRT_FEATURE_HTTP_AUTH_DIGEST_NONCE_RANDOM)

/* 使用系统安全随机 salt 构建无状态 nonce。 */
XRT_API bool xrtHttpDigestNonceCreate(
	xbytesview Key,
	xbytesview Context,
	int64 iIssuedSeconds,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	uint8 Salt[XRT_HTTP_DIGEST_NONCE_SALT_SIZE];
	char Nonce[XRT_HTTP_DIGEST_NONCE_TEXT_SIZE];
	bool bWrite;
	bool bResult = false;

	if ( !__xrtHttpDigestNonceInputValid(
		Key, Context, iIssuedSeconds
	) || !__xrtHttpDigestNonceOutputValid(
		Key, Context, NULL, pOutput, iCapacity, pSize
	) ) {
		return false;
	}
	if ( !__xrtHttpDigestNonceOutputPrepare(
		pOutput, iCapacity, pSize, &bWrite
	) ) {
		return false;
	}
	if ( !bWrite ) {
		return true;
	}
	if ( !xrtSecureRandom(Salt, sizeof(Salt)) ||
		!__xrtHttpDigestNonceEncode(
			Key, Context, iIssuedSeconds, Salt, Nonce
		) ) {
		goto cleanup;
	}
	memcpy(pOutput, Nonce, sizeof(Nonce));
	{
		size_t iRequired = sizeof(Nonce);

		memcpy(pSize, &iRequired, sizeof(iRequired));
	}
	bResult = true;

cleanup:
	xrtSecureZero(Salt, sizeof(Salt));
	xrtSecureZero(Nonce, sizeof(Nonce));
	return bResult;
}

#endif



#undef XRT_HTTP_DIGEST_NONCE_VERSION
#undef XRT_HTTP_DIGEST_NONCE_BODY_SIZE
#undef XRT_HTTP_DIGEST_NONCE_BINARY_SIZE
#undef XRT_HTTP_DIGEST_NONCE_MAX_MESSAGE

#endif
