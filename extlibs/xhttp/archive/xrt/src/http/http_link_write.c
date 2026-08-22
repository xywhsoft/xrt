#include "../internal/xrt_http_link.h"
#include "../internal/xrt_url.h"



#if defined(XRT_FEATURE_HTTP_LINK_WRITE)

/* 从可能未对齐的 Link 数组读取一个描述符。 */
static void __xrtHttpLinkValueLoad(
	const xhttplinkvalue* pLinks,
	size_t iIndex,
	xhttplinkvalue* pLink
)
{
	memcpy(
		pLink,
		(const uint8*)pLinks +
		(iIndex * sizeof(*pLinks)),
		sizeof(*pLink)
	);
}



/* 从可能未对齐的参数数组读取一个描述符。 */
static void __xrtHttpLinkParamValueLoad(
	const xhttplinkparamvalue* pParams,
	size_t iIndex,
	xhttplinkparamvalue* pParam
)
{
	memcpy(
		pParam,
		(const uint8*)pParams +
		(iIndex * sizeof(*pParams)),
		sizeof(*pParam)
	);
}



/* 判断参数名称是 Link 的必需关系槽。 */
static bool __xrtHttpLinkWriteRel(xstrview Name)
{
	return xrtHttpTokenEqual(Name, XRT_STR_LITERAL("rel"));
}



/* 判断参数名称只允许出现一次。 */
static bool __xrtHttpLinkWriteSingleton(xstrview Name)
{
	return xrtHttpTokenEqual(
		Name, XRT_STR_LITERAL("anchor")
	) || xrtHttpTokenEqual(
		Name, XRT_STR_LITERAL("rev")
	) || xrtHttpTokenEqual(
		Name, XRT_STR_LITERAL("media")
	) || xrtHttpTokenEqual(
		Name, XRT_STR_LITERAL("title")
	) || xrtHttpTokenEqual(
		Name, XRT_STR_LITERAL("title*")
	) || xrtHttpTokenEqual(
		Name, XRT_STR_LITERAL("type")
	);
}



/* 检查当前单值参数之前是否已有同名项。 */
static bool __xrtHttpLinkWriteParamSeen(
	const xhttplinkparamvalue* pParams,
	size_t iBefore,
	xstrview Name
)
{
	xhttplinkparamvalue Param;
	size_t i;

	for ( i = 0; i < iBefore; i++ ) {
		__xrtHttpLinkParamValueLoad(pParams, i, &Param);
		if ( xrtHttpTokenEqual(Param.Name, Name) ) {
			return true;
		}
	}
	return false;
}



/* 验证一个生产侧参数及已知语义。 */
static bool __xrtHttpLinkWriteParamValid(
	const xhttplinkparamvalue* pParam
)
{
	xhttpextvalue ExtValue;
	xhttpparam Param;
	uint32 iKnown = XHTTP_PARAM_HAS_VALUE |
		XHTTP_PARAM_QUOTED;

	if ( !xrtHttpTokenValid(pParam->Name) ||
		!__xrtHttpViewValid(pParam->Value) ||
		((pParam->Flags & ~iKnown) != 0) ||
		(((pParam->Flags & XHTTP_PARAM_QUOTED) != 0) &&
		 ((pParam->Flags & XHTTP_PARAM_HAS_VALUE) == 0)) ||
		(((pParam->Flags & XHTTP_PARAM_HAS_VALUE) == 0) &&
		 (pParam->Value.Size != 0)) ||
		__xrtHttpLinkWriteRel(pParam->Name) ) {
		return false;
	}
	if ( (pParam->Flags & XHTTP_PARAM_HAS_VALUE) != 0 ) {
		if ( (pParam->Flags & XHTTP_PARAM_QUOTED) != 0 ) {
			size_t iSize;

			if ( !xrtHttpQuotedWrite(
				pParam->Value, NULL, 0, &iSize
			) ) {
				return false;
			}
		} else if ( !xrtHttpTokenValid(pParam->Value) ) {
			return false;
		}
	}
	Param.Name = pParam->Name;
	Param.Value = pParam->Value;
	Param.Flags = pParam->Flags;
	if ( xrtHttpTokenEqual(
		pParam->Name, XRT_STR_LITERAL("anchor")
	) ) {
		return __xrtHttpLinkUriParamValid(&Param);
	}
	if ( xrtHttpTokenEqual(
		pParam->Name, XRT_STR_LITERAL("rev")
	) ) {
		return __xrtHttpLinkRelationsParamValid(&Param);
	}
	if ( xrtHttpTokenEqual(
		pParam->Name, XRT_STR_LITERAL("hreflang")
	) ) {
		return __xrtHttpLinkLanguageParamValid(&Param);
	}
	if ( xrtHttpTokenEqual(
		pParam->Name, XRT_STR_LITERAL("media")
	) ) {
		size_t iSize;

		return ((pParam->Flags &
			XHTTP_PARAM_HAS_VALUE) != 0) &&
			xrtHttpParamValueWrite(
				&Param, NULL, 0, &iSize
			) && (iSize != 0);
	}
	if ( xrtHttpTokenEqual(
		pParam->Name, XRT_STR_LITERAL("title")
	) ) {
		return (pParam->Flags &
			XHTTP_PARAM_HAS_VALUE) != 0;
	}
	if ( xrtHttpTokenEqual(
		pParam->Name, XRT_STR_LITERAL("title*")
	) ) {
		return ((pParam->Flags & XHTTP_PARAM_HAS_VALUE) != 0) &&
			((pParam->Flags & XHTTP_PARAM_QUOTED) == 0) &&
			__xrtHttpExtValueSplit(
				pParam->Value, &ExtValue
			);
	}
	if ( xrtHttpTokenEqual(
		pParam->Name, XRT_STR_LITERAL("type")
	) ) {
		return __xrtHttpLinkTypeParamValid(&Param);
	}
	return true;
}



/* 验证一个 Link 写入描述符。 */
static bool __xrtHttpLinkWriteValueValid(
	const xhttplinkvalue* pLink
)
{
	xhttplinkparamvalue Param;
	xurl Target;
	size_t iBytes;
	size_t i;

	if ( !__xrtHttpViewValid(pLink->Target) ||
		!__xrtHttpViewValid(pLink->Relations) ||
		(pLink->ParameterCount >
		 (SIZE_MAX / sizeof(Param))) ||
		!__xrtUrlParseValue(pLink->Target, &Target) ||
		!__xrtHttpLinkRelationsValueValid(pLink->Relations) ) {
		return false;
	}
	iBytes = pLink->ParameterCount * sizeof(Param);
	if ( !__xrtRangeValid(pLink->Parameters, iBytes) ) {
		return false;
	}
	for ( i = 0; i < pLink->ParameterCount; i++ ) {
		__xrtHttpLinkParamValueLoad(
			pLink->Parameters, i, &Param
		);
		if ( !__xrtHttpLinkWriteParamValid(&Param) ||
			(__xrtHttpLinkWriteSingleton(Param.Name) &&
			 __xrtHttpLinkWriteParamSeen(
				pLink->Parameters, i, Param.Name
			 )) ) {
			return false;
		}
	}
	return true;
}



/* 计算一个参数的线路长度。 */
static bool __xrtHttpLinkWriteParamMeasure(
	const xhttplinkparamvalue* pParam,
	size_t* pSize
)
{
	size_t iValue = 0;

	if ( (pParam->Flags & XHTTP_PARAM_HAS_VALUE) != 0 ) {
		if ( (pParam->Flags & XHTTP_PARAM_QUOTED) != 0 ) {
			if ( !xrtHttpQuotedWrite(
				pParam->Value, NULL, 0, &iValue
			) ) {
				return false;
			}
		} else {
			iValue = pParam->Value.Size;
		}
	}
	return __xrtHttpSizeAdd(pSize, 2u) &&
		__xrtHttpSizeAdd(pSize, pParam->Name.Size) &&
		(((pParam->Flags & XHTTP_PARAM_HAS_VALUE) == 0) ||
		 (__xrtHttpSizeAdd(pSize, 1u) &&
		  __xrtHttpSizeAdd(pSize, iValue)));
}



/* 计算一个 Link 元素的线路长度。 */
static bool __xrtHttpLinkWriteValueMeasure(
	const xhttplinkvalue* pLink,
	size_t* pSize
)
{
	xhttplinkparamvalue Param;
	size_t iRelation;
	size_t i;

	if ( xrtHttpTokenValid(pLink->Relations) ) {
		iRelation = pLink->Relations.Size;
	} else if ( !xrtHttpQuotedWrite(
		pLink->Relations, NULL, 0, &iRelation
	) ) {
		return false;
	}
	if ( !__xrtHttpSizeAdd(pSize, 1u) ||
		!__xrtHttpSizeAdd(pSize, pLink->Target.Size) ||
		!__xrtHttpSizeAdd(pSize, 7u) ||
		!__xrtHttpSizeAdd(pSize, iRelation) ) {
		return false;
	}
	for ( i = 0; i < pLink->ParameterCount; i++ ) {
		__xrtHttpLinkParamValueLoad(
			pLink->Parameters, i, &Param
		);
		if ( !__xrtHttpLinkWriteParamMeasure(
			&Param, pSize
		) ) {
			return false;
		}
	}
	return true;
}



/* 向已确认容量的缓冲写出一个参数。 */
static void __xrtHttpLinkWriteParamUnchecked(
	const xhttplinkparamvalue* pParam,
	uint8* pOutput,
	size_t* pOffset
)
{
	pOutput[(*pOffset)++] = (uint8)';';
	pOutput[(*pOffset)++] = (uint8)' ';
	memcpy(
		pOutput + *pOffset,
		pParam->Name.Data,
		pParam->Name.Size
	);
	*pOffset += pParam->Name.Size;
	if ( (pParam->Flags & XHTTP_PARAM_HAS_VALUE) == 0 ) {
		return;
	}
	pOutput[(*pOffset)++] = (uint8)'=';
	if ( (pParam->Flags & XHTTP_PARAM_QUOTED) != 0 ) {
		*pOffset += __xrtHttpQuotedWriteUnchecked(
			pParam->Value, pOutput + *pOffset
		);
	} else {
		memcpy(
			pOutput + *pOffset,
			pParam->Value.Data,
			pParam->Value.Size
		);
		*pOffset += pParam->Value.Size;
	}
}



/* 向已确认容量的缓冲写出一个 Link 元素。 */
static void __xrtHttpLinkWriteValueUnchecked(
	const xhttplinkvalue* pLink,
	uint8* pOutput,
	size_t* pOffset
)
{
	xhttplinkparamvalue Param;
	size_t i;

	pOutput[(*pOffset)++] = (uint8)'<';
	memcpy(
		pOutput + *pOffset,
		pLink->Target.Data,
		pLink->Target.Size
	);
	*pOffset += pLink->Target.Size;
	memcpy(pOutput + *pOffset, ">; rel=", 7u);
	*pOffset += 7u;
	if ( xrtHttpTokenValid(pLink->Relations) ) {
		memcpy(
			pOutput + *pOffset,
			pLink->Relations.Data,
			pLink->Relations.Size
		);
		*pOffset += pLink->Relations.Size;
	} else {
		*pOffset += __xrtHttpQuotedWriteUnchecked(
			pLink->Relations, pOutput + *pOffset
		);
	}
	for ( i = 0; i < pLink->ParameterCount; i++ ) {
		__xrtHttpLinkParamValueLoad(
			pLink->Parameters, i, &Param
		);
		__xrtHttpLinkWriteParamUnchecked(
			&Param, pOutput, pOffset
		);
	}
}



/* 判断输出范围是否覆盖任一写入描述符或借用视图。 */
static bool __xrtHttpLinkWriteOutputOverlap(
	const xhttplinkvalue* pLinks,
	size_t iCount,
	const void* pOutput,
	size_t iSize
)
{
	xhttplinkvalue Link;
	xhttplinkparamvalue Param;
	size_t iLinks = iCount * sizeof(Link);
	size_t iParams;
	size_t i;
	size_t j;

	if ( __xrtRangesOverlap(
		pLinks, iLinks, pOutput, iSize
	) ) {
		return true;
	}
	for ( i = 0; i < iCount; i++ ) {
		__xrtHttpLinkValueLoad(pLinks, i, &Link);
		if ( __xrtRangesOverlap(
			Link.Target.Data, Link.Target.Size,
			pOutput, iSize
		) || __xrtRangesOverlap(
			Link.Relations.Data, Link.Relations.Size,
			pOutput, iSize
		) ) {
			return true;
		}
		iParams = Link.ParameterCount * sizeof(Param);
		if ( __xrtRangesOverlap(
			Link.Parameters, iParams, pOutput, iSize
		) ) {
			return true;
		}
		for ( j = 0; j < Link.ParameterCount; j++ ) {
			__xrtHttpLinkParamValueLoad(
				Link.Parameters, j, &Param
			);
			if ( __xrtRangesOverlap(
				Param.Name.Data, Param.Name.Size,
				pOutput, iSize
			) || __xrtRangesOverlap(
				Param.Value.Data, Param.Value.Size,
				pOutput, iSize
			) ) {
				return true;
			}
		}
	}
	return false;
}



/* 规范写出 Link 元素数组。 */
XRT_API bool xrtHttpLinkWrite(
	const xhttplinkvalue* pLinks,
	size_t iCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xhttplinkvalue Link;
	uint8* pWrite = (uint8*)pOutput;
	size_t iRequired = 0;
	size_t iLinks;
	size_t iOffset = 0;
	size_t i;

	if ( (iCount > (SIZE_MAX / sizeof(Link))) ||
		!__xrtRangeValid(pSize, sizeof(iRequired)) ||
		((pOutput == NULL) && (iCapacity != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	iLinks = iCount * sizeof(Link);
	if ( !__xrtRangeValid(pLinks, iLinks) ||
		__xrtRangesOverlap(
			pLinks, iLinks, pSize, sizeof(iRequired)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	for ( i = 0; i < iCount; i++ ) {
		__xrtHttpLinkValueLoad(pLinks, i, &Link);
		if ( !__xrtHttpLinkWriteValueValid(&Link) ) {
			if ( xrtGetError() == NULL ) {
				__xrtErrorSetValue();
			}
			return false;
		}
		if ( ((i != 0) &&
			 !__xrtHttpSizeAdd(&iRequired, 2u)) ||
			!__xrtHttpLinkWriteValueMeasure(
				&Link, &iRequired
			) ) {
			return false;
		}
	}
	if ( __xrtHttpLinkWriteOutputOverlap(
		pLinks, iCount, pSize, sizeof(iRequired)
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pOutput == NULL ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		return true;
	}
	if ( !__xrtRangeValid(pOutput, iCapacity) ||
		__xrtRangesOverlap(
			pOutput, iCapacity, pSize, sizeof(iRequired)
		) || __xrtHttpLinkWriteOutputOverlap(
			pLinks, iCount, pOutput, iCapacity
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( iCapacity < iRequired ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		__xrtErrorSetRange();
		return false;
	}
	for ( i = 0; i < iCount; i++ ) {
		__xrtHttpLinkValueLoad(pLinks, i, &Link);
		if ( i != 0 ) {
			pWrite[iOffset++] = (uint8)',';
			pWrite[iOffset++] = (uint8)' ';
		}
		__xrtHttpLinkWriteValueUnchecked(
			&Link, pWrite, &iOffset
		);
	}
	memcpy(pSize, &iOffset, sizeof(iOffset));
	return true;
}



/* 规范写出一个 link-value。 */
XRT_API bool xrtHttpLinkElementWrite(
	const xhttplinkvalue* pLink,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	return xrtHttpLinkWrite(
		pLink, 1u, pOutput, iCapacity, pSize
	);
}



/* 构建零结尾 Link 字段值。 */
XRT_API str xrtHttpLinkBuild(
	const xhttplinkvalue* pLinks,
	size_t iCount,
	size_t* pSize
)
{
	str sOutput;
	size_t iRequired;

	if ( !xrtHttpLinkWrite(
		pLinks, iCount, NULL, 0, &iRequired
	) ) {
		return NULL;
	}
	if ( (pSize != NULL) &&
		(!__xrtRangeValid(pSize, sizeof(iRequired)) ||
		 __xrtHttpLinkWriteOutputOverlap(
			pLinks, iCount, pSize, sizeof(iRequired)
		 )) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( iRequired == SIZE_MAX ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	sOutput = (str)xrtMalloc(iRequired + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !xrtHttpLinkWrite(
		pLinks, iCount, sOutput,
		iRequired, &iRequired
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
