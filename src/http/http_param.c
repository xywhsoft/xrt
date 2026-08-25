#include "../internal/xrt_http.h"



#if defined(XRT_FEATURE_HTTP_PARAM)

/* 验证不含外层双引号的 quoted-string 正文并计算解码长度。 */
static bool __xrtHttpQuotedBodyValid(
	xstrview Body,
	size_t* pDecoded
)
{
	size_t iDecoded = 0;
	size_t i;

	if ( !__xrtHttpViewValid(Body) ) {
		return false;
	}
	for ( i = 0; i < Body.Size; i++ ) {
		uint8 iByte = (uint8)Body.Data[i];

		if ( iByte == (uint8)'\\' ) {
			if ( (++i >= Body.Size) ||
				!__xrtHttpQuotedPairByte((uint8)Body.Data[i]) ) {
				__xrtErrorSetValue();
				return false;
			}
		} else if ( !__xrtHttpQuotedTextByte(iByte) ) {
			__xrtErrorSetValue();
			return false;
		}
		if ( iDecoded == SIZE_MAX ) {
			__xrtErrorSetSizeOverflow();
			return false;
		}
		iDecoded++;
	}
	if ( pDecoded != NULL ) {
		*pDecoded = iDecoded;
	}
	return true;
}



/* 计算语义值写成 quoted-string 后的线缆长度。 */
static bool __xrtHttpQuotedMeasure(
	xstrview Value,
	size_t* pSize
)
{
	size_t iRequired = 2;
	size_t i;

	if ( !__xrtHttpViewValid(Value) || (pSize == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	for ( i = 0; i < Value.Size; i++ ) {
		uint8 iByte = (uint8)Value.Data[i];
		size_t iAdd;

		if ( !__xrtHttpQuotedTextByte(iByte) &&
			(iByte != (uint8)'"') &&
			(iByte != (uint8)'\\') ) {
			__xrtErrorSetValue();
			return false;
		}
		iAdd = ((iByte == (uint8)'"') ||
			(iByte == (uint8)'\\')) ? 2u : 1u;
		if ( iRequired > (SIZE_MAX - iAdd) ) {
			__xrtErrorSetSizeOverflow();
			return false;
		}
		iRequired += iAdd;
	}
	*pSize = iRequired;
	return true;
}



/* 解码已经验证的 quoted-string 正文。 */
static size_t __xrtHttpQuotedBodyRead(
	xstrview Body,
	uint8* pOutput
)
{
	size_t iOutput = 0;
	size_t i;

	for ( i = 0; i < Body.Size; i++ ) {
		uint8 iByte = (uint8)Body.Data[i];

		if ( iByte == (uint8)'\\' ) {
			iByte = (uint8)Body.Data[++i];
		}
		pOutput[iOutput++] = iByte;
	}
	return iOutput;
}



/* 把已经验证的值写为 quoted-string。 */
size_t __xrtHttpQuotedWriteUnchecked(
	xstrview Value,
	bytes pOutput
)
{
	size_t iOffset = 0;
	size_t i;

	pOutput[iOffset++] = (uint8)'"';
	for ( i = 0; i < Value.Size; i++ ) {
		uint8 iByte = (uint8)Value.Data[i];

		if ( (iByte == (uint8)'"') ||
			(iByte == (uint8)'\\') ) {
			pOutput[iOffset++] = (uint8)'\\';
		}
		pOutput[iOffset++] = iByte;
	}
	pOutput[iOffset++] = (uint8)'"';
	return iOffset;
}



/* 解码已经验证的参数值。 */
size_t __xrtHttpParamValueWriteUnchecked(
	const xhttpparam* pParam,
	bytes pOutput
)
{
	if ( (pParam->Flags & XHTTP_PARAM_QUOTED) != 0 ) {
		return __xrtHttpQuotedBodyRead(pParam->Value, pOutput);
	}
	if ( pParam->Value.Size != 0 ) {
		memcpy(pOutput, pParam->Value.Data, pParam->Value.Size);
	}
	return pParam->Value.Size;
}



/* 向双遍参数 writer 追加一段固定字节。 */
bool __xrtHttpParamWriterBytes(
	xrt_http_param_writer* pWriter,
	const void* pData,
	size_t iSize
)
{
	if ( pWriter->Size > (SIZE_MAX - iSize) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	if ( pWriter->Output != NULL ) {
		memcpy(pWriter->Output + pWriter->Size, pData, iSize);
	}
	pWriter->Size += iSize;
	return true;
}



/* 追加参数分隔符、名称和等号。 */
bool __xrtHttpParamWriterName(
	xrt_http_param_writer* pWriter,
	xstrview Name,
	bool bFirst
)
{
	return (bFirst || __xrtHttpParamWriterBytes(
		pWriter, ", ", 2u
	)) && __xrtHttpParamWriterBytes(
		pWriter, Name.Data, Name.Size
	) && __xrtHttpParamWriterBytes(pWriter, "=", 1u);
}



/* 追加一个规范 quoted-string 参数。 */
bool __xrtHttpParamWriterQuoted(
	xrt_http_param_writer* pWriter,
	xstrview Name,
	xstrview Value,
	bool bFirst
)
{
	size_t iQuoted;

	if ( !xrtHttpQuotedWrite(Value, NULL, 0, &iQuoted) ) {
		return false;
	}
	if ( !__xrtHttpParamWriterName(pWriter, Name, bFirst) ) {
		return false;
	}
	if ( pWriter->Size > (SIZE_MAX - iQuoted) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	if ( pWriter->Output != NULL ) {
		(void)__xrtHttpQuotedWriteUnchecked(
			Value, pWriter->Output + pWriter->Size
		);
	}
	pWriter->Size += iQuoted;
	return true;
}



/* 追加一个已经验证的 token 参数。 */
bool __xrtHttpParamWriterToken(
	xrt_http_param_writer* pWriter,
	xstrview Name,
	xstrview Value,
	bool bFirst
)
{
	return __xrtHttpParamWriterName(pWriter, Name, bFirst) &&
		__xrtHttpParamWriterBytes(
			pWriter, Value.Data, Value.Size
		);
}



/* 判断保留转义的 quoted-string 正文解码后是否是非空 token。 */
static bool __xrtHttpQuotedBodyTokenValid(xstrview Body)
{
	size_t iDecoded = 0;
	size_t i;

	if ( !__xrtRangeValid(Body.Data, Body.Size) ) {
		return false;
	}
	for ( i = 0; i < Body.Size; i++ ) {
		uint8 iByte = (uint8)Body.Data[i];

		if ( iByte == (uint8)'\\' ) {
			if ( (++i >= Body.Size) ||
				!__xrtHttpQuotedPairByte((uint8)Body.Data[i]) ) {
				return false;
			}
			iByte = (uint8)Body.Data[i];
		} else if ( !__xrtHttpQuotedTextByte(iByte) ) {
			return false;
		}
		if ( !__xrtHttpTokenByte(iByte) ) {
			return false;
		}
		iDecoded++;
	}
	return iDecoded != 0;
}



/* 检查写出参数与输入视图是否重叠。 */
static bool __xrtHttpParamOutputOverlap(
	xstrview Name,
	xstrview Value,
	const void* pOutput,
	size_t iSize
)
{
	return __xrtRangesOverlap(
		Name.Data, Name.Size, pOutput, iSize
	) || __xrtRangesOverlap(
		Value.Data, Value.Size, pOutput, iSize
	);
}



/* 读取一个 token、可选值及调用方指定的列表分隔符。 */
xhttpnext __xrtHttpNameValueNext(
	xstrview Parameters,
	size_t* pOffset,
	xhttpparam* pParam,
	char iSeparator,
	bool bIgnoreEmpty
)
{
	xhttpparam Param = { 0 };
	size_t iName;
	size_t iValue;
	size_t iNext;
	size_t iOffset;
	size_t i;

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
	i = iOffset;

	/* 指令列表忽略由连续或首尾逗号产生的空成员。 */
	for ( ;; ) {
		while ( (i < Parameters.Size) &&
			((Parameters.Data[i] == ' ') ||
			 (Parameters.Data[i] == '\t')) ) {
			i++;
		}
		if ( i == Parameters.Size ) {
			memcpy(pOffset, &i, sizeof(i));
			return XHTTP_NEXT_END;
		}
		if ( Parameters.Data[i] != iSeparator ) {
			break;
		}
		if ( !bIgnoreEmpty ) {
			__xrtErrorSetValue();
			return XHTTP_NEXT_ERROR;
		}
		i++;
	}

	/* 参数名称必须是一个非空 token。 */
	iName = i;
	while ( (i < Parameters.Size) &&
		__xrtHttpTokenByte((uint8)Parameters.Data[i]) ) {
		i++;
	}
	if ( i == iName ) {
		__xrtErrorSetValue();
		return XHTTP_NEXT_ERROR;
	}
	Param.Name.Data = Parameters.Data + iName;
	Param.Name.Size = i - iName;
	while ( (i < Parameters.Size) &&
		((Parameters.Data[i] == ' ') ||
		 (Parameters.Data[i] == '\t')) ) {
		i++;
	}

	/* 等号可省略；存在等号时值必须是 token 或 quoted-string。 */
	if ( (i < Parameters.Size) && (Parameters.Data[i] == '=') ) {
		Param.Flags = XHTTP_PARAM_HAS_VALUE;
		i++;
		while ( (i < Parameters.Size) &&
			((Parameters.Data[i] == ' ') ||
			 (Parameters.Data[i] == '\t')) ) {
			i++;
		}
		if ( i == Parameters.Size ) {
			__xrtErrorSetValue();
			return XHTTP_NEXT_ERROR;
		}
		if ( Parameters.Data[i] == '"' ) {
			Param.Flags |= XHTTP_PARAM_QUOTED;
			iValue = ++i;
			while ( i < Parameters.Size ) {
				uint8 iByte = (uint8)Parameters.Data[i];

				if ( iByte == (uint8)'"' ) {
					break;
				}
				if ( iByte == (uint8)'\\' ) {
					if ( (++i >= Parameters.Size) ||
						!__xrtHttpQuotedPairByte(
							(uint8)Parameters.Data[i]
						) ) {
						__xrtErrorSetValue();
						return XHTTP_NEXT_ERROR;
					}
				} else if ( !__xrtHttpQuotedTextByte(iByte) ) {
					__xrtErrorSetValue();
					return XHTTP_NEXT_ERROR;
				}
				i++;
			}
			if ( (i == Parameters.Size) ||
				(Parameters.Data[i] != '"') ) {
				__xrtErrorSetValue();
				return XHTTP_NEXT_ERROR;
			}
			Param.Value.Data = Parameters.Data + iValue;
			Param.Value.Size = i - iValue;
			i++;
		} else {
			iValue = i;
			while ( (i < Parameters.Size) &&
				__xrtHttpTokenByte((uint8)Parameters.Data[i]) ) {
				i++;
			}
			if ( i == iValue ) {
				__xrtErrorSetValue();
				return XHTTP_NEXT_ERROR;
			}
			Param.Value.Data = Parameters.Data + iValue;
			Param.Value.Size = i - iValue;
		}
		while ( (i < Parameters.Size) &&
			((Parameters.Data[i] == ' ') ||
			 (Parameters.Data[i] == '\t')) ) {
			i++;
		}
	}

	/* 严格参数拒绝空尾项，指令列表由下次调用统一跳过空项。 */
	iNext = i;
	if ( i < Parameters.Size ) {
		if ( Parameters.Data[i] != iSeparator ) {
			__xrtErrorSetValue();
			return XHTTP_NEXT_ERROR;
		}
		iNext = i + 1u;
		if ( !bIgnoreEmpty ) {
			i = iNext;
			while ( (i < Parameters.Size) &&
				((Parameters.Data[i] == ' ') ||
				 (Parameters.Data[i] == '\t')) ) {
				i++;
			}
			if ( (i == Parameters.Size) ||
				(Parameters.Data[i] == iSeparator) ) {
				__xrtErrorSetValue();
				return XHTTP_NEXT_ERROR;
			}
		}
	}
	memcpy(pParam, &Param, sizeof(Param));
	memcpy(pOffset, &iNext, sizeof(iNext));
	return XHTTP_NEXT_ITEM;
}



/* 严格读取分号分隔参数列表的下一项。 */
XRT_API xhttpnext xrtHttpParamNext(
	xstrview Parameters,
	size_t* pOffset,
	xhttpparam* pParam
)
{
	return __xrtHttpNameValueNext(
		Parameters, pOffset, pParam, ';', false
	);
}



/* 严格统计完整参数列表。 */
XRT_API bool xrtHttpParamCount(
	xstrview Parameters,
	size_t* pCount
)
{
	xhttpparam Param;
	xhttpnext Next;
	size_t iOffset = 0;
	size_t iCount = 0;

	if ( !__xrtHttpViewValid(Parameters) ||
		!__xrtRangeValid(pCount, sizeof(iCount)) ||
		__xrtRangesOverlap(
			pCount, sizeof(iCount), Parameters.Data, Parameters.Size
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(pCount, &iCount, sizeof(iCount));
	for ( ;; ) {
		Next = xrtHttpParamNext(Parameters, &iOffset, &Param);
		if ( Next == XHTTP_NEXT_ERROR ) {
			return false;
		}
		if ( Next == XHTTP_NEXT_END ) {
			memcpy(pCount, &iCount, sizeof(iCount));
			return true;
		}
		if ( iCount == SIZE_MAX ) {
			__xrtErrorSetSizeOverflow();
			return false;
		}
		iCount++;
	}
}



/* 严格查找参数，同时验证命中项之后的剩余文本。 */
XRT_API xhttpnext xrtHttpParamFind(
	xstrview Parameters,
	xstrview Name,
	xhttpparam* pParam
)
{
	xhttpparam Param;
	xhttpparam Found;
	xhttpnext Next;
	size_t iOffset = 0;
	bool bFound = false;

	if ( !__xrtHttpViewValid(Parameters) ||
		!__xrtRangeValid(pParam, sizeof(Found)) ||
		!xrtHttpTokenValid(Name) ||
		__xrtRangesOverlap(
			pParam, sizeof(Found), Parameters.Data, Parameters.Size
		) || __xrtRangesOverlap(
			pParam, sizeof(Found), Name.Data, Name.Size
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	memset(&Found, 0, sizeof(Found));
	memcpy(pParam, &Found, sizeof(Found));
	for ( ;; ) {
		Next = xrtHttpParamNext(Parameters, &iOffset, &Param);
		if ( Next == XHTTP_NEXT_ERROR ) {
			return XHTTP_NEXT_ERROR;
		}
		if ( Next == XHTTP_NEXT_END ) {
			if ( bFound ) {
				memcpy(pParam, &Found, sizeof(Found));
				return XHTTP_NEXT_ITEM;
			}
			return XHTTP_NEXT_END;
		}
		if ( !bFound && xrtHttpTokenEqual(Param.Name, Name) ) {
			Found = Param;
			bFound = true;
		}
	}
}



/* 读取逗号分隔指令列表的下一项，并忽略空列表成员。 */
XRT_API xhttpnext xrtHttpDirectiveNext(
	xstrview Directives,
	size_t* pOffset,
	xhttpparam* pDirective
)
{
	return __xrtHttpNameValueNext(
		Directives, pOffset, pDirective, ',', true
	);
}



/* 严格统计完整指令列表。 */
XRT_API bool xrtHttpDirectiveCount(
	xstrview Directives,
	size_t* pCount
)
{
	xhttpparam Directive;
	xhttpnext Next;
	size_t iOffset = 0;
	size_t iCount = 0;

	if ( !__xrtHttpViewValid(Directives) ||
		!__xrtRangeValid(pCount, sizeof(iCount)) ||
		__xrtRangesOverlap(
			pCount, sizeof(iCount),
			Directives.Data, Directives.Size
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(pCount, &iCount, sizeof(iCount));
	for ( ;; ) {
		Next = xrtHttpDirectiveNext(
			Directives, &iOffset, &Directive
		);
		if ( Next == XHTTP_NEXT_ERROR ) {
			return false;
		}
		if ( Next == XHTTP_NEXT_END ) {
			memcpy(pCount, &iCount, sizeof(iCount));
			return true;
		}
		if ( iCount == SIZE_MAX ) {
			__xrtErrorSetSizeOverflow();
			return false;
		}
		iCount++;
	}
}



/* 查找首个指令，并继续验证余下全部列表成员。 */
XRT_API xhttpnext xrtHttpDirectiveFind(
	xstrview Directives,
	xstrview Name,
	xhttpparam* pDirective
)
{
	xhttpparam Directive;
	xhttpparam Found;
	xhttpnext Next;
	size_t iOffset = 0;
	bool bFound = false;

	if ( !__xrtHttpViewValid(Directives) ||
		!__xrtRangeValid(pDirective, sizeof(Found)) ||
		!xrtHttpTokenValid(Name) ||
		__xrtRangesOverlap(
			pDirective, sizeof(Found),
			Directives.Data, Directives.Size
		) || __xrtRangesOverlap(
			pDirective, sizeof(Found),
			Name.Data, Name.Size
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	memset(&Found, 0, sizeof(Found));
	memcpy(pDirective, &Found, sizeof(Found));
	for ( ;; ) {
		Next = xrtHttpDirectiveNext(
			Directives, &iOffset, &Directive
		);
		if ( Next == XHTTP_NEXT_ERROR ) {
			return XHTTP_NEXT_ERROR;
		}
		if ( Next == XHTTP_NEXT_END ) {
			if ( bFound ) {
				memcpy(pDirective, &Found, sizeof(Found));
				return XHTTP_NEXT_ITEM;
			}
			return XHTTP_NEXT_END;
		}
		if ( !bFound && xrtHttpTokenEqual(
			Directive.Name, Name
		) ) {
			Found = Directive;
			bFound = true;
		}
	}
}



/* 判断文本是否是完整 quoted-string。 */
XRT_API bool xrtHttpQuotedValid(xstrview Quoted)
{
	xstrview Body;

	if ( !__xrtHttpViewValid(Quoted) ||
		(Quoted.Size < 2) ||
		(Quoted.Data[0] != '"') ||
		(Quoted.Data[Quoted.Size - 1u] != '"') ) {
		return false;
	}
	Body.Data = Quoted.Data + 1;
	Body.Size = Quoted.Size - 2u;
	return __xrtHttpQuotedBodyValid(Body, NULL);
}



/* 解码完整 quoted-string。 */
XRT_API bool xrtHttpQuotedRead(
	xstrview Quoted,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xstrview Body;
	size_t iCheckSize;
	size_t iRequired;

	if ( !__xrtRangeValid(pSize, sizeof(iRequired)) ||
		((pOutput == NULL) && (iCapacity != 0)) ||
		__xrtRangesOverlap(
			pSize, sizeof(iRequired), Quoted.Data, Quoted.Size
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtHttpViewValid(Quoted) ||
		(Quoted.Size < 2u) ||
		(Quoted.Data[0] != '"') ||
		(Quoted.Data[Quoted.Size - 1u] != '"') ) {
		__xrtErrorSetValue();
		return false;
	}
	Body.Data = Quoted.Data + 1;
	Body.Size = Quoted.Size - 2u;
	if ( !__xrtHttpQuotedBodyValid(Body, &iRequired) ) {
		return false;
	}
	if ( pOutput == NULL ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		return true;
	}
	iCheckSize = __xrtOutputCheckSize(iRequired, iCapacity);
	if ( !__xrtRangeValid(pOutput, iCheckSize) ||
		__xrtRangesOverlap(
			pSize, sizeof(iRequired), pOutput, iCheckSize
	) || __xrtRangesOverlap(
			Quoted.Data, Quoted.Size, pOutput, iCheckSize
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( iCapacity < iRequired ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		__xrtErrorSetRange();
		return false;
	}
	(void)__xrtHttpQuotedBodyRead(Body, (uint8*)pOutput);
	memcpy(pSize, &iRequired, sizeof(iRequired));
	return true;
}



/* 写出 quoted-string。 */
XRT_API bool xrtHttpQuotedWrite(
	xstrview Value,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	uint8* pWrite = (uint8*)pOutput;
	size_t iCheckSize;
	size_t iRequired;
	size_t iOffset = 0;

	if ( !__xrtRangeValid(pSize, sizeof(iRequired)) ||
		((pOutput == NULL) && (iCapacity != 0)) ||
		__xrtRangesOverlap(
			pSize, sizeof(iRequired), Value.Data, Value.Size
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtHttpQuotedMeasure(Value, &iRequired) ) {
		return false;
	}
	if ( pOutput == NULL ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		return true;
	}
	iCheckSize = __xrtOutputCheckSize(iRequired, iCapacity);
	if ( !__xrtRangeValid(pOutput, iCheckSize) ||
		__xrtRangesOverlap(
			pSize, sizeof(iRequired), pOutput, iCheckSize
	) || __xrtRangesOverlap(
			Value.Data, Value.Size, pOutput, iCheckSize
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( iCapacity < iRequired ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		__xrtErrorSetRange();
		return false;
	}
	iOffset = __xrtHttpQuotedWriteUnchecked(Value, pWrite);
	memcpy(pSize, &iOffset, sizeof(iOffset));
	return true;
}



/* 构建零结尾 quoted-string。 */
XRT_API str xrtHttpQuotedBuild(
	xstrview Value,
	size_t* pSize
)
{
	str sOutput;
	size_t iRequired;

	if ( ((pSize != NULL) &&
		!__xrtRangeValid(pSize, sizeof(iRequired))) ||
		((pSize != NULL) && __xrtRangesOverlap(
			pSize, sizeof(iRequired), Value.Data, Value.Size
		)) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( !xrtHttpQuotedWrite(Value, NULL, 0, &iRequired) ||
		(iRequired == SIZE_MAX) ) {
		return NULL;
	}
	sOutput = (str)xrtMalloc(iRequired + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !xrtHttpQuotedWrite(
		Value, sOutput, iRequired, &iRequired
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



/* 判断参数的语义值是否是非空 token。 */
XRT_API bool xrtHttpParamTokenValid(const xhttpparam* pParam)
{
	xhttpparam Param;
	uint32 iFlags;

	if ( !__xrtRangeValid(pParam, sizeof(Param)) ) {
		return false;
	}
	memcpy(&Param, pParam, sizeof(Param));
	iFlags = Param.Flags;
	if ( (iFlags != XHTTP_PARAM_HAS_VALUE) &&
		(iFlags != (
			XHTTP_PARAM_HAS_VALUE |
			XHTTP_PARAM_QUOTED
		)) ) {
		return false;
	}
	if ( (iFlags & XHTTP_PARAM_QUOTED) != 0 ) {
		return __xrtHttpQuotedBodyTokenValid(Param.Value);
	}
	if ( !__xrtRangeValid(Param.Value.Data, Param.Value.Size) ) {
		return false;
	}
	return xrtHttpTokenValid(Param.Value);
}



/* 读取参数值的下一个语义字节。 */
bool __xrtHttpParamSemanticNext(
	const xhttpparam* pParam,
	size_t* pOffset,
	uint8* pByte
)
{
	size_t iOffset = *pOffset;
	uint8 iByte;

	if ( iOffset >= pParam->Value.Size ) {
		return false;
	}
	iByte = (uint8)pParam->Value.Data[iOffset++];
	if ( ((pParam->Flags & XHTTP_PARAM_QUOTED) != 0) &&
		(iByte == (uint8)'\\') ) {
		iByte = (uint8)pParam->Value.Data[iOffset++];
	}
	*pOffset = iOffset;
	*pByte = iByte;
	return true;
}



/* 验证参数值游标尚未使用或仍绑定原描述符。 */
static bool __xrtHttpParamValueCursorValid(
	const xhttpparamvaluecursor* pCursor,
	const xhttpparam* pParam,
	const xhttpparam* pValue
)
{
	if ( pCursor->Validated > 1u ) {
		return false;
	}
	if ( pCursor->Validated == 0 ) {
		return (pCursor->Source == NULL) &&
			(pCursor->Value == NULL) &&
			(pCursor->ValueSize == 0) &&
			(pCursor->Offset == 0) &&
			(pCursor->Flags == 0);
	}
	return (pCursor->Source == pParam) &&
		(pCursor->Value == pValue->Value.Data) &&
		(pCursor->ValueSize == pValue->Value.Size) &&
		(pCursor->Flags == pValue->Flags) &&
		(pCursor->Offset <= pValue->Value.Size);
}



/* 初始化参数语义值游标。 */
XRT_API void xrtHttpParamValueCursorInit(
	xhttpparamvaluecursor* pCursor
)
{
	xhttpparamvaluecursor Cursor;

	if ( !__xrtRangeValid(pCursor, sizeof(Cursor)) ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memset(&Cursor, 0, sizeof(Cursor));
	memcpy(pCursor, &Cursor, sizeof(Cursor));
}



/* 逐字节读取已经验证的参数语义值。 */
XRT_API xhttpnext xrtHttpParamValueNext(
	const xhttpparam* pParam,
	xhttpparamvaluecursor* pCursor,
	uint8* pByte
)
{
	xhttpparamvaluecursor Cursor;
	xhttpparam Param;
	uint8 iByte = 0;
	size_t iIgnored;

	if ( !__xrtRangeValid(pParam, sizeof(Param)) ||
		!__xrtRangeValid(pCursor, sizeof(Cursor)) ||
		!__xrtRangeValid(pByte, sizeof(iByte)) ||
		__xrtRangesOverlap(
			pParam, sizeof(Param), pCursor, sizeof(Cursor)
		) || __xrtRangesOverlap(
			pParam, sizeof(Param), pByte, sizeof(iByte)
		) || __xrtRangesOverlap(
			pCursor, sizeof(Cursor), pByte, sizeof(iByte)
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	memcpy(&Param, pParam, sizeof(Param));
	memcpy(&Cursor, pCursor, sizeof(Cursor));
	if ( !__xrtHttpParamValueCursorValid(
		&Cursor, pParam, &Param
	) || __xrtRangesOverlap(
		pParam, sizeof(Param),
		Param.Value.Data, Param.Value.Size
	) || __xrtRangesOverlap(
		Param.Value.Data, Param.Value.Size,
		pCursor, sizeof(Cursor)
	) || __xrtRangesOverlap(
		Param.Value.Data, Param.Value.Size,
		pByte, sizeof(iByte)
	) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	memcpy(pByte, &iByte, sizeof(iByte));
	if ( Cursor.Validated == 0 ) {
		if ( !xrtHttpParamValueWrite(
			&Param, NULL, 0, &iIgnored
		) ) {
			return XHTTP_NEXT_ERROR;
		}
		Cursor.Source = pParam;
		Cursor.Value = Param.Value.Data;
		Cursor.ValueSize = Param.Value.Size;
		Cursor.Flags = Param.Flags;
		Cursor.Validated = 1u;
	}
	if ( !__xrtHttpParamSemanticNext(
		&Param, &Cursor.Offset, &iByte
	) ) {
		memcpy(pCursor, &Cursor, sizeof(Cursor));
		return XHTTP_NEXT_END;
	}
	memcpy(pByte, &iByte, sizeof(iByte));
	memcpy(pCursor, &Cursor, sizeof(Cursor));
	return XHTTP_NEXT_ITEM;
}



/* 比较参数的解码 token 值，不为普通不匹配设置错误。 */
XRT_API bool xrtHttpParamTokenEqual(
	const xhttpparam* pInput,
	xstrview Token
)
{
	xhttpparam Param;
	size_t iOffset = 0;
	size_t iToken = 0;
	uint8 iByte;

	if ( !xrtHttpParamTokenValid(pInput) ||
		!xrtHttpTokenValid(Token) ) {
		return false;
	}
	memcpy(&Param, pInput, sizeof(Param));
	while ( __xrtHttpParamSemanticNext(
		&Param, &iOffset, &iByte
	) ) {
		if ( (iToken >= Token.Size) ||
			(__xrtHttpAsciiLower(iByte) !=
			 __xrtHttpAsciiLower(
				(uint8)Token.Data[iToken]
			 )) ) {
			return false;
		}
		iToken++;
	}
	return iToken == Token.Size;
}



/* 解码参数值。 */
XRT_API bool xrtHttpParamValueWrite(
	const xhttpparam* pParam,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xhttpparam Param;
	size_t iCheckSize;
	size_t iRequired;

	if ( !__xrtRangeValid(pParam, sizeof(Param)) ||
		!__xrtRangeValid(pSize, sizeof(iRequired)) ||
		((pOutput == NULL) && (iCapacity != 0)) ||
		__xrtRangesOverlap(
			pSize, sizeof(iRequired), pParam, sizeof(Param)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(&Param, pParam, sizeof(Param));
	if ( ((Param.Flags & ~(uint32)(
			XHTTP_PARAM_HAS_VALUE | XHTTP_PARAM_QUOTED
		)) != 0) ||
		((Param.Flags & XHTTP_PARAM_HAS_VALUE) == 0) ||
		!__xrtHttpViewValid(Param.Value) ||
		__xrtRangesOverlap(
			pParam, sizeof(Param), Param.Value.Data,
			Param.Value.Size
		) || __xrtRangesOverlap(
			pSize, sizeof(iRequired), Param.Value.Data,
			Param.Value.Size
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (Param.Flags & XHTTP_PARAM_QUOTED) != 0 ) {
		if ( !__xrtHttpQuotedBodyValid(
			Param.Value, &iRequired
		) ) {
			return false;
		}
	} else {
		if ( !xrtHttpTokenValid(Param.Value) ) {
			__xrtErrorSetValue();
			return false;
		}
		iRequired = Param.Value.Size;
	}
	if ( pOutput == NULL ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		return true;
	}
	iCheckSize = __xrtOutputCheckSize(iRequired, iCapacity);
	if ( !__xrtRangeValid(pOutput, iCheckSize) ||
		__xrtRangesOverlap(
			pOutput, iCheckSize, pParam, sizeof(Param)
	) || __xrtRangesOverlap(
			pOutput, iCheckSize, pSize, sizeof(iRequired)
	) || __xrtRangesOverlap(
			pOutput, iCheckSize, Param.Value.Data,
			Param.Value.Size
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( iCapacity < iRequired ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		__xrtErrorSetRange();
		return false;
	}
	(void)__xrtHttpParamValueWriteUnchecked(
		&Param, (bytes)pOutput
	);
	memcpy(pSize, &iRequired, sizeof(iRequired));
	return true;
}



/* 写出单个 HTTP 参数。 */
XRT_API bool xrtHttpParamWrite(
	xstrview Name,
	xstrview Value,
	uint32 iFlags,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	uint8* pWrite = (uint8*)pOutput;
	size_t iCheckSize;
	size_t iRequired;
	size_t iValueSize = 0;
	size_t iOffset = 0;

	if ( !__xrtRangeValid(pSize, sizeof(iRequired)) ||
		((pOutput == NULL) && (iCapacity != 0)) ||
		((iFlags & ~(uint32)(
			XHTTP_PARAM_HAS_VALUE | XHTTP_PARAM_QUOTED
		)) != 0) ||
		(((iFlags & XHTTP_PARAM_QUOTED) != 0) &&
		 ((iFlags & XHTTP_PARAM_HAS_VALUE) == 0)) ||
		!xrtHttpTokenValid(Name) ||
		!__xrtHttpViewValid(Value) ||
		(((iFlags & XHTTP_PARAM_HAS_VALUE) == 0) &&
		 (Value.Size != 0)) ||
		__xrtRangesOverlap(
			pSize, sizeof(iRequired), Name.Data, Name.Size
		) || __xrtRangesOverlap(
			pSize, sizeof(iRequired), Value.Data, Value.Size
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (iFlags & XHTTP_PARAM_HAS_VALUE) != 0 ) {
		if ( (iFlags & XHTTP_PARAM_QUOTED) != 0 ) {
			if ( !__xrtHttpQuotedMeasure(Value, &iValueSize) ) {
				return false;
			}
		} else if ( !xrtHttpTokenValid(Value) ) {
			__xrtErrorSetValue();
			return false;
		} else {
			iValueSize = Value.Size;
		}
	}
	if ( Name.Size > (SIZE_MAX -
		(((iFlags & XHTTP_PARAM_HAS_VALUE) != 0) ? 1u : 0u)) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	iRequired = Name.Size +
		(((iFlags & XHTTP_PARAM_HAS_VALUE) != 0) ? 1u : 0u);
	if ( iRequired > (SIZE_MAX - iValueSize) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	iRequired += iValueSize;
	if ( pOutput == NULL ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		return true;
	}
	iCheckSize = __xrtOutputCheckSize(iRequired, iCapacity);
	if ( !__xrtRangeValid(pOutput, iCheckSize) ||
		__xrtRangesOverlap(
			pSize, sizeof(iRequired), pOutput, iCheckSize
	) || __xrtHttpParamOutputOverlap(
		Name, Value, pOutput, iCheckSize
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( iCapacity < iRequired ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		__xrtErrorSetRange();
		return false;
	}
	memcpy(pWrite + iOffset, Name.Data, Name.Size);
	iOffset += Name.Size;
	if ( (iFlags & XHTTP_PARAM_HAS_VALUE) != 0 ) {
		pWrite[iOffset++] = (uint8)'=';
		if ( (iFlags & XHTTP_PARAM_QUOTED) != 0 ) {
			iOffset += __xrtHttpQuotedWriteUnchecked(
				Value, pWrite + iOffset
			);
		} else {
			memcpy(pWrite + iOffset, Value.Data, Value.Size);
			iOffset += Value.Size;
		}
	}
	memcpy(pSize, &iOffset, sizeof(iOffset));
	return true;
}



/* 构建零结尾单个 HTTP 参数。 */
XRT_API str xrtHttpParamBuild(
	xstrview Name,
	xstrview Value,
	uint32 iFlags,
	size_t* pSize
)
{
	str sOutput;
	size_t iRequired;

	if ( ((pSize != NULL) &&
		!__xrtRangeValid(pSize, sizeof(iRequired))) ||
		((pSize != NULL) && __xrtRangesOverlap(
			pSize, sizeof(iRequired), Name.Data, Name.Size
		)) || ((pSize != NULL) && __xrtRangesOverlap(
			pSize, sizeof(iRequired), Value.Data, Value.Size
		)) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( !xrtHttpParamWrite(
		Name, Value, iFlags, NULL, 0, &iRequired
	) || (iRequired == SIZE_MAX) ) {
		return NULL;
	}
	sOutput = (str)xrtMalloc(iRequired + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !xrtHttpParamWrite(
		Name, Value, iFlags, sOutput, iRequired, &iRequired
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
