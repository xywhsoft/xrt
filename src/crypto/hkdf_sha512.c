#include "../internal/xrt_crypto.h"



#if defined(XRT_FEATURE_CRYPTO_HKDF_SHA512)

#define XRT_HKDF_SHA384_MAX_SIZE (255u * XRT_SHA384_SIZE)
#define XRT_HKDF_SHA512_MAX_SIZE (255u * XRT_SHA512_SIZE)



/* 按具体摘要家族计算 HMAC，用于 HKDF Extract。 */
static bool __xrtHkdfSha512Hmac(
	bool bSha384,
	const void* pKey,
	size_t iKeySize,
	const void* pData,
	size_t iDataSize,
	void* pMac
)
{
	if ( bSha384 ) {
		return xrtHmacSha384(
			pKey, iKeySize, pData, iDataSize, pMac
		);
	}
	return xrtHmacSha512(pKey, iKeySize, pData, iDataSize, pMac);
}



/* 按具体摘要家族初始化共享 HMAC 状态。 */
static bool __xrtHkdfSha512HmacInit(
	bool bSha384,
	xhmacsha512* pState,
	const void* pKey,
	size_t iKeySize
)
{
	if ( bSha384 ) {
		return xrtHmacSha384Init(pState, pKey, iKeySize);
	}
	return xrtHmacSha512Init(pState, pKey, iKeySize);
}



/* 按具体摘要家族追加共享 HMAC 状态。 */
static bool __xrtHkdfSha512HmacUpdate(
	bool bSha384,
	xhmacsha512* pState,
	const void* pData,
	size_t iDataSize
)
{
	if ( bSha384 ) {
		return xrtHmacSha384Update(pState, pData, iDataSize);
	}
	return xrtHmacSha512Update(pState, pData, iDataSize);
}



/* 按具体摘要家族完成共享 HMAC 状态。 */
static bool __xrtHkdfSha512HmacFinal(
	bool bSha384,
	const xhmacsha512* pState,
	void* pMac
)
{
	if ( bSha384 ) {
		return xrtHmacSha384Final(pState, pMac);
	}
	return xrtHmacSha512Final(pState, pMac);
}



/* 使用空 salt 规则为 SHA-384 或 SHA-512 提取 PRK。 */
static bool __xrtHkdfSha512Extract(
	bool bSha384,
	const void* pSalt,
	size_t iSaltSize,
	const void* pIkm,
	size_t iIkmSize,
	void* pPrk,
	size_t iDigestSize
)
{
	uint8 arrSalt[XRT_SHA512_SIZE];
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
		memset(arrSalt, 0, iDigestSize);
		pUseSalt = arrSalt;
		iUseSaltSize = iDigestSize;
	}
	bResult = __xrtHkdfSha512Hmac(
		bSha384, pUseSalt, iUseSaltSize, pIkm, iIkmSize, pPrk
	);
	xrtSecureZero(arrSalt, sizeof(arrSalt));
	return bResult;
}



/* 复用具体 SHA-384/512 的预计算 PRK 状态逐块生成 OKM。 */
static bool __xrtHkdfSha512Expand(
	bool bSha384,
	const void* pPrk,
	size_t iPrkSize,
	const void* pInfo,
	size_t iInfoSize,
	void* pOkm,
	size_t iOkmSize,
	size_t iDigestSize,
	size_t iMaxSize
)
{
	xhmacsha512 Base;
	xhmacsha512 State;
	uint8 arrBlock[XRT_SHA512_SIZE];
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
	if ( iOkmSize > iMaxSize ) {
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
	if ( !__xrtHkdfSha512HmacInit(bSha384, &Base, pPrk, iPrkSize) ) {
		goto cleanup;
	}
	while ( iOffset < iOkmSize ) {
		size_t iCopy;

		State = Base;
		if ( !__xrtHkdfSha512HmacUpdate(
				bSha384, &State, arrBlock, iBlockSize
			) || !__xrtHkdfSha512HmacUpdate(
				bSha384, &State, pInfo, iInfoSize
			) || !__xrtHkdfSha512HmacUpdate(
				bSha384, &State, &iCounter, 1
			) || !__xrtHkdfSha512HmacFinal(
				bSha384, &State, arrBlock
			) ) {
			goto cleanup;
		}
		iBlockSize = iDigestSize;
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



/* 初始化或重置 SHA-384 HKDF 的提取阶段。 */
XRT_API bool xrtHkdfSha384Extract(
	const void* pSalt,
	size_t iSaltSize,
	const void* pIkm,
	size_t iIkmSize,
	void* pPrk
)
{
	return __xrtHkdfSha512Extract(
		true, pSalt, iSaltSize, pIkm, iIkmSize, pPrk, XRT_SHA384_SIZE
	);
}



/* 展开 SHA-384 PRK。 */
XRT_API bool xrtHkdfSha384Expand(
	const void* pPrk,
	size_t iPrkSize,
	const void* pInfo,
	size_t iInfoSize,
	void* pOkm,
	size_t iOkmSize
)
{
	return __xrtHkdfSha512Expand(
		true, pPrk, iPrkSize, pInfo, iInfoSize, pOkm, iOkmSize,
		XRT_SHA384_SIZE, XRT_HKDF_SHA384_MAX_SIZE
	);
}



/* 组合 SHA-384 HKDF 的提取和展开阶段。 */
XRT_API bool xrtHkdfSha384(
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
	uint8 arrPrk[XRT_SHA384_SIZE];
	bool bResult;

	bResult = xrtHkdfSha384Extract(
		pSalt, iSaltSize, pIkm, iIkmSize, arrPrk
	) && xrtHkdfSha384Expand(
		arrPrk, sizeof(arrPrk), pInfo, iInfoSize, pOkm, iOkmSize
	);
	xrtSecureZero(arrPrk, sizeof(arrPrk));
	return bResult;
}



/* 初始化或重置 SHA-512 HKDF 的提取阶段。 */
XRT_API bool xrtHkdfSha512Extract(
	const void* pSalt,
	size_t iSaltSize,
	const void* pIkm,
	size_t iIkmSize,
	void* pPrk
)
{
	return __xrtHkdfSha512Extract(
		false, pSalt, iSaltSize, pIkm, iIkmSize, pPrk, XRT_SHA512_SIZE
	);
}



/* 展开 SHA-512 PRK。 */
XRT_API bool xrtHkdfSha512Expand(
	const void* pPrk,
	size_t iPrkSize,
	const void* pInfo,
	size_t iInfoSize,
	void* pOkm,
	size_t iOkmSize
)
{
	return __xrtHkdfSha512Expand(
		false, pPrk, iPrkSize, pInfo, iInfoSize, pOkm, iOkmSize,
		XRT_SHA512_SIZE, XRT_HKDF_SHA512_MAX_SIZE
	);
}



/* 组合 SHA-512 HKDF 的提取和展开阶段。 */
XRT_API bool xrtHkdfSha512(
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
	uint8 arrPrk[XRT_SHA512_SIZE];
	bool bResult;

	bResult = xrtHkdfSha512Extract(
		pSalt, iSaltSize, pIkm, iIkmSize, arrPrk
	) && xrtHkdfSha512Expand(
		arrPrk, sizeof(arrPrk), pInfo, iInfoSize, pOkm, iOkmSize
	);
	xrtSecureZero(arrPrk, sizeof(arrPrk));
	return bResult;
}



#undef XRT_HKDF_SHA384_MAX_SIZE
#undef XRT_HKDF_SHA512_MAX_SIZE

#endif
