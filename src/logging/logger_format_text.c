#include "../internal/xrt_internal.h"
#include <xrt/logger.h>

#include <stdio.h>



#if defined(XRT_FEATURE_LOGGER_FORMAT_TEXT)

/* 流式格式器保存 Writer、已写长度和失败状态。 */
typedef struct xlogtextwriter {
	xlogwriteproc Write;
	ptr UserData;
	size_t Written;
	bool Failed;
} xlogtextwriter;



/* 判断文本配置标志和固定偏移是否合法。 */
static bool __xrtLogTextConfigValid(const xlogtextconfig* pConfig)
{
	const uint32 iKnown =
		XLOG_TEXT_TIME |
		XLOG_TEXT_LEVEL |
		XLOG_TEXT_LOGGER |
		XLOG_TEXT_SOURCE |
		XLOG_TEXT_THREAD |
		XLOG_TEXT_FIELDS |
		XLOG_TEXT_NEWLINE |
		XLOG_TEXT_RAW_MESSAGE;

	return
		(pConfig != NULL) &&
		((pConfig->Flags & ~iKnown) == 0) &&
		(pConfig->UtcOffset >= -86399) &&
		(pConfig->UtcOffset <= 86399);
}



/* 建立 Writer 没有提供具体原因时的稳定输出错误。 */
static void __xrtLogTextOutputError(void)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = XERR_IO;
	Desc.Code = XLOG_ERROR_TEXT_OUTPUT;
	Desc.Domain = "xrt.log";
	Desc.Operation = "format-text";
	Desc.Message = "log text writer failed";
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 提交一段借用字节并检查总长度溢出。 */
static bool __xrtLogTextEmit(
	xlogtextwriter* pWriter,
	const void* pData,
	size_t iSize
)
{
	if ( pWriter->Failed ) {
		return false;
	}
	if ( iSize == 0 ) {
		return true;
	}
	if ( pWriter->Written > (SIZE_MAX - iSize) ) {
		__xrtErrorSetSizeOverflow();
		pWriter->Failed = true;
		return false;
	}
	if ( !pWriter->Write(
		(xbytesview){ (cbytes)pData, iSize },
		pWriter->UserData
	) ) {
		pWriter->Failed = true;
		return false;
	}
	pWriter->Written += iSize;
	return true;
}



/* 提交静态 ASCII 文本。 */
static bool __xrtLogTextAscii(xlogtextwriter* pWriter, cstr sText)
{
	return __xrtLogTextEmit(pWriter, sText, strlen(sText));
}



/* 提交字符串视图原始字节。 */
static bool __xrtLogTextView(xlogtextwriter* pWriter, xstrview Text)
{
	return __xrtLogTextEmit(pWriter, Text.Data, Text.Size);
}



/* 把保证非空的错误文本转换为借用视图。 */
static xstrview __xrtLogTextCString(cstr sText)
{
	return (xstrview){ sText, strlen(sText) };
}



/* 把控制字节和反斜杠转义为稳定单行文本。 */
static bool __xrtLogTextEscape(
	xlogtextwriter* pWriter,
	xstrview Text,
	bool bQuote
)
{
	static const char sHex[] = "0123456789ABCDEF";
	size_t iSegment = 0;

	if ( bQuote && !__xrtLogTextAscii(pWriter, "\"") ) {
		return false;
	}
	for ( size_t i = 0; i < Text.Size; i++ ) {
		uint8 iByte = (uint8)Text.Data[i];
		bool bEscape =
			(iByte < UINT8_C(0x20)) ||
			(iByte == UINT8_C(0x7F)) ||
			(iByte == (uint8)'\\') ||
			(bQuote && (iByte == (uint8)'\"'));

		if ( !bEscape ) {
			continue;
		}
		if (
			(i > iSegment) &&
			!__xrtLogTextEmit(pWriter, Text.Data + iSegment, i - iSegment)
		) {
			return false;
		}
		if ( iByte == (uint8)'\b' ) {
			if ( !__xrtLogTextAscii(pWriter, "\\b") ) {
				return false;
			}
		} else if ( iByte == (uint8)'\f' ) {
			if ( !__xrtLogTextAscii(pWriter, "\\f") ) {
				return false;
			}
		} else if ( iByte == (uint8)'\n' ) {
			if ( !__xrtLogTextAscii(pWriter, "\\n") ) {
				return false;
			}
		} else if ( iByte == (uint8)'\r' ) {
			if ( !__xrtLogTextAscii(pWriter, "\\r") ) {
				return false;
			}
		} else if ( iByte == (uint8)'\t' ) {
			if ( !__xrtLogTextAscii(pWriter, "\\t") ) {
				return false;
			}
		} else if (
			(iByte == (uint8)'\\') ||
			(iByte == (uint8)'\"')
		) {
			char arrEscape[2] = { '\\', (char)iByte };

			if ( !__xrtLogTextEmit(pWriter, arrEscape, sizeof(arrEscape)) ) {
				return false;
			}
		} else {
			char arrEscape[4] = {
				'\\', 'x', sHex[iByte >> 4], sHex[iByte & UINT8_C(0x0F)]
			};

			if ( !__xrtLogTextEmit(pWriter, arrEscape, sizeof(arrEscape)) ) {
				return false;
			}
		}
		iSegment = i + 1u;
	}
	if (
		(Text.Size > iSegment) &&
		!__xrtLogTextEmit(
			pWriter,
			Text.Data + iSegment,
			Text.Size - iSegment
		)
	) {
		return false;
	}
	return !bQuote || __xrtLogTextAscii(pWriter, "\"");
}



/* 判断字段或元数据名称可以直接写为单个 token。 */
static bool __xrtLogTextToken(xstrview Text)
{
	if ( Text.Size == 0 ) {
		return false;
	}
	for ( size_t i = 0; i < Text.Size; i++ ) {
		uint8 iByte = (uint8)Text.Data[i];
		bool bValid =
			((iByte >= (uint8)'a') && (iByte <= (uint8)'z')) ||
			((iByte >= (uint8)'A') && (iByte <= (uint8)'Z')) ||
			((iByte >= (uint8)'0') && (iByte <= (uint8)'9')) ||
			(iByte == (uint8)'_') || (iByte == (uint8)'-') ||
			(iByte == (uint8)'.') || (iByte == (uint8)'/') ||
			(iByte == (uint8)'\\');

		if ( !bValid ) {
			return false;
		}
	}
	return true;
}



/* 写出 token，必要时使用带转义的引号。 */
static bool __xrtLogTextName(xlogtextwriter* pWriter, xstrview Text)
{
	return __xrtLogTextToken(Text)
		? __xrtLogTextView(pWriter, Text)
		: __xrtLogTextEscape(pWriter, Text, true);
}



/* 写出有符号十进制整数。 */
static bool __xrtLogTextInt(xlogtextwriter* pWriter, int64 iValue)
{
	char arrText[32];
	size_t iSize;

	return
		xrtIntWrite(iValue, 10u, arrText, sizeof(arrText), &iSize, 0) &&
		__xrtLogTextEmit(pWriter, arrText, iSize);
}



/* 写出无符号十进制整数。 */
static bool __xrtLogTextUInt(xlogtextwriter* pWriter, uint64 iValue)
{
	char arrText[32];
	size_t iSize;

	return
		xrtUIntWrite(iValue, 10u, arrText, sizeof(arrText), &iSize, 0) &&
		__xrtLogTextEmit(pWriter, arrText, iSize);
}



/* 写出稳定往返浮点文本。 */
static bool __xrtLogTextFloat(xlogtextwriter* pWriter, double fValue)
{
	char arrText[64];
	size_t iSize;

	return
		xrtNumWrite(fValue, arrText, sizeof(arrText), &iSize, 0) &&
		__xrtLogTextEmit(pWriter, arrText, iSize);
}



/* 写出带微秒和固定 UTC 偏移的时间文本。 */
static bool __xrtLogTextTime(
	xlogtextwriter* pWriter,
	xtime iTime,
	int iOffset
)
{
	xdatetime DateTime;
	char arrText[96];
	int iSize;
	int iAbsolute;
	int iOffsetHour;
	int iOffsetMinute;
	int iOffsetSecond;

	if ( !xrtTimeSplitAt(iTime, iOffset, &DateTime) ) {
		return false;
	}
	if ( iOffset == 0 ) {
		iSize = snprintf(
			arrText,
			sizeof(arrText),
			"%lld-%02d-%02dT%02d:%02d:%02d.%06dZ",
			(long long)DateTime.Year,
			DateTime.Month,
			DateTime.Day,
			DateTime.Hour,
			DateTime.Minute,
			DateTime.Second,
			DateTime.Microsecond
		);
	} else {
		iAbsolute = iOffset < 0 ? -iOffset : iOffset;
		iOffsetHour = iAbsolute / 3600;
		iOffsetMinute = (iAbsolute / 60) % 60;
		iOffsetSecond = iAbsolute % 60;
		if ( iOffsetSecond == 0 ) {
			iSize = snprintf(
				arrText,
				sizeof(arrText),
				"%lld-%02d-%02dT%02d:%02d:%02d.%06d%c%02d:%02d",
				(long long)DateTime.Year,
				DateTime.Month,
				DateTime.Day,
				DateTime.Hour,
				DateTime.Minute,
				DateTime.Second,
				DateTime.Microsecond,
				iOffset < 0 ? '-' : '+',
				iOffsetHour,
				iOffsetMinute
			);
		} else {
			iSize = snprintf(
				arrText,
				sizeof(arrText),
				"%lld-%02d-%02dT%02d:%02d:%02d.%06d%c%02d:%02d:%02d",
				(long long)DateTime.Year,
				DateTime.Month,
				DateTime.Day,
				DateTime.Hour,
				DateTime.Minute,
				DateTime.Second,
				DateTime.Microsecond,
				iOffset < 0 ? '-' : '+',
				iOffsetHour,
				iOffsetMinute,
				iOffsetSecond
			);
		}
	}
	if ( (iSize < 0) || ((size_t)iSize >= sizeof(arrText)) ) {
		__xrtErrorSetInternal();
		return false;
	}
	return __xrtLogTextEmit(pWriter, arrText, (size_t)iSize);
}



/* 写出一个字段值。 */
static bool __xrtLogTextFieldValue(
	xlogtextwriter* pWriter,
	const xlogfield* pField,
	int iOffset
)
{
	if ( pField->Type == XLOG_FIELD_NULL ) {
		return __xrtLogTextAscii(pWriter, "null");
	}
	if ( pField->Type == XLOG_FIELD_BOOL ) {
		return __xrtLogTextAscii(
			pWriter,
			pField->Value.Boolean ? "true" : "false"
		);
	}
	if ( pField->Type == XLOG_FIELD_INT ) {
		return __xrtLogTextInt(pWriter, pField->Value.Integer);
	}
	if ( pField->Type == XLOG_FIELD_UINT ) {
		return __xrtLogTextUInt(pWriter, pField->Value.Unsigned);
	}
	if ( pField->Type == XLOG_FIELD_FLOAT ) {
		return __xrtLogTextFloat(pWriter, pField->Value.Float);
	}
	if ( pField->Type == XLOG_FIELD_STRING ) {
		return __xrtLogTextEscape(pWriter, pField->Value.String, true);
	}
	if ( pField->Type == XLOG_FIELD_TIME ) {
		return __xrtLogTextTime(pWriter, pField->Value.Time, iOffset);
	}
	if ( pField->Value.Error == NULL ) {
		return __xrtLogTextAscii(pWriter, "null");
	}
	return
		__xrtLogTextAscii(pWriter, "{kind=") &&
		__xrtLogTextInt(
			pWriter,
			(int64)xrtErrorKind(pField->Value.Error)
		) &&
		__xrtLogTextAscii(pWriter, ",domain=") &&
		__xrtLogTextEscape(
			pWriter,
			__xrtLogTextCString(xrtErrorDomain(pField->Value.Error)),
			true
		) &&
		__xrtLogTextAscii(pWriter, ",code=") &&
		__xrtLogTextInt(
			pWriter,
			(int64)xrtErrorCode(pField->Value.Error)
		) &&
		__xrtLogTextAscii(pWriter, ",message=") &&
		__xrtLogTextEscape(
			pWriter,
			__xrtLogTextCString(xrtErrorMessage(pField->Value.Error)),
			true
		) &&
		__xrtLogTextAscii(pWriter, "}");
}



/* 写出全部结构化字段。 */
static bool __xrtLogTextFields(
	xlogtextwriter* pWriter,
	const xlogrecord* pRecord,
	int iOffset
)
{
	for ( size_t i = 0; i < pRecord->FieldCount; i++ ) {
		if (
			!__xrtLogTextAscii(pWriter, " ") ||
			!__xrtLogTextName(pWriter, pRecord->Fields[i].Name) ||
			!__xrtLogTextAscii(pWriter, "=") ||
			!__xrtLogTextFieldValue(
				pWriter,
				&pRecord->Fields[i],
				iOffset
			)
		) {
			return false;
		}
	}
	return true;
}



/* 写出一个前缀项，并维护统一空格分隔。 */
static bool __xrtLogTextPrefixSpace(
	xlogtextwriter* pWriter,
	bool* pAny
)
{
	if ( *pAny && !__xrtLogTextAscii(pWriter, " ") ) {
		return false;
	}
	*pAny = true;
	return true;
}



/* 按完整、简单或纯消息预设初始化配置。 */
XRT_API bool xrtLogTextConfigInit(
	xlogtextconfig* pConfig,
	xlogtextstyle Style
)
{
	if (
		(pConfig == NULL) ||
		(Style < XLOG_TEXT_FULL) ||
		(Style > XLOG_TEXT_MESSAGE)
	) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pConfig->UtcOffset = 0;
	if ( Style == XLOG_TEXT_FULL ) {
		pConfig->Flags =
			XLOG_TEXT_TIME |
			XLOG_TEXT_LEVEL |
			XLOG_TEXT_LOGGER |
			XLOG_TEXT_SOURCE |
			XLOG_TEXT_THREAD |
			XLOG_TEXT_FIELDS |
			XLOG_TEXT_NEWLINE;
	} else if ( Style == XLOG_TEXT_SIMPLE ) {
		pConfig->Flags =
			XLOG_TEXT_LEVEL |
			XLOG_TEXT_LOGGER |
			XLOG_TEXT_FIELDS |
			XLOG_TEXT_NEWLINE;
	} else {
		pConfig->Flags = XLOG_TEXT_FIELDS | XLOG_TEXT_NEWLINE;
	}
	return true;
}



/* 校验文本配置并建立稳定参数错误。 */
XRT_API bool xrtLogTextConfigValidate(const xlogtextconfig* pConfig)
{
	if ( !__xrtLogTextConfigValid(pConfig) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return true;
}



/* 无中间整行分配地格式化记录。 */
XRT_API bool xrtLogTextWrite(
	const xlogrecord* pRecord,
	const xlogtextconfig* pConfig,
	xlogwriteproc pWrite,
	ptr pUserData,
	size_t* pWritten
)
{
	xlogtextwriter Writer;
	xerror* pPrevious;
	bool bPrefix = false;
	bool bResult = true;

	if ( !xrtLogRecordValidate(pRecord) ) {
		if ( pWritten != NULL ) {
			*pWritten = 0;
		}
		return false;
	}
	if ( !xrtLogTextConfigValidate(pConfig) || (pWrite == NULL) ) {
		__xrtErrorSetInvalidArgument();
		if ( pWritten != NULL ) {
			*pWritten = 0;
		}
		return false;
	}
	memset(&Writer, 0, sizeof(Writer));
	Writer.Write = pWrite;
	Writer.UserData = pUserData;
	pPrevious = xrtErrorRef(xrtGetError());
	xrtClearError();
	if ( (pConfig->Flags & XLOG_TEXT_TIME) != 0 ) {
		bResult =
			__xrtLogTextPrefixSpace(&Writer, &bPrefix) &&
			__xrtLogTextTime(&Writer, pRecord->Time, pConfig->UtcOffset);
	}
	if ( bResult && ((pConfig->Flags & XLOG_TEXT_LEVEL) != 0) ) {
		bResult =
			__xrtLogTextPrefixSpace(&Writer, &bPrefix) &&
			__xrtLogTextAscii(&Writer, xrtLogLevelName(pRecord->Level));
	}
	if (
		bResult && ((pConfig->Flags & XLOG_TEXT_LOGGER) != 0) &&
		(pRecord->Logger.Size != 0)
	) {
		bResult =
			__xrtLogTextPrefixSpace(&Writer, &bPrefix) &&
			__xrtLogTextName(&Writer, pRecord->Logger);
	}
	if (
		bResult && ((pConfig->Flags & XLOG_TEXT_SOURCE) != 0) &&
		(pRecord->File.Size != 0)
	) {
		bResult =
			__xrtLogTextPrefixSpace(&Writer, &bPrefix) &&
			__xrtLogTextName(&Writer, pRecord->File);
		if ( bResult && (pRecord->Line != 0) ) {
			bResult =
				__xrtLogTextAscii(&Writer, ":") &&
				__xrtLogTextUInt(&Writer, pRecord->Line);
		}
		if ( bResult && (pRecord->Function.Size != 0) ) {
			bResult =
				__xrtLogTextAscii(&Writer, " ") &&
				__xrtLogTextName(&Writer, pRecord->Function);
		}
	}
	if (
		bResult && ((pConfig->Flags & XLOG_TEXT_THREAD) != 0) &&
		(pRecord->ThreadId != 0)
	) {
		bResult =
			__xrtLogTextPrefixSpace(&Writer, &bPrefix) &&
			__xrtLogTextAscii(&Writer, "thread=") &&
			__xrtLogTextUInt(&Writer, pRecord->ThreadId);
	}
	if ( bResult && bPrefix ) {
		bResult = __xrtLogTextAscii(&Writer, " - ");
	}
	if ( bResult ) {
		bResult = (pConfig->Flags & XLOG_TEXT_RAW_MESSAGE) != 0
			? __xrtLogTextView(&Writer, pRecord->Message)
			: __xrtLogTextEscape(&Writer, pRecord->Message, false);
	}
	if (
		bResult && ((pConfig->Flags & XLOG_TEXT_FIELDS) != 0)
	) {
		bResult = __xrtLogTextFields(&Writer, pRecord, pConfig->UtcOffset);
	}
	if (
		bResult && ((pConfig->Flags & XLOG_TEXT_NEWLINE) != 0)
	) {
		bResult = __xrtLogTextAscii(&Writer, "\n");
	}
	if ( pWritten != NULL ) {
		*pWritten = Writer.Written;
	}
	if ( bResult ) {
		__xrtErrorSetOwned(pPrevious);
		return true;
	}
	xrtErrorFree(pPrevious);
	if ( xrtGetError() == NULL ) {
		__xrtLogTextOutputError();
	}
	return false;
}

#endif
