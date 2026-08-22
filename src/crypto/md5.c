#include "../internal/xrt_crypto.h"



#if defined(XRT_FEATURE_CRYPTO_MD5)

#define XRT_MD5_GUARD UINT32_C(0x4D443520)
#define XRT_MD5_MAX_BYTES (UINT64_MAX >> 3u)



static const uint32 __xrtMd5Constants[64] = {
	UINT32_C(0xD76AA478), UINT32_C(0xE8C7B756), UINT32_C(0x242070DB), UINT32_C(0xC1BDCEEE),
	UINT32_C(0xF57C0FAF), UINT32_C(0x4787C62A), UINT32_C(0xA8304613), UINT32_C(0xFD469501),
	UINT32_C(0x698098D8), UINT32_C(0x8B44F7AF), UINT32_C(0xFFFF5BB1), UINT32_C(0x895CD7BE),
	UINT32_C(0x6B901122), UINT32_C(0xFD987193), UINT32_C(0xA679438E), UINT32_C(0x49B40821),
	UINT32_C(0xF61E2562), UINT32_C(0xC040B340), UINT32_C(0x265E5A51), UINT32_C(0xE9B6C7AA),
	UINT32_C(0xD62F105D), UINT32_C(0x02441453), UINT32_C(0xD8A1E681), UINT32_C(0xE7D3FBC8),
	UINT32_C(0x21E1CDE6), UINT32_C(0xC33707D6), UINT32_C(0xF4D50D87), UINT32_C(0x455A14ED),
	UINT32_C(0xA9E3E905), UINT32_C(0xFCEFA3F8), UINT32_C(0x676F02D9), UINT32_C(0x8D2A4C8A),
	UINT32_C(0xFFFA3942), UINT32_C(0x8771F681), UINT32_C(0x6D9D6122), UINT32_C(0xFDE5380C),
	UINT32_C(0xA4BEEA44), UINT32_C(0x4BDECFA9), UINT32_C(0xF6BB4B60), UINT32_C(0xBEBFBC70),
	UINT32_C(0x289B7EC6), UINT32_C(0xEAA127FA), UINT32_C(0xD4EF3085), UINT32_C(0x04881D05),
	UINT32_C(0xD9D4D039), UINT32_C(0xE6DB99E5), UINT32_C(0x1FA27CF8), UINT32_C(0xC4AC5665),
	UINT32_C(0xF4292244), UINT32_C(0x432AFF97), UINT32_C(0xAB9423A7), UINT32_C(0xFC93A039),
	UINT32_C(0x655B59C3), UINT32_C(0x8F0CCC92), UINT32_C(0xFFEFF47D), UINT32_C(0x85845DD1),
	UINT32_C(0x6FA87E4F), UINT32_C(0xFE2CE6E0), UINT32_C(0xA3014314), UINT32_C(0x4E0811A1),
	UINT32_C(0xF7537E82), UINT32_C(0xBD3AF235), UINT32_C(0x2AD7D2BB), UINT32_C(0xEB86D391)
};



static const uint8 __xrtMd5Shifts[64] = {
	7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
	5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
	4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
	6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21
};



/* 压缩一个完整 MD5 块，轮函数和字序严格遵循 RFC 1321。 */
static void __xrtMd5Transform(uint32* pState, const uint8* pBlock)
{
	uint32 Words[16];
	uint32 a = pState[0];
	uint32 b = pState[1];
	uint32 c = pState[2];
	uint32 d = pState[3];

	for ( uint32 i = 0; i < 16u; i++ ) {
		Words[i] = __xrtCryptoLoadLe32(pBlock + (i * 4u));
	}
	for ( uint32 i = 0; i < 64u; i++ ) {
		uint32 f;
		uint32 g;
		uint32 iNext;

		if ( i < 16u ) {
			f = (b & c) | ((~b) & d);
			g = i;
		} else if ( i < 32u ) {
			f = (d & b) | ((~d) & c);
			g = ((5u * i) + 1u) & 15u;
		} else if ( i < 48u ) {
			f = b ^ c ^ d;
			g = ((3u * i) + 5u) & 15u;
		} else {
			f = c ^ (b | (~d));
			g = (7u * i) & 15u;
		}
		iNext = d;
		d = c;
		c = b;
		b += __xrtCryptoRotateLeft32(
			a + f + __xrtMd5Constants[i] + Words[g],
			(uint32)__xrtMd5Shifts[i]
		);
		a = iNext;
	}
	pState[0] += a;
	pState[1] += b;
	pState[2] += c;
	pState[3] += d;
	xrtSecureZero(Words, sizeof(Words));
}



/* 验证公开状态字段仍满足初始化标记、尾部长度和消息上限。 */
static bool __xrtMd5Validate(const xmd5* pState)
{
	if ( pState == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (pState->Guard != XRT_MD5_GUARD) ||
		 (pState->Size > XRT_MD5_MAX_BYTES) ||
		 (pState->BufferSize >= XRT_MD5_BLOCK_SIZE) ||
		 (pState->BufferSize != (uint32)(pState->Size & 63u)) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	return true;
}



/* 初始化 RFC 1321 规定的向量和空消息状态。 */
XRT_API void xrtMd5Init(xmd5* pState)
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
	pState->Guard = XRT_MD5_GUARD;
}



/* 先验证全部失败条件，再按尾部、完整块、剩余尾部顺序追加数据。 */
XRT_API bool xrtMd5Update(xmd5* pState, const void* pData, size_t iSize)
{
	const uint8* pRead = (const uint8*)pData;
	size_t iRemain = iSize;
	uint64 iNextSize;

	if ( !__xrtMd5Validate(pState) ) {
		return false;
	}
	if ( (pRead == NULL) && (iSize != 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (uint64)iSize > (XRT_MD5_MAX_BYTES - pState->Size) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	iNextSize = pState->Size + (uint64)iSize;
	if ( (pState->BufferSize != 0) && (iRemain != 0) ) {
		size_t iCopy = XRT_MD5_BLOCK_SIZE - pState->BufferSize;

		if ( iCopy > iRemain ) {
			iCopy = iRemain;
		}
		memcpy(pState->Buffer + pState->BufferSize, pRead, iCopy);
		pState->BufferSize += (uint32)iCopy;
		pRead += iCopy;
		iRemain -= iCopy;
		if ( pState->BufferSize == XRT_MD5_BLOCK_SIZE ) {
			__xrtMd5Transform(pState->State, pState->Buffer);
			pState->BufferSize = 0;
		}
	}
	while ( iRemain >= XRT_MD5_BLOCK_SIZE ) {
		__xrtMd5Transform(pState->State, pRead);
		pRead += XRT_MD5_BLOCK_SIZE;
		iRemain -= XRT_MD5_BLOCK_SIZE;
	}
	if ( iRemain != 0 ) {
		memcpy(pState->Buffer, pRead, iRemain);
		pState->BufferSize = (uint32)iRemain;
	}
	pState->Size = iNextSize;
	return true;
}



/* 在状态副本上添加 MD5 padding，并以小端格式输出摘要。 */
XRT_API bool xrtMd5Final(const xmd5* pState, void* pDigest)
{
	xmd5 Copy;
	uint8 arrDigest[XRT_MD5_SIZE];
	uint32 iOffset;

	if ( !__xrtMd5Validate(pState) ) {
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
		memset(Copy.Buffer + iOffset, 0, XRT_MD5_BLOCK_SIZE - iOffset);
		__xrtMd5Transform(Copy.State, Copy.Buffer);
		iOffset = 0;
	}
	memset(Copy.Buffer + iOffset, 0, 56u - iOffset);
	__xrtCryptoStoreLe64(Copy.Buffer + 56, Copy.Size << 3u);
	__xrtMd5Transform(Copy.State, Copy.Buffer);
	for ( uint32 i = 0; i < 4u; i++ ) {
		__xrtCryptoStoreLe32(arrDigest + (i * 4u), Copy.State[i]);
	}
	memcpy(pDigest, arrDigest, sizeof(arrDigest));
	xrtSecureZero(arrDigest, sizeof(arrDigest));
	xrtSecureZero(&Copy, sizeof(Copy));
	return true;
}



/* 组合栈上流状态完成一次 MD5 计算，并清理全部中间状态。 */
XRT_API bool xrtMd5(const void* pData, size_t iSize, void* pDigest)
{
	xmd5 State;
	bool bResult;

	if ( pDigest == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	xrtMd5Init(&State);
	bResult = xrtMd5Update(&State, pData, iSize) &&
		xrtMd5Final(&State, pDigest);
	xrtSecureZero(&State, sizeof(State));
	return bResult;
}



#undef XRT_MD5_GUARD
#undef XRT_MD5_MAX_BYTES

#endif
