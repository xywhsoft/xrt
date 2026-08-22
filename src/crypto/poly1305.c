#include "../internal/xrt_crypto.h"



#if defined(XRT_FEATURE_CRYPTO_POLY1305)

#define XRT_POLY1305_GUARD UINT32_C(0x504F4C59)
#define XRT_POLY1305_MASK UINT32_C(0x03FFFFFF)



/* 验证 Poly1305 状态标识与尾部边界。 */
static bool __xrtPoly1305Validate(const xpoly1305* pState)
{
	if ( pState == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (pState->Guard != XRT_POLY1305_GUARD) ||
		 (pState->BufferSize >= XRT_POLY1305_BLOCK_SIZE) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	return true;
}



/* 按 26 位 limb 批量吸收完整块；iHibit 区分普通块与最终补齐块。 */
static void __xrtPoly1305Blocks(
	xpoly1305* pState,
	const uint8* pData,
	size_t iSize,
	uint32 iHibit
)
{
	uint32 iR0 = pState->R[0];
	uint32 iR1 = pState->R[1];
	uint32 iR2 = pState->R[2];
	uint32 iR3 = pState->R[3];
	uint32 iR4 = pState->R[4];
	uint32 iS1 = iR1 * 5u;
	uint32 iS2 = iR2 * 5u;
	uint32 iS3 = iR3 * 5u;
	uint32 iS4 = iR4 * 5u;
	uint32 iH0 = pState->H[0];
	uint32 iH1 = pState->H[1];
	uint32 iH2 = pState->H[2];
	uint32 iH3 = pState->H[3];
	uint32 iH4 = pState->H[4];

	while ( iSize >= XRT_POLY1305_BLOCK_SIZE ) {
		uint64 iD0;
		uint64 iD1;
		uint64 iD2;
		uint64 iD3;
		uint64 iD4;
		uint32 iCarry;

		iH0 += __xrtCryptoLoadLe32(pData) & XRT_POLY1305_MASK;
		iH1 += (__xrtCryptoLoadLe32(pData + 3) >> 2u) & XRT_POLY1305_MASK;
		iH2 += (__xrtCryptoLoadLe32(pData + 6) >> 4u) & XRT_POLY1305_MASK;
		iH3 += (__xrtCryptoLoadLe32(pData + 9) >> 6u) & XRT_POLY1305_MASK;
		iH4 += (__xrtCryptoLoadLe32(pData + 12) >> 8u) | iHibit;

		iD0 = ((uint64)iH0 * iR0) + ((uint64)iH1 * iS4) +
			((uint64)iH2 * iS3) + ((uint64)iH3 * iS2) +
			((uint64)iH4 * iS1);
		iD1 = ((uint64)iH0 * iR1) + ((uint64)iH1 * iR0) +
			((uint64)iH2 * iS4) + ((uint64)iH3 * iS3) +
			((uint64)iH4 * iS2);
		iD2 = ((uint64)iH0 * iR2) + ((uint64)iH1 * iR1) +
			((uint64)iH2 * iR0) + ((uint64)iH3 * iS4) +
			((uint64)iH4 * iS3);
		iD3 = ((uint64)iH0 * iR3) + ((uint64)iH1 * iR2) +
			((uint64)iH2 * iR1) + ((uint64)iH3 * iR0) +
			((uint64)iH4 * iS4);
		iD4 = ((uint64)iH0 * iR4) + ((uint64)iH1 * iR3) +
			((uint64)iH2 * iR2) + ((uint64)iH3 * iR1) +
			((uint64)iH4 * iR0);

		iCarry = (uint32)(iD0 >> 26u);
		iH0 = (uint32)iD0 & XRT_POLY1305_MASK;
		iD1 += iCarry;
		iCarry = (uint32)(iD1 >> 26u);
		iH1 = (uint32)iD1 & XRT_POLY1305_MASK;
		iD2 += iCarry;
		iCarry = (uint32)(iD2 >> 26u);
		iH2 = (uint32)iD2 & XRT_POLY1305_MASK;
		iD3 += iCarry;
		iCarry = (uint32)(iD3 >> 26u);
		iH3 = (uint32)iD3 & XRT_POLY1305_MASK;
		iD4 += iCarry;
		iCarry = (uint32)(iD4 >> 26u);
		iH4 = (uint32)iD4 & XRT_POLY1305_MASK;
		iH0 += iCarry * 5u;
		iCarry = iH0 >> 26u;
		iH0 &= XRT_POLY1305_MASK;
		iH1 += iCarry;

		pData += XRT_POLY1305_BLOCK_SIZE;
		iSize -= XRT_POLY1305_BLOCK_SIZE;
	}
	pState->H[0] = iH0;
	pState->H[1] = iH1;
	pState->H[2] = iH2;
	pState->H[3] = iH3;
	pState->H[4] = iH4;
}



/* 夹紧 r 并保存 s，在成功后一次提交新状态。 */
XRT_API bool xrtPoly1305Init(xpoly1305* pState, const void* pKey)
{
	const uint8* pBytes = (const uint8*)pKey;
	xpoly1305 Next;

	if ( (pState == NULL) || (pKey == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(&Next, 0, sizeof(Next));
	Next.R[0] = __xrtCryptoLoadLe32(pBytes) & UINT32_C(0x03FFFFFF);
	Next.R[1] = (__xrtCryptoLoadLe32(pBytes + 3) >> 2u) & UINT32_C(0x03FFFF03);
	Next.R[2] = (__xrtCryptoLoadLe32(pBytes + 6) >> 4u) & UINT32_C(0x03FFC0FF);
	Next.R[3] = (__xrtCryptoLoadLe32(pBytes + 9) >> 6u) & UINT32_C(0x03F03FFF);
	Next.R[4] = (__xrtCryptoLoadLe32(pBytes + 12) >> 8u) & UINT32_C(0x000FFFFF);
	for ( size_t i = 0; i < 4; i++ ) {
		Next.Pad[i] = __xrtCryptoLoadLe32(pBytes + 16 + (i * 4));
	}
	Next.Guard = XRT_POLY1305_GUARD;
	*pState = Next;
	xrtSecureZero(&Next, sizeof(Next));
	return true;
}



/* 先补足旧尾部，再直接处理调用方的完整块并保存新尾部。 */
XRT_API bool xrtPoly1305Update(
	xpoly1305* pState,
	const void* pData,
	size_t iSize
)
{
	const uint8* pRead = (const uint8*)pData;

	if ( !__xrtPoly1305Validate(pState) ) {
		return false;
	}
	if ( (pData == NULL) && (iSize != 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( __xrtCryptoRangesOverlap(
		pState, sizeof(*pState), pData, iSize
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pState->BufferSize != 0 ) {
		size_t iNeed = XRT_POLY1305_BLOCK_SIZE - pState->BufferSize;
		size_t iCopy = iSize < iNeed ? iSize : iNeed;

		if ( iCopy != 0 ) {
			memcpy(pState->Buffer + pState->BufferSize, pRead, iCopy);
		}
		pState->BufferSize += (uint32)iCopy;
		pRead += iCopy;
		iSize -= iCopy;
		if ( pState->BufferSize != XRT_POLY1305_BLOCK_SIZE ) {
			return true;
		}
		__xrtPoly1305Blocks(
			pState, pState->Buffer, XRT_POLY1305_BLOCK_SIZE, 1u << 24u
		);
		pState->BufferSize = 0;
	}
	if ( iSize >= XRT_POLY1305_BLOCK_SIZE ) {
		size_t iBlocks = iSize & ~(size_t)(XRT_POLY1305_BLOCK_SIZE - 1u);

		__xrtPoly1305Blocks(pState, pRead, iBlocks, 1u << 24u);
		pRead += iBlocks;
		iSize -= iBlocks;
	}
	if ( iSize != 0 ) {
		memcpy(pState->Buffer, pRead, iSize);
		pState->BufferSize = (uint32)iSize;
	}
	return true;
}



/* 在状态快照上完成约减和加 pad，不消耗调用方的流状态。 */
XRT_API bool xrtPoly1305Final(const xpoly1305* pState, void* pTag)
{
	xpoly1305 Final;
	uint8 Tag[XRT_POLY1305_TAG_SIZE];
	uint32 iH0;
	uint32 iH1;
	uint32 iH2;
	uint32 iH3;
	uint32 iH4;
	uint32 iG0;
	uint32 iG1;
	uint32 iG2;
	uint32 iG3;
	uint32 iG4;
	uint32 iCarry;
	uint32 iMask;
	uint64 iValue;

	if ( !__xrtPoly1305Validate(pState) ) {
		return false;
	}
	if ( (pTag == NULL) || __xrtCryptoRangesOverlap(
		pState, sizeof(*pState), pTag, XRT_POLY1305_TAG_SIZE
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	Final = *pState;
	if ( Final.BufferSize != 0 ) {
		Final.Buffer[Final.BufferSize++] = 1;
		if ( Final.BufferSize < XRT_POLY1305_BLOCK_SIZE ) {
			memset(
				Final.Buffer + Final.BufferSize,
				0,
				XRT_POLY1305_BLOCK_SIZE - Final.BufferSize
			);
		}
		__xrtPoly1305Blocks(
			&Final, Final.Buffer, XRT_POLY1305_BLOCK_SIZE, 0
		);
	}
	iH0 = Final.H[0];
	iH1 = Final.H[1];
	iH2 = Final.H[2];
	iH3 = Final.H[3];
	iH4 = Final.H[4];

	iCarry = iH1 >> 26u;
	iH1 &= XRT_POLY1305_MASK;
	iH2 += iCarry;
	iCarry = iH2 >> 26u;
	iH2 &= XRT_POLY1305_MASK;
	iH3 += iCarry;
	iCarry = iH3 >> 26u;
	iH3 &= XRT_POLY1305_MASK;
	iH4 += iCarry;
	iCarry = iH4 >> 26u;
	iH4 &= XRT_POLY1305_MASK;
	iH0 += iCarry * 5u;
	iCarry = iH0 >> 26u;
	iH0 &= XRT_POLY1305_MASK;
	iH1 += iCarry;

	iG0 = iH0 + 5u;
	iCarry = iG0 >> 26u;
	iG0 &= XRT_POLY1305_MASK;
	iG1 = iH1 + iCarry;
	iCarry = iG1 >> 26u;
	iG1 &= XRT_POLY1305_MASK;
	iG2 = iH2 + iCarry;
	iCarry = iG2 >> 26u;
	iG2 &= XRT_POLY1305_MASK;
	iG3 = iH3 + iCarry;
	iCarry = iG3 >> 26u;
	iG3 &= XRT_POLY1305_MASK;
	iG4 = iH4 + iCarry - (1u << 26u);

	iMask = (iG4 >> 31u) - 1u;
	iG0 &= iMask;
	iG1 &= iMask;
	iG2 &= iMask;
	iG3 &= iMask;
	iG4 &= iMask;
	iMask = ~iMask;
	iH0 = (iH0 & iMask) | iG0;
	iH1 = (iH1 & iMask) | iG1;
	iH2 = (iH2 & iMask) | iG2;
	iH3 = (iH3 & iMask) | iG3;
	iH4 = (iH4 & iMask) | iG4;

	iH0 |= iH1 << 26u;
	iH1 = (iH1 >> 6u) | (iH2 << 20u);
	iH2 = (iH2 >> 12u) | (iH3 << 14u);
	iH3 = (iH3 >> 18u) | (iH4 << 8u);
	iValue = (uint64)iH0 + Final.Pad[0];
	__xrtCryptoStoreLe32(Tag, (uint32)iValue);
	iValue = (uint64)iH1 + Final.Pad[1] + (iValue >> 32u);
	__xrtCryptoStoreLe32(Tag + 4, (uint32)iValue);
	iValue = (uint64)iH2 + Final.Pad[2] + (iValue >> 32u);
	__xrtCryptoStoreLe32(Tag + 8, (uint32)iValue);
	iValue = (uint64)iH3 + Final.Pad[3] + (iValue >> 32u);
	__xrtCryptoStoreLe32(Tag + 12, (uint32)iValue);
	memcpy(pTag, Tag, sizeof(Tag));
	xrtSecureZero(Tag, sizeof(Tag));
	xrtSecureZero(&Final, sizeof(Final));
	return true;
}



/* 组合栈上状态完成一次 Poly1305 认证。 */
XRT_API bool xrtPoly1305(
	const void* pKey,
	const void* pData,
	size_t iSize,
	void* pTag
)
{
	xpoly1305 State;
	bool bResult;

	memset(&State, 0, sizeof(State));
	bResult = xrtPoly1305Init(&State, pKey) &&
		xrtPoly1305Update(&State, pData, iSize) &&
		xrtPoly1305Final(&State, pTag);
	xrtSecureZero(&State, sizeof(State));
	return bResult;
}



#undef XRT_POLY1305_GUARD
#undef XRT_POLY1305_MASK

#endif
