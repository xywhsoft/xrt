#include "../internal/xrt_internal.h"



#if defined(XRT_FEATURE_HASH_KEYED)

/* SipHash-2-4 的固定初始化常量。 */
#define XRT_SIP_V0 UINT64_C(0x736F6D6570736575)
#define XRT_SIP_V1 UINT64_C(0x646F72616E646F6D)
#define XRT_SIP_V2 UINT64_C(0x6C7967656E657261)
#define XRT_SIP_V3 UINT64_C(0x7465646279746573)
#define XRT_SIP_GUARD UINT32_C(0x53495032)



/* 循环左移 64 位字。 */
static uint64 __xrtSipRotate(uint64 iValue, unsigned int iBits)
{
	return (iValue << iBits) | (iValue >> (64u - iBits));
}



/* 执行一轮 SipHash 状态混合。 */
static void __xrtSipRound(uint64 State[4])
{
	State[0] += State[1];
	State[1] = __xrtSipRotate(State[1], 13);
	State[1] ^= State[0];
	State[0] = __xrtSipRotate(State[0], 32);
	State[2] += State[3];
	State[3] = __xrtSipRotate(State[3], 16);
	State[3] ^= State[2];
	State[0] += State[3];
	State[3] = __xrtSipRotate(State[3], 21);
	State[3] ^= State[0];
	State[2] += State[1];
	State[1] = __xrtSipRotate(State[1], 17);
	State[1] ^= State[2];
	State[2] = __xrtSipRotate(State[2], 32);
}



/* 按固定小端序读取一个完整消息字。 */
static uint64 __xrtSipRead(const unsigned char* pData)
{
	return __xrtReadLe64(pData);
}



/* 把一个完整消息字压入状态。 */
static void __xrtSipCompress(uint64 State[4], uint64 iWord)
{
	State[3] ^= iWord;
	__xrtSipRound(State);
	__xrtSipRound(State);
	State[0] ^= iWord;
}



/* 从两个明确的 64 位字创建密钥。 */
XRT_API xsipkey xrtSipKey(uint64 iLow, uint64 iHigh)
{
	xsipkey Key;

	Key.Low = iLow;
	Key.High = iHigh;
	return Key;
}



/* 初始化流式状态，不执行分配，也不依赖全局随机状态。 */
XRT_API void xrtSipHashInit(xsiphash* pState, xsipkey Key)
{
	if ( pState == NULL ) {
		__xrtErrorSetInvalidArgument();
		return;
	}

	pState->State[0] = XRT_SIP_V0 ^ Key.Low;
	pState->State[1] = XRT_SIP_V1 ^ Key.High;
	pState->State[2] = XRT_SIP_V2 ^ Key.Low;
	pState->State[3] = XRT_SIP_V3 ^ Key.High;
	pState->Total = 0;
	memset(pState->Tail, 0, sizeof(pState->Tail));
	pState->Guard = XRT_SIP_GUARD;
	pState->TailSize = 0;
}



/* 追加一块输入，同时保留不足八字节的尾部。 */
XRT_API bool xrtSipHashUpdate(xsiphash* pState, const void* pData, size_t iSize)
{
	const unsigned char* pBytes = (const unsigned char*)pData;
	size_t iRemain = iSize;

	if ( (pState == NULL) || ((pData == NULL) && (iSize != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( __xrtRangesOverlap(pState, sizeof(*pState), pData, iSize) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (pState->Guard != XRT_SIP_GUARD) || (pState->TailSize > 7u) ||
		 ((pState->Total & UINT64_C(7)) != pState->TailSize) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	if ( (uint64)iSize > (UINT64_MAX - pState->Total) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	if ( iSize == 0 ) {
		return true;
	}

	if ( pState->TailSize != 0 ) {
		size_t iNeed = 8u - pState->TailSize;
		size_t iTake = iRemain < iNeed ? iRemain : iNeed;

		memcpy(pState->Tail + pState->TailSize, pBytes, iTake);
		pState->TailSize = (uint8)(pState->TailSize + iTake);
		pBytes += iTake;
		iRemain -= iTake;
		if ( pState->TailSize == 8u ) {
			__xrtSipCompress(pState->State, __xrtSipRead(pState->Tail));
			pState->TailSize = 0;
		}
	}

	while ( iRemain >= 8u ) {
		__xrtSipCompress(pState->State, __xrtSipRead(pBytes));
		pBytes += 8;
		iRemain -= 8;
	}

	if ( iRemain != 0 ) {
		memcpy(pState->Tail, pBytes, iRemain);
		pState->TailSize = (uint8)iRemain;
	}
	pState->Total += (uint64)iSize;
	return true;
}



/* 在状态副本上完成尾块和四轮终结混合。 */
XRT_API uint64 xrtSipHashFinal(const xsiphash* pState)
{
	uint64 State[4];
	uint64 iFinal;

	if ( pState == NULL ) {
		__xrtErrorSetInvalidArgument();
		return 0;
	}
	if ( (pState->Guard != XRT_SIP_GUARD) || (pState->TailSize > 7u) ||
		 ((pState->Total & UINT64_C(7)) != pState->TailSize) ) {
		__xrtErrorSetInvalidState();
		return 0;
	}

	memcpy(State, pState->State, sizeof(State));
	iFinal = pState->Total << 56;
	for ( size_t i = 0; i < pState->TailSize; i++ ) {
		iFinal |= (uint64)pState->Tail[i] << (i * 8u);
	}
	__xrtSipCompress(State, iFinal);
	State[2] ^= UINT64_C(0xFF);
	__xrtSipRound(State);
	__xrtSipRound(State);
	__xrtSipRound(State);
	__xrtSipRound(State);
	return State[0] ^ State[1] ^ State[2] ^ State[3];
}



/* 常见连续输入路径直接复用流式实现，保证只有一套语义。 */
XRT_API uint64 xrtSipHash(const void* pData, size_t iSize, xsipkey Key)
{
	xsiphash State;

	xrtSipHashInit(&State, Key);
	if ( !xrtSipHashUpdate(&State, pData, iSize) ) {
		return 0;
	}
	return xrtSipHashFinal(&State);
}



#undef XRT_SIP_V0
#undef XRT_SIP_V1
#undef XRT_SIP_V2
#undef XRT_SIP_V3
#undef XRT_SIP_GUARD

#endif
