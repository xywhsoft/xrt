#include "../internal/xrt_crypto.h"



#if defined(XRT_FEATURE_CRYPTO_HMAC_SHA256)

#define XRT_HMAC_SHA256_GUARD UINT32_C(0x484D3236)



/* 验证 HMAC-SHA256 状态的算法标识。 */
static bool __xrtHmacSha256Validate(const xhmacsha256* pState)
{
	if ( pState == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pState->Guard != XRT_HMAC_SHA256_GUARD ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	return true;
}



/* 预计算 HMAC 的 inner/outer 密钥块，并在成功后一次提交状态。 */
XRT_API bool xrtHmacSha256Init(
	xhmacsha256* pState,
	const void* pKey,
	size_t iKeySize
)
{
	xhmacsha256 Next;
	uint8 arrKey[XRT_SHA256_BLOCK_SIZE];
	uint8 arrInner[XRT_SHA256_BLOCK_SIZE];
	uint8 arrOuter[XRT_SHA256_BLOCK_SIZE];
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
		if ( !xrtSha256(pKey, iKeySize, arrKey) ) {
			goto cleanup;
		}
	} else if ( iKeySize != 0 ) {
		memcpy(arrKey, pKey, iKeySize);
	}
	for ( size_t i = 0; i < sizeof(arrKey); i++ ) {
		arrInner[i] = arrKey[i] ^ 0x36u;
		arrOuter[i] = arrKey[i] ^ 0x5Cu;
	}
	xrtSha256Init(&Next.Inner);
	xrtSha256Init(&Next.Outer);
	if ( !xrtSha256Update(&Next.Inner, arrInner, sizeof(arrInner)) ||
		 !xrtSha256Update(&Next.Outer, arrOuter, sizeof(arrOuter)) ) {
		goto cleanup;
	}
	Next.Guard = XRT_HMAC_SHA256_GUARD;
	*pState = Next;
	bResult = true;

cleanup:
	xrtSecureZero(arrOuter, sizeof(arrOuter));
	xrtSecureZero(arrInner, sizeof(arrInner));
	xrtSecureZero(arrKey, sizeof(arrKey));
	xrtSecureZero(&Next, sizeof(Next));
	return bResult;
}



/* 在验证算法标识后把数据交给 inner 摘要状态。 */
XRT_API bool xrtHmacSha256Update(
	xhmacsha256* pState,
	const void* pData,
	size_t iSize
)
{
	if ( !__xrtHmacSha256Validate(pState) ) {
		return false;
	}
	return xrtSha256Update(&pState->Inner, pData, iSize);
}



/* 从 inner/outer 快照完成 HMAC，原状态保持可继续追加。 */
XRT_API bool xrtHmacSha256Final(const xhmacsha256* pState, void* pMac)
{
	xsha256 Outer;
	uint8 arrInner[XRT_SHA256_SIZE];
	uint8 arrMac[XRT_SHA256_SIZE];
	bool bResult = false;

	if ( !__xrtHmacSha256Validate(pState) ) {
		return false;
	}
	if ( pMac == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	Outer = pState->Outer;
	if ( xrtSha256Final(&pState->Inner, arrInner) &&
		 xrtSha256Update(&Outer, arrInner, sizeof(arrInner)) &&
		 xrtSha256Final(&Outer, arrMac) ) {
		memcpy(pMac, arrMac, sizeof(arrMac));
		bResult = true;
	}
	xrtSecureZero(arrMac, sizeof(arrMac));
	xrtSecureZero(arrInner, sizeof(arrInner));
	xrtSecureZero(&Outer, sizeof(Outer));
	return bResult;
}



/* 组合栈上 HMAC 状态完成一次连续数据认证。 */
XRT_API bool xrtHmacSha256(
	const void* pKey,
	size_t iKeySize,
	const void* pData,
	size_t iSize,
	void* pMac
)
{
	xhmacsha256 State;
	bool bResult;

	if ( pMac == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	bResult = xrtHmacSha256Init(&State, pKey, iKeySize) &&
		xrtHmacSha256Update(&State, pData, iSize) &&
		xrtHmacSha256Final(&State, pMac);
	xrtSecureZero(&State, sizeof(State));
	return bResult;
}



#undef XRT_HMAC_SHA256_GUARD

#endif
