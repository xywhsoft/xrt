#include "../internal/xrt_crypto.h"



#if defined(XRT_FEATURE_CRYPTO_HKDF_SHA256)

#define XRT_HKDF_SHA256_MAX_SIZE (255u * XRT_SHA256_SIZE)



/* 使用 RFC 5869 的空 salt 规则提取固定长度 PRK。 */
XRT_API bool xrtHkdfSha256Extract(
	const void* pSalt,
	size_t iSaltSize,
	const void* pIkm,
	size_t iIkmSize,
	void* pPrk
)
{
	uint8 arrSalt[XRT_SHA256_SIZE];
	const void* pUseSalt = pSalt;
	size_t iUseSaltSize = iSaltSize;
	bool bResult;

	if ( pPrk == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( ((pSalt == NULL) && (iSaltSize != 0)) ||
		 ((pIkm == NULL) && (iIkmSize != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( iSaltSize == 0 ) {
		memset(arrSalt, 0, sizeof(arrSalt));
		pUseSalt = arrSalt;
		iUseSaltSize = sizeof(arrSalt);
	}
	bResult = xrtHmacSha256(
		pUseSalt, iUseSaltSize, pIkm, iIkmSize, pPrk
	);
	xrtSecureZero(arrSalt, sizeof(arrSalt));
	return bResult;
}



/* 复用预计算 PRK 状态，逐块生成 RFC 5869 OKM。 */
XRT_API bool xrtHkdfSha256Expand(
	const void* pPrk,
	size_t iPrkSize,
	const void* pInfo,
	size_t iInfoSize,
	void* pOkm,
	size_t iOkmSize
)
{
	xhmacsha256 Base;
	xhmacsha256 State;
	uint8 arrBlock[XRT_SHA256_SIZE];
	uint8 iCounter = 1;
	size_t iBlockSize = 0;
	size_t iOffset = 0;
	bool bResult = false;

	if ( ((pPrk == NULL) && (iPrkSize != 0)) ||
		 ((pInfo == NULL) && (iInfoSize != 0)) ||
		 ((pOkm == NULL) && (iOkmSize != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( iOkmSize > XRT_HKDF_SHA256_MAX_SIZE ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	if ( __xrtCryptoRangesOverlap(pOkm, iOkmSize, pInfo, iInfoSize) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( iOkmSize == 0 ) {
		return true;
	}
	if ( !xrtHmacSha256Init(&Base, pPrk, iPrkSize) ) {
		goto cleanup;
	}
	while ( iOffset < iOkmSize ) {
		size_t iCopy;

		State = Base;
		if ( !xrtHmacSha256Update(&State, arrBlock, iBlockSize) ||
			 !xrtHmacSha256Update(&State, pInfo, iInfoSize) ||
			 !xrtHmacSha256Update(&State, &iCounter, 1) ||
			 !xrtHmacSha256Final(&State, arrBlock) ) {
			goto cleanup;
		}
		iBlockSize = sizeof(arrBlock);
		iCopy = iOkmSize - iOffset;
		if ( iCopy > iBlockSize ) {
			iCopy = iBlockSize;
		}
		memcpy((uint8*)pOkm + iOffset, arrBlock, iCopy);
		iOffset += iCopy;
		iCounter++;
	}
	bResult = true;

cleanup:
	xrtSecureZero(arrBlock, sizeof(arrBlock));
	xrtSecureZero(&State, sizeof(State));
	xrtSecureZero(&Base, sizeof(Base));
	return bResult;
}



/* 在栈上保存 PRK，组合 Extract 与 Expand 完成一次派生。 */
XRT_API bool xrtHkdfSha256(
	const void* pSalt,
	size_t iSaltSize,
	const void* pIkm,
	size_t iIkmSize,
	const void* pInfo,
	size_t iInfoSize,
	void* pOkm,
	size_t iOkmSize
)
{
	uint8 arrPrk[XRT_SHA256_SIZE];
	bool bResult;

	bResult = xrtHkdfSha256Extract(
		pSalt, iSaltSize, pIkm, iIkmSize, arrPrk
	) && xrtHkdfSha256Expand(
		arrPrk, sizeof(arrPrk), pInfo, iInfoSize, pOkm, iOkmSize
	);
	xrtSecureZero(arrPrk, sizeof(arrPrk));
	return bResult;
}



#undef XRT_HKDF_SHA256_MAX_SIZE

#endif
