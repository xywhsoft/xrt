#include "../internal/xrt_number.h"



#if defined(XRT_FEATURE_NUMBER_INTEGER)

#define XRT_NUMBER_WRITE_FLAGS \
	((uint32)XNUMBER_UPPER | (uint32)XNUMBER_PREFIX | (uint32)XNUMBER_PLUS)
#define XRT_NUMBER_PARSE_FLAGS \
	((uint32)XNUMBER_PARSE_SPACE | (uint32)XNUMBER_PARSE_PREFIX | \
	(uint32)XNUMBER_PARSE_SEPARATOR)



/*
	两位数字表继承旧 jnum 的快速十进制思路。
	新实现只对无符号幅值运算，避免 INT64_MIN 取负产生未定义行为。
*/
static const char __xrtNumberDigits100[200] = {
	'0', '0', '0', '1', '0', '2', '0', '3', '0', '4',
	'0', '5', '0', '6', '0', '7', '0', '8', '0', '9',
	'1', '0', '1', '1', '1', '2', '1', '3', '1', '4',
	'1', '5', '1', '6', '1', '7', '1', '8', '1', '9',
	'2', '0', '2', '1', '2', '2', '2', '3', '2', '4',
	'2', '5', '2', '6', '2', '7', '2', '8', '2', '9',
	'3', '0', '3', '1', '3', '2', '3', '3', '3', '4',
	'3', '5', '3', '6', '3', '7', '3', '8', '3', '9',
	'4', '0', '4', '1', '4', '2', '4', '3', '4', '4',
	'4', '5', '4', '6', '4', '7', '4', '8', '4', '9',
	'5', '0', '5', '1', '5', '2', '5', '3', '5', '4',
	'5', '5', '5', '6', '5', '7', '5', '8', '5', '9',
	'6', '0', '6', '1', '6', '2', '6', '3', '6', '4',
	'6', '5', '6', '6', '6', '7', '6', '8', '6', '9',
	'7', '0', '7', '1', '7', '2', '7', '3', '7', '4',
	'7', '5', '7', '6', '7', '7', '7', '8', '7', '9',
	'8', '0', '8', '1', '8', '2', '8', '3', '8', '4',
	'8', '5', '8', '6', '8', '7', '8', '8', '8', '9',
	'9', '0', '9', '1', '9', '2', '9', '3', '9', '4',
	'9', '5', '9', '6', '9', '7', '9', '8', '9', '9'
};



/* 校验输出基数和标志，并计算可选前缀长度。 */
static bool __xrtNumberWriteConfig(
	uint32 iBase,
	uint32 iFlags,
	size_t* pPrefixSize,
	cstr sOperation
)
{
	if ( (iFlags & ~XRT_NUMBER_WRITE_FLAGS) != 0 ) {
		__xrtNumberError(XERR_VALUE, XNUMBER_ERROR_CONFIG,
			sOperation, "invalid integer write flags");
		return false;
	}
	if ( (iBase < 2u) || (iBase > 36u) ) {
		__xrtNumberError(XERR_VALUE, XNUMBER_ERROR_CONFIG,
			sOperation, "integer output base must be between 2 and 36");
		return false;
	}
	*pPrefixSize = 0;
	if ( (iFlags & (uint32)XNUMBER_PREFIX) != 0 ) {
		if ( (iBase != 2u) && (iBase != 8u) && (iBase != 16u) ) {
			__xrtNumberError(XERR_VALUE, XNUMBER_ERROR_CONFIG,
				sOperation, "integer prefix is only defined for base 2, 8 or 16");
			return false;
		}
		*pPrefixSize = 2;
	}
	return true;
}



/* 使用两位数字表把无符号十进制幅值写到临时缓冲尾部。 */
static char* __xrtNumberDecimal(uint64 iValue, char* sEnd)
{
	while ( iValue >= UINT64_C(100) ) {
		uint64 iQuotient = iValue / UINT64_C(100);
		uint32 iRemainder = (uint32)(iValue - (iQuotient * UINT64_C(100)));

		sEnd -= 2;
		memcpy(sEnd, &__xrtNumberDigits100[iRemainder * 2u], 2);
		iValue = iQuotient;
	}
	if ( iValue < UINT64_C(10) ) {
		*--sEnd = (char)('0' + (char)iValue);
	} else {
		sEnd -= 2;
		memcpy(sEnd, &__xrtNumberDigits100[(size_t)iValue * 2u], 2);
	}
	return sEnd;
}



/* 按任意支持基数把无符号幅值写到临时缓冲尾部。 */
static char* __xrtNumberBase(
	uint64 iValue,
	uint32 iBase,
	bool bUpper,
	char* sEnd
)
{
	static const char sLower[] = "0123456789abcdefghijklmnopqrstuvwxyz";
	static const char sUpper[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	const char* sDigits = bUpper ? sUpper : sLower;

	if ( iBase == 10u ) {
		return __xrtNumberDecimal(iValue, sEnd);
	}
	do {
		uint64 iQuotient = iValue / (uint64)iBase;
		uint32 iRemainder = (uint32)(iValue - (iQuotient * (uint64)iBase));

		*--sEnd = sDigits[iRemainder];
		iValue = iQuotient;
	} while ( iValue != 0 );
	return sEnd;
}



/* 在临时缓冲里构造完整整数文本，并返回起点和长度。 */
static bool __xrtNumberBuild(
	uint64 iMagnitude,
	bool bNegative,
	uint32 iBase,
	uint32 iFlags,
	char* sBuffer,
	const char** pText,
	size_t* pSize,
	cstr sOperation
)
{
	char* sEnd = sBuffer + 68;
	char* sText;
	size_t iPrefixSize;
	bool bUpper;

	if ( !__xrtNumberWriteConfig(iBase, iFlags, &iPrefixSize, sOperation) ) {
		return false;
	}
	bUpper = (iFlags & (uint32)XNUMBER_UPPER) != 0;
	sText = __xrtNumberBase(iMagnitude, iBase, bUpper, sEnd);
	if ( iPrefixSize != 0 ) {
		char iPrefix;

		if ( iBase == 2u ) {
			iPrefix = bUpper ? 'B' : 'b';
		} else if ( iBase == 8u ) {
			iPrefix = bUpper ? 'O' : 'o';
		} else {
			iPrefix = bUpper ? 'X' : 'x';
		}
		*--sText = iPrefix;
		*--sText = '0';
	}
	if ( bNegative ) {
		*--sText = '-';
	} else if ( (iFlags & (uint32)XNUMBER_PLUS) != 0 ) {
		*--sText = '+';
	}
	*pText = sText;
	*pSize = (size_t)(sEnd - sText);
	return true;
}



/* 按指定基数写出无符号整数。 */
XRT_API bool xrtUIntWrite(uint64 iValue, uint32 iBase,
	char* sOutput, size_t iCapacity, size_t* pOutputSize, uint32 iFlags)
{
	char sBuffer[68];
	const char* sText;
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
	if ( !__xrtNumberBuild(iValue, false, iBase, iFlags,
		sBuffer, &sText, &iSize, "uint-write") ) {
		return false;
	}
	return __xrtNumberWriteResult(
		sText, iSize, sOutput, iCapacity, pOutputSize);
}



/* 按指定基数写出有符号整数。 */
XRT_API bool xrtIntWrite(int64 iValue, uint32 iBase,
	char* sOutput, size_t iCapacity, size_t* pOutputSize, uint32 iFlags)
{
	char sBuffer[68];
	const char* sText;
	size_t iSize;
	uint64 iMagnitude;
	bool bNegative;

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
	bNegative = iValue < 0;
	if ( bNegative ) {
		iMagnitude = (uint64)(-(iValue + 1)) + UINT64_C(1);
	} else {
		iMagnitude = (uint64)iValue;
	}
	if ( !__xrtNumberBuild(iMagnitude, bNegative, iBase, iFlags,
		sBuffer, &sText, &iSize, "int-write") ) {
		return false;
	}
	return __xrtNumberWriteResult(
		sText, iSize, sOutput, iCapacity, pOutputSize);
}



/* 分配并写出无符号整数文本。 */
XRT_API str xrtUIntString(uint64 iValue, uint32 iBase, uint32 iFlags)
{
	size_t iSize;
	str sOutput;

	if ( !xrtUIntWrite(iValue, iBase, NULL, 0, &iSize, iFlags) ) {
		return NULL;
	}
	sOutput = (str)xrtMalloc(iSize + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !xrtUIntWrite(
		iValue, iBase, sOutput, iSize + 1u, &iSize, iFlags) ) {
		xrtFree(sOutput);
		return NULL;
	}
	return sOutput;
}



/* 分配并写出有符号整数文本。 */
XRT_API str xrtIntString(int64 iValue, uint32 iBase, uint32 iFlags)
{
	size_t iSize;
	str sOutput;

	if ( !xrtIntWrite(iValue, iBase, NULL, 0, &iSize, iFlags) ) {
		return NULL;
	}
	sOutput = (str)xrtMalloc(iSize + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !xrtIntWrite(
		iValue, iBase, sOutput, iSize + 1u, &iSize, iFlags) ) {
		xrtFree(sOutput);
		return NULL;
	}
	return sOutput;
}



/* 把 ASCII 数字转换为 0 到 35 的值。 */
static bool __xrtNumberDigit(uint8 iByte, uint32* pValue)
{
	if ( (iByte >= (uint8)'0') && (iByte <= (uint8)'9') ) {
		*pValue = (uint32)(iByte - (uint8)'0');
		return true;
	}
	if ( (iByte >= (uint8)'A') && (iByte <= (uint8)'Z') ) {
		*pValue = (uint32)(iByte - (uint8)'A') + 10u;
		return true;
	}
	if ( (iByte >= (uint8)'a') && (iByte <= (uint8)'z') ) {
		*pValue = (uint32)(iByte - (uint8)'a') + 10u;
		return true;
	}
	return false;
}



/* 根据解析标志裁剪两端 ASCII 空白。 */
static bool __xrtNumberTrim(
	xstrview Text,
	uint32 iFlags,
	xstrview* pTrimmed,
	cstr sOperation
)
{
	size_t iStart = 0;
	size_t iEnd = Text.Size;

	if ( ((Text.Data == NULL) && (Text.Size != 0)) ||
		 ((iFlags & ~XRT_NUMBER_PARSE_FLAGS) != 0) ) {
		if ( (Text.Data == NULL) && (Text.Size != 0) ) {
			__xrtErrorSetInvalidArgument();
		} else {
			__xrtNumberError(XERR_VALUE, XNUMBER_ERROR_CONFIG,
				sOperation, "invalid integer parse flags");
		}
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



/* 识别可选进制前缀，并完成自动基数选择。 */
static bool __xrtNumberBasePrefix(
	xstrview Text,
	size_t* pPosition,
	uint32* pBase,
	uint32 iFlags,
	cstr sOperation
)
{
	size_t iPosition = *pPosition;
	uint32 iBase = *pBase;
	uint32 iPrefixBase = 0;

	if ( (iBase != 0) && ((iBase < 2u) || (iBase > 36u)) ) {
		__xrtNumberError(XERR_VALUE, XNUMBER_ERROR_CONFIG,
			sOperation, "integer parse base must be zero or between 2 and 36");
		return false;
	}
	if ( ((iFlags & (uint32)XNUMBER_PARSE_PREFIX) != 0) &&
		 ((Text.Size - iPosition) >= 2u) &&
		 (Text.Data[iPosition] == '0') ) {
		uint8 iByte = (uint8)Text.Data[iPosition + 1u];

		if ( (iByte == (uint8)'b') || (iByte == (uint8)'B') ) {
			iPrefixBase = 2u;
		} else if ( (iByte == (uint8)'o') || (iByte == (uint8)'O') ) {
			iPrefixBase = 8u;
		} else if ( (iByte == (uint8)'x') || (iByte == (uint8)'X') ) {
			iPrefixBase = 16u;
		}
	}
	if ( iPrefixBase != 0 ) {
		if ( (iBase != 0) && (iBase != iPrefixBase) ) {
			__xrtNumberError(XERR_PROTOCOL, XNUMBER_ERROR_FORMAT,
				sOperation, "integer prefix does not match the requested base");
			return false;
		}
		iBase = iPrefixBase;
		iPosition += 2u;
	}
	if ( iBase == 0 ) {
		iBase = 10u;
	}
	*pPosition = iPosition;
	*pBase = iBase;
	return true;
}



/* 解析已经处理符号和前缀的无符号幅值。 */
static bool __xrtNumberMagnitude(
	xstrview Text,
	size_t iPosition,
	uint32 iBase,
	uint32 iFlags,
	uint64 iLimit,
	uint64* pValue,
	cstr sOperation
)
{
	uint64 iValue = 0;
	size_t iDigits = 0;
	bool bSeparator = false;

	for ( ; iPosition < Text.Size; iPosition++ ) {
		uint8 iByte = (uint8)Text.Data[iPosition];
		uint32 iDigit;

		if ( (iByte == (uint8)'_') &&
			((iFlags & (uint32)XNUMBER_PARSE_SEPARATOR) != 0) ) {
			if ( (iDigits == 0) || bSeparator ) {
				__xrtNumberError(XERR_PROTOCOL, XNUMBER_ERROR_FORMAT,
					sOperation, "integer separator must appear between digits");
				return false;
			}
			bSeparator = true;
			continue;
		}
		if ( !__xrtNumberDigit(iByte, &iDigit) || (iDigit >= iBase) ) {
			__xrtNumberError(XERR_PROTOCOL, XNUMBER_ERROR_FORMAT,
				sOperation, "integer text contains a digit outside its base");
			return false;
		}
		if ( iValue > ((iLimit - (uint64)iDigit) / (uint64)iBase) ) {
			__xrtNumberError(XERR_RANGE, XNUMBER_ERROR_RANGE,
				sOperation, "integer text is outside the destination range");
			return false;
		}
		iValue = (iValue * (uint64)iBase) + (uint64)iDigit;
		iDigits++;
		bSeparator = false;
	}
	if ( (iDigits == 0) || bSeparator ) {
		__xrtNumberError(XERR_PROTOCOL, XNUMBER_ERROR_FORMAT,
			sOperation, "integer text does not end with a digit");
		return false;
	}
	*pValue = iValue;
	return true;
}



/* 严格解析无符号整数。 */
XRT_API bool xrtUIntParse(xstrview Text, uint32 iBase,
	uint32 iFlags, uint64* pValue)
{
	xstrview Trimmed;
	size_t iPosition = 0;
	uint64 iValue;

	if ( pValue == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtNumberTrim(
		Text, iFlags, &Trimmed, "uint-parse") ) {
		return false;
	}
	if ( Trimmed.Size == 0 ) {
		__xrtNumberError(XERR_PROTOCOL, XNUMBER_ERROR_FORMAT,
			"uint-parse", "unsigned integer text is empty");
		return false;
	}
	if ( (Trimmed.Data[0] == '+') || (Trimmed.Data[0] == '-') ) {
		__xrtNumberError(XERR_PROTOCOL, XNUMBER_ERROR_FORMAT,
			"uint-parse", "unsigned integer text cannot contain a sign");
		return false;
	}
	if ( !__xrtNumberBasePrefix(
		Trimmed, &iPosition, &iBase, iFlags, "uint-parse") ||
		 !__xrtNumberMagnitude(Trimmed, iPosition, iBase, iFlags,
		UINT64_MAX, &iValue, "uint-parse") ) {
		return false;
	}
	*pValue = iValue;
	return true;
}



/* 严格解析有符号整数。 */
XRT_API bool xrtIntParse(xstrview Text, uint32 iBase,
	uint32 iFlags, int64* pValue)
{
	xstrview Trimmed;
	size_t iPosition = 0;
	uint64 iMagnitude;
	uint64 iLimit;
	int64 iValue;
	bool bNegative = false;

	if ( pValue == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtNumberTrim(
		Text, iFlags, &Trimmed, "int-parse") ) {
		return false;
	}
	if ( Trimmed.Size == 0 ) {
		__xrtNumberError(XERR_PROTOCOL, XNUMBER_ERROR_FORMAT,
			"int-parse", "signed integer text is empty");
		return false;
	}
	if ( (Trimmed.Data[0] == '+') || (Trimmed.Data[0] == '-') ) {
		bNegative = Trimmed.Data[0] == '-';
		iPosition++;
	}
	if ( !__xrtNumberBasePrefix(
		Trimmed, &iPosition, &iBase, iFlags, "int-parse") ) {
		return false;
	}
	iLimit = bNegative ?
		((uint64)INT64_MAX + UINT64_C(1)) : (uint64)INT64_MAX;
	if ( !__xrtNumberMagnitude(Trimmed, iPosition, iBase, iFlags,
		iLimit, &iMagnitude, "int-parse") ) {
		return false;
	}
	if ( bNegative ) {
		if ( iMagnitude == ((uint64)INT64_MAX + UINT64_C(1)) ) {
			iValue = INT64_MIN;
		} else {
			iValue = -(int64)iMagnitude;
		}
	} else {
		iValue = (int64)iMagnitude;
	}
	*pValue = iValue;
	return true;
}

#undef XRT_NUMBER_WRITE_FLAGS
#undef XRT_NUMBER_PARSE_FLAGS

#endif
