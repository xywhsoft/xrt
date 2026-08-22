#include "../internal/xrt_crypto.h"



#if defined(XRT_FEATURE_CRYPTO_PBKDF2_SHA256) || \
	defined(XRT_FEATURE_CRYPTO_PBKDF2_SHA512)

/* 三种 HMAC 状态的最大存储；条件成员保持精细裁剪。 */
typedef union __xrt_pbkdf2_state {
#if defined(XRT_FEATURE_CRYPTO_PBKDF2_SHA256)
	xhmacsha256 Sha256;
#endif
#if defined(XRT_FEATURE_CRYPTO_PBKDF2_SHA512)
	xhmacsha512 Sha512;
#endif
} __xrtpbkdf2state;



/* 把具体 HMAC 实现适配到唯一的 PBKDF2 派生循环。 */
typedef struct __xrt_pbkdf2_ops {
	size_t StateSize;
	size_t DigestSize;
	uint64 MaxSaltSize;
	bool (*Init)(void*, const void*, size_t);
	bool (*Update)(void*, const void*, size_t);
	bool (*Final)(const void*, void*);
} __xrtpbkdf2ops;



/* 在任何输出写入前验证公开参数、范围和 PBKDF2 块计数。 */
static bool __xrtPbkdf2Validate(
	const void* pPassword,
	size_t iPasswordSize,
	const void* pSalt,
	size_t iSaltSize,
	uint32 iIterations,
	void* pOutput,
	size_t iOutputSize,
	const __xrtpbkdf2ops* pOps
)
{
	size_t iBlocks;

	if ( !__xrtRangeValid(pPassword, iPasswordSize) ||
		 !__xrtRangeValid(pSalt, iSaltSize) ||
		 !__xrtRangeValid(pOutput, iOutputSize) ||
		 (iIterations == 0) || (iOutputSize == 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( __xrtRangesOverlap(
			pOutput, iOutputSize, pPassword, iPasswordSize
		) || __xrtRangesOverlap(
			pOutput, iOutputSize, pSalt, iSaltSize
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (uint64)iSaltSize > pOps->MaxSaltSize ) {
		__xrtErrorSetRange();
		return false;
	}
	iBlocks = ((iOutputSize - 1u) / pOps->DigestSize) + 1u;
	if ( iBlocks > UINT32_MAX ) {
		__xrtErrorSetRange();
		return false;
	}
	return true;
}



/* 复用预计算 HMAC 状态，按 RFC 8018 逐块执行 F(P, S, c, i)。 */
static bool __xrtPbkdf2(
	const void* pPassword,
	size_t iPasswordSize,
	const void* pSalt,
	size_t iSaltSize,
	uint32 iIterations,
	void* pOutput,
	size_t iOutputSize,
	const __xrtpbkdf2ops* pOps
)
{
	__xrtpbkdf2state Key;
	__xrtpbkdf2state Salted;
	__xrtpbkdf2state Round;
	uint8 arrPrevious[XRT_SHA512_SIZE];
	uint8 arrBlock[XRT_SHA512_SIZE];
	uint8 arrCounter[4];
	size_t iOffset = 0;
	uint32 iBlock = 1;
	bool bResult = false;

	memset(&Key, 0, sizeof(Key));
	memset(&Salted, 0, sizeof(Salted));
	memset(&Round, 0, sizeof(Round));
	memset(arrPrevious, 0, sizeof(arrPrevious));
	memset(arrBlock, 0, sizeof(arrBlock));
	if ( !__xrtPbkdf2Validate(
			pPassword, iPasswordSize, pSalt, iSaltSize,
			iIterations, pOutput, iOutputSize, pOps
		) || !pOps->Init(&Key, pPassword, iPasswordSize) ) {
		goto cleanup;
	}
	memcpy(&Salted, &Key, pOps->StateSize);
	if ( !pOps->Update(&Salted, pSalt, iSaltSize) ) {
		goto cleanup;
	}
	while ( iOffset < iOutputSize ) {
		size_t iCopy = iOutputSize - iOffset;

		__xrtCryptoStoreBe32(arrCounter, iBlock);
		memcpy(&Round, &Salted, pOps->StateSize);
		if ( !pOps->Update(&Round, arrCounter, sizeof(arrCounter)) ||
			 !pOps->Final(&Round, arrPrevious) ) {
			goto cleanup;
		}
		memcpy(arrBlock, arrPrevious, pOps->DigestSize);
		for ( uint32 iRound = 1; iRound < iIterations; iRound++ ) {
			memcpy(&Round, &Key, pOps->StateSize);
			if ( !pOps->Update(
					&Round, arrPrevious, pOps->DigestSize
				) || !pOps->Final(&Round, arrPrevious) ) {
				goto cleanup;
			}
			for ( size_t i = 0; i < pOps->DigestSize; i++ ) {
				arrBlock[i] ^= arrPrevious[i];
			}
		}
		if ( iCopy > pOps->DigestSize ) {
			iCopy = pOps->DigestSize;
		}
		memcpy((uint8*)pOutput + iOffset, arrBlock, iCopy);
		iOffset += iCopy;
		if ( iOffset < iOutputSize ) {
			iBlock++;
		}
	}
	bResult = true;

cleanup:
	xrtSecureZero(arrCounter, sizeof(arrCounter));
	xrtSecureZero(arrBlock, sizeof(arrBlock));
	xrtSecureZero(arrPrevious, sizeof(arrPrevious));
	xrtSecureZero(&Round, sizeof(Round));
	xrtSecureZero(&Salted, sizeof(Salted));
	xrtSecureZero(&Key, sizeof(Key));
	return bResult;
}



#if defined(XRT_FEATURE_CRYPTO_PBKDF2_SHA256)

/* 适配 HMAC-SHA256 初始化。 */
static bool __xrtPbkdf2Sha256Init(
	void* pState,
	const void* pKey,
	size_t iKeySize
)
{
	return xrtHmacSha256Init((xhmacsha256*)pState, pKey, iKeySize);
}



/* 适配 HMAC-SHA256 分块更新。 */
static bool __xrtPbkdf2Sha256Update(
	void* pState,
	const void* pData,
	size_t iSize
)
{
	return xrtHmacSha256Update((xhmacsha256*)pState, pData, iSize);
}



/* 适配 HMAC-SHA256 快照输出。 */
static bool __xrtPbkdf2Sha256Final(const void* pState, void* pMac)
{
	return xrtHmacSha256Final((const xhmacsha256*)pState, pMac);
}



/* 使用预计算 HMAC-SHA256 状态完成 PBKDF2 派生。 */
XRT_API bool xrtPbkdf2Sha256(
	const void* pPassword,
	size_t iPasswordSize,
	const void* pSalt,
	size_t iSaltSize,
	uint32 iIterations,
	void* pOutput,
	size_t iOutputSize
)
{
	static const __xrtpbkdf2ops Ops = {
		sizeof(xhmacsha256),
		XRT_SHA256_SIZE,
		(UINT64_MAX >> 3u) - XRT_SHA256_BLOCK_SIZE - 4u,
		__xrtPbkdf2Sha256Init,
		__xrtPbkdf2Sha256Update,
		__xrtPbkdf2Sha256Final
	};

	return __xrtPbkdf2(
		pPassword, iPasswordSize, pSalt, iSaltSize,
		iIterations, pOutput, iOutputSize, &Ops
	);
}

#endif



#if defined(XRT_FEATURE_CRYPTO_PBKDF2_SHA512)

/* 适配 HMAC-SHA384 初始化。 */
static bool __xrtPbkdf2Sha384Init(
	void* pState,
	const void* pKey,
	size_t iKeySize
)
{
	return xrtHmacSha384Init((xhmacsha384*)pState, pKey, iKeySize);
}



/* 适配 HMAC-SHA384 分块更新。 */
static bool __xrtPbkdf2Sha384Update(
	void* pState,
	const void* pData,
	size_t iSize
)
{
	return xrtHmacSha384Update((xhmacsha384*)pState, pData, iSize);
}



/* 适配 HMAC-SHA384 快照输出。 */
static bool __xrtPbkdf2Sha384Final(const void* pState, void* pMac)
{
	return xrtHmacSha384Final((const xhmacsha384*)pState, pMac);
}



/* 适配 HMAC-SHA512 初始化。 */
static bool __xrtPbkdf2Sha512Init(
	void* pState,
	const void* pKey,
	size_t iKeySize
)
{
	return xrtHmacSha512Init((xhmacsha512*)pState, pKey, iKeySize);
}



/* 适配 HMAC-SHA512 分块更新。 */
static bool __xrtPbkdf2Sha512Update(
	void* pState,
	const void* pData,
	size_t iSize
)
{
	return xrtHmacSha512Update((xhmacsha512*)pState, pData, iSize);
}



/* 适配 HMAC-SHA512 快照输出。 */
static bool __xrtPbkdf2Sha512Final(const void* pState, void* pMac)
{
	return xrtHmacSha512Final((const xhmacsha512*)pState, pMac);
}



/* 使用预计算 HMAC-SHA384 状态完成 PBKDF2 派生。 */
XRT_API bool xrtPbkdf2Sha384(
	const void* pPassword,
	size_t iPasswordSize,
	const void* pSalt,
	size_t iSaltSize,
	uint32 iIterations,
	void* pOutput,
	size_t iOutputSize
)
{
	static const __xrtpbkdf2ops Ops = {
		sizeof(xhmacsha384),
		XRT_SHA384_SIZE,
		UINT64_MAX,
		__xrtPbkdf2Sha384Init,
		__xrtPbkdf2Sha384Update,
		__xrtPbkdf2Sha384Final
	};

	return __xrtPbkdf2(
		pPassword, iPasswordSize, pSalt, iSaltSize,
		iIterations, pOutput, iOutputSize, &Ops
	);
}



/* 使用预计算 HMAC-SHA512 状态完成 PBKDF2 派生。 */
XRT_API bool xrtPbkdf2Sha512(
	const void* pPassword,
	size_t iPasswordSize,
	const void* pSalt,
	size_t iSaltSize,
	uint32 iIterations,
	void* pOutput,
	size_t iOutputSize
)
{
	static const __xrtpbkdf2ops Ops = {
		sizeof(xhmacsha512),
		XRT_SHA512_SIZE,
		UINT64_MAX,
		__xrtPbkdf2Sha512Init,
		__xrtPbkdf2Sha512Update,
		__xrtPbkdf2Sha512Final
	};

	return __xrtPbkdf2(
		pPassword, iPasswordSize, pSalt, iSaltSize,
		iIterations, pOutput, iOutputSize, &Ops
	);
}

#endif

#endif
