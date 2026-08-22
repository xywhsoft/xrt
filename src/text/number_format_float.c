#include "../internal/xrt_number_format.h"



#if defined(XRT_FEATURE_NUMBER_FORMAT)

#define XRT_NUMBER_FORMAT_BIG_LIMBS 80u
#define XRT_NUMBER_FORMAT_DECIMAL_CHUNKS 96u



/* 精确十进制保存全部有效数字和小数点前的有效位置。 */
typedef struct xrt_number_decimal {
	char Digits[XRT_NUMBER_FORMAT_CORE_CAPACITY];
	size_t Size;
	int32 Point;
} xrt_number_decimal;



/* 固定容量大整数只服务于 IEEE-754 double 的精确十进制展开。 */
typedef struct xrt_number_format_big {
	uint32 Limb[XRT_NUMBER_FORMAT_BIG_LIMBS];
	size_t Size;
} xrt_number_format_big;



/* 从 double 读取稳定的 IEEE-754 位模式。 */
static uint64 __xrtNumberFormatBits(double fValue)
{
	uint64 iBits;

	memcpy(&iBits, &fValue, sizeof(iBits));
	return iBits;
}



/* 用无符号幅值初始化固定容量大整数。 */
static void __xrtNumberFormatBigSet(xrt_number_format_big* pBig,
	uint64 iValue)
{
	memset(pBig, 0, sizeof(*pBig));
	pBig->Limb[0] = (uint32)iValue;
	pBig->Limb[1] = (uint32)(iValue >> 32u);
	pBig->Size = pBig->Limb[1] != 0 ? 2u : 1u;
}



/* 大整数乘以一个 32 位小整数。 */
static bool __xrtNumberFormatBigMultiply(xrt_number_format_big* pBig,
	uint32 iValue)
{
	uint64 iCarry = 0;

	for ( size_t i = 0; i < pBig->Size; i++ ) {
		uint64 iProduct = ((uint64)pBig->Limb[i] * iValue) + iCarry;

		pBig->Limb[i] = (uint32)iProduct;
		iCarry = iProduct >> 32u;
	}
	if ( iCarry != 0 ) {
		if ( pBig->Size == XRT_NUMBER_FORMAT_BIG_LIMBS ) {
			return false;
		}
		pBig->Limb[pBig->Size++] = (uint32)iCarry;
	}
	return true;
}



/* 大整数左移任意位数。 */
static bool __xrtNumberFormatBigShift(xrt_number_format_big* pBig,
	uint32 iBits)
{
	size_t iWords = iBits / 32u;
	uint32 iRest = iBits % 32u;
	size_t iOldSize = pBig->Size;
	uint64 iCarry = 0;

	if ( (iOldSize + iWords + (iRest != 0 ? 1u : 0u)) >
		XRT_NUMBER_FORMAT_BIG_LIMBS ) {
		return false;
	}
	for ( size_t i = iOldSize; i != 0; i-- ) {
		pBig->Limb[i - 1u + iWords] = pBig->Limb[i - 1u];
	}
	if ( iWords != 0 ) {
		memset(pBig->Limb, 0, iWords * sizeof(pBig->Limb[0]));
	}
	pBig->Size = iOldSize + iWords;
	if ( iRest != 0 ) {
		for ( size_t i = iWords; i < pBig->Size; i++ ) {
			uint64 iValue = ((uint64)pBig->Limb[i] << iRest) | iCarry;

			pBig->Limb[i] = (uint32)iValue;
			iCarry = iValue >> 32u;
		}
		if ( iCarry != 0 ) {
			pBig->Limb[pBig->Size++] = (uint32)iCarry;
		}
	}
	return true;
}



/* 大整数除以十亿并返回余数。 */
static uint32 __xrtNumberFormatBigDivide(xrt_number_format_big* pBig)
{
	uint64 iRemainder = 0;

	for ( size_t i = pBig->Size; i != 0; i-- ) {
		uint64 iValue =
			(iRemainder << 32u) | pBig->Limb[i - 1u];

		pBig->Limb[i - 1u] =
			(uint32)(iValue / UINT64_C(1000000000));
		iRemainder = iValue % UINT64_C(1000000000);
	}
	while ( (pBig->Size > 1u) &&
		(pBig->Limb[pBig->Size - 1u] == 0) ) {
		pBig->Size--;
	}
	return (uint32)iRemainder;
}



/* 写出不带前导零的 32 位十进制块。 */
static size_t __xrtNumberFormatBlock(uint32 iValue, char* sOutput)
{
	char sReverse[10];
	size_t iSize = 0;

	do {
		sReverse[iSize++] = (char)('0' + (iValue % 10u));
		iValue /= 10u;
	} while ( iValue != 0 );
	for ( size_t i = 0; i < iSize; i++ ) {
		sOutput[i] = sReverse[iSize - i - 1u];
	}
	return iSize;
}



/* 把固定容量大整数展开成无前导零十进制数字。 */
static bool __xrtNumberFormatBigDecimal(xrt_number_format_big Big,
	xrt_number_decimal* pDecimal)
{
	uint32 arrChunk[XRT_NUMBER_FORMAT_DECIMAL_CHUNKS];
	size_t iChunkCount = 0;
	size_t iWrite;

	while ( (Big.Size != 1u) || (Big.Limb[0] != 0) ) {
		if ( iChunkCount == XRT_NUMBER_FORMAT_DECIMAL_CHUNKS ) {
			return false;
		}
		arrChunk[iChunkCount++] = __xrtNumberFormatBigDivide(&Big);
	}
	if ( iChunkCount == 0 ) {
		pDecimal->Digits[0] = '0';
		pDecimal->Size = 1;
		return true;
	}
	iWrite = __xrtNumberFormatBlock(
		arrChunk[iChunkCount - 1u], pDecimal->Digits);
	for ( size_t i = iChunkCount - 1u; i != 0; i-- ) {
		uint32 iValue = arrChunk[i - 1u];

		for ( size_t j = 0; j < 9u; j++ ) {
			pDecimal->Digits[iWrite + 8u - j] =
				(char)('0' + (iValue % 10u));
			iValue /= 10u;
		}
		iWrite += 9u;
	}
	pDecimal->Size = iWrite;
	return true;
}



/* 精确展开一个有限 double 的绝对值。 */
static bool __xrtNumberFormatExact(double fValue,
	xrt_number_decimal* pDecimal)
{
	uint64 iBits = __xrtNumberFormatBits(fValue);
	uint32 iRawExponent =
		(uint32)((iBits >> 52u) & UINT64_C(0x7FF));
	uint64 iMantissa = iBits & UINT64_C(0x000FFFFFFFFFFFFF);
	int32 iExponent;
	int32 iScale = 0;
	xrt_number_format_big Big;

	if ( (iBits << 1u) == 0 ) {
		pDecimal->Digits[0] = '0';
		pDecimal->Size = 1;
		pDecimal->Point = 1;
		return true;
	}
	if ( iRawExponent == 0 ) {
		iExponent = -1074;
	} else {
		iMantissa |= UINT64_C(1) << 52u;
		iExponent = (int32)iRawExponent - 1023 - 52;
	}
	__xrtNumberFormatBigSet(&Big, iMantissa);
	if ( iExponent >= 0 ) {
		if ( !__xrtNumberFormatBigShift(&Big, (uint32)iExponent) ) {
			__xrtNumberError(XERR_INTERNAL, XNUMBER_ERROR_RANGE,
				"num-format", "exact decimal shift workspace is too small");
			return false;
		}
	} else {
		iScale = -iExponent;
		for ( int32 i = 0; i < iScale; i++ ) {
			if ( !__xrtNumberFormatBigMultiply(&Big, 5u) ) {
				__xrtNumberError(XERR_INTERNAL, XNUMBER_ERROR_RANGE,
					"num-format",
					"exact decimal multiply workspace is too small");
				return false;
			}
		}
	}
	if ( !__xrtNumberFormatBigDecimal(Big, pDecimal) ) {
		__xrtNumberError(XERR_INTERNAL, XNUMBER_ERROR_RANGE,
			"num-format", "exact decimal digit workspace is too small");
		return false;
	}
	while ( (iScale > 0) && (pDecimal->Size > 1u) &&
		(pDecimal->Digits[pDecimal->Size - 1u] == '0') ) {
		pDecimal->Size--;
		iScale--;
	}
	pDecimal->Point = (int32)pDecimal->Size - iScale;
	return true;
}



/* 判断被舍弃十进制部分是否应按最近偶数规则向上舍入。 */
static bool __xrtNumberFormatRoundUp(const xrt_number_decimal* pDecimal,
	size_t iDiscard, char iRetained)
{
	char iFirst = pDecimal->Digits[iDiscard];

	if ( iFirst > '5' ) {
		return true;
	}
	if ( iFirst < '5' ) {
		return false;
	}
	for ( size_t i = iDiscard + 1u; i < pDecimal->Size; i++ ) {
		if ( pDecimal->Digits[i] != '0' ) {
			return true;
		}
	}
	return ((iRetained - '0') & 1) != 0;
}



/* 把精确十进制舍入为小数点移动后的无符号整数。 */
static bool __xrtNumberFormatRound(const xrt_number_decimal* pDecimal,
	int64 iKeep, char* sOutput, size_t* pSize)
{
	size_t iSize;
	bool bRound = false;

	if ( iKeep > (int64)(XRT_NUMBER_FORMAT_CORE_CAPACITY - 1u) ) {
		return false;
	}
	if ( iKeep <= 0 ) {
		sOutput[0] = '0';
		iSize = 1;
		if ( iKeep == 0 ) {
			bRound = __xrtNumberFormatRoundUp(pDecimal, 0, '0');
		}
	} else {
		size_t iTake = (size_t)iKeep < pDecimal->Size ?
			(size_t)iKeep : pDecimal->Size;

		memcpy(sOutput, pDecimal->Digits, iTake);
		if ( (size_t)iKeep > iTake ) {
			memset(sOutput + iTake, '0', (size_t)iKeep - iTake);
		}
		iSize = (size_t)iKeep;
		if ( (size_t)iKeep < pDecimal->Size ) {
			bRound = __xrtNumberFormatRoundUp(
				pDecimal, (size_t)iKeep, sOutput[iSize - 1u]);
		}
	}
	if ( bRound ) {
		size_t i = iSize;

		while ( (i != 0) && (sOutput[i - 1u] == '9') ) {
			sOutput[--i] = '0';
		}
		if ( i == 0 ) {
			memmove(sOutput + 1u, sOutput, iSize);
			sOutput[0] = '1';
			iSize++;
		} else {
			sOutput[i - 1u]++;
		}
	}
	*pSize = iSize;
	return true;
}



/* 按固定小数位数构造不带符号的核心文本。 */
static bool __xrtNumberFormatFixed(
	const xrt_number_decimal* pDecimal,
	size_t iPrecision,
	bool bAlternate,
	char* sOutput,
	size_t* pSize
)
{
	char sUnits[XRT_NUMBER_FORMAT_CORE_CAPACITY];
	int64 iKeep = (int64)pDecimal->Point + (int64)iPrecision;
	size_t iUnitSize;
	size_t iWrite = 0;

	if ( !__xrtNumberFormatRound(
			pDecimal, iKeep, sUnits, &iUnitSize) ) {
		return false;
	}
	if ( iPrecision == 0 ) {
		memcpy(sOutput, sUnits, iUnitSize);
		iWrite = iUnitSize;
		if ( bAlternate ) {
			sOutput[iWrite++] = '.';
		}
	} else if ( iUnitSize > iPrecision ) {
		size_t iInteger = iUnitSize - iPrecision;

		memcpy(sOutput, sUnits, iInteger);
		iWrite = iInteger;
		sOutput[iWrite++] = '.';
		memcpy(sOutput + iWrite, sUnits + iInteger, iPrecision);
		iWrite += iPrecision;
	} else {
		sOutput[iWrite++] = '0';
		sOutput[iWrite++] = '.';
		memset(sOutput + iWrite, '0', iPrecision - iUnitSize);
		iWrite += iPrecision - iUnitSize;
		memcpy(sOutput + iWrite, sUnits, iUnitSize);
		iWrite += iUnitSize;
	}
	*pSize = iWrite;
	return true;
}



/* 舍入为指定数量的十进制有效数字并返回科学指数。 */
static bool __xrtNumberFormatSignificant(
	const xrt_number_decimal* pDecimal,
	size_t iPrecision,
	char* sDigits,
	int32* pExponent
)
{
	size_t iSize;

	if ( !__xrtNumberFormatRound(
			pDecimal, (int64)iPrecision, sDigits, &iSize) ) {
		return false;
	}
	*pExponent = pDecimal->Point - 1;
	if ( iSize > iPrecision ) {
		(*pExponent)++;
		iSize = iPrecision;
	}
	if ( iSize < iPrecision ) {
		memset(sDigits + iSize, '0', iPrecision - iSize);
	}
	return true;
}



/* 写出带符号且至少两位的科学指数。 */
static size_t __xrtNumberFormatExponent(
	int32 iExponent, bool bUpper, char* sOutput)
{
	char sReverse[12];
	uint32 iMagnitude = iExponent < 0 ?
		(uint32)(-(iExponent + 1)) + 1u : (uint32)iExponent;
	size_t iSize = 0;
	size_t iWrite = 0;

	do {
		sReverse[iSize++] = (char)('0' + (iMagnitude % 10u));
		iMagnitude /= 10u;
	} while ( iMagnitude != 0 );
	sOutput[iWrite++] = bUpper ? 'E' : 'e';
	sOutput[iWrite++] = iExponent < 0 ? '-' : '+';
	if ( iSize < 2u ) {
		sOutput[iWrite++] = '0';
	}
	for ( size_t i = iSize; i != 0; i-- ) {
		sOutput[iWrite++] = sReverse[i - 1u];
	}
	return iWrite;
}



/* 从舍入后的有效数字构造科学计数核心文本。 */
static size_t __xrtNumberFormatScientificDigits(
	const char* sDigits,
	size_t iPrecision,
	int32 iExponent,
	bool bUpper,
	bool bAlternate,
	char* sOutput
)
{
	size_t iWrite = 0;

	sOutput[iWrite++] = sDigits[0];
	if ( (iPrecision > 1u) || bAlternate ) {
		sOutput[iWrite++] = '.';
		if ( iPrecision > 1u ) {
			memcpy(sOutput + iWrite, sDigits + 1u, iPrecision - 1u);
			iWrite += iPrecision - 1u;
		}
	}
	iWrite += __xrtNumberFormatExponent(
		iExponent, bUpper, sOutput + iWrite);
	return iWrite;
}



/* 按科学计数法构造不带符号的核心文本。 */
static bool __xrtNumberFormatScientific(
	const xrt_number_decimal* pDecimal,
	size_t iPrecision,
	bool bUpper,
	bool bAlternate,
	char* sOutput,
	size_t* pSize
)
{
	char sDigits[XRT_NUMBER_FORMAT_PRECISION_MAX + 2u];
	int32 iExponent;
	size_t iSignificant = iPrecision + 1u;

	if ( !__xrtNumberFormatSignificant(
			pDecimal, iSignificant, sDigits, &iExponent) ) {
		return false;
	}
	*pSize = __xrtNumberFormatScientificDigits(
		sDigits, iSignificant, iExponent,
		bUpper, bAlternate, sOutput);
	return true;
}



/* 删除一般格式科学计数尾部无意义的零。 */
static size_t __xrtNumberFormatTrimScientific(char* sText, size_t iSize)
{
	size_t iExponent = 0;
	size_t iEnd;

	while ( (iExponent < iSize) &&
		(sText[iExponent] != 'e') && (sText[iExponent] != 'E') ) {
		iExponent++;
	}
	iEnd = iExponent;
	while ( (iEnd != 0) && (sText[iEnd - 1u] == '0') ) {
		iEnd--;
	}
	if ( (iEnd != 0) && (sText[iEnd - 1u] == '.') ) {
		iEnd--;
	}
	memmove(sText + iEnd, sText + iExponent, iSize - iExponent);
	return iEnd + (iSize - iExponent);
}



/* 删除一般格式固定小数尾部无意义的零。 */
static size_t __xrtNumberFormatTrimFixed(char* sText, size_t iSize)
{
	size_t iDot = 0;

	while ( (iDot < iSize) && (sText[iDot] != '.') ) {
		iDot++;
	}
	if ( iDot == iSize ) {
		return iSize;
	}
	while ( (iSize > (iDot + 1u)) && (sText[iSize - 1u] == '0') ) {
		iSize--;
	}
	if ( iSize == (iDot + 1u) ) {
		iSize = iDot;
	}
	return iSize;
}



/* 从舍入后的有效数字构造一般格式的固定计数形式。 */
static size_t __xrtNumberFormatGeneralFixed(
	const char* sDigits,
	size_t iPrecision,
	int32 iExponent,
	bool bAlternate,
	char* sOutput
)
{
	int32 iPoint = iExponent + 1;
	size_t iWrite = 0;

	if ( iPoint <= 0 ) {
		sOutput[iWrite++] = '0';
		sOutput[iWrite++] = '.';
		memset(sOutput + iWrite, '0', (size_t)-iPoint);
		iWrite += (size_t)-iPoint;
		memcpy(sOutput + iWrite, sDigits, iPrecision);
		iWrite += iPrecision;
	} else if ( (size_t)iPoint >= iPrecision ) {
		memcpy(sOutput, sDigits, iPrecision);
		iWrite = iPrecision;
		memset(sOutput + iWrite, '0', (size_t)iPoint - iPrecision);
		iWrite += (size_t)iPoint - iPrecision;
		if ( bAlternate ) {
			sOutput[iWrite++] = '.';
		}
	} else {
		memcpy(sOutput, sDigits, (size_t)iPoint);
		iWrite = (size_t)iPoint;
		sOutput[iWrite++] = '.';
		memcpy(sOutput + iWrite, sDigits + iPoint,
			iPrecision - (size_t)iPoint);
		iWrite += iPrecision - (size_t)iPoint;
	}
	return bAlternate ? iWrite :
		__xrtNumberFormatTrimFixed(sOutput, iWrite);
}



/* 按一般格式构造固定或科学计数核心文本。 */
static bool __xrtNumberFormatGeneral(
	const xrt_number_decimal* pDecimal,
	size_t iPrecision,
	bool bUpper,
	bool bAlternate,
	char* sOutput,
	size_t* pSize
)
{
	char sDigits[XRT_NUMBER_FORMAT_PRECISION_MAX + 1u];
	int32 iExponent;

	if ( iPrecision == 0 ) {
		iPrecision = 1;
	}
	if ( !__xrtNumberFormatSignificant(
			pDecimal, iPrecision, sDigits, &iExponent) ) {
		return false;
	}
	if ( (iExponent < -4) || (iExponent >= (int32)iPrecision) ) {
		*pSize = __xrtNumberFormatScientificDigits(
			sDigits, iPrecision, iExponent,
			bUpper, bAlternate, sOutput);
		if ( !bAlternate ) {
			*pSize = __xrtNumberFormatTrimScientific(sOutput, *pSize);
		}
	} else {
		*pSize = __xrtNumberFormatGeneralFixed(
			sDigits, iPrecision, iExponent, bAlternate, sOutput);
	}
	return true;
}



/* 为最短往返文本补入可选小数点并统一指数大小写。 */
static size_t __xrtNumberFormatShortestCore(
	double fValue,
	bool bAlternate,
	bool bUpper,
	char* sOutput,
	bool* pNegative,
	bool* pSpecial
)
{
	char sRaw[48];
	size_t iRawSize = 0;
	size_t iStart = 0;
	size_t iExponent = XRT_NPOS;
	bool bDot = false;

	(void)xrtNumWrite(fValue, sRaw, sizeof(sRaw), &iRawSize, 0);
	*pNegative = (__xrtNumberFormatBits(fValue) >> 63u) != 0;
	if ( (iRawSize != 0) && (sRaw[0] == '-') ) {
		iStart = 1;
	}
	*pSpecial = ((iRawSize - iStart) == 3u) &&
		((memcmp(sRaw + iStart, "inf", 3) == 0) ||
		 (memcmp(sRaw + iStart, "nan", 3) == 0));
	if ( *pSpecial && (memcmp(sRaw + iStart, "nan", 3) == 0) ) {
		*pNegative = false;
	}
	for ( size_t i = iStart; i < iRawSize; i++ ) {
		if ( sRaw[i] == '.' ) {
			bDot = true;
		} else if ( (sRaw[i] == 'e') || (sRaw[i] == 'E') ) {
			iExponent = i;
			break;
		}
	}
	if ( *pSpecial && bUpper ) {
		for ( size_t i = iStart; i < iRawSize; i++ ) {
			sOutput[i - iStart] =
				(char)(sRaw[i] - ('a' - 'A'));
		}
		return iRawSize - iStart;
	}
	if ( bUpper && (iExponent != XRT_NPOS) ) {
		sRaw[iExponent] = 'E';
	}
	if ( bAlternate && !*pSpecial && !bDot ) {
		size_t iInsert = iExponent == XRT_NPOS ? iRawSize : iExponent;
		size_t iPrefix = iInsert - iStart;

		memcpy(sOutput, sRaw + iStart, iPrefix);
		sOutput[iPrefix] = '.';
		memcpy(sOutput + iPrefix + 1u, sRaw + iInsert,
			iRawSize - iInsert);
		return (iRawSize - iStart) + 1u;
	}
	memcpy(sOutput, sRaw + iStart, iRawSize - iStart);
	return iRawSize - iStart;
}



/* 构造已经去掉符号的浮点核心文本。 */
bool __xrtNumberFormatFloatCore(
	double fValue,
	const xrt_number_format_options* pOptions,
	char* sCore,
	size_t* pCoreSize,
	bool* pNegative,
	bool* pSpecial
)
{
	xrt_number_decimal Decimal;
	uint64 iBits = __xrtNumberFormatBits(fValue);
	uint32 iRawExponent =
		(uint32)((iBits >> 52u) & UINT64_C(0x7FF));
	uint64 iRawMantissa = iBits & UINT64_C(0x000FFFFFFFFFFFFF);
	size_t iPrecision;
	bool bUpper = (pOptions->Type == 'F') ||
		(pOptions->Type == 'E') || (pOptions->Type == 'G');

	*pNegative = (iBits >> 63u) != 0;
	*pSpecial = iRawExponent == 0x7FFu;
	if ( *pSpecial && (iRawMantissa != 0) ) {
		*pNegative = false;
	}
	if ( *pSpecial ) {
		const char* sSpecial = iRawMantissa != 0 ? "nan" : "inf";

		memcpy(sCore, sSpecial, 3);
		*pCoreSize = 3;
		if ( bUpper ) {
			for ( size_t i = 0; i < *pCoreSize; i++ ) {
				sCore[i] = (char)(sCore[i] - ('a' - 'A'));
			}
		}
		if ( pOptions->Type == '%' ) {
			sCore[(*pCoreSize)++] = '%';
		}
		return true;
	}
	if ( (pOptions->Type == 0) && !pOptions->HasPrecision ) {
		*pCoreSize = __xrtNumberFormatShortestCore(
			fValue, pOptions->Alternate, false,
			sCore, pNegative, pSpecial);
		return true;
	}

	iBits &= UINT64_C(0x7FFFFFFFFFFFFFFF);
	memcpy(&fValue, &iBits, sizeof(fValue));
	if ( !__xrtNumberFormatExact(fValue, &Decimal) ) {
		return false;
	}
	iPrecision = pOptions->HasPrecision ? pOptions->Precision : 6u;
	if ( (pOptions->Type == 'f') || (pOptions->Type == 'F') ) {
		return __xrtNumberFormatFixed(
			&Decimal, iPrecision, pOptions->Alternate,
			sCore, pCoreSize);
	}
	if ( (pOptions->Type == 'e') || (pOptions->Type == 'E') ) {
		return __xrtNumberFormatScientific(
			&Decimal, iPrecision, bUpper, pOptions->Alternate,
			sCore, pCoreSize);
	}
	if ( pOptions->Type == '%' ) {
		Decimal.Point += 2;
		if ( !__xrtNumberFormatFixed(
				&Decimal, iPrecision, pOptions->Alternate,
				sCore, pCoreSize) ) {
			return false;
		}
		sCore[(*pCoreSize)++] = '%';
		return true;
	}
	return __xrtNumberFormatGeneral(
		&Decimal, iPrecision, bUpper, pOptions->Alternate,
		sCore, pCoreSize);
}

#endif
