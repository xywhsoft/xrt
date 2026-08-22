#include "../internal/xrt_random.h"



#if defined(XRT_FEATURE_RANDOM_TEXT)



/* 使用已经验证的状态和字母表写入随机文本。 */
static void __xrtRngTextWrite(xrng* pRng, xstrview Alphabet,
	char* sOutput, size_t iLength)
{
	for ( size_t i = 0; i < iLength; i++ ) {
		sOutput[i] = Alphabet.Data[
			(size_t)__xrtRngBelow32Ready(pRng, (uint32)Alphabet.Size)
		];
	}
	sOutput[iLength] = 0;
}



/* 使用已经验证的状态和字母表创建独立随机字符串。 */
static str __xrtRngStringCreate(xrng* pRng, xstrview Alphabet,
	size_t iLength)
{
	str sOutput;

	if ( iLength == SIZE_MAX ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	sOutput = (str)xrtMalloc(iLength + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	__xrtRngTextWrite(pRng, Alphabet, sOutput, iLength);
	return sOutput;
}



/* 把可复现随机文本写入调用方缓冲区并补零。 */
XRT_API bool xrtRngText(xrng* pRng, xstrview Alphabet,
	char* sOutput, size_t iCapacity, size_t iLength)
{
	if ( pRng == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtRngReady(pRng) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	if ( !__xrtRandomAlphabetValid(Alphabet) ) {
		return false;
	}
	if ( sOutput == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( iLength == SIZE_MAX ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	if ( iCapacity <= iLength ) {
		__xrtErrorSetRange();
		return false;
	}
	if ( __xrtRangesOverlap(sOutput, iLength + 1u,
			Alphabet.Data, Alphabet.Size) ||
		 __xrtRangesOverlap(sOutput, iLength + 1u,
			pRng, sizeof(*pRng)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}

	/* 每个输出字节都使用 32 位拒绝采样，避免模偏差和双倍状态消耗。 */
	__xrtRngTextWrite(pRng, Alphabet, sOutput, iLength);
	return true;
}



/* 使用自定义字母表创建独立随机字符串。 */
XRT_API str xrtRngStringFrom(xrng* pRng, xstrview Alphabet, size_t iLength)
{
	if ( pRng == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( !xrtRngReady(pRng) ) {
		__xrtErrorSetInvalidState();
		return NULL;
	}
	if ( !__xrtRandomAlphabetValid(Alphabet) ) {
		return NULL;
	}
	return __xrtRngStringCreate(pRng, Alphabet, iLength);
}



/* 使用 URL-safe 默认字母表创建独立随机字符串。 */
XRT_API str xrtRngString(xrng* pRng, size_t iLength)
{
	xstrview Alphabet = __xrtRandomDefaultAlphabet();

	if ( pRng == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( !xrtRngReady(pRng) ) {
		__xrtErrorSetInvalidState();
		return NULL;
	}
	return __xrtRngStringCreate(pRng, Alphabet, iLength);
}

#endif
