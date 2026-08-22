#include "../internal/xrt_http.h"

#include <xrt/charset.h>
#include <xrt/codec.h>
#include <xrt/http_content_disposition.h>



#if defined(XRT_FEATURE_HTTP_CONTENT_DISPOSITION)

/* 提取常用参数，并严格验证所有 RFC 8187 扩展参数。 */
static bool __xrtHttpContentDispositionParameter(
	const xhttpparam* pParam,
	void* pContext
)
{
	xcontentdisposition* pDisposition =
		(xcontentdisposition*)pContext;
	xhttpextvalue ExtValue;
	bool bExtended = (pParam->Name.Size > 1u) &&
		(pParam->Name.Data[pParam->Name.Size - 1u] == '*');

	if ( bExtended &&
		(((pParam->Flags & XHTTP_PARAM_QUOTED) != 0) ||
		 !__xrtHttpExtValueSplit(pParam->Value, &ExtValue)) ) {
		__xrtErrorSetValue();
		return false;
	}
	if ( xrtHttpTokenEqual(
		pParam->Name, XRT_STR_LITERAL("name")
	) ) {
		pDisposition->Name = *pParam;
		pDisposition->Flags |= XCONTENT_DISPOSITION_NAME;
	} else if ( xrtHttpTokenEqual(
		pParam->Name, XRT_STR_LITERAL("filename")
	) ) {
		pDisposition->FileName = *pParam;
		pDisposition->Flags |= XCONTENT_DISPOSITION_FILENAME;
	} else if ( xrtHttpTokenEqual(
		pParam->Name, XRT_STR_LITERAL("filename*")
	) ) {
		pDisposition->FileNameExt = *pParam;
		pDisposition->Flags |= XCONTENT_DISPOSITION_FILENAME_EXT;
	}
	return true;
}



/* 从权威 Type 和 Parameters 视图重建经过验证的派生字段。 */
static bool __xrtHttpContentDispositionInspect(
	xstrview Type,
	xstrview Parameters,
	xcontentdisposition* pDisposition
)
{
	xcontentdisposition Disposition = { 0 };

	if ( !xrtHttpTokenValid(Type) ||
		!__xrtHttpViewValid(Parameters) ) {
		return false;
	}
	Disposition.Type = Type;
	Disposition.Parameters = Parameters;
	if ( (Parameters.Size != 0) &&
		!__xrtMimeParametersInspect(
			Parameters,
			__xrtHttpContentDispositionParameter,
			&Disposition
		) ) {
		return false;
	}
	*pDisposition = Disposition;
	return true;
}



/* 严格解析 Content-Disposition。 */
XRT_API bool xrtHttpContentDispositionParse(
	xstrview Text,
	xcontentdisposition* pDisposition
)
{
	xcontentdisposition Disposition;
	xstrview Main;
	cstr sSemi;

	if ( !__xrtRangeValid(
		pDisposition, sizeof(*pDisposition)
	) ||
		!__xrtHttpViewValid(Text) ||
		__xrtRangesOverlap(
			pDisposition, sizeof(*pDisposition),
			Text.Data, Text.Size
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(pDisposition, 0, sizeof(*pDisposition));
	Text = xrtHttpOwsTrim(Text);
	if ( Text.Size == 0 ) {
		__xrtErrorSetValue();
		return false;
	}
	sSemi = (cstr)memchr(Text.Data, ';', Text.Size);
	if ( sSemi == NULL ) {
		Main = Text;
	} else {
		Main = (xstrview){
			Text.Data, (size_t)(sSemi - Text.Data)
		};
		Disposition.Parameters = xrtHttpOwsTrim((xstrview){
			sSemi + 1,
			Text.Size - (size_t)(sSemi + 1 - Text.Data)
		});
	}
	if ( (sSemi != NULL) &&
		(Disposition.Parameters.Size == 0) ) {
		__xrtErrorSetValue();
		return false;
	}
	if ( !__xrtHttpContentDispositionInspect(
		xrtHttpOwsTrim(Main),
		Disposition.Parameters,
		&Disposition
	) ) {
		__xrtErrorSetValue();
		return false;
	}
	*pDisposition = Disposition;
	return true;
}



/* 查找 Content-Disposition 参数。 */
XRT_API xhttpnext xrtHttpContentDispositionParam(
	const xcontentdisposition* pDisposition,
	xstrview Name,
	xhttpparam* pParam
)
{
	xcontentdisposition Input;
	xcontentdisposition Disposition;

	if ( !__xrtRangeValid(pDisposition, sizeof(Input)) ||
		!__xrtRangeValid(pParam, sizeof(*pParam)) ||
		__xrtRangesOverlap(
			pDisposition, sizeof(Input), pParam, sizeof(*pParam)
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	memset(pParam, 0, sizeof(*pParam));
	memcpy(&Input, pDisposition, sizeof(Input));
	if ( !__xrtHttpContentDispositionInspect(
		Input.Type, Input.Parameters, &Disposition
	) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	if ( Disposition.Parameters.Size == 0 ) {
		memset(pParam, 0, sizeof(*pParam));
		return XHTTP_NEXT_END;
	}
	return xrtHttpParamFind(
		Disposition.Parameters, Name, pParam
	);
}



/* 流式验证 percent 解码后的 UTF-8，不分配临时文件名缓冲。 */
static bool __xrtHttpContentDispositionUtf8Valid(xstrview Encoded)
{
	xutf8state State;
	xutfstatus Status;
	size_t iOffset = 0;

	if ( !__xrtHttpViewValid(Encoded) ) {
		return false;
	}
	xrtUtf8StateInit(&State);
	while ( iOffset < Encoded.Size ) {
		size_t iEscape = iOffset;

		while ( (iEscape < Encoded.Size) &&
			(Encoded.Data[iEscape] != '%') ) {
			iEscape++;
		}
		if ( iEscape != iOffset ) {
			Status = xrtUtf8StateFeed(
				&State,
				(xstrview){
					Encoded.Data + iOffset,
					iEscape - iOffset
				},
				false
			);
			if ( (Status == XUTF_INVALID) ||
				(Status == XUTF_OVERFLOW) ) {
				return false;
			}
		}
		if ( iEscape == Encoded.Size ) {
			break;
		}
		if ( (Encoded.Size - iEscape) < 3u ) {
			return false;
		}
		{
			uint8 iByte;
			size_t iSize;

			if ( !xrtPercentDecode(
				(xstrview){ Encoded.Data + iEscape, 3u },
				&iByte, sizeof(iByte), &iSize
			) || (iSize != 1u) ) {
				return false;
			}
			Status = xrtUtf8StateFeed(
				&State,
				(xstrview){ (cstr)&iByte, 1u },
				false
			);
			if ( (Status == XUTF_INVALID) ||
				(Status == XUTF_OVERFLOW) ) {
				return false;
			}
		}
		iOffset = iEscape + 3u;
	}
	return xrtUtf8StateFeed(
		&State, (xstrview){ NULL, 0 }, true
	) == XUTF_OK;
}



/* 读取 Content-Disposition 的文件名字节。 */
XRT_API bool xrtHttpContentDispositionFileNameWrite(
	const xcontentdisposition* pDisposition,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xcontentdisposition Input;
	xcontentdisposition Disposition;
	xhttpextvalue ExtValue;
	xstrview Views[2];
	size_t iRequired;
	size_t iWritten;
	bool bExtended = false;

	if ( !__xrtRangeValid(pDisposition, sizeof(Input)) ||
		!__xrtRangeValid(pSize, sizeof(*pSize)) ||
		!__xrtRangeValid(pOutput, iCapacity) ||
		__xrtRangesOverlap(
			pDisposition, sizeof(Input),
			pSize, sizeof(*pSize)
		) || __xrtRangesOverlap(
			pSize, sizeof(*pSize), pOutput, iCapacity
		) || ((pOutput != NULL) && __xrtRangesOverlap(
			pDisposition, sizeof(Input),
			pOutput, iCapacity
		)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(&Input, pDisposition, sizeof(Input));
	if ( !__xrtHttpContentDispositionInspect(
		Input.Type, Input.Parameters, &Disposition
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	Views[0] = Disposition.Type;
	Views[1] = Disposition.Parameters;
	if ( __xrtMimeOutputOverlap(
		Views, 2u, pSize, sizeof(*pSize)
	) || ((pOutput != NULL) && __xrtMimeOutputOverlap(
		Views, 2u, pOutput, iCapacity
	)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (Disposition.Flags &
		XCONTENT_DISPOSITION_FILENAME_EXT) != 0 ) {
		if ( __xrtHttpExtValueSplit(
			Disposition.FileNameExt.Value, &ExtValue
		) && xrtHttpTokenEqual(
			ExtValue.Charset, XRT_STR_LITERAL("UTF-8")
		) && __xrtHttpContentDispositionUtf8Valid(
			ExtValue.Encoded
		) ) {
			bExtended = true;
		}
	}
	if ( bExtended ) {
		if ( !xrtHttpExtValueRead(
			&ExtValue, NULL, 0, &iRequired
		) ) {
			return false;
		}
	} else if ( (Disposition.Flags &
		XCONTENT_DISPOSITION_FILENAME) != 0 ) {
		if ( !xrtHttpParamValueWrite(
			&Disposition.FileName, NULL, 0, &iRequired
		) ) {
			return false;
		}
	} else {
		__xrtErrorSetValue();
		return false;
	}
	if ( pOutput == NULL ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		return true;
	}
	if ( iCapacity < iRequired ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		__xrtErrorSetRange();
		return false;
	}
	if ( bExtended ) {
		if ( !xrtHttpExtValueRead(
			&ExtValue, pOutput, iRequired, &iWritten
		) ) {
			return false;
		}
	} else if ( !xrtHttpParamValueWrite(
		&Disposition.FileName,
		pOutput,
		iRequired,
		&iWritten
	) ) {
		return false;
	}
	memcpy(pSize, &iWritten, sizeof(iWritten));
	return true;
}



/* 构建零结尾文件名字节。 */
XRT_API str xrtHttpContentDispositionFileNameBuild(
	const xcontentdisposition* pDisposition,
	size_t* pSize
)
{
	str sOutput;
	size_t iRequired;

	if ( !xrtHttpContentDispositionFileNameWrite(
		pDisposition, NULL, 0, &iRequired
	) || (iRequired == SIZE_MAX) ) {
		return NULL;
	}
	sOutput = (str)xrtMalloc(iRequired + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !xrtHttpContentDispositionFileNameWrite(
		pDisposition, sOutput, iRequired, &iRequired
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



/* 写出 Content-Disposition。 */
XRT_API bool xrtHttpContentDispositionWrite(
	const xcontentdisposition* pDisposition,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xcontentdisposition Input;
	xcontentdisposition Disposition;
	uint8* pWrite = (uint8*)pOutput;
	xstrview Views[2];
	size_t iRequired;
	size_t iOffset = 0;

	if ( !__xrtRangeValid(pDisposition, sizeof(Input)) ||
		!__xrtRangeValid(pSize, sizeof(*pSize)) ||
		!__xrtRangeValid(pOutput, iCapacity) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(&Input, pDisposition, sizeof(Input));
	if ( !__xrtHttpContentDispositionInspect(
		Input.Type, Input.Parameters, &Disposition
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	Views[0] = Disposition.Type;
	Views[1] = Disposition.Parameters;
	iRequired = 0;
	if ( !__xrtMimeSizeAdd(
		&iRequired, Disposition.Type.Size
	) ) {
		return false;
	}
	if ( Disposition.Parameters.Size != 0 ) {
		if ( !__xrtMimeSizeAdd(&iRequired, 2u) ||
			!__xrtMimeSizeAdd(
				&iRequired, Disposition.Parameters.Size
			) ) {
			return false;
		}
	}
	if ( __xrtMimeOutputOverlap(
		Views, 2, pSize, sizeof(*pSize)
	) || __xrtRangesOverlap(
		pDisposition, sizeof(Input),
		pSize, sizeof(*pSize)
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pOutput == NULL ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		return true;
	}
	if ( __xrtMimeOutputOverlap(
		Views, 2, pOutput, iRequired
	) || __xrtRangesOverlap(
		pDisposition, sizeof(Input),
		pOutput, iRequired
	) || __xrtRangesOverlap(
		pSize, sizeof(*pSize), pOutput, iRequired
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( iCapacity < iRequired ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		__xrtErrorSetRange();
		return false;
	}
	memcpy(
		pWrite + iOffset,
		Disposition.Type.Data,
		Disposition.Type.Size
	);
	iOffset += Disposition.Type.Size;
	if ( Disposition.Parameters.Size != 0 ) {
		pWrite[iOffset++] = (uint8)';';
		pWrite[iOffset++] = (uint8)' ';
		memcpy(
			pWrite + iOffset,
			Disposition.Parameters.Data,
			Disposition.Parameters.Size
		);
		iOffset += Disposition.Parameters.Size;
	}
	memcpy(pSize, &iOffset, sizeof(iOffset));
	return true;
}



/* 适配 Content-Disposition Writer 到共享 Build 函数。 */
static bool __xrtHttpContentDispositionWriteAdapter(
	const void* pValue,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	return xrtHttpContentDispositionWrite(
		(const xcontentdisposition*)pValue,
		pOutput,
		iCapacity,
		pSize
	);
}



/* 构建零结尾 Content-Disposition。 */
XRT_API str xrtHttpContentDispositionBuild(
	const xcontentdisposition* pDisposition,
	size_t* pSize
)
{
	return __xrtMimeBuild(
		pDisposition,
		__xrtHttpContentDispositionWriteAdapter,
		pSize
	);
}

#endif
