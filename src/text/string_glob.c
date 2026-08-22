#include <stdio.h>

#include "../internal/xrt_charset.h"
#include "../internal/xrt_string.h"



#if defined(XRT_FEATURE_STRING_GLOB)

/* 通配模式解析后的单个操作。 */
typedef enum xrt_glob_kind {
	XRT_GLOB_LITERAL = 0,
	XRT_GLOB_ANY,
	XRT_GLOB_STAR,
	XRT_GLOB_CLASS
} xrt_glob_kind;



/* 单个模式操作保留下一操作位置以及字符类边界。 */
typedef struct xrt_glob_token {
	xrt_glob_kind Kind;
	uint32 Scalar;
	size_t Begin;
	size_t End;
	size_t Next;
	bool Negated;
} xrt_glob_token;



/* ASCII 大小写无关模式只折叠基础拉丁字母。 */
static uint32 __xrtGlobFold(uint32 iScalar, uint32 iFlags)
{
	if ( ((iFlags & XSTR_GLOB_CASE_ASCII) != 0) &&
		 (iScalar >= (uint32)'A') && (iScalar <= (uint32)'Z') ) {
		return iScalar + ((uint32)'a' - (uint32)'A');
	}
	return iScalar;
}



/* 报告带字节位置的通配模式语法错误。 */
static void __xrtGlobSetPatternError(size_t iOffset, cstr sMessage)
{
	char sData[64];
	xerrordesc Desc;
	xerror* pError;

	(void)snprintf(sData, sizeof(sData), "offset=%llu",
		(unsigned long long)iOffset);
	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = XERR_VALUE;
	Desc.Domain = "xrt.string";
	Desc.Code = XSTR_ERROR_PATTERN;
	Desc.Operation = "glob";
	Desc.Message = sMessage;
	Desc.Data = sData;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 解码模式中的一个普通标量或反斜杠转义。 */
static bool __xrtGlobScalar(xstrview Pattern, size_t* pPosition,
	uint32* pScalar)
{
	size_t iPosition = *pPosition;
	xrt_utf_decode Decode;

	if ( iPosition == Pattern.Size ) {
		__xrtGlobSetPatternError(iPosition, "incomplete glob escape");
		return false;
	}
	if ( Pattern.Data[iPosition] == '\\' ) {
		iPosition++;
		if ( iPosition == Pattern.Size ) {
			__xrtGlobSetPatternError(iPosition - 1u, "incomplete glob escape");
			return false;
		}
	}
	Decode = __xrtUtf8Decode((const unsigned char*)Pattern.Data + iPosition,
		Pattern.Size - iPosition);
	if ( Decode.Status != XUTF_OK ) {
		__xrtUtfSetInvalid("string-glob-pattern", iPosition);
		return false;
	}
	*pScalar = Decode.Scalar;
	*pPosition = iPosition + Decode.Read;
	return true;
}



/* 解析字符类并按需判断一个文本标量是否属于该类。 */
static bool __xrtGlobClass(xstrview Pattern, size_t iBegin,
	uint32 iText, uint32 iFlags, bool bTest, size_t* pNext, bool* pMatch)
{
	size_t iPosition = iBegin + 1u;
	bool bNegated = false;
	bool bAny = false;
	bool bMatched = false;

	if ( (iPosition < Pattern.Size) &&
		((Pattern.Data[iPosition] == '!') || (Pattern.Data[iPosition] == '^')) ) {
		bNegated = true;
		iPosition++;
	}
	while ( iPosition < Pattern.Size ) {
		uint32 iFirst;
		uint32 iLast;
		size_t iAfterFirst;

		if ( (Pattern.Data[iPosition] == ']') && bAny ) {
			*pNext = iPosition + 1u;
			*pMatch = bNegated ? !bMatched : bMatched;
			return true;
		}
		if ( !__xrtGlobScalar(Pattern, &iPosition, &iFirst) ) {
			return false;
		}
		bAny = true;
		iAfterFirst = iPosition;

		/* 中间的减号构成闭区间，首尾减号按普通字符处理。 */
		if ( (iPosition < Pattern.Size) && (Pattern.Data[iPosition] == '-') &&
			 ((iPosition + 1u) < Pattern.Size) &&
			 (Pattern.Data[iPosition + 1u] != ']') ) {
			uint32 iRangeFirst;
			uint32 iRangeLast;

			iPosition++;
			if ( !__xrtGlobScalar(Pattern, &iPosition, &iLast) ) {
				return false;
			}
			if ( iFirst > iLast ) {
				__xrtGlobSetPatternError(iAfterFirst,
					"glob character range is reversed");
				return false;
			}
			if ( bTest ) {
				uint32 iValue = __xrtGlobFold(iText, iFlags);

				iRangeFirst = __xrtGlobFold(iFirst, iFlags);
				iRangeLast = __xrtGlobFold(iLast, iFlags);
				bMatched = bMatched ||
					((iText >= iFirst) && (iText <= iLast)) ||
					((iRangeFirst <= iRangeLast) &&
					 (iValue >= iRangeFirst) && (iValue <= iRangeLast));
			}
		} else if ( bTest ) {
			bMatched = bMatched ||
				(__xrtGlobFold(iText, iFlags) == __xrtGlobFold(iFirst, iFlags));
		}
	}
	__xrtGlobSetPatternError(iBegin, "glob character class is not closed");
	return false;
}



/* 解析一个通配模式操作。 */
static bool __xrtGlobToken(xstrview Pattern, size_t iPosition,
	xrt_glob_token* pToken)
{
	xrt_utf_decode Decode;

	memset(pToken, 0, sizeof(xrt_glob_token));
	if ( iPosition == Pattern.Size ) {
		pToken->Next = iPosition;
		return true;
	}
	if ( Pattern.Data[iPosition] == '*' ) {
		pToken->Kind = XRT_GLOB_STAR;
		pToken->Next = iPosition + 1u;
		return true;
	}
	if ( Pattern.Data[iPosition] == '?' ) {
		pToken->Kind = XRT_GLOB_ANY;
		pToken->Next = iPosition + 1u;
		return true;
	}
	if ( Pattern.Data[iPosition] == '[' ) {
		bool bMatch;

		pToken->Kind = XRT_GLOB_CLASS;
		pToken->Begin = iPosition;
		if ( !__xrtGlobClass(Pattern, iPosition, 0, 0, false,
			&pToken->Next, &bMatch) ) {
			return false;
		}
		return true;
	}
	if ( Pattern.Data[iPosition] == '\\' ) {
		pToken->Kind = XRT_GLOB_LITERAL;
		if ( !__xrtGlobScalar(Pattern, &iPosition, &pToken->Scalar) ) {
			return false;
		}
		pToken->Next = iPosition;
		return true;
	}
	Decode = __xrtUtf8Decode((const unsigned char*)Pattern.Data + iPosition,
		Pattern.Size - iPosition);
	if ( Decode.Status != XUTF_OK ) {
		__xrtUtfSetInvalid("string-glob-pattern", iPosition);
		return false;
	}
	pToken->Kind = XRT_GLOB_LITERAL;
	pToken->Scalar = Decode.Scalar;
	pToken->Next = iPosition + Decode.Read;
	return true;
}



/* 校验模式语法，避免不匹配的短路路径隐藏后缀错误。 */
static bool __xrtGlobPatternValid(xstrview Pattern)
{
	size_t iPosition = 0;

	while ( iPosition < Pattern.Size ) {
		xrt_glob_token Token;

		if ( !__xrtGlobToken(Pattern, iPosition, &Token) ) {
			return false;
		}
		iPosition = Token.Next;
	}
	return true;
}



/* 判断模式操作是否消费当前文本标量。 */
static bool __xrtGlobTokenMatch(xstrview Pattern,
	const xrt_glob_token* pToken, uint32 iText, uint32 iFlags, bool* pMatch)
{
	*pMatch = false;
	if ( pToken->Kind == XRT_GLOB_ANY ) {
		*pMatch = true;
		return true;
	}
	if ( pToken->Kind == XRT_GLOB_LITERAL ) {
		*pMatch = __xrtGlobFold(iText, iFlags) ==
			__xrtGlobFold(pToken->Scalar, iFlags);
		return true;
	}
	if ( pToken->Kind == XRT_GLOB_CLASS ) {
		size_t iNext;

		return __xrtGlobClass(Pattern, pToken->Begin, iText, iFlags, true,
			&iNext, pMatch);
	}
	return true;
}



/* 使用严格 UTF-8 通配模式匹配完整字符串。 */
XRT_API bool xrtStrGlob(xstrview Text, xstrview Pattern, uint32 iFlags)
{
	size_t iError = XRT_NPOS;
	size_t iText = 0;
	size_t iPattern = 0;
	size_t iStarText = XRT_NPOS;
	size_t iStarPattern = XRT_NPOS;

	if ( ((iFlags & ~(uint32)XSTR_GLOB_CASE_ASCII) != 0) ||
		 ((Text.Data == NULL) && (Text.Size != 0)) ||
		 ((Pattern.Data == NULL) && (Pattern.Size != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtUtf8Valid(Text, &iError) ) {
		__xrtUtfSetInvalid("string-glob-text", iError);
		return false;
	}
	if ( !xrtUtf8Valid(Pattern, &iError) ) {
		__xrtUtfSetInvalid("string-glob-pattern", iError);
		return false;
	}
	if ( !__xrtGlobPatternValid(Pattern) ) {
		return false;
	}

	/* 只保留最近一个星号，失败时每次多消费一个完整标量。 */
	while ( iText < Text.Size ) {
		xrt_glob_token Token;
		xrt_utf_decode TextScalar;
		bool bMatch;

		if ( !__xrtGlobToken(Pattern, iPattern, &Token) ) {
			return false;
		}
		if ( (iPattern < Pattern.Size) && (Token.Kind == XRT_GLOB_STAR) ) {
			iPattern = Token.Next;
			while ( (iPattern < Pattern.Size) && (Pattern.Data[iPattern] == '*') ) {
				iPattern++;
			}
			iStarPattern = iPattern;
			iStarText = iText;
			continue;
		}
		TextScalar = __xrtUtf8Decode((const unsigned char*)Text.Data + iText,
			Text.Size - iText);
		if ( (iPattern < Pattern.Size) &&
			 __xrtGlobTokenMatch(Pattern, &Token, TextScalar.Scalar,
				iFlags, &bMatch) && bMatch ) {
			iText += TextScalar.Read;
			iPattern = Token.Next;
			continue;
		}
		if ( iStarPattern == XRT_NPOS ) {
			return false;
		}
		TextScalar = __xrtUtf8Decode((const unsigned char*)Text.Data + iStarText,
			Text.Size - iStarText);
		iStarText += TextScalar.Read;
		iText = iStarText;
		iPattern = iStarPattern;
	}

	/* 空文本后只允许模式中剩余连续星号。 */
	while ( (iPattern < Pattern.Size) && (Pattern.Data[iPattern] == '*') ) {
		iPattern++;
	}
	return iPattern == Pattern.Size;
}

#endif
