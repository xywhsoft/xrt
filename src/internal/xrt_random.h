#ifndef XRT_INTERNAL_RANDOM_H
#define XRT_INTERNAL_RANDOM_H

#include "xrt_internal.h"



#if defined(XRT_FEATURE_RANDOM_TEXT) || \
	defined(XRT_FEATURE_RANDOM_SECURE_TEXT)

#define XRT_RANDOM_DEFAULT_ALPHABET \
	"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_"



/* 返回随机文本便捷接口共用的 URL-safe 字母表。 */
static inline xstrview __xrtRandomDefaultAlphabet(void)
{
	static const char sAlphabet[] = XRT_RANDOM_DEFAULT_ALPHABET;
	xstrview Alphabet = { sAlphabet, sizeof(sAlphabet) - 1u };

	return Alphabet;
}



/* 验证字母表是非空、无重复的可见 ASCII 集合。 */
static inline bool __xrtRandomAlphabetValid(xstrview Alphabet)
{
	uint64 arrSeen[2] = { 0, 0 };

	if ( (Alphabet.Data == NULL) || (Alphabet.Size == 0) ||
		 (Alphabet.Size > 94u) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	for ( size_t i = 0; i < Alphabet.Size; i++ ) {
		uint8 iByte = (uint8)Alphabet.Data[i];
		size_t iWord;
		uint64 iMask;

		if ( (iByte < 0x21u) || (iByte > 0x7Eu) ) {
			__xrtErrorSetInvalidArgument();
			return false;
		}
		iWord = (size_t)iByte >> 6u;
		iMask = UINT64_C(1) << (iByte & 63u);
		if ( (arrSeen[iWord] & iMask) != 0 ) {
			__xrtErrorSetInvalidArgument();
			return false;
		}
		arrSeen[iWord] |= iMask;
	}
	return true;
}

#endif



#if defined(XRT_FEATURE_RANDOM)

/* 对已经验证的状态执行 32 位无偏有界采样。 */
uint32 __xrtRngBelow32Ready(xrng* pRng, uint32 iBound);

#endif



#if defined(XRT_FEATURE_RANDOM_DEFAULT)

/* 返回当前线程随机状态，首次调用时完成创建和自动播种。 */
xrng* __xrtRandCurrent(void);

#endif

#endif
