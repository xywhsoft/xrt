#include "../internal/xrt_crypto.h"



#if defined(XRT_FEATURE_CRYPTO_HMAC_SHA512)

#define XRT_HMAC_SHA384_GUARD UINT32_C(0x484D3338)
#define XRT_HMAC_SHA512_GUARD UINT32_C(0x484D3531)



/* 验证 HMAC-SHA384/512 状态的具体算法标识。 */
static bool __xrtHmacSha512Validate(const xhmacsha512* pState, uint32 iGuard)
{
	if ( pState == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pState->Guard != iGuard ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	return true;
}



/* 按具体算法初始化或更新共享的 SHA-384/512 摘要状态。 */
static void __xrtHmacSha512InitHash(xsha512* pState, uint32 iGuard)
{
	if ( iGuard == XRT_HMAC_SHA384_GUARD ) {
		xrtSha384Init(pState);
	} else {
		xrtSha512Init(pState);
	}
}



/* 按具体算法追加共享的 SHA-384/512 摘要状态。 */
static bool __xrtHmacSha512UpdateHash(
	xsha512* pState,
	const void* pData,
	size_t iSize,
	uint32 iGuard
)
{
	if ( iGuard == XRT_HMAC_SHA384_GUARD ) {
		return xrtSha384Update(pState, pData, iSize);
	}
	return xrtSha512Update(pState, pData, iSize);
}



/* 按具体算法从共享的 SHA-384/512 状态输出摘要。 */
static bool __xrtHmacSha512FinalHash(
	const xsha512* pState,
	void* pDigest,
	uint32 iGuard
)
{
	if ( iGuard == XRT_HMAC_SHA384_GUARD ) {
		return xrtSha384Final(pState, pDigest);
	}
	return xrtSha512Final(pState, pDigest);
}



/* 按具体算法计算超过块长的 HMAC 密钥摘要。 */
static bool __xrtHmacSha512HashKey(
	const void* pKey,
	size_t iKeySize,
	uint8* pDigest,
	uint32 iGuard
)
{
	if ( iGuard == XRT_HMAC_SHA384_GUARD ) {
		return xrtSha384(pKey, iKeySize, pDigest);
	}
	return xrtSha512(pKey, iKeySize, pDigest);
}



/* 预计算 HMAC-SHA384/512 的 inner/outer 密钥块并一次提交状态。 */
static bool __xrtHmacSha512Init(
	xhmacsha512* pState,
	const void* pKey,
	size_t iKeySize,
	uint32 iGuard
)
{
	xhmacsha512 Next;
	uint8 arrKey[XRT_SHA512_BLOCK_SIZE];
	uint8 arrInner[XRT_SHA512_BLOCK_SIZE];
	uint8 arrOuter[XRT_SHA512_BLOCK_SIZE];
	bool bResult = false;

	if ( pState == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (pKey == NULL) && (iKeySize != 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(&Next, 0, sizeof(Next));
	memset(arrKey, 0, sizeof(arrKey));
	if ( iKeySize > sizeof(arrKey) ) {
		if ( !__xrtHmacSha512HashKey(pKey, iKeySize, arrKey, iGuard) ) {
			goto cleanup;
		}
	} else if ( iKeySize != 0 ) {
		memcpy(arrKey, pKey, iKeySize);
	}
	for ( size_t i = 0; i < sizeof(arrKey); i++ ) {
		arrInner[i] = arrKey[i] ^ 0x36u;
		arrOuter[i] = arrKey[i] ^ 0x5Cu;
	}
	__xrtHmacSha512InitHash(&Next.Inner, iGuard);
	__xrtHmacSha512InitHash(&Next.Outer, iGuard);
	if ( !__xrtHmacSha512UpdateHash(
			&Next.Inner, arrInner, sizeof(arrInner), iGuard
		) || !__xrtHmacSha512UpdateHash(
			&Next.Outer, arrOuter, sizeof(arrOuter), iGuard
		) ) {
		goto cleanup;
	}
	Next.Guard = iGuard;
	*pState = Next;
	bResult = true;

cleanup:
	xrtSecureZero(arrOuter, sizeof(arrOuter));
	xrtSecureZero(arrInner, sizeof(arrInner));
	xrtSecureZero(arrKey, sizeof(arrKey));
	xrtSecureZero(&Next, sizeof(Next));
	return bResult;
}



/* 向具体算法的 inner 状态追加 HMAC 数据。 */
static bool __xrtHmacSha512Update(
	xhmacsha512* pState,
	const void* pData,
	size_t iSize,
	uint32 iGuard
)
{
	if ( !__xrtHmacSha512Validate(pState, iGuard) ) {
		return false;
	}
	return __xrtHmacSha512UpdateHash(&pState->Inner, pData, iSize, iGuard);
}



/* 从 inner/outer 快照完成具体算法的 HMAC。 */
static bool __xrtHmacSha512Final(
	const xhmacsha512* pState,
	void* pMac,
	uint32 iGuard,
	size_t iDigestSize
)
{
	xsha512 Outer;
	uint8 arrInner[XRT_SHA512_SIZE];
	uint8 arrMac[XRT_SHA512_SIZE];
	bool bResult = false;

	if ( !__xrtHmacSha512Validate(pState, iGuard) ) {
		return false;
	}
	if ( pMac == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	Outer = pState->Outer;
	if ( __xrtHmacSha512FinalHash(&pState->Inner, arrInner, iGuard) &&
		 __xrtHmacSha512UpdateHash(&Outer, arrInner, iDigestSize, iGuard) &&
		 __xrtHmacSha512FinalHash(&Outer, arrMac, iGuard) ) {
		memcpy(pMac, arrMac, iDigestSize);
		bResult = true;
	}
	xrtSecureZero(arrMac, sizeof(arrMac));
	xrtSecureZero(arrInner, sizeof(arrInner));
	xrtSecureZero(&Outer, sizeof(Outer));
	return bResult;
}



/* 组合栈上状态完成一次具体 SHA-384/512 HMAC。 */
static bool __xrtHmacSha512(
	const void* pKey,
	size_t iKeySize,
	const void* pData,
	size_t iSize,
	void* pMac,
	uint32 iGuard,
	size_t iDigestSize
)
{
	xhmacsha512 State;
	bool bResult;

	if ( pMac == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	bResult = __xrtHmacSha512Init(&State, pKey, iKeySize, iGuard) &&
		__xrtHmacSha512Update(&State, pData, iSize, iGuard) &&
		__xrtHmacSha512Final(&State, pMac, iGuard, iDigestSize);
	xrtSecureZero(&State, sizeof(State));
	return bResult;
}



/* 初始化 HMAC-SHA384 状态。 */
XRT_API bool xrtHmacSha384Init(
	xhmacsha384* pState,
	const void* pKey,
	size_t iKeySize
)
{
	return __xrtHmacSha512Init(
		pState, pKey, iKeySize, XRT_HMAC_SHA384_GUARD
	);
}



/* 向 HMAC-SHA384 状态追加数据。 */
XRT_API bool xrtHmacSha384Update(
	xhmacsha384* pState,
	const void* pData,
	size_t iSize
)
{
	return __xrtHmacSha512Update(
		pState, pData, iSize, XRT_HMAC_SHA384_GUARD
	);
}



/* 从 HMAC-SHA384 状态快照输出认证码。 */
XRT_API bool xrtHmacSha384Final(const xhmacsha384* pState, void* pMac)
{
	return __xrtHmacSha512Final(
		pState, pMac, XRT_HMAC_SHA384_GUARD, XRT_SHA384_SIZE
	);
}



/* 一次计算 HMAC-SHA384。 */
XRT_API bool xrtHmacSha384(
	const void* pKey,
	size_t iKeySize,
	const void* pData,
	size_t iSize,
	void* pMac
)
{
	return __xrtHmacSha512(
		pKey, iKeySize, pData, iSize, pMac,
		XRT_HMAC_SHA384_GUARD, XRT_SHA384_SIZE
	);
}



/* 初始化 HMAC-SHA512 状态。 */
XRT_API bool xrtHmacSha512Init(
	xhmacsha512* pState,
	const void* pKey,
	size_t iKeySize
)
{
	return __xrtHmacSha512Init(
		pState, pKey, iKeySize, XRT_HMAC_SHA512_GUARD
	);
}



/* 向 HMAC-SHA512 状态追加数据。 */
XRT_API bool xrtHmacSha512Update(
	xhmacsha512* pState,
	const void* pData,
	size_t iSize
)
{
	return __xrtHmacSha512Update(
		pState, pData, iSize, XRT_HMAC_SHA512_GUARD
	);
}



/* 从 HMAC-SHA512 状态快照输出认证码。 */
XRT_API bool xrtHmacSha512Final(const xhmacsha512* pState, void* pMac)
{
	return __xrtHmacSha512Final(
		pState, pMac, XRT_HMAC_SHA512_GUARD, XRT_SHA512_SIZE
	);
}



/* 一次计算 HMAC-SHA512。 */
XRT_API bool xrtHmacSha512(
	const void* pKey,
	size_t iKeySize,
	const void* pData,
	size_t iSize,
	void* pMac
)
{
	return __xrtHmacSha512(
		pKey, iKeySize, pData, iSize, pMac,
		XRT_HMAC_SHA512_GUARD, XRT_SHA512_SIZE
	);
}



#undef XRT_HMAC_SHA384_GUARD
#undef XRT_HMAC_SHA512_GUARD

#endif
