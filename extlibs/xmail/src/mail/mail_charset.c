#include "../internal/xrt_mail.h"



#if defined(XMAIL_FEATURE_MAIL_CHARSET)

/* 内置集合不携带大型码表，保持字符集层可独立裁剪。 */
typedef enum __xmailcharset {
	__XMAIL_CHARSET_UNKNOWN = 0,
	__XMAIL_CHARSET_UTF8,
	__XMAIL_CHARSET_ASCII,
	__XMAIL_CHARSET_LATIN1,
	__XMAIL_CHARSET_WINDOWS_1252
} __xmailcharset;



/* 比较不区分大小写的 ASCII 字符集名称。 */
static bool __xrtMailCharsetEqual(xstrview Text, cstr sValue)
{
	return __xrtMailAsciiEqualI(
		Text,
		__xrtMailView(sValue, strlen(sValue))
	);
}



/* 把字符集别名归一到内置集合。 */
static __xmailcharset __xrtMailCharset(xstrview Charset)
{
	if ( __xrtMailCharsetEqual(Charset, "UTF-8") ||
		 __xrtMailCharsetEqual(Charset, "UTF8") ) {
		return __XMAIL_CHARSET_UTF8;
	}
	if ( __xrtMailCharsetEqual(Charset, "US-ASCII") ||
		 __xrtMailCharsetEqual(Charset, "ASCII") ) {
		return __XMAIL_CHARSET_ASCII;
	}
	if ( __xrtMailCharsetEqual(Charset, "ISO-8859-1") ||
		 __xrtMailCharsetEqual(Charset, "ISO8859-1") ||
		 __xrtMailCharsetEqual(Charset, "LATIN1") ||
		 __xrtMailCharsetEqual(Charset, "LATIN-1") ) {
		return __XMAIL_CHARSET_LATIN1;
	}
	if ( __xrtMailCharsetEqual(Charset, "WINDOWS-1252") ||
		 __xrtMailCharsetEqual(Charset, "CP1252") ) {
		return __XMAIL_CHARSET_WINDOWS_1252;
	}
	return __XMAIL_CHARSET_UNKNOWN;
}



/* 返回 Windows-1252 高位控制区对应的 Unicode 标量。 */
static uint32 __xrtMailCharsetWindows1252(unsigned char iByte)
{
	static const uint16 arrMap[32] = {
		0x20ACu, 0x0000u, 0x201Au, 0x0192u,
		0x201Eu, 0x2026u, 0x2020u, 0x2021u,
		0x02C6u, 0x2030u, 0x0160u, 0x2039u,
		0x0152u, 0x0000u, 0x017Du, 0x0000u,
		0x0000u, 0x2018u, 0x2019u, 0x201Cu,
		0x201Du, 0x2022u, 0x2013u, 0x2014u,
		0x02DCu, 0x2122u, 0x0161u, 0x203Au,
		0x0153u, 0x0000u, 0x017Eu, 0x0178u
	};

	return arrMap[iByte - 0x80u];
}



/* 无错误副作用地查询内置字符集。 */
bool __xrtMailCharsetSupported(xstrview Charset)
{
	return __xrtMailCharset(Charset) != __XMAIL_CHARSET_UNKNOWN;
}



/* 无错误副作用地计量或转换，供协议解析器执行事务式预检。 */
bool __xrtMailCharsetToUtf8(
	xstrview Charset,
	xbytesview Source,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	__xmailcharset Encoding = __xrtMailCharset(Charset);
	size_t iOutput = 0;

	if ( Encoding == __XMAIL_CHARSET_UNKNOWN ) {
		return false;
	}
	if ( Encoding == __XMAIL_CHARSET_UTF8 ) {
		xstrview Text = __xrtMailView(
			(const char*)Source.Data,
			Source.Size
		);

		if ( !xrtUtf8Valid(Text, NULL) ||
			 ((sOutput != NULL) && (Source.Size > iCapacity)) ) {
			return false;
		}
		if ( sOutput != NULL ) {
			memcpy(sOutput, Source.Data, Source.Size);
		}
		*pOutputSize = Source.Size;
		return true;
	}
	for ( size_t i = 0; i < Source.Size; i++ ) {
		unsigned char iByte = Source.Data[i];
		uint32 iScalar;
		char arrScalar[4];
		size_t iScalarSize;

		if ( (Encoding == __XMAIL_CHARSET_ASCII) && (iByte >= 0x80u) ) {
			return false;
		}
		if ( (Encoding == __XMAIL_CHARSET_ASCII) || (iByte < 0x80u) ) {
			iScalar = iByte;
		} else if ( (Encoding == __XMAIL_CHARSET_WINDOWS_1252) &&
			 (iByte < 0xA0u) ) {
			iScalar = __xrtMailCharsetWindows1252(iByte);
			if ( iScalar == 0u ) {
				return false;
			}
		} else {
			iScalar = iByte;
		}
		iScalarSize = xrtUtf8Encode(iScalar, arrScalar);
		if ( (iScalarSize == 0) || (iScalarSize > (SIZE_MAX - iOutput)) ) {
			return false;
		}
		if ( (sOutput != NULL) &&
			 (iScalarSize > (iCapacity - iOutput)) ) {
			return false;
		}
		if ( sOutput != NULL ) {
			memcpy(sOutput + iOutput, arrScalar, iScalarSize);
		}
		iOutput += iScalarSize;
	}
	*pOutputSize = iOutput;
	return true;
}



/* 判断字符集名称是否属于内置集合。 */
XRT_API bool xrtMailCharsetSupported(xstrview Charset)
{
	if ( !__xrtMailViewValid(Charset) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	return __xrtMailCharsetSupported(Charset);
}



/* 把内置字符集转换成 UTF-8。 */
XRT_API bool xrtMailCharsetToUtf8Write(
	xstrview Charset,
	xbytesview Source,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	size_t iRequired;

	if ( !__xrtMailViewValid(Charset) ||
		 !xrtMemRangeValid(Source.Data, Source.Size) ||
		 !xrtMemRangeValid(sOutput, iCapacity) ||
		 !xrtMemRangeValid(pOutputSize, sizeof(*pOutputSize)) ||
		 xrtMemRangesOverlap(
			pOutputSize, sizeof(*pOutputSize), Source.Data, Source.Size
		) || ((sOutput != NULL) && xrtMemRangesOverlap(
			pOutputSize, sizeof(*pOutputSize), sOutput, iCapacity
		)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !__xrtMailCharsetSupported(Charset) ) {
		__xrtMailError(
			XERR_UNSUPPORTED,
			XMAIL_ERROR_CHARSET,
			"unsupported mail character set"
		);
		return false;
	}
	if ( !__xrtMailCharsetToUtf8(
		Charset, Source, NULL, 0, &iRequired
	) ) {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_CHARSET,
			"invalid text for the declared mail character set"
		);
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
	if ( xrtMemRangesOverlap(
		sOutput, iRequired + 1u, Source.Data, Source.Size
	) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !__xrtMailCharsetToUtf8(
		Charset, Source, sOutput, iRequired, &iRequired
	) ) {
		__xrtMailError(
			XERR_STATE,
			XMAIL_ERROR_CHARSET,
			"measured mail character conversion did not fit"
		);
		return false;
	}
	sOutput[iRequired] = 0;
	*pOutputSize = iRequired;
	return true;
}



/* 创建独立的 UTF-8 转换结果。 */
XRT_API str xrtMailCharsetToUtf8(
	xstrview Charset,
	xbytesview Source,
	size_t* pOutputSize
)
{
	size_t iRequired = 0;
	str sOutput;

	if ( !xrtMemRangeValid(
		pOutputSize,
		pOutputSize != NULL ? sizeof(*pOutputSize) : 0
	) || !xrtMailCharsetToUtf8Write(
		Charset, Source, NULL, 0, &iRequired
	) ) {
		return NULL;
	}
	if ( iRequired == SIZE_MAX ) {
		__xrtMailSetSizeOverflow();
		return NULL;
	}
	sOutput = (str)xrtMalloc(iRequired + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !xrtMailCharsetToUtf8Write(
		Charset,
		Source,
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

#endif
