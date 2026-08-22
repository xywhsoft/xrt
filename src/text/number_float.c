#include "../internal/xrt_number.h"



#if defined(XRT_FEATURE_NUMBER_FLOAT)

#define XRT_NUMBER_FLOAT_FLAGS \
	((uint32)XNUMBER_FLOAT_COMPACT)
#define XRT_NUMBER_FLOAT_PARSE_FLAGS \
	((uint32)XNUMBER_PARSE_SPACE | (uint32)XNUMBER_PARSE_SEPARATOR | \
	(uint32)XNUMBER_PARSE_SPECIAL)
#define XRT_NUMBER_FLOAT_MAX_DIGITS	769u



/* 浮点扫描只保存正确舍入所需的高 769 位，其他低位折叠为 sticky 位。 */
typedef struct xrt_number_float_scan {
	uint8 Digits[XRT_NUMBER_FLOAT_MAX_DIGITS];
	size_t TotalDigits;
	size_t FractionDigits;
	size_t FirstNonzero;
	size_t LastNonzero;
	uint32 StoredDigits;
	bool HasNonzero;
} xrt_number_float_scan;



/* 指数使用符号和无符号幅值表示，避免解析 INT64_MIN 一类边界。 */
typedef struct xrt_number_float_exponent {
	uint64 Magnitude;
	bool Negative;
	bool Overflow;
} xrt_number_float_exponent;



/* 判断一个字节是否为十进制数字。 */
static bool __xrtNumberFloatDigit(uint8 iByte)
{
	return (iByte >= (uint8)'0') && (iByte <= (uint8)'9');
}



/* 把 ASCII 字母折叠为小写，非字母保持不变。 */
static uint8 __xrtNumberFloatLower(uint8 iByte)
{
	if ( (iByte >= (uint8)'A') && (iByte <= (uint8)'Z') ) {
		return iByte + ((uint8)'a' - (uint8)'A');
	}
	return iByte;
}



/* 按显式长度比较不区分 ASCII 大小写的字面量。 */
static bool __xrtNumberFloatLiteral(
	xstrview Text,
	cstr sLiteral,
	size_t iLiteralSize
)
{
	if ( Text.Size != iLiteralSize ) {
		return false;
	}
	for ( size_t i = 0; i < iLiteralSize; i++ ) {
		if ( __xrtNumberFloatLower((uint8)Text.Data[i]) !=
			(uint8)sLiteral[i] ) {
			return false;
		}
	}
	return true;
}



/* 统一报告浮点文本格式错误。 */
static bool __xrtNumberFloatFormatError(cstr sMessage)
{
	__xrtNumberError(
		XERR_VALUE,
		XNUMBER_ERROR_FORMAT,
		"num-parse",
		sMessage
	);
	return false;
}



/* 校验标志并按需裁掉输入两端的 ASCII 空白。 */
static bool __xrtNumberFloatTrim(
	xstrview Text,
	uint32 iFlags,
	xstrview* pTrimmed
)
{
	size_t iStart = 0;
	size_t iEnd = Text.Size;

	if ( (Text.Data == NULL) && (Text.Size != 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (iFlags & ~XRT_NUMBER_FLOAT_PARSE_FLAGS) != 0 ) {
		__xrtNumberError(
			XERR_VALUE,
			XNUMBER_ERROR_CONFIG,
			"num-parse",
			"invalid floating-point parse flags"
		);
		return false;
	}
	if ( (iFlags & (uint32)XNUMBER_PARSE_SPACE) != 0 ) {
		while ( (iStart < iEnd) &&
			__xrtNumberAsciiSpace((uint8)Text.Data[iStart]) ) {
			iStart++;
		}
		while ( (iEnd > iStart) &&
			__xrtNumberAsciiSpace((uint8)Text.Data[iEnd - 1u]) ) {
			iEnd--;
		}
	}
	pTrimmed->Data = (Text.Data == NULL) ? NULL : Text.Data + iStart;
	pTrimmed->Size = iEnd - iStart;
	return true;
}



/* 记录尾数中的一个数字，同时维护首尾非零位置和有限高位缓存。 */
static void __xrtNumberFloatRecord(
	xrt_number_float_scan* pScan,
	uint8 iDigit,
	bool bFraction
)
{
	size_t iPosition = pScan->TotalDigits;

	if ( iDigit != (uint8)'0' ) {
		if ( !pScan->HasNonzero ) {
			pScan->HasNonzero = true;
			pScan->FirstNonzero = iPosition;
		}
		pScan->LastNonzero = iPosition;
	}
	if ( pScan->HasNonzero &&
		(pScan->StoredDigits < XRT_NUMBER_FLOAT_MAX_DIGITS) ) {
		pScan->Digits[pScan->StoredDigits++] = iDigit;
	}
	pScan->TotalDigits++;
	if ( bFraction ) {
		pScan->FractionDigits++;
	}
}



/*
	读取尾数的一段十进制数字。
	下划线只允许出现在两个数字之间，因此不会形成隐式 token 边界。
*/
static bool __xrtNumberFloatMantissaSequence(
	xstrview Text,
	size_t* pPosition,
	uint32 iFlags,
	bool bFraction,
	xrt_number_float_scan* pScan,
	size_t* pDigitCount
)
{
	size_t iPosition = *pPosition;
	size_t iCount = 0;

	while ( iPosition < Text.Size ) {
		uint8 iByte = (uint8)Text.Data[iPosition];

		if ( __xrtNumberFloatDigit(iByte) ) {
			__xrtNumberFloatRecord(pScan, iByte, bFraction);
			iCount++;
			iPosition++;
			continue;
		}
		if ( iByte == (uint8)'_' ) {
			if ( ((iFlags & (uint32)XNUMBER_PARSE_SEPARATOR) == 0) ||
				(iCount == 0) ||
				((iPosition + 1u) >= Text.Size) ||
				!__xrtNumberFloatDigit(
					(uint8)Text.Data[iPosition + 1u]) ) {
				return false;
			}
			iPosition++;
			continue;
		}
		break;
	}
	*pPosition = iPosition;
	*pDigitCount = iCount;
	return true;
}



/* 读取科学计数法指数并检测任意长度的十进制溢出。 */
static bool __xrtNumberFloatExponentSequence(
	xstrview Text,
	size_t* pPosition,
	uint32 iFlags,
	xrt_number_float_exponent* pExponent
)
{
	size_t iPosition = *pPosition;
	size_t iDigits = 0;

	memset(pExponent, 0, sizeof(*pExponent));
	if ( iPosition < Text.Size ) {
		uint8 iByte = (uint8)Text.Data[iPosition];

		if ( (iByte == (uint8)'+') || (iByte == (uint8)'-') ) {
			pExponent->Negative = iByte == (uint8)'-';
			iPosition++;
		}
	}
	while ( iPosition < Text.Size ) {
		uint8 iByte = (uint8)Text.Data[iPosition];

		if ( __xrtNumberFloatDigit(iByte) ) {
			uint64 iDigit = (uint64)(iByte - (uint8)'0');

			if ( !pExponent->Overflow ) {
				if ( (pExponent->Magnitude >
					(UINT64_MAX / UINT64_C(10))) ||
					((pExponent->Magnitude ==
					(UINT64_MAX / UINT64_C(10))) &&
					(iDigit > (UINT64_MAX % UINT64_C(10)))) ) {
					pExponent->Overflow = true;
				} else {
					pExponent->Magnitude =
						(pExponent->Magnitude * UINT64_C(10)) +
						iDigit;
				}
			}
			iDigits++;
			iPosition++;
			continue;
		}
		if ( iByte == (uint8)'_' ) {
			if ( ((iFlags & (uint32)XNUMBER_PARSE_SEPARATOR) == 0) ||
				(iDigits == 0) ||
				((iPosition + 1u) >= Text.Size) ||
				!__xrtNumberFloatDigit(
					(uint8)Text.Data[iPosition + 1u]) ) {
				return false;
			}
			iPosition++;
			continue;
		}
		break;
	}
	if ( iDigits == 0 ) {
		return false;
	}
	*pPosition = iPosition;
	return true;
}



/* 合并两个带符号幅值；同号加法溢出时保留最终符号。 */
static xrt_number_float_exponent __xrtNumberFloatExponentAdd(
	xrt_number_float_exponent Left,
	xrt_number_float_exponent Right
)
{
	xrt_number_float_exponent Result;

	if ( Left.Overflow ) {
		return Left;
	}
	if ( Right.Overflow ) {
		return Right;
	}
	if ( Left.Negative == Right.Negative ) {
		Result.Negative = Left.Negative;
		Result.Overflow =
			Left.Magnitude > (UINT64_MAX - Right.Magnitude);
		Result.Magnitude = Result.Overflow ?
			UINT64_MAX : Left.Magnitude + Right.Magnitude;
		return Result;
	}
	Result.Overflow = false;
	if ( Left.Magnitude >= Right.Magnitude ) {
		Result.Negative = Left.Negative;
		Result.Magnitude = Left.Magnitude - Right.Magnitude;
	} else {
		Result.Negative = Right.Negative;
		Result.Magnitude = Right.Magnitude - Left.Magnitude;
	}
	if ( Result.Magnitude == 0 ) {
		Result.Negative = false;
	}
	return Result;
}



/* 把两个 size_t 计数之差转换为指数的符号幅值。 */
static xrt_number_float_exponent __xrtNumberFloatCountDifference(
	size_t iPositive,
	size_t iNegative
)
{
	xrt_number_float_exponent Result;

	Result.Overflow = false;
	if ( iPositive >= iNegative ) {
		Result.Negative = false;
		Result.Magnitude = (uint64)(iPositive - iNegative);
	} else {
		Result.Negative = true;
		Result.Magnitude = (uint64)(iNegative - iPositive);
	}
	return Result;
}



/* 把扫描结果归一化为数值内核所需的有效数和两个十进制指数。 */
static bool __xrtNumberFloatNormalize(
	xrt_number_float_scan* pScan,
	xrt_number_float_exponent LiteralExponent,
	uint64* pSignificand,
	int32* pSignificandExponent,
	uint32* pDigitCount,
	int32* pDigitExponent
)
{
	size_t iSignificantCount =
		(pScan->LastNonzero - pScan->FirstNonzero) + 1u;
	size_t iTrailingZeros =
		(pScan->TotalDigits - pScan->LastNonzero) - 1u;
	size_t iHighScale =
		(iSignificantCount > 19u) ?
		(iSignificantCount - 19u) : 0;
	xrt_number_float_exponent CountExponent;
	xrt_number_float_exponent Combined;
	uint32 iRetained = (iSignificantCount >
		XRT_NUMBER_FLOAT_MAX_DIGITS) ?
		XRT_NUMBER_FLOAT_MAX_DIGITS :
		(uint32)iSignificantCount;
	uint32 iHighDigits =
		(iSignificantCount > 19u) ? 19u : (uint32)iSignificantCount;
	uint64 iSignificand = 0;
	int32 iExponent;

	/*
		尾随零和高位截断尺度都属于正指数，小数位数属于负指数；
		三者的正计数总和不超过尾数实际长度。
	*/
	CountExponent = __xrtNumberFloatCountDifference(
		iTrailingZeros + iHighScale,
		pScan->FractionDigits
	);
	Combined = __xrtNumberFloatExponentAdd(
		LiteralExponent, CountExponent);
	if ( Combined.Overflow ) {
		if ( Combined.Negative ) {
			*pSignificandExponent = -344;
			return true;
		}
		__xrtNumberError(
			XERR_RANGE,
			XNUMBER_ERROR_RANGE,
			"num-parse",
			"floating-point value overflows double"
		);
		return false;
	}
	if ( Combined.Negative ) {
		if ( Combined.Magnitude > UINT64_C(343) ) {
			*pSignificandExponent = -344;
			return true;
		}
		iExponent = -(int32)Combined.Magnitude;
	} else {
		if ( Combined.Magnitude > UINT64_C(308) ) {
			__xrtNumberError(
				XERR_RANGE,
				XNUMBER_ERROR_RANGE,
				"num-parse",
				"floating-point value overflows double"
			);
			return false;
		}
		iExponent = (int32)Combined.Magnitude;
	}

	for ( uint32 i = 0; i < iHighDigits; i++ ) {
		iSignificand = (iSignificand * UINT64_C(10)) +
			(uint64)(pScan->Digits[i] - (uint8)'0');
	}
	if ( iSignificantCount > 19u ) {
		iSignificand +=
			pScan->Digits[19] >= (uint8)'5';
	}
	if ( iSignificantCount > XRT_NUMBER_FLOAT_MAX_DIGITS ) {
		pScan->Digits[XRT_NUMBER_FLOAT_MAX_DIGITS - 1u] = (uint8)'1';
	}

	*pSignificand = iSignificand;
	*pSignificandExponent = iExponent;
	*pDigitCount = iRetained;
	*pDigitExponent =
		iExponent + (int32)iHighDigits - (int32)iRetained;
	return true;
}



/* 解析显式允许的 inf、infinity 和 nan 特殊值。 */
static bool __xrtNumberFloatSpecial(
	xstrview Text,
	bool bNegative,
	uint64* pBits
)
{
	if ( __xrtNumberFloatLiteral(Text, "inf", 3) ||
		__xrtNumberFloatLiteral(Text, "infinity", 8) ) {
		*pBits = UINT64_C(0x7FF0000000000000);
	} else if ( __xrtNumberFloatLiteral(Text, "nan", 3) ) {
		*pBits = UINT64_C(0x7FF8000000000000);
	} else {
		return false;
	}
	if ( bNegative ) {
		*pBits |= UINT64_C(0x8000000000000000);
	}
	return true;
}



/* 完成浮点文本语法扫描、归一化和正确舍入转换。 */
static bool __xrtNumberFloatParseText(
	xstrview Text,
	uint32 iFlags,
	uint64* pBits
)
{
	xrt_number_float_scan Scan;
	xrt_number_float_exponent LiteralExponent;
	size_t iPosition = 0;
	size_t iIntegerDigits;
	size_t iFractionDigits;
	bool bNegative = false;
	uint64 iSignificand;
	int32 iSignificandExponent;
	uint32 iDigitCount;
	int32 iDigitExponent;
	uint64 iBits;

	memset(&Scan, 0, sizeof(Scan));
	memset(&LiteralExponent, 0, sizeof(LiteralExponent));
	if ( Text.Size == 0 ) {
		return __xrtNumberFloatFormatError(
			"floating-point text is empty");
	}
	if ( (Text.Data[iPosition] == '+') ||
		(Text.Data[iPosition] == '-') ) {
		bNegative = Text.Data[iPosition] == '-';
		iPosition++;
		if ( iPosition == Text.Size ) {
			return __xrtNumberFloatFormatError(
				"floating-point sign is not followed by a value");
		}
	}

	if ( (iFlags & (uint32)XNUMBER_PARSE_SPECIAL) != 0 ) {
		xstrview Special = {
			Text.Data + iPosition,
			Text.Size - iPosition
		};

		if ( __xrtNumberFloatSpecial(Special, bNegative, pBits) ) {
			return true;
		}
	}

	if ( !__xrtNumberFloatMantissaSequence(
		Text,
		&iPosition,
		iFlags,
		false,
		&Scan,
		&iIntegerDigits
	) ) {
		return __xrtNumberFloatFormatError(
			"invalid separator in floating-point integer part");
	}
	iFractionDigits = 0;
	if ( (iPosition < Text.Size) &&
		(Text.Data[iPosition] == '.') ) {
		iPosition++;
		if ( !__xrtNumberFloatMantissaSequence(
			Text,
			&iPosition,
			iFlags,
			true,
			&Scan,
			&iFractionDigits
		) ) {
			return __xrtNumberFloatFormatError(
				"invalid separator in floating-point fraction");
		}
	}
	if ( (iIntegerDigits == 0) && (iFractionDigits == 0) ) {
		return __xrtNumberFloatFormatError(
			"floating-point text has no decimal digit");
	}

	if ( (iPosition < Text.Size) &&
		((Text.Data[iPosition] == 'e') ||
		(Text.Data[iPosition] == 'E')) ) {
		iPosition++;
		if ( !__xrtNumberFloatExponentSequence(
			Text,
			&iPosition,
			iFlags,
			&LiteralExponent
		) ) {
			return __xrtNumberFloatFormatError(
				"floating-point exponent has no valid digit");
		}
	}
	if ( iPosition != Text.Size ) {
		return __xrtNumberFloatFormatError(
			"unexpected character after floating-point value");
	}

	if ( !Scan.HasNonzero ) {
		*pBits = bNegative ?
			UINT64_C(0x8000000000000000) : 0;
		return true;
	}
	if ( !__xrtNumberFloatNormalize(
		&Scan,
		LiteralExponent,
		&iSignificand,
		&iSignificandExponent,
		&iDigitCount,
		&iDigitExponent
	) ) {
		return false;
	}
	if ( iSignificandExponent < -343 ) {
		*pBits = bNegative ?
			UINT64_C(0x8000000000000000) : 0;
		return true;
	}
	if ( !__xrtNumberFloatConvert(
		Scan.Digits,
		iDigitCount,
		iDigitExponent,
		iSignificand,
		iSignificandExponent,
		&iBits
	) ) {
		__xrtNumberError(
			XERR_RANGE,
			XNUMBER_ERROR_RANGE,
			"num-parse",
			"floating-point value overflows double"
		);
		return false;
	}
	if ( bNegative ) {
		iBits |= UINT64_C(0x8000000000000000);
	}
	*pBits = iBits;
	return true;
}



/* 写出 double 的最短往返文本。 */
XRT_API bool xrtNumWrite(
	double fValue,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize,
	uint32 iFlags
)
{
	char sBuffer[40];
	size_t iSize;

	if ( (pOutputSize == NULL) ||
		((sOutput == NULL) && (iCapacity != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (sOutput != NULL) && __xrtRangesOverlap(
		pOutputSize, sizeof(*pOutputSize), sOutput, iCapacity) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (iFlags & ~XRT_NUMBER_FLOAT_FLAGS) != 0 ) {
		__xrtNumberError(
			XERR_VALUE,
			XNUMBER_ERROR_CONFIG,
			"num-write",
			"invalid floating-point write flags"
		);
		return false;
	}
	iSize = __xrtNumberFloatFormat(
		fValue,
		sBuffer,
		(iFlags & (uint32)XNUMBER_FLOAT_COMPACT) != 0
	);
	return __xrtNumberWriteResult(
		sBuffer, iSize, sOutput, iCapacity, pOutputSize);
}



/* 分配并写出 double 的最短往返文本。 */
XRT_API str xrtNumString(double fValue, uint32 iFlags)
{
	size_t iSize;
	str sText;

	if ( !xrtNumWrite(fValue, NULL, 0, &iSize, iFlags) ) {
		return NULL;
	}
	sText = (str)xrtMalloc(iSize + 1u);
	if ( sText == NULL ) {
		return NULL;
	}
	if ( !xrtNumWrite(
		fValue, sText, iSize + 1u, &iSize, iFlags) ) {
		xrtFree(sText);
		return NULL;
	}
	return sText;
}



/* 严格解析完整十进制浮点文本并在成功后发布结果。 */
XRT_API bool xrtNumParse(
	xstrview Text,
	uint32 iFlags,
	double* pValue
)
{
	xstrview Trimmed;
	uint64 iBits;
	double fValue;

	if ( pValue == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtNumberFloatTrim(Text, iFlags, &Trimmed) ) {
		return false;
	}
	if ( !__xrtNumberFloatParseText(Trimmed, iFlags, &iBits) ) {
		return false;
	}
	memcpy(&fValue, &iBits, sizeof(fValue));
	*pValue = fValue;
	return true;
}

#endif
