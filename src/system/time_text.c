#include "../internal/xrt_time.h"



#if defined(XRT_FEATURE_TIME_TEXT)

/* 英文名称固定为协议无关的 ASCII，不受进程 locale 影响。 */
static const char* __xrtTimeMonthShort[12] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};
static const char* __xrtTimeMonthLong[12] = {
	"January", "February", "March", "April", "May", "June",
	"July", "August", "September", "October", "November", "December"
};
static const char* __xrtTimeWeekShort[7] = {
	"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};
static const char* __xrtTimeWeekLong[7] = {
	"Sunday", "Monday", "Tuesday", "Wednesday",
	"Thursday", "Friday", "Saturday"
};



/* 文本写入器同时统计完整长度和有界缓冲区写入位置。 */
typedef struct __xrt_time_writer {
	char* Data;
	size_t Capacity;
	size_t Size;
	bool Overflow;
} __xrt_time_writer;



/* 校验长度视图，空视图允许空指针。 */
static bool __xrtTimeTextViewValid(xstrview Text)
{
	return (Text.Data != NULL) || (Text.Size == 0);
}



/* 初始化写入器并预先终止非空缓冲区。 */
static void __xrtTimeWriterInit(__xrt_time_writer* pWriter,
	char* sBuffer, size_t iCapacity)
{
	pWriter->Data = sBuffer;
	pWriter->Capacity = iCapacity;
	pWriter->Size = 0;
	pWriter->Overflow = false;
	if ( (sBuffer != NULL) && (iCapacity != 0) ) {
		sBuffer[0] = '\0';
	}
}



/* 失败时清空调用方缓冲，避免把不完整协议文本误当作结果使用。 */
static void __xrtTimeWriterAbort(__xrt_time_writer* pWriter)
{
	if ( (pWriter->Data != NULL) && (pWriter->Capacity != 0) ) {
		pWriter->Data[0] = '\0';
	}
}



/* 追加一个字节，并在 size_t 达到极值时记录失败。 */
static void __xrtTimeWriterByte(__xrt_time_writer* pWriter, char iByte)
{
	if ( pWriter->Overflow ) {
		return;
	}
	if ( pWriter->Size == SIZE_MAX ) {
		pWriter->Overflow = true;
		return;
	}
	if ( (pWriter->Data != NULL) && ((pWriter->Size + 1) < pWriter->Capacity) ) {
		pWriter->Data[pWriter->Size] = iByte;
	}
	pWriter->Size++;
}



/* 追加明确长度文本。 */
static void __xrtTimeWriterText(__xrt_time_writer* pWriter, cstr sText, size_t iSize)
{
	for ( size_t i = 0; i < iSize; i++ ) {
		__xrtTimeWriterByte(pWriter, sText[i]);
	}
}



/* 追加零结尾文本。 */
static void __xrtTimeWriterCStr(__xrt_time_writer* pWriter, cstr sText)
{
	__xrtTimeWriterText(pWriter, sText, strlen(sText));
}



/* 追加带最小零填充宽度的无符号十进制整数。 */
static void __xrtTimeWriterUInt(__xrt_time_writer* pWriter,
	uint64 iValue, int iWidth, char iFill)
{
	char arrDigits[32];
	int iCount = 0;

	do {
		arrDigits[iCount++] = (char)('0' + (iValue % 10));
		iValue /= 10;
	} while ( iValue != 0 );
	while ( iCount < iWidth ) {
		__xrtTimeWriterByte(pWriter, iFill);
		iWidth--;
	}
	while ( iCount != 0 ) {
		__xrtTimeWriterByte(pWriter, arrDigits[--iCount]);
	}
}



/* 追加至少四位的完整有符号年份。 */
static void __xrtTimeWriterYear(__xrt_time_writer* pWriter, int64 iYear)
{
	uint64 iMagnitude;

	if ( iYear < 0 ) {
		__xrtTimeWriterByte(pWriter, '-');
		iMagnitude = UINT64_C(0) - (uint64)iYear;
	} else {
		iMagnitude = (uint64)iYear;
	}
	__xrtTimeWriterUInt(pWriter, iMagnitude, 4, '0');
}



/* 终止缓冲并返回完整所需长度。 */
static size_t __xrtTimeWriterFinish(__xrt_time_writer* pWriter, cstr sOperation)
{
	if ( (pWriter->Data != NULL) && (pWriter->Capacity != 0) ) {
		size_t iEnd = pWriter->Size < pWriter->Capacity ?
			pWriter->Size : pWriter->Capacity - 1;

		pWriter->Data[iEnd] = '\0';
	}
	if ( pWriter->Overflow ) {
		__xrtTimeWriterAbort(pWriter);
		__xrtTimeSetError(XERR_RANGE, XTIME_ERROR_OVERFLOW, sOperation,
			"formatted time text is too large", 0);
		return XRT_NPOS;
	}
	return pWriter->Size;
}



/* 追加四位 UTC 偏移，零偏移也保留正号。 */
static void __xrtTimeWriterOffset(__xrt_time_writer* pWriter,
	int iOffset, bool bColon)
{
	uint32 iAbsolute = iOffset < 0 ? (uint32)(-iOffset) : (uint32)iOffset;
	uint32 iHour = iAbsolute / 3600;
	uint32 iMinute = (iAbsolute % 3600) / 60;

	__xrtTimeWriterByte(pWriter, iOffset < 0 ? '-' : '+');
	__xrtTimeWriterUInt(pWriter, iHour, 2, '0');
	if ( bColon ) {
		__xrtTimeWriterByte(pWriter, ':');
	}
	__xrtTimeWriterUInt(pWriter, iMinute, 2, '0');
}



/* 判断占位符是否属于自定义时间格式的稳定词汇表。 */
static bool __xrtTimeTokenValid(char iToken)
{
	switch ( iToken ) {
		case '%':
		case 'Y':
		case 'y':
		case 'm':
		case 'b':
		case 'B':
		case 'd':
		case 'e':
		case 'H':
		case 'I':
		case 'M':
		case 'S':
		case 'f':
		case 'p':
		case 'P':
		case 'a':
		case 'A':
		case 'w':
		case 'j':
		case 'q':
		case 'z':
		case 'F':
		case 'T':
		case 'R':
			return true;
		default:
			return false;
	}
}



/* '-' 修饰符只对可变宽的数字字段有效。 */
static bool __xrtTimeNoPadTokenValid(char iToken)
{
	switch ( iToken ) {
		case 'm':
		case 'd':
		case 'e':
		case 'H':
		case 'I':
		case 'M':
		case 'S':
			return true;
		default:
			return false;
	}
}



/* 展开一个时间格式占位符。 */
static bool __xrtTimeWriteToken(__xrt_time_writer* pWriter,
	const xdatetime* pDateTime, char iToken, bool bColon, bool bNoPad)
{
	int iHour12;
	int iWidth = bNoPad ? 0 : 2;

	switch ( iToken ) {
		case '%': __xrtTimeWriterByte(pWriter, '%'); break;
		case 'Y': __xrtTimeWriterYear(pWriter, pDateTime->Year); break;
		case 'y':
			__xrtTimeWriterUInt(pWriter, (pDateTime->Year < 0 ?
				(UINT64_C(0) - (uint64)pDateTime->Year) :
				(uint64)pDateTime->Year) % 100, 2, '0');
			break;
		case 'm': __xrtTimeWriterUInt(pWriter, (uint64)pDateTime->Month, iWidth, '0'); break;
		case 'b': __xrtTimeWriterCStr(pWriter, __xrtTimeMonthShort[pDateTime->Month - 1]); break;
		case 'B': __xrtTimeWriterCStr(pWriter, __xrtTimeMonthLong[pDateTime->Month - 1]); break;
		case 'd': __xrtTimeWriterUInt(pWriter, (uint64)pDateTime->Day, iWidth, '0'); break;
		case 'e': __xrtTimeWriterUInt(pWriter, (uint64)pDateTime->Day,
			bNoPad ? 0 : 2, bNoPad ? '0' : ' '); break;
		case 'H': __xrtTimeWriterUInt(pWriter, (uint64)pDateTime->Hour, iWidth, '0'); break;
		case 'I':
			iHour12 = pDateTime->Hour % 12;
			__xrtTimeWriterUInt(pWriter,
				(uint64)(iHour12 == 0 ? 12 : iHour12), iWidth, '0');
			break;
		case 'M': __xrtTimeWriterUInt(pWriter, (uint64)pDateTime->Minute, iWidth, '0'); break;
		case 'S': __xrtTimeWriterUInt(pWriter, (uint64)pDateTime->Second, iWidth, '0'); break;
		case 'f': __xrtTimeWriterUInt(pWriter, (uint64)pDateTime->Microsecond, 6, '0'); break;
		case 'p': __xrtTimeWriterCStr(pWriter, pDateTime->Hour < 12 ? "AM" : "PM"); break;
		case 'P': __xrtTimeWriterCStr(pWriter, pDateTime->Hour < 12 ? "am" : "pm"); break;
		case 'a': __xrtTimeWriterCStr(pWriter, __xrtTimeWeekShort[pDateTime->Weekday]); break;
		case 'A': __xrtTimeWriterCStr(pWriter, __xrtTimeWeekLong[pDateTime->Weekday]); break;
		case 'w': __xrtTimeWriterByte(pWriter, (char)('0' + pDateTime->Weekday)); break;
		case 'j': __xrtTimeWriterUInt(pWriter, (uint64)pDateTime->YearDay, 3, '0'); break;
		case 'q': __xrtTimeWriterByte(pWriter, (char)('0' +
			(((pDateTime->Month - 1) / 3) + 1))); break;
		case 'z': __xrtTimeWriterOffset(pWriter, pDateTime->Offset, bColon); break;
		case 'F':
			__xrtTimeWriterYear(pWriter, pDateTime->Year);
			__xrtTimeWriterByte(pWriter, '-');
			__xrtTimeWriterUInt(pWriter, (uint64)pDateTime->Month, 2, '0');
			__xrtTimeWriterByte(pWriter, '-');
			__xrtTimeWriterUInt(pWriter, (uint64)pDateTime->Day, 2, '0');
			break;
		case 'T':
			__xrtTimeWriterUInt(pWriter, (uint64)pDateTime->Hour, 2, '0');
			__xrtTimeWriterByte(pWriter, ':');
			__xrtTimeWriterUInt(pWriter, (uint64)pDateTime->Minute, 2, '0');
			__xrtTimeWriterByte(pWriter, ':');
			__xrtTimeWriterUInt(pWriter, (uint64)pDateTime->Second, 2, '0');
			break;
		case 'R':
			__xrtTimeWriterUInt(pWriter, (uint64)pDateTime->Hour, 2, '0');
			__xrtTimeWriterByte(pWriter, ':');
			__xrtTimeWriterUInt(pWriter, (uint64)pDateTime->Minute, 2, '0');
			break;
		default: return false;
	}
	return true;
}



/* 直接扫描格式，不建立固定长度 token 数组。 */
static size_t __xrtTimeWriteParts(char* sBuffer, size_t iCapacity,
	const xdatetime* pDateTime, xstrview Format)
{
	__xrt_time_writer tWriter;
	size_t iPosition = 0;

	__xrtTimeWriterInit(&tWriter, sBuffer, iCapacity);
	while ( iPosition < Format.Size ) {
		char iByte = Format.Data[iPosition++];
		bool bColon = false;
		bool bNoPad = false;

		if ( iByte == '\0' ) {
			__xrtTimeWriterAbort(&tWriter);
			__xrtTimeSetError(XERR_VALUE, XTIME_ERROR_FORMAT, "format",
				"time format contains an embedded null byte", 0);
			return XRT_NPOS;
		}
		if ( iByte != '%' ) {
			__xrtTimeWriterByte(&tWriter, iByte);
			continue;
		}
		if ( iPosition >= Format.Size ) {
			__xrtTimeWriterAbort(&tWriter);
			__xrtTimeSetError(XERR_VALUE, XTIME_ERROR_FORMAT, "format",
				"time format ends with an incomplete placeholder", 0);
			return XRT_NPOS;
		}
		if ( Format.Data[iPosition] == ':' ) {
			bColon = true;
			iPosition++;
			if ( iPosition >= Format.Size ) {
				__xrtTimeWriterAbort(&tWriter);
				__xrtTimeSetError(XERR_VALUE, XTIME_ERROR_FORMAT, "format",
					"time format ends with an incomplete placeholder", 0);
				return XRT_NPOS;
			}
		} else if ( Format.Data[iPosition] == '-' ) {
			bNoPad = true;
			iPosition++;
			if ( iPosition >= Format.Size ) {
				__xrtTimeWriterAbort(&tWriter);
				__xrtTimeSetError(XERR_VALUE, XTIME_ERROR_FORMAT, "format",
					"time format ends with an incomplete placeholder", 0);
				return XRT_NPOS;
			}
		}
		iByte = Format.Data[iPosition++];
		if ( bColon && (iByte != 'z') ) {
			__xrtTimeWriterAbort(&tWriter);
			__xrtTimeSetError(XERR_VALUE, XTIME_ERROR_FORMAT, "format",
				"the colon modifier is only valid for the UTC offset", 0);
			return XRT_NPOS;
		}
		if ( bNoPad && !__xrtTimeNoPadTokenValid(iByte) ) {
			__xrtTimeWriterAbort(&tWriter);
			__xrtTimeSetError(XERR_VALUE, XTIME_ERROR_FORMAT, "format",
				"the no-padding modifier is only valid for numeric fields", 0);
			return XRT_NPOS;
		}
		if ( (iByte == 'z') && ((pDateTime->Offset % 60) != 0) ) {
			__xrtTimeWriterAbort(&tWriter);
			__xrtTimeSetError(XERR_RANGE, XTIME_ERROR_RANGE, "format",
				"the time format cannot represent UTC offset seconds", 0);
			return XRT_NPOS;
		}
		if ( !__xrtTimeWriteToken(&tWriter,
			pDateTime, iByte, bColon, bNoPad) ) {
			__xrtTimeWriterAbort(&tWriter);
			__xrtTimeSetError(XERR_VALUE, XTIME_ERROR_FORMAT, "format",
				"time format contains an unknown placeholder", 0);
			return XRT_NPOS;
		}
	}
	return __xrtTimeWriterFinish(&tWriter, "format");
}



/* 规范化分解字段后写入自定义格式。 */
XRT_API size_t xrtDateTimeWrite(char* sBuffer, size_t iCapacity,
	const xdatetime* pDateTime, xstrview Format)
{
	xdatetime tCanonical;
	xtime iTime;

	if ( (sBuffer != NULL) && __xrtTimeTextViewValid(Format) &&
		 __xrtRangesOverlap(sBuffer, iCapacity, Format.Data, Format.Size) ) {
		__xrtErrorSetInvalidArgument();
		return XRT_NPOS;
	}
	if ( (sBuffer != NULL) && (iCapacity != 0) ) {
		sBuffer[0] = '\0';
	}
	if ( ((sBuffer == NULL) && (iCapacity != 0)) || (pDateTime == NULL) ||
		 !__xrtTimeTextViewValid(Format) ) {
		__xrtErrorSetInvalidArgument();
		return XRT_NPOS;
	}
	if ( !xrtTimeMake(pDateTime, &iTime) ||
		 !xrtTimeSplitAt(iTime, pDateTime->Offset, &tCanonical) ) {
		return XRT_NPOS;
	}
	return __xrtTimeWriteParts(sBuffer, iCapacity, &tCanonical, Format);
}



/* 按固定偏移写入绝对时间。 */
XRT_API size_t xrtTimeWrite(char* sBuffer, size_t iCapacity,
	xtime iTime, int iOffset, xstrview Format)
{
	xdatetime tDateTime;

	if ( (sBuffer != NULL) && __xrtTimeTextViewValid(Format) &&
		 __xrtRangesOverlap(sBuffer, iCapacity, Format.Data, Format.Size) ) {
		__xrtErrorSetInvalidArgument();
		return XRT_NPOS;
	}
	if ( (sBuffer != NULL) && (iCapacity != 0) ) {
		sBuffer[0] = '\0';
	}
	if ( ((sBuffer == NULL) && (iCapacity != 0)) ||
		 !__xrtTimeTextViewValid(Format) ) {
		__xrtErrorSetInvalidArgument();
		return XRT_NPOS;
	}
	if ( !xrtTimeSplitAt(iTime, iOffset, &tDateTime) ) {
		return XRT_NPOS;
	}
	return __xrtTimeWriteParts(sBuffer, iCapacity, &tDateTime, Format);
}



/* 按已知长度分配并写入时间文本。 */
static str __xrtTimeAllocText(size_t iSize)
{
	if ( iSize == SIZE_MAX ) {
		__xrtTimeSetError(XERR_RANGE, XTIME_ERROR_OVERFLOW, "format",
			"formatted time text is too large", 0);
		return NULL;
	}
	return (str)xrtMalloc(iSize + 1);
}



/* 创建分解时间文本。 */
XRT_API str xrtDateTimeFormat(const xdatetime* pDateTime, xstrview Format)
{
	size_t iSize = xrtDateTimeWrite(NULL, 0, pDateTime, Format);
	str sResult;

	if ( iSize == XRT_NPOS ) {
		return NULL;
	}
	sResult = __xrtTimeAllocText(iSize);
	if ( sResult == NULL ) {
		return NULL;
	}
	if ( xrtDateTimeWrite(sResult, iSize + 1, pDateTime, Format) == XRT_NPOS ) {
		xrtFree(sResult);
		return NULL;
	}
	return sResult;
}



/* 创建固定偏移时间文本。 */
XRT_API str xrtTimeFormat(xtime iTime, int iOffset, xstrview Format)
{
	size_t iSize = xrtTimeWrite(NULL, 0, iTime, iOffset, Format);
	str sResult;

	if ( iSize == XRT_NPOS ) {
		return NULL;
	}
	sResult = __xrtTimeAllocText(iSize);
	if ( sResult == NULL ) {
		return NULL;
	}
	if ( xrtTimeWrite(sResult, iSize + 1, iTime, iOffset, Format) == XRT_NPOS ) {
		xrtFree(sResult);
		return NULL;
	}
	return sResult;
}



/* 自定义格式解析期间保存字段存在性和待验证派生值。 */
typedef struct __xrt_time_parse_state {
	xdatetime Value;
	bool HasYear;
	bool HasMonth;
	bool HasDay;
	bool HasHour24;
	bool HasHour12;
	bool HasAMPM;
	bool HasWeekday;
	bool HasYearDay;
	bool HasQuarter;
	int AMPM;
	int Weekday;
	int YearDay;
	int Quarter;
} __xrt_time_parse_state;



/* 返回不受 locale 影响的 ASCII 小写字节。 */
static unsigned char __xrtTimeAsciiLower(unsigned char iByte)
{
	if ( (iByte >= (unsigned char)'A') && (iByte <= (unsigned char)'Z') ) {
		return (unsigned char)(iByte + ((unsigned char)'a' - (unsigned char)'A'));
	}
	return iByte;
}



/* 比较输入片段和 ASCII 名称，忽略 ASCII 大小写。 */
static bool __xrtTimeTextCaseEqual(cstr sText, size_t iSize, cstr sExpected)
{
	if ( strlen(sExpected) != iSize ) {
		return false;
	}
	for ( size_t i = 0; i < iSize; i++ ) {
		if ( __xrtTimeAsciiLower((unsigned char)sText[i]) !=
			 __xrtTimeAsciiLower((unsigned char)sExpected[i]) ) {
			return false;
		}
	}
	return true;
}



/* 解析固定宽度无符号十进制整数。 */
static bool __xrtTimeParseDigits(xstrview Text, size_t* pPosition,
	int iWidth, int* pValue)
{
	int iValue = 0;

	if ( (Text.Size - *pPosition) < (size_t)iWidth ) {
		return false;
	}
	for ( int i = 0; i < iWidth; i++ ) {
		unsigned char iByte = (unsigned char)Text.Data[*pPosition + (size_t)i];

		if ( (iByte < (unsigned char)'0') || (iByte > (unsigned char)'9') ) {
			return false;
		}
		iValue = (iValue * 10) + (int)(iByte - (unsigned char)'0');
	}
	*pPosition += (size_t)iWidth;
	*pValue = iValue;
	return true;
}



/* 解析一到最大宽度位数字，供无填充数字占位符使用。 */
static bool __xrtTimeParseVariableDigits(xstrview Text, size_t* pPosition,
	int iMaximum, int* pValue)
{
	size_t iPosition = *pPosition;
	int iValue = 0;
	int iCount = 0;

	while ( (iPosition < Text.Size) && (iCount < iMaximum) &&
		 (Text.Data[iPosition] >= '0') && (Text.Data[iPosition] <= '9') ) {
		iValue = (iValue * 10) + (int)(Text.Data[iPosition] - '0');
		iPosition++;
		iCount++;
	}
	if ( iCount == 0 ) {
		return false;
	}
	*pPosition = iPosition;
	*pValue = iValue;
	return true;
}



/* 解析完整有符号年份；紧邻数字占位符时固定消费四位。 */
static bool __xrtTimeParseYear(xstrview Text, size_t* pPosition,
	bool bFourDigits, int64* pYear)
{
	size_t iPosition = *pPosition;
	bool bNegative = false;
	uint64 iValue = 0;
	size_t iDigits = 0;
	size_t iMaximum = bFourDigits ? 4 : 19;

	if ( iPosition < Text.Size ) {
		if ( Text.Data[iPosition] == '-' ) {
			bNegative = true;
			iPosition++;
		} else if ( Text.Data[iPosition] == '+' ) {
			iPosition++;
		}
	}
	while ( (iPosition < Text.Size) && (iDigits < iMaximum) &&
		 (Text.Data[iPosition] >= '0') && (Text.Data[iPosition] <= '9') ) {
		uint32 iDigit = (uint32)(Text.Data[iPosition] - '0');

		if ( iValue > ((UINT64_MAX - iDigit) / 10) ) {
			return false;
		}
		iValue = (iValue * 10) + iDigit;
		iDigits++;
		iPosition++;
	}
	if ( (iDigits < 4) || (bFourDigits && (iDigits != 4)) ) {
		return false;
	}
	if ( bNegative ) {
		if ( iValue > (UINT64_C(1) << 63) ) {
			return false;
		}
		*pYear = iValue == (UINT64_C(1) << 63) ?
			INT64_MIN : -(int64)iValue;
	} else {
		if ( iValue > INT64_MAX ) {
			return false;
		}
		*pYear = (int64)iValue;
	}
	*pPosition = iPosition;
	return true;
}



/* 判断格式当前位置是否紧邻另一个数字占位符。 */
static bool __xrtTimeNextTokenNumeric(xstrview Format, size_t iPosition)
{
	char iToken;
	size_t iTokenPosition;

	if ( (iPosition + 1) >= Format.Size || (Format.Data[iPosition] != '%') ) {
		return false;
	}
	iTokenPosition = iPosition + 1;
	if ( (Format.Data[iTokenPosition] == '-') ||
		 (Format.Data[iTokenPosition] == ':') ) {
		iTokenPosition++;
	}
	if ( iTokenPosition >= Format.Size ) {
		return false;
	}
	iToken = Format.Data[iTokenPosition];
	return (iToken == 'y') || (iToken == 'm') || (iToken == 'd') ||
		(iToken == 'H') || (iToken == 'I') || (iToken == 'M') ||
		(iToken == 'S') || (iToken == 'f') || (iToken == 'w') ||
		(iToken == 'j') || (iToken == 'q');
}



/* 解析英文月份名称并返回一到十二。 */
static bool __xrtTimeParseMonthName(
	xstrview Text,
	size_t* pPosition,
	bool bLong,
	bool bCaseSensitive,
	int* pMonth
)
{
	for ( int i = 0; i < 12; i++ ) {
		cstr sName = bLong ? __xrtTimeMonthLong[i] : __xrtTimeMonthShort[i];
		size_t iSize = strlen(sName);

		if ( ((Text.Size - *pPosition) >= iSize) &&
			 (bCaseSensitive ?
			  (memcmp(Text.Data + *pPosition, sName, iSize) == 0) :
			  __xrtTimeTextCaseEqual(
				Text.Data + *pPosition,
				iSize,
				sName
			)) ) {
			*pPosition += iSize;
			*pMonth = i + 1;
			return true;
		}
	}
	return false;
}



/* 解析英文星期名称并返回星期日零起始值。 */
static bool __xrtTimeParseWeekName(
	xstrview Text,
	size_t* pPosition,
	bool bLong,
	bool bCaseSensitive,
	int* pWeekday
)
{
	for ( int i = 0; i < 7; i++ ) {
		cstr sName = bLong ? __xrtTimeWeekLong[i] : __xrtTimeWeekShort[i];
		size_t iSize = strlen(sName);

		if ( ((Text.Size - *pPosition) >= iSize) &&
			 (bCaseSensitive ?
			  (memcmp(Text.Data + *pPosition, sName, iSize) == 0) :
			  __xrtTimeTextCaseEqual(
				Text.Data + *pPosition,
				iSize,
				sName
			)) ) {
			*pPosition += iSize;
			*pWeekday = i;
			return true;
		}
	}
	return false;
}



/* 解析 Z、+HHMM 或 +HH:MM 固定偏移。 */
static bool __xrtTimeParseOffset(xstrview Text, size_t* pPosition,
	bool bColon, int* pOffset)
{
	size_t iPosition = *pPosition;
	int iSign;
	int iHour;
	int iMinute;

	if ( iPosition >= Text.Size ) {
		return false;
	}
	if ( (Text.Data[iPosition] == 'Z') || (Text.Data[iPosition] == 'z') ) {
		*pOffset = 0;
		*pPosition = iPosition + 1;
		return true;
	}
	if ( (Text.Data[iPosition] != '+') && (Text.Data[iPosition] != '-') ) {
		return false;
	}
	iSign = Text.Data[iPosition++] == '-' ? -1 : 1;
	if ( !__xrtTimeParseDigits(Text, &iPosition, 2, &iHour) ) {
		return false;
	}
	if ( bColon ) {
		if ( (iPosition >= Text.Size) || (Text.Data[iPosition++] != ':') ) {
			return false;
		}
	}
	if ( !__xrtTimeParseDigits(Text, &iPosition, 2, &iMinute) ||
		 (iHour > 23) || (iMinute > 59) ) {
		return false;
	}
	*pOffset = iSign * ((iHour * 3600) + (iMinute * 60));
	*pPosition = iPosition;
	return true;
}



/* 解析一个格式占位符并更新字段状态。 */
static bool __xrtTimeParseToken(xstrview Text, size_t* pTextPosition,
	xstrview Format, size_t iFormatPosition, char iToken, bool bColon, bool bNoPad,
	__xrt_time_parse_state* pState)
{
	int iValue;

	switch ( iToken ) {
		case '%':
			if ( (*pTextPosition >= Text.Size) ||
				 (Text.Data[*pTextPosition] != '%') ) {
				return false;
			}
			(*pTextPosition)++;
			break;
		case 'Y':
			if ( !__xrtTimeParseYear(Text, pTextPosition,
				__xrtTimeNextTokenNumeric(Format, iFormatPosition),
				&pState->Value.Year) ) {
				return false;
			}
			pState->HasYear = true;
			break;
		case 'y':
			if ( !__xrtTimeParseDigits(Text, pTextPosition, 2, &iValue) ) {
				return false;
			}
			pState->Value.Year = 2000 + iValue;
			pState->HasYear = true;
			break;
		case 'm':
			if ( !(bNoPad ?
				__xrtTimeParseVariableDigits(Text, pTextPosition, 2,
					&pState->Value.Month) :
				__xrtTimeParseDigits(Text, pTextPosition, 2,
					&pState->Value.Month)) ) {
				return false;
			}
			pState->HasMonth = true;
			break;
		case 'b':
			if ( !__xrtTimeParseMonthName(Text, pTextPosition, false, false,
				&pState->Value.Month) ) {
				return false;
			}
			pState->HasMonth = true;
			break;
		case 'B':
			if ( !__xrtTimeParseMonthName(Text, pTextPosition, true, false,
				&pState->Value.Month) ) {
				return false;
			}
			pState->HasMonth = true;
			break;
		case 'd':
			if ( !(bNoPad ?
				__xrtTimeParseVariableDigits(Text, pTextPosition, 2,
					&pState->Value.Day) :
				__xrtTimeParseDigits(Text, pTextPosition, 2,
					&pState->Value.Day)) ) {
				return false;
			}
			pState->HasDay = true;
			break;
		case 'e':
			if ( bNoPad ) {
				if ( !__xrtTimeParseVariableDigits(Text, pTextPosition, 2,
					&pState->Value.Day) ) {
					return false;
				}
			} else if ( (*pTextPosition < Text.Size) &&
				 (Text.Data[*pTextPosition] == ' ') ) {
				(*pTextPosition)++;
				if ( !__xrtTimeParseDigits(Text, pTextPosition, 1,
					&pState->Value.Day) ) {
					return false;
				}
			} else if ( !__xrtTimeParseDigits(Text, pTextPosition, 2, &pState->Value.Day) ) {
				return false;
			}
			pState->HasDay = true;
			break;
		case 'H':
			if ( !(bNoPad ?
				__xrtTimeParseVariableDigits(Text, pTextPosition, 2,
					&pState->Value.Hour) :
				__xrtTimeParseDigits(Text, pTextPosition, 2,
					&pState->Value.Hour)) ) {
				return false;
			}
			pState->HasHour24 = true;
			break;
		case 'I':
			if ( !(bNoPad ?
				__xrtTimeParseVariableDigits(Text, pTextPosition, 2,
					&pState->Value.Hour) :
				__xrtTimeParseDigits(Text, pTextPosition, 2,
					&pState->Value.Hour)) ) {
				return false;
			}
			pState->HasHour12 = true;
			break;
		case 'M':
			if ( !(bNoPad ?
				__xrtTimeParseVariableDigits(Text, pTextPosition, 2,
					&pState->Value.Minute) :
				__xrtTimeParseDigits(Text, pTextPosition, 2,
					&pState->Value.Minute)) ) {
				return false;
			}
			break;
		case 'S':
			if ( !(bNoPad ?
				__xrtTimeParseVariableDigits(Text, pTextPosition, 2,
					&pState->Value.Second) :
				__xrtTimeParseDigits(Text, pTextPosition, 2,
					&pState->Value.Second)) ) {
				return false;
			}
			break;
		case 'f':
			if ( !__xrtTimeParseDigits(Text, pTextPosition, 6,
				&pState->Value.Microsecond) ) {
				return false;
			}
			break;
		case 'p':
		case 'P':
			if ( (Text.Size - *pTextPosition) < 2 ) {
				return false;
			}
			if ( __xrtTimeTextCaseEqual(Text.Data + *pTextPosition, 2, "AM") ) {
				pState->AMPM = 0;
			} else if ( __xrtTimeTextCaseEqual(Text.Data + *pTextPosition, 2, "PM") ) {
				pState->AMPM = 1;
			} else {
				return false;
			}
			*pTextPosition += 2;
			pState->HasAMPM = true;
			break;
		case 'a':
			if ( !__xrtTimeParseWeekName(Text, pTextPosition, false, false,
				&pState->Weekday) ) {
				return false;
			}
			pState->HasWeekday = true;
			break;
		case 'A':
			if ( !__xrtTimeParseWeekName(Text, pTextPosition, true, false,
				&pState->Weekday) ) {
				return false;
			}
			pState->HasWeekday = true;
			break;
		case 'w':
			if ( !__xrtTimeParseDigits(Text, pTextPosition, 1,
				&pState->Weekday) ) {
				return false;
			}
			pState->HasWeekday = true;
			break;
		case 'j':
			if ( !__xrtTimeParseDigits(Text, pTextPosition, 3,
				&pState->YearDay) ) {
				return false;
			}
			pState->HasYearDay = true;
			break;
		case 'q':
			if ( !__xrtTimeParseDigits(Text, pTextPosition, 1,
				&pState->Quarter) ) {
				return false;
			}
			pState->HasQuarter = true;
			break;
		case 'z':
			if ( !__xrtTimeParseOffset(Text, pTextPosition, bColon,
				&pState->Value.Offset) ) {
				return false;
			}
			break;
		case 'F':
			if ( !__xrtTimeParseYear(Text, pTextPosition, false, &pState->Value.Year) ||
				 (*pTextPosition >= Text.Size) || (Text.Data[(*pTextPosition)++] != '-') ||
				 !__xrtTimeParseDigits(Text, pTextPosition, 2, &pState->Value.Month) ||
				 (*pTextPosition >= Text.Size) || (Text.Data[(*pTextPosition)++] != '-') ||
				 !__xrtTimeParseDigits(Text, pTextPosition, 2,
					&pState->Value.Day) ) {
				return false;
			}
			pState->HasYear = pState->HasMonth = pState->HasDay = true;
			break;
		case 'T':
			if ( !__xrtTimeParseDigits(Text, pTextPosition, 2, &pState->Value.Hour) ||
				 (*pTextPosition >= Text.Size) || (Text.Data[(*pTextPosition)++] != ':') ||
				 !__xrtTimeParseDigits(Text, pTextPosition, 2, &pState->Value.Minute) ||
				 (*pTextPosition >= Text.Size) || (Text.Data[(*pTextPosition)++] != ':') ||
				 !__xrtTimeParseDigits(Text, pTextPosition, 2,
					&pState->Value.Second) ) {
				return false;
			}
			pState->HasHour24 = true;
			break;
		case 'R':
			if ( !__xrtTimeParseDigits(Text, pTextPosition, 2, &pState->Value.Hour) ||
				 (*pTextPosition >= Text.Size) || (Text.Data[(*pTextPosition)++] != ':') ||
				 !__xrtTimeParseDigits(Text, pTextPosition, 2,
					&pState->Value.Minute) ) {
				return false;
			}
			pState->HasHour24 = true;
			break;
		default:
			return false;
	}
	return true;
}



/* 解析完整格式文本，不接受任何未消费字节。 */
static bool __xrtTimeParseFormat(xstrview Text, xstrview Format,
	xdatetime* pDateTime, bool* pFormatValid)
{
	__xrt_time_parse_state tState;
	size_t iTextPosition = 0;
	size_t iFormatPosition = 0;
	xtime iTime;
	xdatetime tCanonical;

	memset(&tState, 0, sizeof(tState));
	*pFormatValid = true;
	tState.Value.Year = 1970;
	tState.Value.Month = 1;
	tState.Value.Day = 1;
	tState.Value.IsDST = -1;
	while ( iFormatPosition < Format.Size ) {
		char iByte = Format.Data[iFormatPosition++];
		bool bColon = false;
		bool bNoPad = false;
		char iToken;

		if ( iByte == '\0' ) {
			*pFormatValid = false;
			return false;
		}
		if ( iByte != '%' ) {
			if ( (iTextPosition >= Text.Size) || (Text.Data[iTextPosition++] != iByte) ) {
				return false;
			}
			continue;
		}
		if ( iFormatPosition >= Format.Size ) {
			*pFormatValid = false;
			return false;
		}
		if ( Format.Data[iFormatPosition] == ':' ) {
			bColon = true;
			iFormatPosition++;
			if ( iFormatPosition >= Format.Size ) {
				*pFormatValid = false;
				return false;
			}
		} else if ( Format.Data[iFormatPosition] == '-' ) {
			bNoPad = true;
			iFormatPosition++;
			if ( iFormatPosition >= Format.Size ) {
				*pFormatValid = false;
				return false;
			}
		}
		iToken = Format.Data[iFormatPosition++];
		if ( bColon && (iToken != 'z') ) {
			*pFormatValid = false;
			return false;
		}
		if ( bNoPad && !__xrtTimeNoPadTokenValid(iToken) ) {
			*pFormatValid = false;
			return false;
		}
		if ( !__xrtTimeTokenValid(iToken) ) {
			*pFormatValid = false;
			return false;
		}
		if ( !__xrtTimeParseToken(Text, &iTextPosition, Format,
			iFormatPosition, iToken, bColon, bNoPad, &tState) ) {
			return false;
		}
	}
	if ( iTextPosition != Text.Size ) {
		return false;
	}
	if ( tState.HasHour12 ) {
		if ( !tState.HasAMPM || (tState.Value.Hour < 1) || (tState.Value.Hour > 12) ) {
			return false;
		}
		if ( (tState.AMPM == 1) && (tState.Value.Hour != 12) ) {
			tState.Value.Hour += 12;
		} else if ( (tState.AMPM == 0) && (tState.Value.Hour == 12) ) {
			tState.Value.Hour = 0;
		}
	} else if ( tState.HasAMPM ) {
		return false;
	}
	if ( tState.HasHour24 && tState.HasHour12 ) {
		return false;
	}
	if ( tState.HasYearDay ) {
		int iMonth = 1;
		int iDay = tState.YearDay;

		if ( (tState.YearDay < 1) ||
			 (tState.YearDay > xrtDaysInYear(tState.Value.Year)) ) {
			return false;
		}
		while ( iDay > xrtDaysInMonth(tState.Value.Year, iMonth) ) {
			iDay -= xrtDaysInMonth(tState.Value.Year, iMonth);
			iMonth++;
		}
		if ( (tState.HasMonth && (tState.Value.Month != iMonth)) ||
			 (tState.HasDay && (tState.Value.Day != iDay)) ) {
			return false;
		}
		tState.Value.Month = iMonth;
		tState.Value.Day = iDay;
		tState.HasMonth = true;
		tState.HasDay = true;
	}
	if ( tState.HasQuarter ) {
		if ( (tState.Quarter < 1) || (tState.Quarter > 4) ) {
			return false;
		}
		if ( tState.HasMonth ) {
			if ( (((tState.Value.Month - 1) / 3) + 1) != tState.Quarter ) {
				return false;
			}
		} else {
			tState.Value.Month = ((tState.Quarter - 1) * 3) + 1;
		}
	}
	if ( (__xrtTimeMakeValue(&tState.Value, &iTime) != __XRT_TIME_MAKE_OK) ||
		 !xrtTimeSplitAt(iTime, tState.Value.Offset, &tCanonical) ) {
		return false;
	}
	if ( tState.HasWeekday && (tState.Weekday != tCanonical.Weekday) ) {
		return false;
	}
	*pDateTime = tCanonical;
	return true;
}



/* 严格解析分解时间。 */
XRT_API bool xrtDateTimeParse(xstrview Text, xstrview Format, xdatetime* pDateTime)
{
	xdatetime tResult;
	bool bFormatValid;

	if ( (pDateTime == NULL) || !__xrtTimeTextViewValid(Text) ||
		 !__xrtTimeTextViewValid(Format) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( __xrtTimeParseFormat(Text, Format, &tResult, &bFormatValid) ) {
		*pDateTime = tResult;
		return true;
	}
	if ( !bFormatValid ) {
		__xrtTimeSetError(XERR_VALUE, XTIME_ERROR_FORMAT, "parse-format",
			"time format is malformed", 0);
		return false;
	}
	__xrtTimeSetError(XERR_VALUE, XTIME_ERROR_PARSE, "parse",
		"time text does not match the complete format", 0);
	return false;
}



/* 严格解析绝对时间。 */
XRT_API bool xrtTimeParse(xstrview Text, xstrview Format, xtime* pTime)
{
	xdatetime tDateTime;
	xtime iResult = 0;

	if ( pTime == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtDateTimeParse(Text, Format, &tDateTime) ||
		 !xrtTimeMake(&tDateTime, &iResult) ) {
		return false;
	}
	*pTime = iResult;
	return true;
}



/* 写入规范 RFC 3339，年份和偏移必须能被该协议无损表达。 */
XRT_API size_t xrtTimeWriteRFC3339(char* sBuffer, size_t iCapacity,
	xtime iTime, int iOffset)
{
	__xrt_time_writer tWriter;
	xdatetime tDateTime;
	char arrFraction[6];
	int iFractionSize = 6;
	int iFraction;

	if ( (sBuffer != NULL) && (iCapacity != 0) ) {
		sBuffer[0] = '\0';
	}
	if ( (sBuffer == NULL) && (iCapacity != 0) ) {
		__xrtErrorSetInvalidArgument();
		return XRT_NPOS;
	}
	if ( (iOffset % 60) != 0 ) {
		__xrtTimeSetError(XERR_RANGE, XTIME_ERROR_RANGE, "rfc3339-format",
			"RFC 3339 cannot represent UTC offset seconds", 0);
		return XRT_NPOS;
	}
	if ( !xrtTimeSplitAt(iTime, iOffset, &tDateTime) ) {
		return XRT_NPOS;
	}
	if ( (tDateTime.Year < 0) || (tDateTime.Year > 9999) ) {
		__xrtTimeSetError(XERR_RANGE, XTIME_ERROR_RANGE, "rfc3339-format",
			"RFC 3339 requires a four-digit non-negative year", 0);
		return XRT_NPOS;
	}

	__xrtTimeWriterInit(&tWriter, sBuffer, iCapacity);
	__xrtTimeWriterUInt(&tWriter, (uint64)tDateTime.Year, 4, '0');
	__xrtTimeWriterByte(&tWriter, '-');
	__xrtTimeWriterUInt(&tWriter, (uint64)tDateTime.Month, 2, '0');
	__xrtTimeWriterByte(&tWriter, '-');
	__xrtTimeWriterUInt(&tWriter, (uint64)tDateTime.Day, 2, '0');
	__xrtTimeWriterByte(&tWriter, 'T');
	__xrtTimeWriterUInt(&tWriter, (uint64)tDateTime.Hour, 2, '0');
	__xrtTimeWriterByte(&tWriter, ':');
	__xrtTimeWriterUInt(&tWriter, (uint64)tDateTime.Minute, 2, '0');
	__xrtTimeWriterByte(&tWriter, ':');
	__xrtTimeWriterUInt(&tWriter, (uint64)tDateTime.Second, 2, '0');
	if ( tDateTime.Microsecond != 0 ) {
		iFraction = tDateTime.Microsecond;
		for ( int i = 5; i >= 0; i-- ) {
			arrFraction[i] = (char)('0' + (iFraction % 10));
			iFraction /= 10;
		}
		while ( (iFractionSize > 0) && (arrFraction[iFractionSize - 1] == '0') ) {
			iFractionSize--;
		}
		__xrtTimeWriterByte(&tWriter, '.');
		__xrtTimeWriterText(&tWriter, arrFraction, (size_t)iFractionSize);
	}
	if ( iOffset == 0 ) {
		__xrtTimeWriterByte(&tWriter, 'Z');
	} else {
		__xrtTimeWriterOffset(&tWriter, iOffset, true);
	}
	return __xrtTimeWriterFinish(&tWriter, "rfc3339-format");
}



/* 创建 RFC 3339 文本。 */
XRT_API str xrtTimeRFC3339(xtime iTime, int iOffset)
{
	size_t iSize = xrtTimeWriteRFC3339(NULL, 0, iTime, iOffset);
	str sResult;

	if ( iSize == XRT_NPOS ) {
		return NULL;
	}
	sResult = __xrtTimeAllocText(iSize);
	if ( sResult == NULL ) {
		return NULL;
	}
	if ( xrtTimeWriteRFC3339(sResult, iSize + 1, iTime, iOffset) == XRT_NPOS ) {
		xrtFree(sResult);
		return NULL;
	}
	return sResult;
}



/* 无错误包装地解析一个 RFC 3339 值。 */
static bool __xrtTimeParseRFC3339Value(xstrview Text, xtime* pTime)
{
	xdatetime tDateTime;
	size_t iPosition = 0;
	size_t iFractionDigits = 0;
	int iValue;
	int iMicrosecond = 0;
	xtime iResult = 0;

	memset(&tDateTime, 0, sizeof(tDateTime));
	if ( !__xrtTimeParseDigits(Text, &iPosition, 4, &iValue) ) {
		return false;
	}
	tDateTime.Year = iValue;
	if ( (iPosition >= Text.Size) || (Text.Data[iPosition++] != '-') ) {
		return false;
	}
	if ( !__xrtTimeParseDigits(Text, &iPosition, 2, &tDateTime.Month) ) {
		return false;
	}
	if ( (iPosition >= Text.Size) || (Text.Data[iPosition++] != '-') ) {
		return false;
	}
	if ( !__xrtTimeParseDigits(Text, &iPosition, 2, &tDateTime.Day) ) {
		return false;
	}
	if ( (iPosition >= Text.Size) ||
		 ((Text.Data[iPosition] != 'T') && (Text.Data[iPosition] != 't')) ) {
		return false;
	}
	iPosition++;
	if ( !__xrtTimeParseDigits(Text, &iPosition, 2, &tDateTime.Hour) ) {
		return false;
	}
	if ( (iPosition >= Text.Size) || (Text.Data[iPosition++] != ':') ) {
		return false;
	}
	if ( !__xrtTimeParseDigits(Text, &iPosition, 2, &tDateTime.Minute) ) {
		return false;
	}
	if ( (iPosition >= Text.Size) || (Text.Data[iPosition++] != ':') ) {
		return false;
	}
	if ( !__xrtTimeParseDigits(Text, &iPosition, 2, &tDateTime.Second) ) {
		return false;
	}
	if ( (iPosition < Text.Size) && (Text.Data[iPosition] == '.') ) {
		iPosition++;
		while ( (iPosition < Text.Size) && (Text.Data[iPosition] >= '0') &&
			 (Text.Data[iPosition] <= '9') ) {
			if ( iFractionDigits < 6 ) {
				iMicrosecond = (iMicrosecond * 10) + (Text.Data[iPosition] - '0');
			}
			iFractionDigits++;
			iPosition++;
		}
		if ( iFractionDigits == 0 ) {
			return false;
		}
		while ( iFractionDigits < 6 ) {
			iMicrosecond *= 10;
			iFractionDigits++;
		}
		tDateTime.Microsecond = iMicrosecond;
	}
	if ( !__xrtTimeParseOffset(Text, &iPosition, true, &tDateTime.Offset) ||
		 (iPosition != Text.Size) ||
		 (__xrtTimeMakeValue(&tDateTime, &iResult) != __XRT_TIME_MAKE_OK) ) {
		return false;
	}
	*pTime = iResult;
	return true;
}



/* 严格解析 RFC 3339。 */
XRT_API bool xrtTimeParseRFC3339(xstrview Text, xtime* pTime)
{
	xtime iResult = 0;

	if ( (pTime == NULL) || !__xrtTimeTextViewValid(Text) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtTimeParseRFC3339Value(Text, &iResult) ) {
		__xrtTimeSetError(XERR_VALUE, XTIME_ERROR_PARSE, "rfc3339-parse",
			"text is not a complete RFC 3339 timestamp", 0);
		return false;
	}
	*pTime = iResult;
	return true;
}



/* 写入 HTTP IMF-fixdate。 */
XRT_API size_t xrtTimeWriteHTTPDate(char* sBuffer, size_t iCapacity, xtime iTime)
{
	__xrt_time_writer tWriter;
	xdatetime tUTC;

	if ( (sBuffer != NULL) && (iCapacity != 0) ) {
		sBuffer[0] = '\0';
	}
	if ( (sBuffer == NULL) && (iCapacity != 0) ) {
		__xrtErrorSetInvalidArgument();
		return XRT_NPOS;
	}
	if ( !xrtTimeSplit(iTime, &tUTC) ) {
		return XRT_NPOS;
	}
	if ( (tUTC.Year < 0) || (tUTC.Year > 9999) ) {
		__xrtTimeSetError(XERR_RANGE, XTIME_ERROR_RANGE, "http-date-format",
			"HTTP-date requires a four-digit non-negative year", 0);
		return XRT_NPOS;
	}

	__xrtTimeWriterInit(&tWriter, sBuffer, iCapacity);
	__xrtTimeWriterCStr(&tWriter, __xrtTimeWeekShort[tUTC.Weekday]);
	__xrtTimeWriterCStr(&tWriter, ", ");
	__xrtTimeWriterUInt(&tWriter, (uint64)tUTC.Day, 2, '0');
	__xrtTimeWriterByte(&tWriter, ' ');
	__xrtTimeWriterCStr(&tWriter, __xrtTimeMonthShort[tUTC.Month - 1]);
	__xrtTimeWriterByte(&tWriter, ' ');
	__xrtTimeWriterUInt(&tWriter, (uint64)tUTC.Year, 4, '0');
	__xrtTimeWriterByte(&tWriter, ' ');
	__xrtTimeWriterUInt(&tWriter, (uint64)tUTC.Hour, 2, '0');
	__xrtTimeWriterByte(&tWriter, ':');
	__xrtTimeWriterUInt(&tWriter, (uint64)tUTC.Minute, 2, '0');
	__xrtTimeWriterByte(&tWriter, ':');
	__xrtTimeWriterUInt(&tWriter, (uint64)tUTC.Second, 2, '0');
	__xrtTimeWriterCStr(&tWriter, " GMT");
	return __xrtTimeWriterFinish(&tWriter, "http-date-format");
}



/* 创建 HTTP IMF-fixdate。 */
XRT_API str xrtTimeHTTPDate(xtime iTime)
{
	size_t iSize = xrtTimeWriteHTTPDate(NULL, 0, iTime);
	str sResult;

	if ( iSize == XRT_NPOS ) {
		return NULL;
	}
	sResult = __xrtTimeAllocText(iSize);
	if ( sResult == NULL ) {
		return NULL;
	}
	if ( xrtTimeWriteHTTPDate(sResult, iSize + 1, iTime) == XRT_NPOS ) {
		xrtFree(sResult);
		return NULL;
	}
	return sResult;
}



/* 从 HTTP 字段构造并验证星期，避免接受自相矛盾的日期。 */
static bool __xrtTimeHTTPBuild(int iWeekday, int64 iYear, int iMonth, int iDay,
	int iHour, int iMinute, int iSecond, xtime* pTime)
{
	xtime iResult = 0;
	xdatetime tUTC;

	memset(&tUTC, 0, sizeof(tUTC));
	tUTC.Year = iYear;
	tUTC.Month = iMonth;
	tUTC.Day = iDay;
	tUTC.Hour = iHour;
	tUTC.Minute = iMinute;
	tUTC.Second = iSecond;
	if ( (__xrtTimeMakeValue(&tUTC, &iResult) != __XRT_TIME_MAKE_OK) ||
		 !xrtTimeSplit(iResult, &tUTC) || (tUTC.Weekday != iWeekday) ) {
		return false;
	}
	*pTime = iResult;
	return true;
}



/* 解析首选 IMF-fixdate。 */
static bool __xrtTimeParseIMFDate(xstrview Text, xtime* pTime)
{
	size_t iPosition = 0;
	int iWeekday;
	int iDay;
	int iMonth;
	int iYear;
	int iHour;
	int iMinute;
	int iSecond;

	if ( Text.Size != 29 ) {
		return false;
	}
	if ( !__xrtTimeParseWeekName(
			Text, &iPosition, false, true, &iWeekday
		) ||
		 (iPosition >= Text.Size) || (Text.Data[iPosition++] != ',') ||
		 (iPosition >= Text.Size) || (Text.Data[iPosition++] != ' ') ||
		 !__xrtTimeParseDigits(Text, &iPosition, 2, &iDay) ||
		 (iPosition >= Text.Size) || (Text.Data[iPosition++] != ' ') ||
		 !__xrtTimeParseMonthName(
			Text, &iPosition, false, true, &iMonth
		) ||
		 (iPosition >= Text.Size) || (Text.Data[iPosition++] != ' ') ||
		 !__xrtTimeParseDigits(Text, &iPosition, 4, &iYear) ||
		 (iPosition >= Text.Size) || (Text.Data[iPosition++] != ' ') ||
		 !__xrtTimeParseDigits(Text, &iPosition, 2, &iHour) ||
		 (iPosition >= Text.Size) || (Text.Data[iPosition++] != ':') ||
		 !__xrtTimeParseDigits(Text, &iPosition, 2, &iMinute) ||
		 (iPosition >= Text.Size) || (Text.Data[iPosition++] != ':') ||
		 !__xrtTimeParseDigits(Text, &iPosition, 2, &iSecond) ||
		 (iPosition >= Text.Size) || (Text.Data[iPosition++] != ' ') ||
		 ((Text.Size - iPosition) != 3) ||
		 (memcmp(Text.Data + iPosition, "GMT", 3) != 0) ) {
		return false;
	}
	return __xrtTimeHTTPBuild(iWeekday, iYear, iMonth, iDay,
		iHour, iMinute, iSecond, pTime);
}



/* 解析旧 RFC 850 日期，并按 HTTP 的五十年窗口解释两位年份。 */
static bool __xrtTimeParseRFC850Date(xstrview Text, xtime* pTime)
{
	size_t iPosition = 0;
	int iWeekday;
	int iDay;
	int iMonth;
	int iYear;
	int iHour;
	int iMinute;
	int iSecond;
	int64 iCurrentYear;
	int64 iCentury;
	int64 iCandidate;

	if ( !__xrtTimeParseWeekName(
			Text, &iPosition, true, true, &iWeekday
		) ||
		 (iPosition >= Text.Size) || (Text.Data[iPosition++] != ',') ||
		 (iPosition >= Text.Size) || (Text.Data[iPosition++] != ' ') ||
		 !__xrtTimeParseDigits(Text, &iPosition, 2, &iDay) ||
		 (iPosition >= Text.Size) || (Text.Data[iPosition++] != '-') ||
		 !__xrtTimeParseMonthName(
			Text, &iPosition, false, true, &iMonth
		) ||
		 (iPosition >= Text.Size) || (Text.Data[iPosition++] != '-') ||
		 !__xrtTimeParseDigits(Text, &iPosition, 2, &iYear) ||
		 (iPosition >= Text.Size) || (Text.Data[iPosition++] != ' ') ||
		 !__xrtTimeParseDigits(Text, &iPosition, 2, &iHour) ||
		 (iPosition >= Text.Size) || (Text.Data[iPosition++] != ':') ||
		 !__xrtTimeParseDigits(Text, &iPosition, 2, &iMinute) ||
		 (iPosition >= Text.Size) || (Text.Data[iPosition++] != ':') ||
		 !__xrtTimeParseDigits(Text, &iPosition, 2, &iSecond) ||
		 (iPosition >= Text.Size) || (Text.Data[iPosition++] != ' ') ||
		 ((Text.Size - iPosition) != 3) ||
		 (memcmp(Text.Data + iPosition, "GMT", 3) != 0) ) {
		return false;
	}
	iCurrentYear = xrtYear(xrtNow());
	iCentury = __xrtTimeFloorDiv(iCurrentYear, 100) * 100;
	iCandidate = iCentury + iYear;
	if ( iCandidate > (iCurrentYear + 50) ) {
		iCandidate -= 100;
	}
	return __xrtTimeHTTPBuild(iWeekday, iCandidate, iMonth, iDay,
		iHour, iMinute, iSecond, pTime);
}



/* 解析旧 ANSI C asctime 日期。 */
static bool __xrtTimeParseAsctimeDate(xstrview Text, xtime* pTime)
{
	size_t iPosition = 0;
	int iWeekday;
	int iMonth;
	int iDay;
	int iHour;
	int iMinute;
	int iSecond;
	int iYear;

	if ( Text.Size != 24 ) {
		return false;
	}
	if ( !__xrtTimeParseWeekName(
			Text, &iPosition, false, true, &iWeekday
		) ||
		 (iPosition >= Text.Size) || (Text.Data[iPosition++] != ' ') ||
		 !__xrtTimeParseMonthName(
			Text, &iPosition, false, true, &iMonth
		) ||
		 (iPosition >= Text.Size) || (Text.Data[iPosition++] != ' ') ) {
		return false;
	}
	if ( (iPosition < Text.Size) && (Text.Data[iPosition] == ' ') ) {
		iPosition++;
		if ( !__xrtTimeParseDigits(Text, &iPosition, 1, &iDay) ) {
			return false;
		}
	} else if ( !__xrtTimeParseDigits(Text, &iPosition, 2, &iDay) ) {
		return false;
	}
	if ( (iPosition >= Text.Size) || (Text.Data[iPosition++] != ' ') ||
		 !__xrtTimeParseDigits(Text, &iPosition, 2, &iHour) ||
		 (iPosition >= Text.Size) || (Text.Data[iPosition++] != ':') ||
		 !__xrtTimeParseDigits(Text, &iPosition, 2, &iMinute) ||
		 (iPosition >= Text.Size) || (Text.Data[iPosition++] != ':') ||
		 !__xrtTimeParseDigits(Text, &iPosition, 2, &iSecond) ||
		 (iPosition >= Text.Size) || (Text.Data[iPosition++] != ' ') ||
		 !__xrtTimeParseDigits(Text, &iPosition, 4, &iYear) ||
		 (iPosition != Text.Size) ) {
		return false;
	}
	return __xrtTimeHTTPBuild(iWeekday, iYear, iMonth, iDay,
		iHour, iMinute, iSecond, pTime);
}



/* 无错误副作用地尝试解析三种 HTTP 日期格式。 */
XRT_API bool xrtTimeTryParseHTTPDate(
	xstrview Text,
	xtime* pTime
)
{
	xtime iResult = 0;
	bool bParsed = false;

	if ( !__xrtRangeValid(pTime, sizeof(*pTime)) ||
		!__xrtRangeValid(Text.Data, Text.Size) ) {
		return false;
	}
	if ( (Text.Size == 29) && (Text.Data[3] == ',') ) {
		bParsed = __xrtTimeParseIMFDate(Text, &iResult);
	} else if ( Text.Size == 24 ) {
		bParsed = __xrtTimeParseAsctimeDate(Text, &iResult);
	} else if ( (Text.Size >= 30) && (Text.Size <= 33) &&
		 (memcmp(Text.Data + Text.Size - 3, "GMT", 3) == 0) ) {
		bParsed = __xrtTimeParseRFC850Date(Text, &iResult);
	}
	if ( !bParsed ) {
		return false;
	}
	*pTime = iResult;
	return true;
}



/* 解析三种 HTTP 日期格式。 */
XRT_API bool xrtTimeParseHTTPDate(xstrview Text, xtime* pTime)
{
	if ( (pTime == NULL) || !__xrtTimeTextViewValid(Text) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtTimeTryParseHTTPDate(Text, pTime) ) {
		__xrtTimeSetError(
			XERR_VALUE,
			XTIME_ERROR_PARSE,
			"http-date-parse",
			"text is not a supported complete HTTP-date",
			0
		);
		return false;
	}
	return true;
}



/* 判断文本是否全部由 ASCII 数字组成。 */
static bool __xrtTimeAllDigits(xstrview Text)
{
	for ( size_t i = 0; i < Text.Size; i++ ) {
		if ( (Text.Data[i] < '0') || (Text.Data[i] > '9') ) {
			return false;
		}
	}
	return true;
}



/* 根据明确形状选择唯一解析器，避免试探路径产生多次错误通知。 */
XRT_API bool xrtTimeParseAny(xstrview Text, xtime* pTime)
{
	if ( (pTime == NULL) || !__xrtTimeTextViewValid(Text) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (Text.Size >= 20) && (Text.Data[4] == '-') && (Text.Data[7] == '-') &&
		 ((Text.Data[10] == 'T') || (Text.Data[10] == 't')) ) {
		return xrtTimeParseRFC3339(Text, pTime);
	}
	if ( ((Text.Size >= 3) &&
		 __xrtTimeTextCaseEqual(Text.Data + Text.Size - 3, 3, "GMT")) ||
		 (Text.Size == 24) ) {
		return xrtTimeParseHTTPDate(Text, pTime);
	}
	if ( (Text.Size == 19) && (Text.Data[4] == '-') && (Text.Data[7] == '-') &&
		 (Text.Data[10] == ' ') ) {
		return xrtTimeParse(Text, XRT_STR_LITERAL("%Y-%m-%d %T"), pTime);
	}
	if ( (Text.Size == 19) && (Text.Data[4] == '/') && (Text.Data[7] == '/') &&
		 (Text.Data[10] == ' ') ) {
		return xrtTimeParse(Text, XRT_STR_LITERAL("%Y/%m/%d %T"), pTime);
	}
	if ( (Text.Size == 19) && (Text.Data[4] == '.') && (Text.Data[7] == '.') &&
		 (Text.Data[10] == ' ') ) {
		return xrtTimeParse(Text, XRT_STR_LITERAL("%Y.%m.%d %T"), pTime);
	}
	if ( (Text.Size == 10) && (Text.Data[4] == '-') && (Text.Data[7] == '-') ) {
		return xrtTimeParse(Text, XRT_STR_LITERAL("%Y-%m-%d"), pTime);
	}
	if ( (Text.Size == 10) && (Text.Data[4] == '/') && (Text.Data[7] == '/') ) {
		return xrtTimeParse(Text, XRT_STR_LITERAL("%Y/%m/%d"), pTime);
	}
	if ( (Text.Size == 14) && __xrtTimeAllDigits(Text) ) {
		return xrtTimeParse(Text, XRT_STR_LITERAL("%Y%m%d%H%M%S"), pTime);
	}
	if ( (Text.Size == 15) && (Text.Data[8] == ' ') &&
		 __xrtTimeAllDigits((xstrview){ Text.Data, 8 }) &&
		 __xrtTimeAllDigits((xstrview){ Text.Data + 9, 6 }) ) {
		return xrtTimeParse(Text, XRT_STR_LITERAL("%Y%m%d %H%M%S"), pTime);
	}
	if ( (Text.Size == 8) && __xrtTimeAllDigits(Text) ) {
		return xrtTimeParse(Text, XRT_STR_LITERAL("%Y%m%d"), pTime);
	}
	if ( (Text.Size == 8) && (Text.Data[2] == ':') && (Text.Data[5] == ':') ) {
		return xrtTimeParse(Text, XRT_STR_LITERAL("%T"), pTime);
	}
	__xrtTimeSetError(XERR_VALUE, XTIME_ERROR_PARSE, "parse-any",
		"time text does not match a supported complete shape", 0);
	return false;
}

#endif
