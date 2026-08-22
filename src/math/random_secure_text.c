#include "../internal/xrt_random.h"



#if defined(XRT_FEATURE_RANDOM_SECURE_TEXT)

#define XRT_SECURE_TEXT_BUFFER_SIZE 256u



/* 使用拒绝采样把系统随机字节均匀映射到已经验证的字母表。 */
static bool __xrtSecureTextWrite(xstrview Alphabet,
	char* sOutput, size_t iLength)
{
	uint8 arrRandom[XRT_SECURE_TEXT_BUFFER_SIZE];
	size_t iAvailable = 0;
	size_t iOffset = 0;
	size_t iWritten = 0;
	uint32 iLimit = 256u - (256u % (uint32)Alphabet.Size);

	while ( iWritten < iLength ) {
		uint8 iValue;

		if ( iOffset == iAvailable ) {
			iAvailable = sizeof(arrRandom);
			if ( !xrtSecureRandom(arrRandom, iAvailable) ) {
				xrtSecureZero(arrRandom, sizeof(arrRandom));
				xrtSecureZero(sOutput, iLength + 1u);
				return false;
			}
			iOffset = 0;
		}

		iValue = arrRandom[iOffset++];
		if ( (uint32)iValue >= iLimit ) {
			continue;
		}
		sOutput[iWritten++] = Alphabet.Data[
			(size_t)((uint32)iValue % (uint32)Alphabet.Size)
		];
	}
	sOutput[iLength] = 0;
	xrtSecureZero(arrRandom, sizeof(arrRandom));
	return true;
}



/* 完整验证输出范围后生成密码安全随机文本。 */
XRT_API bool xrtSecureText(xstrview Alphabet,
	char* sOutput, size_t iCapacity, size_t iLength)
{
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
			Alphabet.Data, Alphabet.Size) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtSecureTextWrite(Alphabet, sOutput, iLength);
}



/* 验证参数并创建由调用方持有的密码安全随机字符串。 */
XRT_API str xrtSecureStringFrom(xstrview Alphabet, size_t iLength)
{
	str sOutput;

	if ( !__xrtRandomAlphabetValid(Alphabet) ) {
		return NULL;
	}
	if ( iLength == SIZE_MAX ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	sOutput = (str)xrtMalloc(iLength + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !__xrtSecureTextWrite(Alphabet, sOutput, iLength) ) {
		xrtFree(sOutput);
		return NULL;
	}
	return sOutput;
}



/* 使用默认 URL-safe 字母表创建密码安全随机字符串。 */
XRT_API str xrtSecureString(size_t iLength)
{
	return xrtSecureStringFrom(__xrtRandomDefaultAlphabet(), iLength);
}



#undef XRT_SECURE_TEXT_BUFFER_SIZE

#endif
