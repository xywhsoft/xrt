#include "../internal/xrt_crypto.h"



#if defined(XRT_FEATURE_CRYPTO_SHA512)

#define XRT_SHA384_GUARD UINT32_C(0x53333834)
#define XRT_SHA512_GUARD UINT32_C(0x53353132)
#define XRT_SHA512_256_GUARD UINT32_C(0x53353236)
#define XRT_SHA512_MAX_HIGH_BYTES (UINT64_MAX >> 3u)



static const uint64 __xrtSha512Constants[80] = {
	UINT64_C(0x428A2F98D728AE22), UINT64_C(0x7137449123EF65CD),
	UINT64_C(0xB5C0FBCFEC4D3B2F), UINT64_C(0xE9B5DBA58189DBBC),
	UINT64_C(0x3956C25BF348B538), UINT64_C(0x59F111F1B605D019),
	UINT64_C(0x923F82A4AF194F9B), UINT64_C(0xAB1C5ED5DA6D8118),
	UINT64_C(0xD807AA98A3030242), UINT64_C(0x12835B0145706FBE),
	UINT64_C(0x243185BE4EE4B28C), UINT64_C(0x550C7DC3D5FFB4E2),
	UINT64_C(0x72BE5D74F27B896F), UINT64_C(0x80DEB1FE3B1696B1),
	UINT64_C(0x9BDC06A725C71235), UINT64_C(0xC19BF174CF692694),
	UINT64_C(0xE49B69C19EF14AD2), UINT64_C(0xEFBE4786384F25E3),
	UINT64_C(0x0FC19DC68B8CD5B5), UINT64_C(0x240CA1CC77AC9C65),
	UINT64_C(0x2DE92C6F592B0275), UINT64_C(0x4A7484AA6EA6E483),
	UINT64_C(0x5CB0A9DCBD41FBD4), UINT64_C(0x76F988DA831153B5),
	UINT64_C(0x983E5152EE66DFAB), UINT64_C(0xA831C66D2DB43210),
	UINT64_C(0xB00327C898FB213F), UINT64_C(0xBF597FC7BEEF0EE4),
	UINT64_C(0xC6E00BF33DA88FC2), UINT64_C(0xD5A79147930AA725),
	UINT64_C(0x06CA6351E003826F), UINT64_C(0x142929670A0E6E70),
	UINT64_C(0x27B70A8546D22FFC), UINT64_C(0x2E1B21385C26C926),
	UINT64_C(0x4D2C6DFC5AC42AED), UINT64_C(0x53380D139D95B3DF),
	UINT64_C(0x650A73548BAF63DE), UINT64_C(0x766A0ABB3C77B2A8),
	UINT64_C(0x81C2C92E47EDAEE6), UINT64_C(0x92722C851482353B),
	UINT64_C(0xA2BFE8A14CF10364), UINT64_C(0xA81A664BBC423001),
	UINT64_C(0xC24B8B70D0F89791), UINT64_C(0xC76C51A30654BE30),
	UINT64_C(0xD192E819D6EF5218), UINT64_C(0xD69906245565A910),
	UINT64_C(0xF40E35855771202A), UINT64_C(0x106AA07032BBD1B8),
	UINT64_C(0x19A4C116B8D2D0C8), UINT64_C(0x1E376C085141AB53),
	UINT64_C(0x2748774CDF8EEB99), UINT64_C(0x34B0BCB5E19B48A8),
	UINT64_C(0x391C0CB3C5C95A63), UINT64_C(0x4ED8AA4AE3418ACB),
	UINT64_C(0x5B9CCA4F7763E373), UINT64_C(0x682E6FF3D6B2B8A3),
	UINT64_C(0x748F82EE5DEFB2FC), UINT64_C(0x78A5636F43172F60),
	UINT64_C(0x84C87814A1F0AB72), UINT64_C(0x8CC702081A6439EC),
	UINT64_C(0x90BEFFFA23631E28), UINT64_C(0xA4506CEBDE82BDE9),
	UINT64_C(0xBEF9A3F7B2C67915), UINT64_C(0xC67178F2E372532B),
	UINT64_C(0xCA273ECEEA26619C), UINT64_C(0xD186B8C721C0C207),
	UINT64_C(0xEADA7DD6CDE0EB1E), UINT64_C(0xF57D4F7FEE6ED178),
	UINT64_C(0x06F067AA72176FBA), UINT64_C(0x0A637DC5A2C898A6),
	UINT64_C(0x113F9804BEF90DAE), UINT64_C(0x1B710B35131C471B),
	UINT64_C(0x28DB77F523047D84), UINT64_C(0x32CAAB7B40C72493),
	UINT64_C(0x3C9EBE0A15C9BEBC), UINT64_C(0x431D67C49C100D4C),
	UINT64_C(0x4CC5D4BECB3E42B6), UINT64_C(0x597F299CFC657E2A),
	UINT64_C(0x5FCB6FAB3AD6FAEC), UINT64_C(0x6C44198C4A475817)
};



/* 压缩一个 SHA-384/512 块，使用 16 字循环调度降低栈占用。 */
static void __xrtSha512Transform(uint64* pState, const uint8* pBlock)
{
	uint64 Words[16];
	uint64 a = pState[0];
	uint64 b = pState[1];
	uint64 c = pState[2];
	uint64 d = pState[3];
	uint64 e = pState[4];
	uint64 f = pState[5];
	uint64 g = pState[6];
	uint64 h = pState[7];

	for ( uint32 i = 0; i < 80u; i++ ) {
		uint64 iWord;
		uint64 iFirst;
		uint64 iSecond;

		if ( i < 16u ) {
			iWord = __xrtCryptoLoadBe64(pBlock + (i * 8u));
		} else {
			uint64 iLeft = Words[(i + 1u) & 15u];
			uint64 iRight = Words[(i + 14u) & 15u];
			uint64 iSmall0 = __xrtCryptoRotateRight64(iLeft, 1u) ^
				__xrtCryptoRotateRight64(iLeft, 8u) ^ (iLeft >> 7u);
			uint64 iSmall1 = __xrtCryptoRotateRight64(iRight, 19u) ^
				__xrtCryptoRotateRight64(iRight, 61u) ^ (iRight >> 6u);

			iWord = Words[i & 15u] + iSmall0 +
				Words[(i + 9u) & 15u] + iSmall1;
		}
		Words[i & 15u] = iWord;
		iFirst = h +
			(__xrtCryptoRotateRight64(e, 14u) ^
			 __xrtCryptoRotateRight64(e, 18u) ^
			 __xrtCryptoRotateRight64(e, 41u)) +
			((e & f) ^ ((~e) & g)) +
			__xrtSha512Constants[i] + iWord;
		iSecond =
			(__xrtCryptoRotateRight64(a, 28u) ^
			 __xrtCryptoRotateRight64(a, 34u) ^
			 __xrtCryptoRotateRight64(a, 39u)) +
			((a & b) ^ (a & c) ^ (b & c));
		h = g;
		g = f;
		f = e;
		e = d + iFirst;
		d = c;
		c = b;
		b = a;
		a = iFirst + iSecond;
	}
	pState[0] += a;
	pState[1] += b;
	pState[2] += c;
	pState[3] += d;
	pState[4] += e;
	pState[5] += f;
	pState[6] += g;
	pState[7] += h;
	xrtSecureZero(Words, sizeof(Words));
}



/* 验证算法标识、128 位长度范围和尾部计数的一致性。 */
static bool __xrtSha512Validate(const xsha512* pState, uint32 iGuard)
{
	if ( pState == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (pState->Guard != iGuard) ||
		 (pState->SizeHigh > XRT_SHA512_MAX_HIGH_BYTES) ||
		 (pState->BufferSize >= XRT_SHA512_BLOCK_SIZE) ||
		 (pState->BufferSize != (uint32)(pState->SizeLow & 127u)) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	return true;
}



/* 在所有失败条件确认后追加数据，并维护 128 位字节计数。 */
static bool __xrtSha512Update(
	xsha512* pState,
	const void* pData,
	size_t iSize,
	uint32 iGuard
)
{
	const uint8* pRead = (const uint8*)pData;
	size_t iRemain = iSize;
	uint64 iNextLow;
	uint64 iNextHigh;
	uint64 iCarry;

	if ( !__xrtSha512Validate(pState, iGuard) ) {
		return false;
	}
	if ( (pRead == NULL) && (iSize != 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	iNextLow = pState->SizeLow + (uint64)iSize;
	iCarry = (iNextLow < pState->SizeLow) ? 1u : 0u;
	if ( pState->SizeHigh > (XRT_SHA512_MAX_HIGH_BYTES - iCarry) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	iNextHigh = pState->SizeHigh + iCarry;
	if ( (pState->BufferSize != 0) && (iRemain != 0) ) {
		size_t iCopy = XRT_SHA512_BLOCK_SIZE - pState->BufferSize;

		if ( iCopy > iRemain ) {
			iCopy = iRemain;
		}
		memcpy(pState->Buffer + pState->BufferSize, pRead, iCopy);
		pState->BufferSize += (uint32)iCopy;
		pRead += iCopy;
		iRemain -= iCopy;
		if ( pState->BufferSize == XRT_SHA512_BLOCK_SIZE ) {
			__xrtSha512Transform(pState->State, pState->Buffer);
			pState->BufferSize = 0;
		}
	}
	while ( iRemain >= XRT_SHA512_BLOCK_SIZE ) {
		__xrtSha512Transform(pState->State, pRead);
		pRead += XRT_SHA512_BLOCK_SIZE;
		iRemain -= XRT_SHA512_BLOCK_SIZE;
	}
	if ( iRemain != 0 ) {
		memcpy(pState->Buffer, pRead, iRemain);
		pState->BufferSize = (uint32)iRemain;
	}
	pState->SizeLow = iNextLow;
	pState->SizeHigh = iNextHigh;
	return true;
}



/* 在状态副本上填充并按指定字数输出 SHA-384 或 SHA-512 摘要。 */
static bool __xrtSha512Final(
	const xsha512* pState,
	void* pDigest,
	uint32 iGuard,
	uint32 iWords
)
{
	xsha512 Copy;
	uint8 arrDigest[XRT_SHA512_SIZE];
	uint64 iBitHigh;
	uint64 iBitLow;
	uint32 iOffset;

	if ( !__xrtSha512Validate(pState, iGuard) ) {
		return false;
	}
	if ( pDigest == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	Copy = *pState;
	iOffset = Copy.BufferSize;
	Copy.Buffer[iOffset++] = 0x80u;
	if ( iOffset > 112u ) {
		memset(Copy.Buffer + iOffset, 0, XRT_SHA512_BLOCK_SIZE - iOffset);
		__xrtSha512Transform(Copy.State, Copy.Buffer);
		iOffset = 0;
	}
	memset(Copy.Buffer + iOffset, 0, 112u - iOffset);
	iBitHigh = (Copy.SizeHigh << 3u) | (Copy.SizeLow >> 61u);
	iBitLow = Copy.SizeLow << 3u;
	__xrtCryptoStoreBe64(Copy.Buffer + 112, iBitHigh);
	__xrtCryptoStoreBe64(Copy.Buffer + 120, iBitLow);
	__xrtSha512Transform(Copy.State, Copy.Buffer);
	for ( uint32 i = 0; i < iWords; i++ ) {
		__xrtCryptoStoreBe64(arrDigest + (i * 8u), Copy.State[i]);
	}
	memcpy(pDigest, arrDigest, iWords * 8u);
	xrtSecureZero(arrDigest, sizeof(arrDigest));
	xrtSecureZero(&Copy, sizeof(Copy));
	return true;
}



/* 初始化 SHA-384 的标准初始向量和算法标识。 */
XRT_API void xrtSha384Init(xsha384* pState)
{
	if ( pState == NULL ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memset(pState, 0, sizeof(*pState));
	pState->State[0] = UINT64_C(0xCBBB9D5DC1059ED8);
	pState->State[1] = UINT64_C(0x629A292A367CD507);
	pState->State[2] = UINT64_C(0x9159015A3070DD17);
	pState->State[3] = UINT64_C(0x152FECD8F70E5939);
	pState->State[4] = UINT64_C(0x67332667FFC00B31);
	pState->State[5] = UINT64_C(0x8EB44A8768581511);
	pState->State[6] = UINT64_C(0xDB0C2E0D64F98FA7);
	pState->State[7] = UINT64_C(0x47B5481DBEFA4FA4);
	pState->Guard = XRT_SHA384_GUARD;
}



/* 追加 SHA-384 数据并保持失败原子性。 */
XRT_API bool xrtSha384Update(xsha384* pState, const void* pData, size_t iSize)
{
	return __xrtSha512Update(pState, pData, iSize, XRT_SHA384_GUARD);
}



/* 从 SHA-384 状态快照输出前六个 64 位字。 */
XRT_API bool xrtSha384Final(const xsha384* pState, void* pDigest)
{
	return __xrtSha512Final(pState, pDigest, XRT_SHA384_GUARD, 6u);
}



/* 组合栈上流状态完成一次 SHA-384 计算。 */
XRT_API bool xrtSha384(const void* pData, size_t iSize, void* pDigest)
{
	xsha384 State;
	bool bResult;

	if ( pDigest == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	xrtSha384Init(&State);
	bResult = xrtSha384Update(&State, pData, iSize) &&
		xrtSha384Final(&State, pDigest);
	xrtSecureZero(&State, sizeof(State));
	return bResult;
}



/* 初始化 SHA-512 的标准初始向量和算法标识。 */
XRT_API void xrtSha512Init(xsha512* pState)
{
	if ( pState == NULL ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memset(pState, 0, sizeof(*pState));
	pState->State[0] = UINT64_C(0x6A09E667F3BCC908);
	pState->State[1] = UINT64_C(0xBB67AE8584CAA73B);
	pState->State[2] = UINT64_C(0x3C6EF372FE94F82B);
	pState->State[3] = UINT64_C(0xA54FF53A5F1D36F1);
	pState->State[4] = UINT64_C(0x510E527FADE682D1);
	pState->State[5] = UINT64_C(0x9B05688C2B3E6C1F);
	pState->State[6] = UINT64_C(0x1F83D9ABFB41BD6B);
	pState->State[7] = UINT64_C(0x5BE0CD19137E2179);
	pState->Guard = XRT_SHA512_GUARD;
}



/* 追加 SHA-512 数据并保持失败原子性。 */
XRT_API bool xrtSha512Update(xsha512* pState, const void* pData, size_t iSize)
{
	return __xrtSha512Update(pState, pData, iSize, XRT_SHA512_GUARD);
}



/* 从 SHA-512 状态快照输出全部八个 64 位字。 */
XRT_API bool xrtSha512Final(const xsha512* pState, void* pDigest)
{
	return __xrtSha512Final(pState, pDigest, XRT_SHA512_GUARD, 8u);
}



/* 组合栈上流状态完成一次 SHA-512 计算。 */
XRT_API bool xrtSha512(const void* pData, size_t iSize, void* pDigest)
{
	xsha512 State;
	bool bResult;

	if ( pDigest == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	xrtSha512Init(&State);
	bResult = xrtSha512Update(&State, pData, iSize) &&
		xrtSha512Final(&State, pDigest);
	xrtSecureZero(&State, sizeof(State));
	return bResult;
}



#if defined(XRT_FEATURE_CRYPTO_SHA512_256)

/* 初始化 FIPS 180-4 定义的 SHA-512/256 初始向量。 */
XRT_API void xrtSha512_256Init(xsha512_256* pState)
{
	if ( pState == NULL ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memset(pState, 0, sizeof(*pState));
	pState->State[0] = UINT64_C(0x22312194FC2BF72C);
	pState->State[1] = UINT64_C(0x9F555FA3C84C64C2);
	pState->State[2] = UINT64_C(0x2393B86B6F53B151);
	pState->State[3] = UINT64_C(0x963877195940EABD);
	pState->State[4] = UINT64_C(0x96283EE2A88EFFE3);
	pState->State[5] = UINT64_C(0xBE5E1E2553863992);
	pState->State[6] = UINT64_C(0x2B0199FC2C85B8AA);
	pState->State[7] = UINT64_C(0x0EB72DDC81C52CA2);
	pState->Guard = XRT_SHA512_256_GUARD;
}



/* 追加 SHA-512/256 数据并保持失败原子性。 */
XRT_API bool xrtSha512_256Update(
	xsha512_256* pState,
	const void* pData,
	size_t iSize
)
{
	return __xrtSha512Update(
		pState, pData, iSize, XRT_SHA512_256_GUARD
	);
}



/* 从 SHA-512/256 状态快照输出前四个 64 位字。 */
XRT_API bool xrtSha512_256Final(
	const xsha512_256* pState,
	void* pDigest
)
{
	return __xrtSha512Final(
		pState, pDigest, XRT_SHA512_256_GUARD, 4u
	);
}



/* 组合栈上流状态完成一次 SHA-512/256 计算。 */
XRT_API bool xrtSha512_256(
	const void* pData,
	size_t iSize,
	void* pDigest
)
{
	xsha512_256 State;
	bool bResult;

	if ( pDigest == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	xrtSha512_256Init(&State);
	bResult = xrtSha512_256Update(&State, pData, iSize) &&
		xrtSha512_256Final(&State, pDigest);
	xrtSecureZero(&State, sizeof(State));
	return bResult;
}

#endif



#undef XRT_SHA384_GUARD
#undef XRT_SHA512_GUARD
#undef XRT_SHA512_256_GUARD
#undef XRT_SHA512_MAX_HIGH_BYTES

#endif
