#include "../internal/xrt_mail.h"



#if defined(XMAIL_FEATURE_MAIL_DATE)

/* 规范输出始终使用带星期、秒和数字时区的现代格式。 */
static xstrview __xrtMailDateFormat(void)
{
	static const char sFormat[] = "%a, %d %b %Y %H:%M:%S %z";

	return __xrtMailView(sFormat, sizeof(sFormat) - 1u);
}



/* 根据可选星期和秒选择唯一解析格式，避免探测失败污染错误状态。 */
static xstrview __xrtMailDateParseFormat(bool bWeekday, bool bSecond)
{
	if ( bWeekday ) {
		static const char sWithSecond[] = "%a, %-d %b %Y %H:%M:%S %z";
		static const char sWithoutSecond[] = "%a, %-d %b %Y %H:%M %z";

		return bSecond ?
			__xrtMailView(sWithSecond, sizeof(sWithSecond) - 1u) :
			__xrtMailView(sWithoutSecond, sizeof(sWithoutSecond) - 1u);
	}
	{
		static const char sWithSecond[] = "%-d %b %Y %H:%M:%S %z";
		static const char sWithoutSecond[] = "%-d %b %Y %H:%M %z";

		return bSecond ?
			__xrtMailView(sWithSecond, sizeof(sWithSecond) - 1u) :
			__xrtMailView(sWithoutSecond, sizeof(sWithoutSecond) - 1u);
	}
}



/* 去掉日期字段外侧的线性空白。 */
static xstrview __xrtMailDateTrim(xstrview Text)
{
	size_t iStart = 0;
	size_t iEnd = Text.Size;

	while ( (iStart < iEnd) &&
		 ((Text.Data[iStart] == ' ') || (Text.Data[iStart] == '\t')) ) {
		iStart++;
	}
	while ( (iEnd > iStart) &&
		 ((Text.Data[iEnd - 1u] == ' ') || (Text.Data[iEnd - 1u] == '\t')) ) {
		iEnd--;
	}
	return __xrtMailSlice(Text, iStart, iEnd - iStart);
}



/* 按线性空白拆分兼容日期，注释和折行仍由上层负责处理。 */
static bool __xrtMailDateTokens(
	xstrview Text,
	xstrview arrToken[6],
	size_t* pCount
)
{
	size_t iPosition = 0;
	size_t iCount = 0;

	while ( iPosition < Text.Size ) {
		size_t iStart;

		while ( (iPosition < Text.Size) &&
			 ((Text.Data[iPosition] == ' ') ||
			  (Text.Data[iPosition] == '\t')) ) {
			iPosition++;
		}
		if ( iPosition == Text.Size ) {
			break;
		}
		if ( iCount == 6u ) {
			return false;
		}
		iStart = iPosition;
		while ( (iPosition < Text.Size) &&
			 (Text.Data[iPosition] != ' ') &&
			 (Text.Data[iPosition] != '\t') ) {
			iPosition++;
		}
		arrToken[iCount++] = __xrtMailSlice(
			Text,
			iStart,
			iPosition - iStart
		);
	}
	*pCount = iCount;
	return (iCount == 5u) || (iCount == 6u);
}



/* 解析两位、三位或四位邮件年份并转换成现代四位形式。 */
static bool __xrtMailDateYear(xstrview Text, int* pYear)
{
	int iYear = 0;

	if ( (Text.Size < 2u) || (Text.Size > 4u) ) {
		return false;
	}
	for ( size_t i = 0; i < Text.Size; i++ ) {
		if ( (Text.Data[i] < '0') || (Text.Data[i] > '9') ) {
			return false;
		}
		iYear = (iYear * 10) + (int)(Text.Data[i] - '0');
	}
	if ( Text.Size == 2u ) {
		iYear += iYear < 50 ? 2000 : 1900;
	} else if ( Text.Size == 3u ) {
		iYear += 1900;
	}
	if ( (iYear < 1900) || (iYear > 9999) ) {
		return false;
	}
	*pYear = iYear;
	return true;
}



/* 把过时命名时区转换成 RFC 5322 数字偏移。 */
static bool __xrtMailDateZone(xstrview Text, char arrZone[5])
{
	static const struct {
		cstr Name;
		cstr Zone;
	} arrNamed[] = {
		{ "UT", "+0000" }, { "GMT", "+0000" },
		{ "EST", "-0500" }, { "EDT", "-0400" },
		{ "CST", "-0600" }, { "CDT", "-0500" },
		{ "MST", "-0700" }, { "MDT", "-0600" },
		{ "PST", "-0800" }, { "PDT", "-0700" }
	};

	if ( (Text.Size == 5u) &&
		 ((Text.Data[0] == '+') || (Text.Data[0] == '-')) ) {
		for ( size_t i = 1u; i < 5u; i++ ) {
			if ( (Text.Data[i] < '0') || (Text.Data[i] > '9') ) {
				return false;
			}
		}
		memcpy(arrZone, Text.Data, 5u);
		return true;
	}
	for ( size_t i = 0; i < (sizeof(arrNamed) / sizeof(arrNamed[0])); i++ ) {
		if ( __xrtMailAsciiEqualI(
			Text,
			__xrtMailView(arrNamed[i].Name, strlen(arrNamed[i].Name))
		) ) {
			memcpy(arrZone, arrNamed[i].Zone, 5u);
			return true;
		}
	}
	if ( Text.Size == 1u ) {
		unsigned char iLetter = __xrtMailAsciiLower(
			(unsigned char)Text.Data[0]
		);
		int iHour;

		if ( iLetter == (unsigned char)'z' ) {
			memcpy(arrZone, "+0000", 5u);
			return true;
		}
		if ( (iLetter >= (unsigned char)'a') &&
			 (iLetter <= (unsigned char)'i') ) {
			iHour = (int)(iLetter - (unsigned char)'a') + 1;
			arrZone[0] = '+';
		} else if ( (iLetter >= (unsigned char)'k') &&
			 (iLetter <= (unsigned char)'m') ) {
			iHour = (int)(iLetter - (unsigned char)'k') + 10;
			arrZone[0] = '+';
		} else if ( (iLetter >= (unsigned char)'n') &&
			 (iLetter <= (unsigned char)'y') ) {
			iHour = (int)(iLetter - (unsigned char)'n') + 1;
			arrZone[0] = '-';
		} else {
			return false;
		}
		arrZone[1] = (char)('0' + (iHour / 10));
		arrZone[2] = (char)('0' + (iHour % 10));
		arrZone[3] = '0';
		arrZone[4] = '0';
		return true;
	}
	return false;
}



/* 向固定规范缓冲追加一个由空格分隔的片段。 */
static bool __xrtMailDateAppend(
	char* sOutput,
	size_t iCapacity,
	size_t* pPosition,
	const char* sText,
	size_t iSize
)
{
	size_t iSeparator = *pPosition != 0 ? 1u : 0u;

	if ( (*pPosition > iCapacity) ||
		 (iSeparator > (iCapacity - *pPosition)) ||
		 (iSize > (iCapacity - *pPosition - iSeparator)) ) {
		return false;
	}
	if ( *pPosition != 0 ) {
		sOutput[(*pPosition)++] = ' ';
	}
	memcpy(sOutput + *pPosition, sText, iSize);
	*pPosition += iSize;
	return true;
}



/* 把兼容输入归一成现代数字时区和四位年份。 */
static bool __xrtMailDateNormalize(
	xstrview Text,
	char* sOutput,
	size_t iCapacity,
	xstrview* pNormalized
)
{
	xstrview arrToken[6];
	size_t iCount;
	size_t iBase;
	size_t iPosition = 0;
	int iYear;
	char arrYear[4];
	char arrZone[5];

	if ( !__xrtMailDateTokens(Text, arrToken, &iCount) ) {
		return false;
	}
	iBase = iCount == 6u ? 1u : 0u;
	if ( (iBase != 0u) &&
		 ((arrToken[0].Size != 4u) ||
		  (arrToken[0].Data[3] != ',')) ) {
		return false;
	}
	if ( !__xrtMailDateYear(arrToken[iBase + 2u], &iYear) ||
		 !__xrtMailDateZone(arrToken[iBase + 4u], arrZone) ) {
		return false;
	}
	arrYear[0] = (char)('0' + ((iYear / 1000) % 10));
	arrYear[1] = (char)('0' + ((iYear / 100) % 10));
	arrYear[2] = (char)('0' + ((iYear / 10) % 10));
	arrYear[3] = (char)('0' + (iYear % 10));
	if ( ((iBase != 0u) && !__xrtMailDateAppend(
		sOutput, iCapacity, &iPosition,
		arrToken[0].Data, arrToken[0].Size
	)) || !__xrtMailDateAppend(
		sOutput, iCapacity, &iPosition,
		arrToken[iBase].Data, arrToken[iBase].Size
	) || !__xrtMailDateAppend(
		sOutput, iCapacity, &iPosition,
		arrToken[iBase + 1u].Data, arrToken[iBase + 1u].Size
	) || !__xrtMailDateAppend(
		sOutput, iCapacity, &iPosition, arrYear, sizeof(arrYear)
	) || !__xrtMailDateAppend(
		sOutput, iCapacity, &iPosition,
		arrToken[iBase + 3u].Data, arrToken[iBase + 3u].Size
	) || !__xrtMailDateAppend(
		sOutput, iCapacity, &iPosition, arrZone, sizeof(arrZone)
	) ) {
		return false;
	}
	*pNormalized = __xrtMailView(sOutput, iPosition);
	return true;
}



/* 校验邮件日期可以无损表达的年份和时区。 */
static bool __xrtMailDateRange(const xdatetime* pDateTime)
{
	if ( (pDateTime->Year < 1900) || (pDateTime->Year > 9999) ||
		 ((pDateTime->Offset % 60) != 0) ) {
		__xrtMailError(
			XERR_RANGE,
			XMAIL_ERROR_HEADER,
			"mail date is outside the RFC 5322 range"
		);
		return false;
	}
	return true;
}



/* 按规范形式写入邮件日期。 */
XRT_API bool xrtMailDateWrite(
	xtime iTime,
	int iOffset,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	xdatetime DateTime;
	size_t iRequired;

	if ( !xrtMemRangeValid(sOutput, iCapacity) ||
		 !xrtMemRangeValid(pOutputSize, sizeof(*pOutputSize)) ||
		 ((sOutput != NULL) && xrtMemRangesOverlap(
			pOutputSize,
			sizeof(*pOutputSize),
			sOutput,
			iCapacity
		)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !xrtTimeSplitAt(iTime, iOffset, &DateTime) ||
		 !__xrtMailDateRange(&DateTime) ) {
		return false;
	}
	iRequired = xrtTimeWrite(NULL, 0, iTime, iOffset, __xrtMailDateFormat());
	if ( iRequired == XRT_NPOS ) {
		return false;
	}
	if ( sOutput == NULL ) {
		*pOutputSize = iRequired;
		return true;
	}
	if ( iCapacity <= iRequired ) {
		*pOutputSize = iRequired;
		__xrtMailSetRange();
		return false;
	}
	if ( xrtTimeWrite(
		sOutput,
		iCapacity,
		iTime,
		iOffset,
		__xrtMailDateFormat()
	) != iRequired ) {
		return false;
	}
	*pOutputSize = iRequired;
	return true;
}



/* 创建独立的规范邮件日期。 */
XRT_API str xrtMailDate(xtime iTime, int iOffset, size_t* pOutputSize)
{
	size_t iRequired;
	str sOutput;

	if ( !xrtMemRangeValid(
		pOutputSize,
		pOutputSize != NULL ? sizeof(*pOutputSize) : 0
	) || !xrtMailDateWrite(iTime, iOffset, NULL, 0, &iRequired) ) {
		return NULL;
	}
	sOutput = (str)xrtMalloc(iRequired + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !xrtMailDateWrite(
		iTime,
		iOffset,
		sOutput,
		iRequired + 1u,
		&iRequired
	) ) {
		xrtFree(sOutput);
		return NULL;
	}
	if ( pOutputSize != NULL ) {
		*pOutputSize = iRequired;
	}
	return sOutput;
}



/* 解析 RFC 5322 日期并保留原时区偏移。 */
XRT_API bool xrtMailDateParse(
	xstrview Text,
	uint32 iFlags,
	xtime* pTime,
	int* pOffset
)
{
	char arrNormalized[96];
	xstrview Trimmed;
	xdatetime DateTime;
	xtime iTime;
	bool bWeekday;
	size_t iColonCount = 0;

	if ( !__xrtMailViewValid(Text) ||
		 (iFlags & ~(uint32)XMAIL_DATE_RELAXED) ||
		 !xrtMemRangeValid(pTime, sizeof(*pTime)) ||
		 !xrtMemRangeValid(pOffset, pOffset != NULL ? sizeof(*pOffset) : 0) ||
		 xrtMemRangesOverlap(pTime, sizeof(*pTime), Text.Data, Text.Size) ||
		 ((pOffset != NULL) &&
		  (xrtMemRangesOverlap(pOffset, sizeof(*pOffset), Text.Data, Text.Size) ||
		   xrtMemRangesOverlap(pTime, sizeof(*pTime), pOffset, sizeof(*pOffset)))) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	Trimmed = __xrtMailDateTrim(Text);
	if ( Trimmed.Size == 0 ) {
		__xrtMailError(XERR_PROTOCOL, XMAIL_ERROR_HEADER, "mail date is empty");
		return false;
	}
	if ( (iFlags & (uint32)XMAIL_DATE_RELAXED) != 0u ) {
		xstrview Normalized;

		if ( !__xrtMailDateNormalize(
			Trimmed,
			arrNormalized,
			sizeof(arrNormalized),
			&Normalized
		) ) {
			__xrtMailError(
				XERR_PROTOCOL,
				XMAIL_ERROR_HEADER,
				"obsolete mail date is invalid"
			);
			return false;
		}
		Trimmed = Normalized;
	}
	for ( size_t i = 0; i < Trimmed.Size; i++ ) {
		if ( Trimmed.Data[i] == ':' ) {
			iColonCount++;
		} else if ( (Trimmed.Data[i] == '\r') || (Trimmed.Data[i] == '\n') ||
			 (Trimmed.Data[i] == 0) ) {
			__xrtMailError(
				XERR_PROTOCOL,
				XMAIL_ERROR_HEADER,
				"mail date contains an invalid byte"
			);
			return false;
		}
	}
	bWeekday = (Trimmed.Size > 3u) && (Trimmed.Data[3] == ',');
	if ( ((iColonCount != 1u) && (iColonCount != 2u)) ||
		 !xrtDateTimeParse(
			Trimmed,
			__xrtMailDateParseFormat(bWeekday, iColonCount == 2u),
			&DateTime
		) || !__xrtMailDateRange(&DateTime) ||
		 !xrtTimeMake(&DateTime, &iTime) ) {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_HEADER,
			"mail date does not match RFC 5322"
		);
		return false;
	}
	*pTime = iTime;
	if ( pOffset != NULL ) {
		*pOffset = DateTime.Offset;
	}
	return true;
}

#endif
