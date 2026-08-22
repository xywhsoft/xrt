#include "../internal/xrt_crypto_int31.h"



#if defined(XRT_FEATURE_CRYPTO_INT31)

/*
	本文件的 31 位字整数算法改编自 BearSSL 0.6。
	保留固定内存访问与无数据分支性质，并用显式 R^2 输入消除通用除法依赖。
*/



/* 按控制位复制一段内存，访问轨迹不依赖数据内容。 */
void __xrtI31Copy(
	uint32 iControl,
	void* pDestination,
	const void* pSource,
	size_t iSize
)
{
	uint8* pWrite = (uint8*)pDestination;
	const uint8* pRead = (const uint8*)pSource;

	while ( iSize > 0 ) {
		*pWrite = (uint8)__xrtI31Select(iControl, *pRead, *pWrite);
		pWrite++;
		pRead++;
		iSize--;
	}
}



/* 清空一个整数，并写入编码后的公告位数。 */
void __xrtI31Zero(uint32* pValue, uint32 iBitLength)
{
	*pValue++ = iBitLength;
	memset(pValue, 0, ((iBitLength + 31u) >> 5u) * sizeof(*pValue));
}



/* 计算一个小端 31 位字数组的编码公告位数。 */
static uint32 __xrtI31BitLength(uint32* pValue, size_t iWords)
{
	uint32 iTop = 0;
	uint32 iTopIndex = 0;
	uint32 iBits;
	uint32 iSelect;

	while ( iWords > 0 ) {
		uint32 iWord;

		iWords--;
		iWord = pValue[iWords];
		iSelect = __xrtI31Equal(iTop, 0);
		iTop = __xrtI31Select(iSelect, iWord, iTop);
		iTopIndex = __xrtI31Select(iSelect, (uint32)iWords, iTopIndex);
	}

	iBits = __xrtI31NotEqual(iTop, 0);
	iSelect = __xrtI31Greater(iTop, UINT32_C(0xFFFF));
	iTop = __xrtI31Select(iSelect, iTop >> 16u, iTop);
	iBits += iSelect << 4u;
	iSelect = __xrtI31Greater(iTop, UINT32_C(0xFF));
	iTop = __xrtI31Select(iSelect, iTop >> 8u, iTop);
	iBits += iSelect << 3u;
	iSelect = __xrtI31Greater(iTop, UINT32_C(0x0F));
	iTop = __xrtI31Select(iSelect, iTop >> 4u, iTop);
	iBits += iSelect << 2u;
	iSelect = __xrtI31Greater(iTop, UINT32_C(0x03));
	iTop = __xrtI31Select(iSelect, iTop >> 2u, iTop);
	iBits += iSelect << 1u;
	iBits += __xrtI31Greater(iTop, 1u);
	return (iTopIndex << 5u) + iBits;
}



/* 从大端字节串解码一个无符号整数。 */
void __xrtI31Decode(uint32* pValue, const void* pSource, size_t iSize)
{
	const uint8* pBytes = (const uint8*)pSource;
	size_t iRemaining = iSize;
	size_t iWord = 1;
	uint32 iAccumulator = 0;
	int iAccumulatorBits = 0;

	while ( iRemaining > 0 ) {
		uint32 iByte = pBytes[--iRemaining];

		iAccumulator |= iByte << iAccumulatorBits;
		iAccumulatorBits += 8;
		if ( iAccumulatorBits >= 31 ) {
			pValue[iWord++] = iAccumulator & XRT_I31_WORD_MASK;
			iAccumulatorBits -= 31;
			iAccumulator = iByte >> (8 - iAccumulatorBits);
		}
	}
	if ( iAccumulatorBits != 0 ) {
		pValue[iWord++] = iAccumulator;
	}
	pValue[0] = __xrtI31BitLength(pValue + 1, iWord - 1u);
}



/* 把整数右移 0 到 30 位。 */
void __xrtI31RightShift(uint32* pValue, int iBits)
{
	size_t iLength = (pValue[0] + 31u) >> 5u;
	uint32 iCarry;

	if ( iLength == 0 ) {
		return;
	}
	iCarry = pValue[1] >> iBits;
	for ( size_t i = 2; i <= iLength; i++ ) {
		uint32 iWord = pValue[i];

		pValue[i - 1u] = ((iWord << (31 - iBits)) | iCarry) &
			XRT_I31_WORD_MASK;
		iCarry = iWord >> iBits;
	}
	pValue[iLength] = iCarry;
}



/* 条件执行同长度整数加法，并返回最高进位。 */
uint32 __xrtI31Add(uint32* pLeft, const uint32* pRight, uint32 iControl)
{
	uint32 iCarry = 0;
	size_t iWords = (pLeft[0] + 63u) >> 5u;

	for ( size_t i = 1; i < iWords; i++ ) {
		uint32 iOriginal = pLeft[i];
		uint32 iValue = iOriginal + pRight[i] + iCarry;

		iCarry = iValue >> 31u;
		pLeft[i] = __xrtI31Select(
			iControl,
			iValue & XRT_I31_WORD_MASK,
			iOriginal
		);
	}
	return iCarry;
}



/* 条件执行同长度整数减法，并返回最高借位。 */
uint32 __xrtI31Subtract(
	uint32* pLeft,
	const uint32* pRight,
	uint32 iControl
)
{
	uint32 iBorrow = 0;
	size_t iWords = (pLeft[0] + 63u) >> 5u;

	for ( size_t i = 1; i < iWords; i++ ) {
		uint32 iOriginal = pLeft[i];
		uint32 iValue = iOriginal - pRight[i] - iBorrow;

		iBorrow = iValue >> 31u;
		pLeft[i] = __xrtI31Select(
			iControl,
			iValue & XRT_I31_WORD_MASK,
			iOriginal
		);
	}
	return iBorrow;
}



/* 判断一个整数是否为零。 */
uint32 __xrtI31IsZero(const uint32* pValue)
{
	uint32 iCombined = 0;
	size_t iWords = (pValue[0] + 31u) >> 5u;

	for ( size_t i = iWords; i > 0; i-- ) {
		iCombined |= pValue[i];
	}
	return ~(iCombined | (0u - iCombined)) >> 31u;
}



/* 从大端字节串解码一个严格小于模数的整数。 */
uint32 __xrtI31DecodeMod(
	uint32* pValue,
	const void* pSource,
	size_t iSize,
	const uint32* pModulus
)
{
	const uint8* pBytes = (const uint8*)pSource;
	size_t iModulusWords = (pModulus[0] + 31u) >> 5u;
	size_t iTotal = iModulusWords << 2u;
	uint32 iResult = 0;

	if ( iTotal < iSize ) {
		iTotal = iSize;
	}
	iTotal += 4u;

	for ( int iPass = 0; iPass < 2; iPass++ ) {
		size_t iWord = 1;
		uint32 iAccumulator = 0;
		int iAccumulatorBits = 0;

		for ( size_t i = 0; i < iTotal; i++ ) {
			uint32 iByte = (i < iSize) ? pBytes[iSize - 1u - i] : 0u;

			iAccumulator |= iByte << iAccumulatorBits;
			iAccumulatorBits += 8;
			if ( iAccumulatorBits >= 31 ) {
				uint32 iInputWord = iAccumulator & XRT_I31_WORD_MASK;

				iAccumulatorBits -= 31;
				iAccumulator = iByte >> (8 - iAccumulatorBits);
				if ( iWord <= iModulusWords ) {
					if ( iPass != 0 ) {
						pValue[iWord] = iResult & iInputWord;
					} else {
						uint32 iComparison =
							(uint32)__xrtI31Compare(iInputWord, pModulus[iWord]);

						iResult = __xrtI31Select(
							__xrtI31Equal(iComparison, 0),
							iResult,
							iComparison
						);
					}
				} else if ( iPass == 0 ) {
					iResult = __xrtI31Select(
						__xrtI31Equal(iInputWord, 0),
						iResult,
						1u
					);
				}
				iWord++;
			}
		}

		iResult >>= 1u;
		iResult |= iResult << 1u;
	}

	pValue[0] = pModulus[0];
	return iResult & 1u;
}



/* 按输入的固定比特轨迹执行模加倍，得到任意长度整数的模数余数。 */
void __xrtI31ReduceBytes(
	uint32* pValue,
	const void* pSource,
	size_t iSize,
	const uint32* pModulus
)
{
	const uint8* pBytes = (const uint8*)pSource;

	__xrtI31Zero(pValue, pModulus[0]);
	for ( size_t i = 0; i < iSize; i++ ) {
		uint32 iByte = pBytes[i];

		for ( int iBit = 7; iBit >= 0; iBit-- ) {
			uint32 iCarry = __xrtI31Add(pValue, pValue, 1u);
			uint32 iBorrow;

			pValue[1] |= (iByte >> iBit) & 1u;
			iBorrow = __xrtI31Subtract(pValue, pModulus, 0u);
			__xrtI31Subtract(
				pValue,
				pModulus,
				iCarry | __xrtI31Not(iBorrow)
			);
		}
	}
}



/* 把整数编码为指定长度的大端字节串。 */
void __xrtI31Encode(void* pDestination, size_t iSize, const uint32* pValue)
{
	uint8* pBytes = (uint8*)pDestination + iSize;
	size_t iWords = (pValue[0] + 31u) >> 5u;
	size_t iWord = 1;
	uint32 iAccumulator = 0;
	int iAccumulatorBits = 0;

	if ( iWords == 0 ) {
		memset(pDestination, 0, iSize);
		return;
	}

	while ( iSize > 0 ) {
		uint32 iValue = (iWord <= iWords) ? pValue[iWord] : 0u;

		iWord++;
		if ( iAccumulatorBits == 0 ) {
			iAccumulator = iValue;
			iAccumulatorBits = 31;
		} else {
			uint32 iOutput = iAccumulator | (iValue << iAccumulatorBits);

			iAccumulatorBits--;
			iAccumulator = iValue >> (31 - iAccumulatorBits);
			if ( iSize >= 4 ) {
				pBytes -= 4;
				iSize -= 4;
				__xrtCryptoStoreBe32(pBytes, iOutput);
			} else {
				uint8* pTail = pBytes - iSize;

				for ( size_t i = 0; i < iSize; i++ ) {
					pTail[i] = (uint8)(iOutput >> ((iSize - 1u - i) * 8u));
				}
				return;
			}
		}
	}
}



/* 以 31 位字执行无符号乘法并累加到目标，用于 RSA CRT 重组。 */
void __xrtI31MultiplyAdd(
	uint32* pDestination,
	const uint32* pLeft,
	const uint32* pRight
)
{
	size_t iLeftWords = (pLeft[0] + 31u) >> 5u;
	size_t iRightWords = (pRight[0] + 31u) >> 5u;
	uint32 iLow = (pLeft[0] & 31u) + (pRight[0] & 31u);
	uint32 iHigh = (pLeft[0] >> 5u) + (pRight[0] >> 5u);

	pDestination[0] = (iHigh << 5u) + iLow +
		(~(iLow - 31u) >> 31u);
	for ( size_t i = 0; i < iRightWords; i++ ) {
		uint32 iFactor = pRight[i + 1u];
		uint64 iCarry = 0;

		for ( size_t k = 0; k < iLeftWords; k++ ) {
			uint64 iValue = (uint64)pDestination[i + k + 1u] +
				((uint64)iFactor * pLeft[k + 1u]) + iCarry;

			pDestination[i + k + 1u] =
				(uint32)iValue & XRT_I31_WORD_MASK;
			iCarry = iValue >> 31u;
		}
		pDestination[i + iLeftWords + 1u] = (uint32)iCarry;
	}
}



/* 计算模数最低字在 2^31 下的负逆元。 */
uint32 __xrtI31NegativeInverse(uint32 iValue)
{
	uint32 iInverse = 2u - iValue;

	iInverse *= 2u - (iInverse * iValue);
	iInverse *= 2u - (iInverse * iValue);
	iInverse *= 2u - (iInverse * iValue);
	iInverse *= 2u - (iInverse * iValue);
	return __xrtI31Select(iValue & 1u, 0u - iInverse, 0u) &
		XRT_I31_WORD_MASK;
}



/* 通过固定次数模加倍计算任意奇模数的 Montgomery R^2。 */
void __xrtI31MontgomerySquare(
	uint32* pSquare,
	const uint32* pModulus
)
{
	size_t iWords = (pModulus[0] + 31u) >> 5u;
	size_t iRounds = iWords * 62u;

	__xrtI31Zero(pSquare, pModulus[0]);
	pSquare[1] = 1u;
	for ( size_t i = 0; i < iRounds; i++ ) {
		uint32 iCarry = __xrtI31Add(pSquare, pSquare, 1u);
		uint32 iBorrow = __xrtI31Subtract(pSquare, pModulus, 0u);

		__xrtI31Subtract(
			pSquare,
			pModulus,
			iCarry | __xrtI31Not(iBorrow)
		);
	}
}



/* 执行 31 位字 Montgomery 模乘。 */
void __xrtI31MontgomeryMultiply(
	uint32* pDestination,
	const uint32* pLeft,
	const uint32* pRight,
	const uint32* pModulus,
	uint32 iModulusInverse
)
{
	size_t iLength = (pModulus[0] + 31u) >> 5u;
	uint32 iHigh = 0;

	__xrtI31Zero(pDestination, pModulus[0]);
	for ( size_t i = 0; i < iLength; i++ ) {
		uint32 iLeft = pLeft[i + 1u];
		uint32 iFactor = (
			(pDestination[1] + (uint32)((uint64)iLeft * pRight[1])) *
			iModulusInverse
		) & XRT_I31_WORD_MASK;
		uint64 iCarry = 0;

		for ( size_t k = 0; k < iLength; k++ ) {
			uint64 iValue = (uint64)pDestination[k + 1u] +
				((uint64)iLeft * pRight[k + 1u]) +
				((uint64)iFactor * pModulus[k + 1u]) + iCarry;

			iCarry = iValue >> 31u;
			pDestination[k] = (uint32)iValue & XRT_I31_WORD_MASK;
		}

		iHigh += (uint32)iCarry;
		pDestination[iLength] = iHigh & XRT_I31_WORD_MASK;
		iHigh >>= 31u;
	}

	pDestination[0] = pModulus[0];
	__xrtI31Subtract(
		pDestination,
		pModulus,
		__xrtI31NotEqual(iHigh, 0) |
			__xrtI31Not(__xrtI31Subtract(pDestination, pModulus, 0))
	);
}



/* 从 Montgomery 表示转换回普通模整数。 */
void __xrtI31FromMontgomery(
	uint32* pValue,
	const uint32* pModulus,
	uint32 iModulusInverse
)
{
	size_t iLength = (pModulus[0] + 31u) >> 5u;

	for ( size_t i = 0; i < iLength; i++ ) {
		uint32 iFactor = (pValue[1] * iModulusInverse) & XRT_I31_WORD_MASK;
		uint64 iCarry = 0;

		for ( size_t k = 0; k < iLength; k++ ) {
			uint64 iValue = (uint64)pValue[k + 1u] +
				((uint64)iFactor * pModulus[k + 1u]) + iCarry;

			iCarry = iValue >> 31u;
			if ( k != 0 ) {
				pValue[k] = (uint32)iValue & XRT_I31_WORD_MASK;
			}
		}
		pValue[iLength] = (uint32)iCarry;
	}

	__xrtI31Subtract(
		pValue,
		pModulus,
		__xrtI31Not(__xrtI31Subtract(pValue, pModulus, 0))
	);
}



/* 使用预计算 R^2 执行固定轨迹模幂。 */
void __xrtI31ModPower(
	uint32* pValue,
	const uint8* pExponent,
	size_t iExponentSize,
	const uint32* pModulus,
	const uint32* pMontgomerySquare,
	uint32 iModulusInverse,
	uint32* pTemporaryLeft,
	uint32* pTemporaryRight
)
{
	size_t iIntegerSize = ((pModulus[0] + 63u) >> 5u) * sizeof(uint32);
	uint32 iBits = (uint32)iExponentSize << 3u;

	__xrtI31MontgomeryMultiply(
		pTemporaryLeft,
		pValue,
		pMontgomerySquare,
		pModulus,
		iModulusInverse
	);
	__xrtI31Zero(pValue, pModulus[0]);
	pValue[1] = 1;

	for ( uint32 i = 0; i < iBits; i++ ) {
		uint32 iControl =
			(pExponent[iExponentSize - 1u - (i >> 3u)] >> (i & 7u)) & 1u;

		__xrtI31MontgomeryMultiply(
			pTemporaryRight,
			pValue,
			pTemporaryLeft,
			pModulus,
			iModulusInverse
		);
		__xrtI31Copy(iControl, pValue, pTemporaryRight, iIntegerSize);
		__xrtI31MontgomeryMultiply(
			pTemporaryRight,
			pTemporaryLeft,
			pTemporaryLeft,
			pModulus,
			iModulusInverse
		);
		memcpy(pTemporaryLeft, pTemporaryRight, iIntegerSize);
	}
}

#endif
