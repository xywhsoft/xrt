/*
	SHA-224/256 压缩流程源自 Brad Conte 的 Public Domain 实现。
	当前版本已重构状态校验、长度边界、失败原子性和统一 XRT API。
*/

#include "../internal/xrt_crypto.h"



#if defined(XRT_FEATURE_CRYPTO_SHA256)

#define XRT_SHA256_GUARD UINT32_C(0x53483236)
#define XRT_SHA224_GUARD UINT32_C(0x53483234)
#define XRT_SHA256_MAX_BYTES (UINT64_MAX >> 3u)



static const uint32 __xrtSha256Constants[64] = {
	UINT32_C(0x428A2F98), UINT32_C(0x71374491),
	UINT32_C(0xB5C0FBCF), UINT32_C(0xE9B5DBA5),
	UINT32_C(0x3956C25B), UINT32_C(0x59F111F1),
	UINT32_C(0x923F82A4), UINT32_C(0xAB1C5ED5),
	UINT32_C(0xD807AA98), UINT32_C(0x12835B01),
	UINT32_C(0x243185BE), UINT32_C(0x550C7DC3),
	UINT32_C(0x72BE5D74), UINT32_C(0x80DEB1FE),
	UINT32_C(0x9BDC06A7), UINT32_C(0xC19BF174),
	UINT32_C(0xE49B69C1), UINT32_C(0xEFBE4786),
	UINT32_C(0x0FC19DC6), UINT32_C(0x240CA1CC),
	UINT32_C(0x2DE92C6F), UINT32_C(0x4A7484AA),
	UINT32_C(0x5CB0A9DC), UINT32_C(0x76F988DA),
	UINT32_C(0x983E5152), UINT32_C(0xA831C66D),
	UINT32_C(0xB00327C8), UINT32_C(0xBF597FC7),
	UINT32_C(0xC6E00BF3), UINT32_C(0xD5A79147),
	UINT32_C(0x06CA6351), UINT32_C(0x14292967),
	UINT32_C(0x27B70A85), UINT32_C(0x2E1B2138),
	UINT32_C(0x4D2C6DFC), UINT32_C(0x53380D13),
	UINT32_C(0x650A7354), UINT32_C(0x766A0ABB),
	UINT32_C(0x81C2C92E), UINT32_C(0x92722C85),
	UINT32_C(0xA2BFE8A1), UINT32_C(0xA81A664B),
	UINT32_C(0xC24B8B70), UINT32_C(0xC76C51A3),
	UINT32_C(0xD192E819), UINT32_C(0xD6990624),
	UINT32_C(0xF40E3585), UINT32_C(0x106AA070),
	UINT32_C(0x19A4C116), UINT32_C(0x1E376C08),
	UINT32_C(0x2748774C), UINT32_C(0x34B0BCB5),
	UINT32_C(0x391C0CB3), UINT32_C(0x4ED8AA4A),
	UINT32_C(0x5B9CCA4F), UINT32_C(0x682E6FF3),
	UINT32_C(0x748F82EE), UINT32_C(0x78A5636F),
	UINT32_C(0x84C87814), UINT32_C(0x8CC70208),
	UINT32_C(0x90BEFFFA), UINT32_C(0xA4506CEB),
	UINT32_C(0xBEF9A3F7), UINT32_C(0xC67178F2)
};



/* 压缩一个完整 SHA-256 块，使用 16 字循环调度降低栈占用。 */
static void __xrtSha256Transform(uint32* pState, const uint8* pBlock)
{
	uint32 Words[16];
	uint32 a = pState[0];
	uint32 b = pState[1];
	uint32 c = pState[2];
	uint32 d = pState[3];
	uint32 e = pState[4];
	uint32 f = pState[5];
	uint32 g = pState[6];
	uint32 h = pState[7];

	for ( uint32 i = 0; i < 64u; i++ ) {
		uint32 iWord;
		uint32 iFirst;
		uint32 iSecond;

		if ( i < 16u ) {
			iWord = __xrtCryptoLoadBe32(pBlock + (i * 4u));
		} else {
			uint32 iLeft = Words[(i + 1u) & 15u];
			uint32 iRight = Words[(i + 14u) & 15u];
			uint32 iSmall0 = __xrtCryptoRotateRight32(iLeft, 7u) ^
				__xrtCryptoRotateRight32(iLeft, 18u) ^ (iLeft >> 3u);
			uint32 iSmall1 = __xrtCryptoRotateRight32(iRight, 17u) ^
				__xrtCryptoRotateRight32(iRight, 19u) ^ (iRight >> 10u);

			iWord = Words[i & 15u] + iSmall0 +
				Words[(i + 9u) & 15u] + iSmall1;
		}
		Words[i & 15u] = iWord;
		iFirst = h +
			(__xrtCryptoRotateRight32(e, 6u) ^
			 __xrtCryptoRotateRight32(e, 11u) ^
			 __xrtCryptoRotateRight32(e, 25u)) +
			((e & f) ^ ((~e) & g)) +
			__xrtSha256Constants[i] + iWord;
		iSecond =
			(__xrtCryptoRotateRight32(a, 2u) ^
			 __xrtCryptoRotateRight32(a, 13u) ^
			 __xrtCryptoRotateRight32(a, 22u)) +
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



/* 验证公开状态字段仍满足初始化标记、尾部长度和消息上限。 */
static bool __xrtSha256Validate(const xsha256* pState, uint32 iGuard)
{
	if ( pState == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (pState->Guard != iGuard) ||
		 (pState->Size > XRT_SHA256_MAX_BYTES) ||
		 (pState->BufferSize >= XRT_SHA256_BLOCK_SIZE) ||
		 (pState->BufferSize != (uint32)(pState->Size & 63u)) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	return true;
}



/* 初始化 SHA-256 标准初始向量和空消息状态。 */
XRT_API void xrtSha256Init(xsha256* pState)
{
	if ( pState == NULL ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memset(pState, 0, sizeof(*pState));
	pState->State[0] = UINT32_C(0x6A09E667);
	pState->State[1] = UINT32_C(0xBB67AE85);
	pState->State[2] = UINT32_C(0x3C6EF372);
	pState->State[3] = UINT32_C(0xA54FF53A);
	pState->State[4] = UINT32_C(0x510E527F);
	pState->State[5] = UINT32_C(0x9B05688C);
	pState->State[6] = UINT32_C(0x1F83D9AB);
	pState->State[7] = UINT32_C(0x5BE0CD19);
	pState->Guard = XRT_SHA256_GUARD;
}



/* 先验证全部失败条件，再按尾部、完整块、剩余尾部顺序追加数据。 */
static bool __xrtSha256Update(
	xsha256* pState,
	const void* pData,
	size_t iSize,
	uint32 iGuard
)
{
	const uint8* pRead = (const uint8*)pData;
	size_t iRemain = iSize;
	uint64 iNextSize;

	if ( !__xrtSha256Validate(pState, iGuard) ) {
		return false;
	}
	if ( (pRead == NULL) && (iSize != 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (uint64)iSize > (XRT_SHA256_MAX_BYTES - pState->Size) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	iNextSize = pState->Size + (uint64)iSize;
	if ( (pState->BufferSize != 0) && (iRemain != 0) ) {
		size_t iCopy = XRT_SHA256_BLOCK_SIZE - pState->BufferSize;

		if ( iCopy > iRemain ) {
			iCopy = iRemain;
		}
		memcpy(pState->Buffer + pState->BufferSize, pRead, iCopy);
		pState->BufferSize += (uint32)iCopy;
		pRead += iCopy;
		iRemain -= iCopy;
		if ( pState->BufferSize == XRT_SHA256_BLOCK_SIZE ) {
			__xrtSha256Transform(pState->State, pState->Buffer);
			pState->BufferSize = 0;
		}
	}
	while ( iRemain >= XRT_SHA256_BLOCK_SIZE ) {
		__xrtSha256Transform(pState->State, pRead);
		pRead += XRT_SHA256_BLOCK_SIZE;
		iRemain -= XRT_SHA256_BLOCK_SIZE;
	}
	if ( iRemain != 0 ) {
		memcpy(pState->Buffer, pRead, iRemain);
		pState->BufferSize = (uint32)iRemain;
	}
	pState->Size = iNextSize;
	return true;
}



/* 向 SHA-256 压缩状态追加数据，公开入口只负责选择算法标记。 */
XRT_API bool xrtSha256Update(xsha256* pState, const void* pData, size_t iSize)
{
	return __xrtSha256Update(pState, pData, iSize, XRT_SHA256_GUARD);
}



/* 在状态副本上添加 SHA-2 padding，并输出请求的前缀摘要。 */
static bool __xrtSha256Final(
	const xsha256* pState,
	void* pDigest,
	size_t iDigestSize,
	uint32 iGuard
)
{
	xsha256 Copy;
	uint8 arrDigest[XRT_SHA256_SIZE];
	uint32 iOffset;

	if ( !__xrtSha256Validate(pState, iGuard) ) {
		return false;
	}
	if ( pDigest == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	Copy = *pState;
	iOffset = Copy.BufferSize;
	Copy.Buffer[iOffset++] = 0x80u;
	if ( iOffset > 56u ) {
		memset(Copy.Buffer + iOffset, 0, XRT_SHA256_BLOCK_SIZE - iOffset);
		__xrtSha256Transform(Copy.State, Copy.Buffer);
		iOffset = 0;
	}
	memset(Copy.Buffer + iOffset, 0, 56u - iOffset);
	__xrtCryptoStoreBe64(Copy.Buffer + 56, Copy.Size << 3u);
	__xrtSha256Transform(Copy.State, Copy.Buffer);
	for ( uint32 i = 0; i < 8u; i++ ) {
		__xrtCryptoStoreBe32(arrDigest + (i * 4u), Copy.State[i]);
	}
	memcpy(pDigest, arrDigest, iDigestSize);
	xrtSecureZero(arrDigest, sizeof(arrDigest));
	xrtSecureZero(&Copy, sizeof(Copy));
	return true;
}



/* 在状态副本上输出完整 32 字节 SHA-256 摘要。 */
XRT_API bool xrtSha256Final(const xsha256* pState, void* pDigest)
{
	return __xrtSha256Final(
		pState, pDigest, XRT_SHA256_SIZE, XRT_SHA256_GUARD
	);
}



/* 组合栈上流状态完成一次 SHA-256 计算，并清理全部中间状态。 */
XRT_API bool xrtSha256(const void* pData, size_t iSize, void* pDigest)
{
	xsha256 State;
	bool bResult;

	if ( pDigest == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	xrtSha256Init(&State);
	bResult = xrtSha256Update(&State, pData, iSize) &&
		xrtSha256Final(&State, pDigest);
	xrtSecureZero(&State, sizeof(State));
	return bResult;
}



#if defined(XRT_FEATURE_CRYPTO_SHA224)

/* 初始化 SHA-224 标准初始向量和空消息状态。 */
XRT_API void xrtSha224Init(xsha224* pState)
{
	if ( pState == NULL ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memset(pState, 0, sizeof(*pState));
	pState->State[0] = UINT32_C(0xC1059ED8);
	pState->State[1] = UINT32_C(0x367CD507);
	pState->State[2] = UINT32_C(0x3070DD17);
	pState->State[3] = UINT32_C(0xF70E5939);
	pState->State[4] = UINT32_C(0xFFC00B31);
	pState->State[5] = UINT32_C(0x68581511);
	pState->State[6] = UINT32_C(0x64F98FA7);
	pState->State[7] = UINT32_C(0xBEFA4FA4);
	pState->Guard = XRT_SHA224_GUARD;
}



/* 向 SHA-224 状态追加数据并严格检查算法标记。 */
XRT_API bool xrtSha224Update(xsha224* pState, const void* pData, size_t iSize)
{
	return __xrtSha256Update(pState, pData, iSize, XRT_SHA224_GUARD);
}



/* 从状态副本输出 SHA-224 的前七个状态字。 */
XRT_API bool xrtSha224Final(const xsha224* pState, void* pDigest)
{
	return __xrtSha256Final(
		pState, pDigest, XRT_SHA224_SIZE, XRT_SHA224_GUARD
	);
}



/* 组合栈上流状态完成一次 SHA-224 计算。 */
XRT_API bool xrtSha224(const void* pData, size_t iSize, void* pDigest)
{
	xsha224 State;
	bool bResult;

	if ( pDigest == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	xrtSha224Init(&State);
	bResult = xrtSha224Update(&State, pData, iSize) &&
		xrtSha224Final(&State, pDigest);
	xrtSecureZero(&State, sizeof(State));
	return bResult;
}

#endif



#undef XRT_SHA224_GUARD
#undef XRT_SHA256_GUARD
#undef XRT_SHA256_MAX_BYTES

#endif
