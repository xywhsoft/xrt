#include "../internal/xrt_number_format.h"



#if defined(XRT_FEATURE_NUMBER_FORMAT)

/* 最终排版描述把符号、前缀、整数区和尾部明确分开。 */
typedef struct xrt_number_format_layout {
	const char* Core;
	size_t CoreSize;
	size_t PrefixSize;
	size_t IntegerSize;
	size_t ZeroSize;
	size_t SpaceSize;
	size_t GroupCount;
	size_t OutputSize;
	size_t GroupWidth;
	char Group;
	bool Negative;
	bool Special;
} xrt_number_format_layout;



/* 统一设置数字展示格式错误。 */
static bool __xrtNumberFormatError(cstr sOperation, cstr sMessage)
{
	__xrtNumberError(XERR_VALUE, XNUMBER_ERROR_FORMAT,
		sOperation, sMessage);
	return false;
}



/* 安全累加输出长度。 */
static bool __xrtNumberFormatAdd(size_t* pValue, size_t iAdd,
	cstr sOperation)
{
	if ( iAdd > (SIZE_MAX - *pValue) ) {
		__xrtNumberError(XERR_RANGE, XNUMBER_ERROR_RANGE,
			sOperation, "formatted number is too large");
		return false;
	}
	*pValue += iAdd;
	return true;
}



/* 解析一个无符号十进制格式字段。 */
static bool __xrtNumberFormatField(xstrview Format, size_t* pPosition,
	size_t* pValue, cstr sOperation)
{
	size_t iPosition = *pPosition;
	size_t iValue = 0;

	while ( (iPosition < Format.Size) &&
		(Format.Data[iPosition] >= '0') &&
		(Format.Data[iPosition] <= '9') ) {
		size_t iDigit = (size_t)(Format.Data[iPosition] - '0');

		if ( (iValue > (SIZE_MAX / 10u)) ||
			 ((iValue == (SIZE_MAX / 10u)) &&
			  (iDigit > (SIZE_MAX % 10u))) ) {
			return __xrtNumberFormatError(
				sOperation, "number format field is too large");
		}
		iValue = (iValue * 10u) + iDigit;
		iPosition++;
	}
	*pPosition = iPosition;
	*pValue = iValue;
	return true;
}



/* 按固定顺序解析稳定的数字展示格式语法。 */
static bool __xrtNumberFormatParse(xstrview Format,
	xrt_number_format_options* pOptions, cstr sOperation)
{
	size_t iPosition = 0;

	memset(pOptions, 0, sizeof(*pOptions));
	if ( (Format.Data == NULL) && (Format.Size != 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (iPosition < Format.Size) &&
		 ((Format.Data[iPosition] == '+') ||
		  (Format.Data[iPosition] == '-')) ) {
		pOptions->HasSign = true;
		pOptions->Plus = Format.Data[iPosition] == '+';
		iPosition++;
	}
	if ( (iPosition < Format.Size) &&
		 (Format.Data[iPosition] == '#') ) {
		pOptions->Alternate = true;
		iPosition++;
	}
	if ( (iPosition < Format.Size) &&
		 (Format.Data[iPosition] == '0') ) {
		pOptions->Zero = true;
		iPosition++;
	}
	if ( !__xrtNumberFormatField(
			Format, &iPosition, &pOptions->Width, sOperation) ) {
		return false;
	}
	if ( (iPosition < Format.Size) &&
		 ((Format.Data[iPosition] == ',') ||
		  (Format.Data[iPosition] == '_')) ) {
		pOptions->Group = Format.Data[iPosition];
		iPosition++;
	}
	if ( (iPosition < Format.Size) &&
		 (Format.Data[iPosition] == '.') ) {
		iPosition++;
		if ( (iPosition == Format.Size) ||
			 (Format.Data[iPosition] < '0') ||
			 (Format.Data[iPosition] > '9') ) {
			return __xrtNumberFormatError(
				sOperation, "number format precision requires digits");
		}
		pOptions->HasPrecision = true;
		if ( !__xrtNumberFormatField(
				Format, &iPosition, &pOptions->Precision, sOperation) ) {
			return false;
		}
		if ( pOptions->Precision > XRT_NUMBER_FORMAT_PRECISION_MAX ) {
			return __xrtNumberFormatError(
				sOperation, "number format precision exceeds 1000 digits");
		}
	}
	if ( iPosition < Format.Size ) {
		pOptions->Type = Format.Data[iPosition++];
	}
	if ( iPosition != Format.Size ) {
		return __xrtNumberFormatError(
			sOperation, "invalid number format syntax");
	}
	return true;
}



/* 校验整数专用的格式类型和分组选项。 */
static bool __xrtNumberIntegerOptions(xrt_number_format_options* pOptions,
	uint32* pBase, uint32* pFlags, size_t* pGroupWidth,
	cstr sOperation)
{
	char iType = pOptions->Type == 0 ? 'd' : pOptions->Type;

	if ( pOptions->HasPrecision ) {
		return __xrtNumberFormatError(
			sOperation, "integer format does not accept precision");
	}
	*pFlags = 0;
	if ( iType == 'd' ) {
		*pBase = 10u;
		*pGroupWidth = 3u;
	} else if ( (iType == 'x') || (iType == 'X') ) {
		*pBase = 16u;
		*pGroupWidth = 4u;
		if ( iType == 'X' ) {
			*pFlags |= (uint32)XNUMBER_UPPER;
		}
	} else if ( iType == 'o' ) {
		*pBase = 8u;
		*pGroupWidth = 4u;
	} else if ( (iType == 'b') || (iType == 'B') ) {
		*pBase = 2u;
		*pGroupWidth = 4u;
		if ( iType == 'B' ) {
			*pFlags |= (uint32)XNUMBER_UPPER;
		}
	} else {
		return __xrtNumberFormatError(
			sOperation, "invalid integer format type");
	}
	if ( (pOptions->Group == ',') && (*pBase != 10u) ) {
		return __xrtNumberFormatError(
			sOperation, "comma grouping is only valid for decimal integers");
	}
	if ( pOptions->Alternate && (*pBase != 10u) ) {
		*pFlags |= (uint32)XNUMBER_PREFIX;
	}
	pOptions->Type = iType;
	return true;
}



/* 校验浮点专用的格式类型。 */
static bool __xrtNumberFloatOptions(xrt_number_format_options* pOptions,
	cstr sOperation)
{
	char iType = pOptions->Type;

	if ( iType == 0 ) {
		if ( pOptions->HasPrecision ) {
			pOptions->Type = 'f';
		}
		return true;
	}
	if ( (iType != 'f') && (iType != 'F') &&
		 (iType != 'e') && (iType != 'E') &&
		 (iType != 'g') && (iType != 'G') &&
		 (iType != '%') ) {
		return __xrtNumberFormatError(
			sOperation, "invalid floating-point format type");
	}
	return true;
}



/* 校验查询/写入参数并保护长度输出不与目标缓冲区重叠。 */
static bool __xrtNumberFormatTarget(char* sOutput, size_t iCapacity,
	size_t* pOutputSize)
{
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
	return true;
}



/* 计算符号、零填充、分组和空格对齐后的最终布局。 */
static bool __xrtNumberFormatLayout(
	const char* sCore,
	size_t iCoreSize,
	size_t iPrefixSize,
	size_t iIntegerSize,
	bool bNegative,
	bool bSpecial,
	const xrt_number_format_options* pOptions,
	size_t iGroupWidth,
	xrt_number_format_layout* pLayout,
	cstr sOperation
)
{
	size_t iSignSize = (bNegative || pOptions->Plus) ? 1u : 0u;
	size_t iGroupCount = 0;
	size_t iOutputSize = iSignSize;
	size_t iZeroSize = 0;

	if ( (pOptions->Group != 0) && !bSpecial &&
		 (iIntegerSize != 0) ) {
		iGroupCount = (iIntegerSize - 1u) / iGroupWidth;
	}
	if ( !__xrtNumberFormatAdd(&iOutputSize, iCoreSize, sOperation) ||
		 !__xrtNumberFormatAdd(&iOutputSize, iGroupCount, sOperation) ) {
		return false;
	}
	if ( pOptions->Zero && !bSpecial &&
		 (pOptions->Width > iOutputSize) ) {
		iZeroSize = pOptions->Width - iOutputSize;
		if ( pOptions->Group != 0 ) {
			size_t iNewGroups =
				(iIntegerSize + iZeroSize - 1u) / iGroupWidth;

			if ( !__xrtNumberFormatAdd(
					&iOutputSize, iZeroSize, sOperation) ||
				 !__xrtNumberFormatAdd(
					&iOutputSize, iNewGroups - iGroupCount, sOperation) ) {
				return false;
			}
			iGroupCount = iNewGroups;
		} else if ( !__xrtNumberFormatAdd(
				&iOutputSize, iZeroSize, sOperation) ) {
			return false;
		}
	}
	pLayout->SpaceSize = 0;
	if ( pOptions->Width > iOutputSize ) {
		pLayout->SpaceSize = pOptions->Width - iOutputSize;
		iOutputSize = pOptions->Width;
	}
	if ( iOutputSize == SIZE_MAX ) {
		__xrtNumberError(XERR_RANGE, XNUMBER_ERROR_RANGE,
			sOperation, "formatted number cannot include a terminator");
		return false;
	}
	pLayout->Core = sCore;
	pLayout->CoreSize = iCoreSize;
	pLayout->PrefixSize = iPrefixSize;
	pLayout->IntegerSize = iIntegerSize;
	pLayout->ZeroSize = iZeroSize;
	pLayout->GroupCount = iGroupCount;
	pLayout->OutputSize = iOutputSize;
	pLayout->GroupWidth = iGroupWidth;
	pLayout->Group = pOptions->Group;
	pLayout->Negative = bNegative;
	pLayout->Special = bSpecial;
	return true;
}



/* 按已经验证和测量的布局写出最终文本。 */
static bool __xrtNumberFormatWriteLayout(
	const xrt_number_format_layout* pLayout,
	const xrt_number_format_options* pOptions,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	size_t iWrite = 0;
	size_t iDigits = pLayout->ZeroSize + pLayout->IntegerSize;
	size_t iFirstGroup = pLayout->GroupWidth;

	if ( sOutput == NULL ) {
		*pOutputSize = pLayout->OutputSize;
		return true;
	}
	if ( iCapacity <= pLayout->OutputSize ) {
		*pOutputSize = pLayout->OutputSize;
		__xrtErrorSetRange();
		return false;
	}
	if ( pLayout->SpaceSize != 0 ) {
		memset(sOutput, ' ', pLayout->SpaceSize);
		iWrite += pLayout->SpaceSize;
	}
	if ( pLayout->Negative ) {
		sOutput[iWrite++] = '-';
	} else if ( pOptions->Plus ) {
		sOutput[iWrite++] = '+';
	}
	if ( pLayout->PrefixSize != 0 ) {
		memcpy(sOutput + iWrite, pLayout->Core, pLayout->PrefixSize);
		iWrite += pLayout->PrefixSize;
	}
	if ( (pLayout->Group != 0) && (iDigits != 0) ) {
		iFirstGroup = iDigits % pLayout->GroupWidth;
		if ( iFirstGroup == 0 ) {
			iFirstGroup = pLayout->GroupWidth;
		}
	}
	for ( size_t i = 0; i < iDigits; i++ ) {
		if ( (pLayout->Group != 0) && (i != 0) &&
			 (i >= iFirstGroup) &&
			 (((i - iFirstGroup) % pLayout->GroupWidth) == 0) ) {
			sOutput[iWrite++] = pLayout->Group;
		}
		sOutput[iWrite++] = i < pLayout->ZeroSize ? '0' :
			pLayout->Core[pLayout->PrefixSize +
				(i - pLayout->ZeroSize)];
	}
	if ( pLayout->CoreSize >
		(pLayout->PrefixSize + pLayout->IntegerSize) ) {
		size_t iTail = pLayout->CoreSize -
			pLayout->PrefixSize - pLayout->IntegerSize;

		memcpy(sOutput + iWrite,
			pLayout->Core + pLayout->PrefixSize + pLayout->IntegerSize,
			iTail);
		iWrite += iTail;
	}
	sOutput[iWrite] = 0;
	*pOutputSize = iWrite;
	return true;
}



/* 把整数解释为 Unicode 标量并按照字符格式写出 UTF-8。 */
static bool __xrtNumberCharacterFormat(
	uint64 iMagnitude,
	bool bNegative,
	const xrt_number_format_options* pOptions,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize,
	cstr sOperation
)
{
	xrt_number_format_layout Layout;
	unsigned char arrCore[4];
	size_t iCoreSize = 0;

	if ( pOptions->HasSign || pOptions->Alternate || pOptions->Zero ||
		 (pOptions->Group != 0) || pOptions->HasPrecision ) {
		return __xrtNumberFormatError(
			sOperation, "character format only accepts width");
	}
	if ( !bNegative && (iMagnitude != 0) &&
		 (iMagnitude <= UINT32_MAX) ) {
		iCoreSize = __xrtUtf8Encode((uint32)iMagnitude, arrCore);
	}
	if ( !__xrtNumberFormatLayout(
			(const char*)arrCore, iCoreSize, 0, iCoreSize,
			false, false, pOptions, 1u, &Layout, sOperation) ) {
		return false;
	}
	return __xrtNumberFormatWriteLayout(
		&Layout, pOptions, sOutput, iCapacity, pOutputSize);
}



/* 使用整数底层写出结果构造展示层布局。 */
static bool __xrtNumberIntegerFormat(
	uint64 iMagnitude,
	bool bNegative,
	xstrview Format,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize,
	cstr sOperation
)
{
	xrt_number_format_options Options;
	xrt_number_format_layout Layout;
	char sCore[70];
	uint32 iBase;
	uint32 iFlags;
	size_t iCoreSize;
	size_t iPrefixSize;
	size_t iGroupWidth;

	if ( !__xrtNumberFormatTarget(sOutput, iCapacity, pOutputSize) ||
		 !__xrtNumberFormatParse(Format, &Options, sOperation) ) {
		return false;
	}
	if ( Options.Type == 'c' ) {
		return __xrtNumberCharacterFormat(
			iMagnitude, bNegative, &Options,
			sOutput, iCapacity, pOutputSize, sOperation);
	}
	if ( !__xrtNumberIntegerOptions(
			&Options, &iBase, &iFlags, &iGroupWidth, sOperation) ) {
		return false;
	}
	if ( !xrtUIntWrite(iMagnitude, iBase, sCore, sizeof(sCore),
			&iCoreSize, iFlags) ) {
		return false;
	}
	iPrefixSize = ((iFlags & (uint32)XNUMBER_PREFIX) != 0) ? 2u : 0u;
	if ( !__xrtNumberFormatLayout(
			sCore, iCoreSize, iPrefixSize, iCoreSize - iPrefixSize,
			bNegative, false, &Options, iGroupWidth, &Layout, sOperation) ) {
		return false;
	}
	return __xrtNumberFormatWriteLayout(
		&Layout, &Options, sOutput, iCapacity, pOutputSize);
}



/* 返回核心文本中参与分组的整数数字数。 */
static size_t __xrtNumberFormatIntegerSize(const char* sCore,
	size_t iCoreSize, bool bSpecial)
{
	size_t iSize = 0;

	if ( bSpecial ) {
		return iCoreSize;
	}
	while ( (iSize < iCoreSize) && (sCore[iSize] != '.') &&
		(sCore[iSize] != 'e') && (sCore[iSize] != 'E') ) {
		iSize++;
	}
	return iSize;
}



/* 构造并排版一个浮点展示结果。 */
static bool __xrtNumberFormatFloat(
	double fValue,
	xstrview Format,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	xrt_number_format_options Options;
	xrt_number_format_layout Layout;
	char sCore[XRT_NUMBER_FORMAT_CORE_CAPACITY];
	size_t iCoreSize;
	size_t iIntegerSize;
	bool bNegative;
	bool bSpecial;

	if ( !__xrtNumberFormatTarget(sOutput, iCapacity, pOutputSize) ||
		 !__xrtNumberFormatParse(Format, &Options, "num-format") ||
		 !__xrtNumberFloatOptions(&Options, "num-format") ) {
		return false;
	}
	if ( !__xrtNumberFormatFloatCore(
			fValue, &Options, sCore, &iCoreSize,
			&bNegative, &bSpecial) ) {
		return false;
	}
	iIntegerSize = __xrtNumberFormatIntegerSize(
		sCore, iCoreSize, bSpecial);
	if ( !__xrtNumberFormatLayout(
			sCore, iCoreSize, 0, iIntegerSize, bNegative, bSpecial,
			&Options, 3u, &Layout, "num-format") ) {
		return false;
	}
	return __xrtNumberFormatWriteLayout(
		&Layout, &Options, sOutput, iCapacity, pOutputSize);
}



/* 按展示格式写出有符号整数。 */
XRT_API bool xrtIntFormatTo(int64 iValue, xstrview Format,
	char* sOutput, size_t iCapacity, size_t* pOutputSize)
{
	bool bNegative = iValue < 0;
	uint64 iMagnitude = bNegative ?
		(uint64)(-(iValue + 1)) + UINT64_C(1) : (uint64)iValue;

	return __xrtNumberIntegerFormat(
		iMagnitude, bNegative, Format, sOutput,
		iCapacity, pOutputSize, "int-format");
}



/* 按展示格式写出无符号整数。 */
XRT_API bool xrtUIntFormatTo(uint64 iValue, xstrview Format,
	char* sOutput, size_t iCapacity, size_t* pOutputSize)
{
	return __xrtNumberIntegerFormat(
		iValue, false, Format, sOutput,
		iCapacity, pOutputSize, "uint-format");
}



/* 按展示格式写出 double。 */
XRT_API bool xrtNumFormatTo(double fValue, xstrview Format,
	char* sOutput, size_t iCapacity, size_t* pOutputSize)
{
	return __xrtNumberFormatFloat(
		fValue, Format, sOutput, iCapacity, pOutputSize);
}



/* 统一分配三个数字展示便捷接口的结果。 */
static str __xrtNumberFormatAllocate(
	int iKind,
	int64 iSigned,
	uint64 iUnsigned,
	double fValue,
	xstrview Format
)
{
	size_t iSize;
	str sOutput;
	bool bResult;

	if ( iKind == 0 ) {
		bResult = xrtIntFormatTo(
			iSigned, Format, NULL, 0, &iSize);
	} else if ( iKind == 1 ) {
		bResult = xrtUIntFormatTo(
			iUnsigned, Format, NULL, 0, &iSize);
	} else {
		bResult = xrtNumFormatTo(
			fValue, Format, NULL, 0, &iSize);
	}
	if ( !bResult ) {
		return NULL;
	}
	sOutput = (str)xrtMalloc(iSize + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( iKind == 0 ) {
		bResult = xrtIntFormatTo(
			iSigned, Format, sOutput, iSize + 1u, &iSize);
	} else if ( iKind == 1 ) {
		bResult = xrtUIntFormatTo(
			iUnsigned, Format, sOutput, iSize + 1u, &iSize);
	} else {
		bResult = xrtNumFormatTo(
			fValue, Format, sOutput, iSize + 1u, &iSize);
	}
	if ( !bResult ) {
		xrtFree(sOutput);
		return NULL;
	}
	return sOutput;
}



/* 格式化有符号整数并分配结果。 */
XRT_API str xrtIntFormat(int64 iValue, xstrview Format)
{
	return __xrtNumberFormatAllocate(0, iValue, 0, 0.0, Format);
}



/* 格式化无符号整数并分配结果。 */
XRT_API str xrtUIntFormat(uint64 iValue, xstrview Format)
{
	return __xrtNumberFormatAllocate(1, 0, iValue, 0.0, Format);
}



/* 格式化 double 并分配结果。 */
XRT_API str xrtNumFormat(double fValue, xstrview Format)
{
	return __xrtNumberFormatAllocate(2, 0, 0, fValue, Format);
}

#endif
