#include "../internal/xrt_http.h"

#include <xrt/http_auth.h>



#if defined(XHTTP_FEATURE_HTTP_AUTH)

/* 在小栈缓冲或精确堆缓冲中构建认证值并始终擦除临时副本。 */
bool __xrtHttpAuthWriteTemporary(
	__xrtHttpAuthWriteFunction pWrite,
	const void* pWriteContext,
	__xrtHttpAuthConsumeFunction pConsume,
	void* pConsumeContext
)
{
	unsigned char Local[XRT_HTTP_AUTH_LOCAL_BYTES];
	bytes pValue = Local;
	size_t iSize;
	size_t iWritten;
	bool bHeap = false;
	bool bWritten;
	bool bResult;

	if ( (pWrite == NULL) || (pConsume == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !pWrite(pWriteContext, NULL, 0, &iSize) ) {
		return false;
	}
	if ( iSize > sizeof(Local) ) {
		pValue = (bytes)xrtMalloc(iSize);
		if ( pValue == NULL ) {
			return false;
		}
		bHeap = true;
	}
	bWritten = pWrite(
		pWriteContext,
		pValue,
		iSize,
		&iWritten
	);
	if ( !bWritten || (iWritten != iSize) ) {
		if ( bWritten && (iWritten != iSize) ) {
			__xrtErrorSetInternal();
		}
		xrtSecureZero(pValue, iSize);
		if ( bHeap ) {
			xrtFree(pValue);
		}
		return false;
	}
	bResult = pConsume(
		pConsumeContext,
		(xstrview){ (cstr)pValue, iSize }
	);
	xrtSecureZero(pValue, iSize);
	if ( bHeap ) {
		xrtFree(pValue);
	}
	return bResult;
}



/* 判断 token68 的非填充字符。 */
static bool __xrtHttpAuthToken68Byte(unsigned char iByte)
{
	return ((iByte >= (unsigned char)'0') &&
		(iByte <= (unsigned char)'9')) ||
		((iByte >= (unsigned char)'A') &&
		 (iByte <= (unsigned char)'Z')) ||
		((iByte >= (unsigned char)'a') &&
		 (iByte <= (unsigned char)'z')) ||
		(iByte == (unsigned char)'-') ||
		(iByte == (unsigned char)'.') ||
		(iByte == (unsigned char)'_') ||
		(iByte == (unsigned char)'~') ||
		(iByte == (unsigned char)'+') ||
		(iByte == (unsigned char)'/');
}



/* 从指定位置扫描 token68，不修改线程错误。 */
static bool __xrtHttpAuthToken68Scan(
	xstrview Text,
	size_t iStart,
	size_t* pEnd
)
{
	size_t i = iStart;

	while ( (i < Text.Size) && __xrtHttpAuthToken68Byte(
		(unsigned char)Text.Data[i]
	) ) {
		i++;
	}
	if ( i == iStart ) {
		return false;
	}
	while ( (i < Text.Size) && (Text.Data[i] == '=') ) {
		i++;
	}
	*pEnd = i;
	return true;
}



/* 跳过 HTTP 可选空白。 */
static size_t __xrtHttpAuthOws(xstrview Text, size_t iOffset)
{
	while ( (iOffset < Text.Size) &&
		((Text.Data[iOffset] == ' ') ||
		 (Text.Data[iOffset] == '\t')) ) {
		iOffset++;
	}
	return iOffset;
}



/* 扫描一个必带值的 auth-param，并返回值后的首字节。 */
static bool __xrtHttpAuthParamScan(
	xstrview Text,
	size_t iStart,
	size_t* pEnd
)
{
	size_t i = iStart;
	size_t iName = i;

	while ( (i < Text.Size) && __xrtHttpTokenByte(
		(unsigned char)Text.Data[i]
	) ) {
		i++;
	}
	if ( i == iName ) {
		return false;
	}
	i = __xrtHttpAuthOws(Text, i);
	if ( (i >= Text.Size) || (Text.Data[i] != '=') ) {
		return false;
	}
	i = __xrtHttpAuthOws(Text, i + 1u);
	if ( i >= Text.Size ) {
		return false;
	}
	if ( Text.Data[i] == '"' ) {
		i++;
		while ( i < Text.Size ) {
			unsigned char iByte = (unsigned char)Text.Data[i++];

			if ( iByte == (unsigned char)'"' ) {
				*pEnd = i;
				return true;
			}
			if ( iByte == (unsigned char)'\\' ) {
				if ( (i >= Text.Size) ||
					!__xrtHttpQuotedPairByte(
						(unsigned char)Text.Data[i]
					) ) {
					return false;
				}
				i++;
			} else if ( !__xrtHttpQuotedTextByte(iByte) ) {
				return false;
			}
		}
		return false;
	}
	iName = i;
	while ( (i < Text.Size) && __xrtHttpTokenByte(
		(unsigned char)Text.Data[i]
	) ) {
		i++;
	}
	if ( i == iName ) {
		return false;
	}
	*pEnd = i;
	return true;
}



/* 判断指定位置是否开始一个 auth-param。 */
static bool __xrtHttpAuthParamAhead(
	xstrview Text,
	size_t iStart
)
{
	size_t i = iStart;

	while ( (i < Text.Size) && __xrtHttpTokenByte(
		(unsigned char)Text.Data[i]
	) ) {
		i++;
	}
	if ( i == iStart ) {
		return false;
	}
	i = __xrtHttpAuthOws(Text, i);
	return (i < Text.Size) && (Text.Data[i] == '=');
}



/* 严格验证发送方 auth-param 列表，不接受空成员。 */
static bool __xrtHttpAuthParamsValid(xstrview Parameters)
{
	size_t i = 0;
	size_t iEnd;

	if ( Parameters.Size == 0 ) {
		return false;
	}
	for ( ;; ) {
		if ( !__xrtHttpAuthParamScan(Parameters, i, &iEnd) ) {
			return false;
		}
		i = __xrtHttpAuthOws(Parameters, iEnd);
		if ( i == Parameters.Size ) {
			return true;
		}
		if ( Parameters.Data[i] != ',' ) {
			return false;
		}
		i = __xrtHttpAuthOws(Parameters, i + 1u);
		if ( i == Parameters.Size ) {
			return false;
		}
	}
}



/* 识别完整认证数据的语法类别。 */
static bool __xrtHttpAuthDataKind(
	xstrview Data,
	xhttpauthkind* pKind
)
{
	size_t iEnd;

	if ( Data.Size == 0 ) {
		*pKind = XHTTP_AUTH_NONE;
		return true;
	}
	if ( __xrtHttpAuthToken68Scan(Data, 0, &iEnd) &&
		(iEnd == Data.Size) ) {
		*pKind = XHTTP_AUTH_TOKEN68;
		return true;
	}
	if ( __xrtHttpAuthParamsValid(Data) ) {
		*pKind = XHTTP_AUTH_PARAMS;
		return true;
	}
	return false;
}



/* 判断文本是否是完整 token68。 */
XRT_API bool xrtHttpAuthToken68Valid(xstrview Text)
{
	size_t iEnd;

	return __xrtRangeValid(Text.Data, Text.Size) &&
		__xrtHttpAuthToken68Scan(Text, 0, &iEnd) &&
		(iEnd == Text.Size);
}



/* 读取并约束一个 auth-param。 */
XRT_API xhttpnext xrtHttpAuthParamNext(
	xstrview Parameters,
	size_t* pOffset,
	xhttpparam* pParam
)
{
	xhttpparam Param = { 0 };
	xhttpnext Next;
	size_t iOffset;

	if ( !__xrtHttpViewValid(Parameters) ||
		!__xrtRangeValid(pOffset, sizeof(iOffset)) ||
		!__xrtRangeValid(pParam, sizeof(Param)) ||
		__xrtRangesOverlap(
			pOffset, sizeof(iOffset), Parameters.Data, Parameters.Size
		) || __xrtRangesOverlap(
			pParam, sizeof(Param), Parameters.Data, Parameters.Size
		) || __xrtRangesOverlap(
			pOffset, sizeof(iOffset), pParam, sizeof(Param)
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	memcpy(&iOffset, pOffset, sizeof(iOffset));
	memcpy(pParam, &Param, sizeof(Param));
	if ( iOffset > Parameters.Size ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	Next = xrtHttpDirectiveNext(Parameters, &iOffset, &Param);
	if ( (Next == XHTTP_NEXT_ITEM) &&
		((Param.Flags & XHTTP_PARAM_HAS_VALUE) == 0) ) {
		__xrtErrorSetValue();
		return XHTTP_NEXT_ERROR;
	}
	if ( Next != XHTTP_NEXT_ERROR ) {
		memcpy(pParam, &Param, sizeof(Param));
		memcpy(pOffset, &iOffset, sizeof(iOffset));
	}
	return Next;
}



/* 读取一项 challenge，并用参数等号消除逗号归属歧义。 */
XRT_API xhttpnext xrtHttpChallengeNext(
	xstrview Challenges,
	size_t* pOffset,
	xhttpauth* pChallenge
)
{
	xhttpauth Challenge = { 0 };
	size_t i;
	size_t iScheme;
	size_t iAfterScheme;
	size_t iData;
	size_t iEnd;
	size_t iNext;
	size_t iOffset;
	size_t iTokenEnd;

	if ( !__xrtHttpViewValid(Challenges) ||
		!__xrtRangeValid(pOffset, sizeof(iOffset)) ||
		!__xrtRangeValid(pChallenge, sizeof(Challenge)) ||
		__xrtRangesOverlap(
			pOffset, sizeof(iOffset), Challenges.Data, Challenges.Size
		) || __xrtRangesOverlap(
			pChallenge, sizeof(Challenge),
			Challenges.Data, Challenges.Size
		) || __xrtRangesOverlap(
			pOffset, sizeof(iOffset), pChallenge, sizeof(Challenge)
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	memcpy(&iOffset, pOffset, sizeof(iOffset));
	memcpy(pChallenge, &Challenge, sizeof(Challenge));
	if ( iOffset > Challenges.Size ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	i = iOffset;
	for ( ;; ) {
		i = __xrtHttpAuthOws(Challenges, i);
		if ( (i >= Challenges.Size) ||
			(Challenges.Data[i] != ',') ) {
			break;
		}
		i++;
	}
	if ( i == Challenges.Size ) {
		memcpy(pOffset, &i, sizeof(i));
		return XHTTP_NEXT_END;
	}
	iScheme = i;
	while ( (i < Challenges.Size) && __xrtHttpTokenByte(
		(unsigned char)Challenges.Data[i]
	) ) {
		i++;
	}
	if ( i == iScheme ) {
		__xrtErrorSetValue();
		return XHTTP_NEXT_ERROR;
	}
	Challenge.Scheme = (xstrview){
		Challenges.Data + iScheme,
		i - iScheme
	};
	iAfterScheme = i;
	i = __xrtHttpAuthOws(Challenges, i);
	if ( (i == Challenges.Size) ||
		(Challenges.Data[i] == ',') ) {
		Challenge.Kind = XHTTP_AUTH_NONE;
		iNext = (i < Challenges.Size) ? (i + 1u) : i;
		goto Publish;
	}
	if ( Challenges.Data[iAfterScheme] != ' ' ) {
		__xrtErrorSetValue();
		return XHTTP_NEXT_ERROR;
	}
	iData = i;
	if ( __xrtHttpAuthToken68Scan(
		Challenges, iData, &iTokenEnd
	) ) {
		i = __xrtHttpAuthOws(Challenges, iTokenEnd);
		if ( (i == Challenges.Size) ||
			(Challenges.Data[i] == ',') ) {
			Challenge.Data = (xstrview){
				Challenges.Data + iData,
				iTokenEnd - iData
			};
			Challenge.Kind = XHTTP_AUTH_TOKEN68;
			iNext = (i < Challenges.Size) ? (i + 1u) : i;
			goto Publish;
		}
	}
	if ( !__xrtHttpAuthParamScan(Challenges, iData, &iEnd) ) {
		__xrtErrorSetValue();
		return XHTTP_NEXT_ERROR;
	}
	for ( ;; ) {
		size_t iSeparator = __xrtHttpAuthOws(Challenges, iEnd);
		size_t iNext;

		if ( iSeparator == Challenges.Size ) {
			i = iSeparator;
			break;
		}
		if ( Challenges.Data[iSeparator] != ',' ) {
			__xrtErrorSetValue();
			return XHTTP_NEXT_ERROR;
		}
		iNext = __xrtHttpAuthOws(Challenges, iSeparator + 1u);
		while ( (iNext < Challenges.Size) &&
			(Challenges.Data[iNext] == ',') ) {
			iNext = __xrtHttpAuthOws(Challenges, iNext + 1u);
		}
		if ( iNext == Challenges.Size ) {
			i = iNext;
			break;
		}
		if ( !__xrtHttpAuthParamAhead(Challenges, iNext) ) {
			i = iNext;
			break;
		}
		if ( !__xrtHttpAuthParamScan(Challenges, iNext, &iEnd) ) {
			__xrtErrorSetValue();
			return XHTTP_NEXT_ERROR;
		}
	}
	Challenge.Data = (xstrview){
		Challenges.Data + iData,
		iEnd - iData
	};
	Challenge.Kind = XHTTP_AUTH_PARAMS;
	iNext = i;

	/* 所有成功分支在解析完成后一次性发布游标和借用视图。 */
Publish:
	memcpy(pChallenge, &Challenge, sizeof(Challenge));
	memcpy(pOffset, &iNext, sizeof(iNext));
	return XHTTP_NEXT_ITEM;
}



/* 把跨字段 challenge 游标恢复为零位置。 */
XRT_API void xrtHttpAuthCursorInit(xhttpauthcursor* pCursor)
{
	xhttpauthcursor Cursor = { 0 };

	if ( !__xrtRangeValid(pCursor, sizeof(Cursor)) ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memcpy(pCursor, &Cursor, sizeof(Cursor));
}



/* 校验一条调用方提供的借用字段，避免游标读取无效视图。 */
static bool __xrtHttpAuthFieldValid(const xhttpfield* pField)
{
	if ( !__xrtHttpViewValid(pField->Name) ||
		!__xrtHttpViewValid(pField->Value) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtHttpTokenValid(pField->Name) ||
		!xrtHttpFieldValueValid(pField->Value) ) {
		__xrtErrorSetValue();
		return false;
	}
	return true;
}



/* 跨重复字段读取 challenge，并把双层位置保存在一个稳定游标中。 */
XRT_API xhttpnext xrtHttpFieldChallengeNext(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name,
	xhttpauthcursor* pCursor,
	xhttpauth* pChallenge
)
{
	xhttpauthcursor Cursor;
	xhttpauth Challenge = { 0 };
	xhttpfield Field;
	xhttpnext Next;

	if ( !__xrtHttpFieldArrayValid(pFields, iCount) ||
		!__xrtHttpViewValid(Name) ||
		!__xrtRangeValid(pCursor, sizeof(Cursor)) ||
		!__xrtRangeValid(pChallenge, sizeof(Challenge)) ||
		__xrtHttpFieldArrayOverlap(
			pFields, iCount, pCursor, sizeof(Cursor)
		) || __xrtHttpFieldArrayOverlap(
			pFields, iCount, pChallenge, sizeof(Challenge)
		) || __xrtRangesOverlap(
			pCursor, sizeof(Cursor), Name.Data, Name.Size
		) || __xrtRangesOverlap(
			pChallenge, sizeof(Challenge), Name.Data, Name.Size
		) || __xrtRangesOverlap(
			pCursor, sizeof(Cursor), pChallenge, sizeof(Challenge)
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	memcpy(&Cursor, pCursor, sizeof(Cursor));
	memcpy(pChallenge, &Challenge, sizeof(Challenge));
	if ( Cursor.FieldIndex > iCount ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	if ( !xrtHttpTokenValid(Name) ) {
		__xrtErrorSetValue();
		return XHTTP_NEXT_ERROR;
	}
	while ( Cursor.FieldIndex < iCount ) {
		__xrtHttpFieldLoad(
			pFields, Cursor.FieldIndex, &Field
		);
		if ( !__xrtHttpAuthFieldValid(&Field) ) {
			return XHTTP_NEXT_ERROR;
		}
		if ( !xrtHttpFieldNameEqual(Field.Name, Name) ) {
			Cursor.FieldIndex++;
			Cursor.ValueOffset = 0;
			continue;
		}
		Next = xrtHttpChallengeNext(
			Field.Value,
			&Cursor.ValueOffset,
			&Challenge
		);
		if ( Next == XHTTP_NEXT_ITEM ) {
			memcpy(pChallenge, &Challenge, sizeof(Challenge));
			memcpy(pCursor, &Cursor, sizeof(Cursor));
			return XHTTP_NEXT_ITEM;
		}
		if ( Next == XHTTP_NEXT_ERROR ) {
			return XHTTP_NEXT_ERROR;
		}
		Cursor.FieldIndex++;
		Cursor.ValueOffset = 0;
	}
	memcpy(pCursor, &Cursor, sizeof(Cursor));
	return XHTTP_NEXT_END;
}



/* 严格解析一份不允许 challenge 列表外壳的凭据。 */
XRT_API bool xrtHttpAuthParse(
	xstrview Value,
	xhttpauth* pAuth
)
{
	xhttpauth Auth = { 0 };
	xhttpnext Next;
	xstrview Trimmed;
	size_t iOffset = 0;
	const char* sStart;
	const char* sEnd;

	if ( !__xrtHttpViewValid(Value) ||
		!__xrtRangeValid(pAuth, sizeof(Auth)) ||
		__xrtRangesOverlap(
			pAuth, sizeof(Auth), Value.Data, Value.Size
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(pAuth, &Auth, sizeof(Auth));
	Trimmed = xrtHttpOwsTrim(Value);
	Next = xrtHttpChallengeNext(Value, &iOffset, &Auth);
	if ( Next != XHTTP_NEXT_ITEM ) {
		if ( Next == XHTTP_NEXT_END ) {
			__xrtErrorSetValue();
		}
		return false;
	}
	sStart = Auth.Scheme.Data;
	sEnd = (Auth.Kind == XHTTP_AUTH_NONE) ?
		(Auth.Scheme.Data + Auth.Scheme.Size) :
		(Auth.Data.Data + Auth.Data.Size);
	if ( (Trimmed.Size == 0) ||
		(sStart != Trimmed.Data) ||
		(sEnd != (Trimmed.Data + Trimmed.Size)) ) {
		__xrtErrorSetValue();
		return false;
	}
	memcpy(pAuth, &Auth, sizeof(Auth));
	return true;
}



/* 校验认证字段写出参数、容量和内存重叠。 */
static bool __xrtHttpAuthWriteValid(
	xstrview Scheme,
	xstrview Data,
	void* pOutput,
	size_t iCapacity,
	size_t iRequired,
	size_t* pSize
)
{
	if ( !__xrtRangeValid(pSize, sizeof(iRequired)) ||
		((pOutput == NULL) && (iCapacity != 0)) ||
		__xrtRangesOverlap(
			pSize, sizeof(iRequired), Scheme.Data, Scheme.Size
		) || __xrtRangesOverlap(
			pSize, sizeof(iRequired), Data.Data, Data.Size
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pOutput == NULL ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		return true;
	}
	if ( !__xrtRangeValid(pOutput, iRequired) ||
		__xrtRangesOverlap(
			pSize, sizeof(iRequired), pOutput, iRequired
	) || __xrtRangesOverlap(
		pOutput, iRequired, Scheme.Data, Scheme.Size
	) || __xrtRangesOverlap(
		pOutput, iRequired, Data.Data, Data.Size
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( iCapacity < iRequired ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		__xrtErrorSetRange();
		return false;
	}
	return true;
}



/* 校验并写出完整认证字段值。 */
XRT_API bool xrtHttpAuthWrite(
	xstrview Scheme,
	xstrview Data,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xhttpauthkind Kind;
	size_t iRequired;
	char* sWrite = (char*)pOutput;

	if ( !__xrtHttpViewValid(Data) ||
		!xrtHttpTokenValid(Scheme) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtHttpAuthDataKind(Data, &Kind) ) {
		__xrtErrorSetValue();
		return false;
	}
	if ( Scheme.Size > (SIZE_MAX -
		((Kind == XHTTP_AUTH_NONE) ? 0u : 1u)) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	iRequired = Scheme.Size +
		((Kind == XHTTP_AUTH_NONE) ? 0u : 1u);
	if ( iRequired > (SIZE_MAX - Data.Size) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	iRequired += Data.Size;
	if ( !__xrtHttpAuthWriteValid(
		Scheme, Data, pOutput, iCapacity, iRequired, pSize
	) ) {
		return false;
	}
	if ( pOutput == NULL ) {
		return true;
	}
	memcpy(sWrite, Scheme.Data, Scheme.Size);
	if ( Kind != XHTTP_AUTH_NONE ) {
		sWrite[Scheme.Size] = ' ';
		memcpy(
			sWrite + Scheme.Size + 1u,
			Data.Data,
			Data.Size
		);
	}
	memcpy(pSize, &iRequired, sizeof(iRequired));
	return true;
}



/* 构建零结尾认证字段值。 */
XRT_API str xrtHttpAuthBuild(
	xstrview Scheme,
	xstrview Data,
	size_t* pSize
)
{
	str sOutput;
	size_t iRequired;

	if ( ((pSize != NULL) &&
		!__xrtRangeValid(pSize, sizeof(iRequired))) ||
		((pSize != NULL) && __xrtRangesOverlap(
			pSize, sizeof(iRequired), Scheme.Data, Scheme.Size
		)) || ((pSize != NULL) && __xrtRangesOverlap(
			pSize, sizeof(iRequired), Data.Data, Data.Size
		)) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( !xrtHttpAuthWrite(
		Scheme, Data, NULL, 0, &iRequired
	) || (iRequired == SIZE_MAX) ) {
		return NULL;
	}
	sOutput = (str)xrtMalloc(iRequired + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !xrtHttpAuthWrite(
		Scheme, Data, sOutput, iRequired, &iRequired
	) ) {
		xrtFree(sOutput);
		return NULL;
	}
	sOutput[iRequired] = '\0';
	if ( pSize != NULL ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
	}
	return sOutput;
}

#endif
