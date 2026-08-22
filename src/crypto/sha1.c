#include "../internal/xrt_crypto.h"



#if defined(XRT_FEATURE_CRYPTO_SHA1)

#define XRT_SHA1_GUARD UINT32_C(0x53484131)
#define XRT_SHA1_MAX_BYTES (UINT64_MAX >> 3u)



/* 压缩一个完整 SHA-1 块，使用 16 字循环调度降低栈占用。 */
static void __xrtSha1Transform(uint32* pState, const uint8* pBlock)
{
	uint32 Words[16];
	uint32 a = pState[0];
	uint32 b = pState[1];
	uint32 c = pState[2];
	uint32 d = pState[3];
	uint32 e = pState[4];

	for ( uint32 i = 0; i < 80u; i++ ) {
		uint32 iWord;
		uint32 f;
		uint32 k;

		if ( i < 16u ) {
			iWord = __xrtCryptoLoadBe32(pBlock + (i * 4u));
		} else {
			iWord = __xrtCryptoRotateLeft32(
				Words[(i - 3u) & 15u] ^
				Words[(i - 8u) & 15u] ^
				Words[(i - 14u) & 15u] ^
				Words[i & 15u],
				1u
			);
		}
		Words[i & 15u] = iWord;
		if ( i < 20u ) {
			f = (b & c) | ((~b) & d);
			k = UINT32_C(0x5A827999);
		} else if ( i < 40u ) {
			f = b ^ c ^ d;
			k = UINT32_C(0x6ED9EBA1);
		} else if ( i < 60u ) {
			f = (b & c) | (b & d) | (c & d);
			k = UINT32_C(0x8F1BBCDC);
		} else {
			f = b ^ c ^ d;
			k = UINT32_C(0xCA62C1D6);
		}
		{
			uint32 iNext = __xrtCryptoRotateLeft32(a, 5u) +
				f + e + k + iWord;

			e = d;
			d = c;
			c = __xrtCryptoRotateLeft32(b, 30u);
			b = a;
			a = iNext;
		}
	}
	pState[0] += a;
	pState[1] += b;
	pState[2] += c;
	pState[3] += d;
	pState[4] += e;
	xrtSecureZero(Words, sizeof(Words));
}



/* 验证公开状态字段仍满足初始化标记、尾部长度和消息上限。 */
static bool __xrtSha1Validate(const xsha1* pState)
{
	if ( pState == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (pState->Guard != XRT_SHA1_GUARD) ||
		 (pState->Size > XRT_SHA1_MAX_BYTES) ||
		 (pState->BufferSize >= XRT_SHA1_BLOCK_SIZE) ||
		 (pState->BufferSize != (uint32)(pState->Size & 63u)) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	return true;
}



/* 初始化 SHA-1 标准初始向量和空消息状态。 */
XRT_API void xrtSha1Init(xsha1* pState)
{
	if ( pState == NULL ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memset(pState, 0, sizeof(*pState));
	pState->State[0] = UINT32_C(0x67452301);
	pState->State[1] = UINT32_C(0xEFCDAB89);
	pState->State[2] = UINT32_C(0x98BADCFE);
	pState->State[3] = UINT32_C(0x10325476);
	pState->State[4] = UINT32_C(0xC3D2E1F0);
	pState->Guard = XRT_SHA1_GUARD;
}



/* 先验证全部失败条件，再按尾部、完整块、剩余尾部顺序追加数据。 */
XRT_API bool xrtSha1Update(xsha1* pState, const void* pData, size_t iSize)
{
	const uint8* pRead = (const uint8*)pData;
	size_t iRemain = iSize;
	uint64 iNextSize;

	if ( !__xrtSha1Validate(pState) ) {
		return false;
	}
	if ( (pRead == NULL) && (iSize != 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (uint64)iSize > (XRT_SHA1_MAX_BYTES - pState->Size) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	iNextSize = pState->Size + (uint64)iSize;
	if ( (pState->BufferSize != 0) && (iRemain != 0) ) {
		size_t iCopy = XRT_SHA1_BLOCK_SIZE - pState->BufferSize;

		if ( iCopy > iRemain ) {
			iCopy = iRemain;
		}
		memcpy(pState->Buffer + pState->BufferSize, pRead, iCopy);
		pState->BufferSize += (uint32)iCopy;
		pRead += iCopy;
		iRemain -= iCopy;
		if ( pState->BufferSize == XRT_SHA1_BLOCK_SIZE ) {
			__xrtSha1Transform(pState->State, pState->Buffer);
			pState->BufferSize = 0;
		}
	}
	while ( iRemain >= XRT_SHA1_BLOCK_SIZE ) {
		__xrtSha1Transform(pState->State, pRead);
		pRead += XRT_SHA1_BLOCK_SIZE;
		iRemain -= XRT_SHA1_BLOCK_SIZE;
	}
	if ( iRemain != 0 ) {
		memcpy(pState->Buffer, pRead, iRemain);
		pState->BufferSize = (uint32)iRemain;
	}
	pState->Size = iNextSize;
	return true;
}



/* 在状态副本上添加 SHA-1 padding，并以大端格式输出摘要。 */
XRT_API bool xrtSha1Final(const xsha1* pState, void* pDigest)
{
	xsha1 Copy;
	uint8 arrDigest[XRT_SHA1_SIZE];
	uint32 iOffset;

	if ( !__xrtSha1Validate(pState) ) {
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
		memset(Copy.Buffer + iOffset, 0, XRT_SHA1_BLOCK_SIZE - iOffset);
		__xrtSha1Transform(Copy.State, Copy.Buffer);
		iOffset = 0;
	}
	memset(Copy.Buffer + iOffset, 0, 56u - iOffset);
	__xrtCryptoStoreBe64(Copy.Buffer + 56, Copy.Size << 3u);
	__xrtSha1Transform(Copy.State, Copy.Buffer);
	for ( uint32 i = 0; i < 5u; i++ ) {
		__xrtCryptoStoreBe32(arrDigest + (i * 4u), Copy.State[i]);
	}
	memcpy(pDigest, arrDigest, sizeof(arrDigest));
	xrtSecureZero(arrDigest, sizeof(arrDigest));
	xrtSecureZero(&Copy, sizeof(Copy));
	return true;
}



/* 组合栈上流状态完成一次 SHA-1 计算，并清理全部中间状态。 */
XRT_API bool xrtSha1(const void* pData, size_t iSize, void* pDigest)
{
	xsha1 State;
	bool bResult;

	if ( pDigest == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	xrtSha1Init(&State);
	bResult = xrtSha1Update(&State, pData, iSize) &&
		xrtSha1Final(&State, pDigest);
	xrtSecureZero(&State, sizeof(State));
	return bResult;
}



#undef XRT_SHA1_GUARD
#undef XRT_SHA1_MAX_BYTES

#endif
