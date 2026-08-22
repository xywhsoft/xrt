#include "../internal/xrt_random.h"



#if defined(XRT_FEATURE_RANDOM_TEXT_DEFAULT)

/* 使用当前线程随机状态把文本写入调用方缓冲区。 */
XRT_API bool xrtRandText(xstrview Alphabet,
	char* sOutput, size_t iCapacity, size_t iLength)
{
	xrng* pRng = __xrtRandCurrent();

	return pRng != NULL &&
		xrtRngText(pRng, Alphabet, sOutput, iCapacity, iLength);
}



/* 使用当前线程随机状态和自定义字母表创建字符串。 */
XRT_API str xrtRandStringFrom(xstrview Alphabet, size_t iLength)
{
	xrng* pRng = __xrtRandCurrent();

	return pRng != NULL ? xrtRngStringFrom(pRng, Alphabet, iLength) : NULL;
}



/* 使用当前线程随机状态和默认字母表创建字符串。 */
XRT_API str xrtRandString(size_t iLength)
{
	xrng* pRng = __xrtRandCurrent();

	return pRng != NULL ? xrtRngString(pRng, iLength) : NULL;
}

#endif
