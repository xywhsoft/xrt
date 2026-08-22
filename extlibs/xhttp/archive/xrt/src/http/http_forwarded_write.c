#include "../internal/xrt_http.h"

#include <xrt/http_forwarded.h>



#if defined(XRT_FEATURE_HTTP_FORWARDED_WRITE)

/* 从可能未对齐的元素数组读取一个描述符。 */
static void __xrtHttpForwardedValueLoad(
	const xhttpforwardedvalue* pElements,
	size_t iIndex,
	xhttpforwardedvalue* pElement
)
{
	memcpy(
		pElement,
		(const uint8*)pElements +
		(iIndex * sizeof(*pElements)),
		sizeof(*pElement)
	);
}



/* 从可能未对齐的扩展数组读取一个描述符。 */
static void __xrtHttpForwardedPairValueLoad(
	const xhttpforwardedpairvalue* pPairs,
	size_t iIndex,
	xhttpforwardedpairvalue* pPair
)
{
	memcpy(
		pPair,
		(const uint8*)pPairs +
		(iIndex * sizeof(*pPairs)),
		sizeof(*pPair)
	);
}



/* 返回存在位对应的标准参数名称和值。 */
static void __xrtHttpForwardedKnownValue(
	const xhttpforwardedvalue* pElement,
	uint32 iFlag,
	xhttpforwardedpairvalue* pPair
)
{
	if ( iFlag == XHTTP_FORWARDED_HAS_FOR ) {
		pPair->Name = XRT_STR_LITERAL("for");
		pPair->Value = pElement->For;
	} else if ( iFlag == XHTTP_FORWARDED_HAS_BY ) {
		pPair->Name = XRT_STR_LITERAL("by");
		pPair->Value = pElement->By;
	} else if ( iFlag == XHTTP_FORWARDED_HAS_HOST ) {
		pPair->Name = XRT_STR_LITERAL("host");
		pPair->Value = pElement->Host;
	} else {
		pPair->Name = XRT_STR_LITERAL("proto");
		pPair->Value = pElement->Proto;
	}
}



/* 判断扩展名称是否占用标准参数槽。 */
static bool __xrtHttpForwardedWriteKnown(xstrview Name)
{
	return xrtHttpTokenEqual(Name, XRT_STR_LITERAL("for")) ||
		xrtHttpTokenEqual(Name, XRT_STR_LITERAL("by")) ||
		xrtHttpTokenEqual(Name, XRT_STR_LITERAL("host")) ||
		xrtHttpTokenEqual(Name, XRT_STR_LITERAL("proto"));
}



/* 检查当前扩展之前是否已有同名扩展。 */
static bool __xrtHttpForwardedExtensionSeen(
	const xhttpforwardedpairvalue* pPairs,
	size_t iBefore,
	xstrview Name
)
{
	xhttpforwardedpairvalue Pair;
	size_t i;

	for ( i = 0; i < iBefore; i++ ) {
		__xrtHttpForwardedPairValueLoad(pPairs, i, &Pair);
		if ( xrtHttpTokenEqual(Pair.Name, Name) ) {
			return true;
		}
	}
	return false;
}



/* 验证一个写入元素及其嵌套扩展的全部内存范围。 */
static bool __xrtHttpForwardedValueMemoryValid(
	const xhttpforwardedvalue* pElement
)
{
	xhttpforwardedpairvalue Pair;
	size_t iBytes;
	size_t i;

	if ( !__xrtHttpViewValid(pElement->For) ||
		!__xrtHttpViewValid(pElement->By) ||
		!__xrtHttpViewValid(pElement->Host) ||
		!__xrtHttpViewValid(pElement->Proto) ||
		(pElement->ExtensionCount >
		 (SIZE_MAX / sizeof(Pair))) ) {
		return false;
	}
	iBytes = pElement->ExtensionCount * sizeof(Pair);
	if ( !__xrtRangeValid(pElement->Extensions, iBytes) ) {
		return false;
	}
	for ( i = 0; i < pElement->ExtensionCount; i++ ) {
		__xrtHttpForwardedPairValueLoad(
			pElement->Extensions, i, &Pair
		);
		if ( !__xrtHttpViewValid(Pair.Name) ||
			!__xrtHttpViewValid(Pair.Value) ) {
			return false;
		}
	}
	return true;
}



/* 验证已经完成内存预检的标准参数和扩展参数语义。 */
static bool __xrtHttpForwardedValueValid(
	const xhttpforwardedvalue* pElement
)
{
	xhttpforwardedpairvalue Pair;
	size_t i;
	uint32 iKnown = XHTTP_FORWARDED_HAS_FOR |
		XHTTP_FORWARDED_HAS_BY |
		XHTTP_FORWARDED_HAS_HOST |
		XHTTP_FORWARDED_HAS_PROTO;

	if ( (pElement->Flags & ~iKnown) != 0 ) {
		return false;
	}
	if ( ((pElement->Flags & XHTTP_FORWARDED_HAS_FOR) != 0) ?
		!xrtHttpForwardedNodeValid(pElement->For) :
		(pElement->For.Size != 0) ) {
		return false;
	}
	if ( ((pElement->Flags & XHTTP_FORWARDED_HAS_BY) != 0) ?
		!xrtHttpForwardedNodeValid(pElement->By) :
		(pElement->By.Size != 0) ) {
		return false;
	}
	if ( ((pElement->Flags & XHTTP_FORWARDED_HAS_HOST) != 0) ?
		!xrtHttpForwardedHostValid(pElement->Host) :
		(pElement->Host.Size != 0) ) {
		return false;
	}
	if ( ((pElement->Flags & XHTTP_FORWARDED_HAS_PROTO) != 0) ?
		!xrtHttpForwardedProtoValid(pElement->Proto) :
		(pElement->Proto.Size != 0) ) {
		return false;
	}
	if ( ((pElement->Flags == 0) &&
		 (pElement->ExtensionCount == 0)) ) {
		return false;
	}
	for ( i = 0; i < pElement->ExtensionCount; i++ ) {
		__xrtHttpForwardedPairValueLoad(
			pElement->Extensions, i, &Pair
		);
		if ( !xrtHttpTokenValid(Pair.Name) ||
			__xrtHttpForwardedWriteKnown(Pair.Name) ||
			__xrtHttpForwardedExtensionSeen(
				pElement->Extensions, i, Pair.Name
			) ) {
			return false;
		}
	}
	return true;
}



/* 计算一个参数的规范线路长度。 */
static bool __xrtHttpForwardedPairMeasure(
	const xhttpforwardedpairvalue* pPair,
	bool bFirst,
	size_t* pSize
)
{
	size_t iValue;

	if ( xrtHttpTokenValid(pPair->Value) ) {
		iValue = pPair->Value.Size;
	} else if ( !xrtHttpQuotedWrite(
		pPair->Value, NULL, 0, &iValue
	) ) {
		return false;
	}
	return (bFirst || __xrtHttpSizeAdd(pSize, 1u)) &&
		__xrtHttpSizeAdd(pSize, pPair->Name.Size) &&
		__xrtHttpSizeAdd(pSize, 1u) &&
		__xrtHttpSizeAdd(pSize, iValue);
}



/* 计算一个代理元素的规范线路长度。 */
static bool __xrtHttpForwardedValueMeasure(
	const xhttpforwardedvalue* pElement,
	size_t* pSize
)
{
	static const uint32 Flags[] = {
		XHTTP_FORWARDED_HAS_FOR,
		XHTTP_FORWARDED_HAS_BY,
		XHTTP_FORWARDED_HAS_HOST,
		XHTTP_FORWARDED_HAS_PROTO
	};
	xhttpforwardedpairvalue Pair;
	size_t iPairs = 0;
	size_t i;

	for ( i = 0; i < (sizeof(Flags) / sizeof(Flags[0])); i++ ) {
		if ( (pElement->Flags & Flags[i]) == 0 ) {
			continue;
		}
		__xrtHttpForwardedKnownValue(
			pElement, Flags[i], &Pair
		);
		if ( !__xrtHttpForwardedPairMeasure(
			&Pair, iPairs == 0, pSize
		) ) {
			return false;
		}
		iPairs++;
	}
	for ( i = 0; i < pElement->ExtensionCount; i++ ) {
		__xrtHttpForwardedPairValueLoad(
			pElement->Extensions, i, &Pair
		);
		if ( !__xrtHttpForwardedPairMeasure(
			&Pair, iPairs == 0, pSize
		) ) {
			return false;
		}
		iPairs++;
	}
	return true;
}



/* 写出一个已经验证和测量的参数。 */
static void __xrtHttpForwardedPairWriteUnchecked(
	const xhttpforwardedpairvalue* pPair,
	bool bFirst,
	uint8* pOutput,
	size_t* pOffset
)
{
	if ( !bFirst ) {
		pOutput[(*pOffset)++] = (uint8)';';
	}
	memcpy(pOutput + *pOffset, pPair->Name.Data, pPair->Name.Size);
	*pOffset += pPair->Name.Size;
	pOutput[(*pOffset)++] = (uint8)'=';
	if ( xrtHttpTokenValid(pPair->Value) ) {
		memcpy(
			pOutput + *pOffset,
			pPair->Value.Data,
			pPair->Value.Size
		);
		*pOffset += pPair->Value.Size;
	} else {
		*pOffset += __xrtHttpQuotedWriteUnchecked(
			pPair->Value, pOutput + *pOffset
		);
	}
}



/* 写出一个已经验证的代理元素。 */
static void __xrtHttpForwardedValueWriteUnchecked(
	const xhttpforwardedvalue* pElement,
	uint8* pOutput,
	size_t* pOffset
)
{
	static const uint32 Flags[] = {
		XHTTP_FORWARDED_HAS_FOR,
		XHTTP_FORWARDED_HAS_BY,
		XHTTP_FORWARDED_HAS_HOST,
		XHTTP_FORWARDED_HAS_PROTO
	};
	xhttpforwardedpairvalue Pair;
	size_t iPairs = 0;
	size_t i;

	for ( i = 0; i < (sizeof(Flags) / sizeof(Flags[0])); i++ ) {
		if ( (pElement->Flags & Flags[i]) == 0 ) {
			continue;
		}
		__xrtHttpForwardedKnownValue(
			pElement, Flags[i], &Pair
		);
		__xrtHttpForwardedPairWriteUnchecked(
			&Pair, iPairs == 0, pOutput, pOffset
		);
		iPairs++;
	}
	for ( i = 0; i < pElement->ExtensionCount; i++ ) {
		__xrtHttpForwardedPairValueLoad(
			pElement->Extensions, i, &Pair
		);
		__xrtHttpForwardedPairWriteUnchecked(
			&Pair, iPairs == 0, pOutput, pOffset
		);
		iPairs++;
	}
}



/* 写出已经完成验证和测量的 Forwarded 元素数组。 */
static void __xrtHttpForwardedWriteUnchecked(
	const xhttpforwardedvalue* pElements,
	size_t iCount,
	uint8* pOutput,
	size_t* pSize
)
{
	xhttpforwardedvalue Element;
	size_t iOffset = 0;
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		__xrtHttpForwardedValueLoad(pElements, i, &Element);
		if ( i != 0 ) {
			pOutput[iOffset++] = (uint8)',';
			pOutput[iOffset++] = (uint8)' ';
		}
		__xrtHttpForwardedValueWriteUnchecked(
			&Element, pOutput, &iOffset
		);
	}
	*pSize = iOffset;
}



/* 判断输出范围是否覆盖描述符或任一借用视图。 */
static bool __xrtHttpForwardedOutputOverlap(
	const xhttpforwardedvalue* pElements,
	size_t iCount,
	const void* pOutput,
	size_t iSize
)
{
	xhttpforwardedvalue Element;
	xhttpforwardedpairvalue Pair;
	size_t iElements = iCount * sizeof(Element);
	size_t iExtensions;
	size_t i;
	size_t j;

	if ( __xrtRangesOverlap(
		pElements, iElements, pOutput, iSize
	) ) {
		return true;
	}
	for ( i = 0; i < iCount; i++ ) {
		__xrtHttpForwardedValueLoad(pElements, i, &Element);
		if ( __xrtRangesOverlap(
			Element.For.Data, Element.For.Size, pOutput, iSize
		) || __xrtRangesOverlap(
			Element.By.Data, Element.By.Size, pOutput, iSize
		) || __xrtRangesOverlap(
			Element.Host.Data, Element.Host.Size, pOutput, iSize
		) || __xrtRangesOverlap(
			Element.Proto.Data, Element.Proto.Size, pOutput, iSize
		) ) {
			return true;
		}
		iExtensions = Element.ExtensionCount * sizeof(Pair);
		if ( __xrtRangesOverlap(
			Element.Extensions, iExtensions, pOutput, iSize
		) ) {
			return true;
		}
		for ( j = 0; j < Element.ExtensionCount; j++ ) {
			__xrtHttpForwardedPairValueLoad(
				Element.Extensions, j, &Pair
			);
			if ( __xrtRangesOverlap(
				Pair.Name.Data, Pair.Name.Size, pOutput, iSize
			) || __xrtRangesOverlap(
				Pair.Value.Data, Pair.Value.Size, pOutput, iSize
			) ) {
				return true;
			}
		}
	}
	return false;
}



/* 在读取任何语义值前预检顶层、嵌套输入和全部输出范围。 */
static bool __xrtHttpForwardedMemoryValid(
	const xhttpforwardedvalue* pElements,
	size_t iCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	bool bSizeOptional
)
{
	xhttpforwardedvalue Element;
	size_t iElements;
	size_t i;

	if ( (iCount == 0) ||
		(iCount > (SIZE_MAX / sizeof(Element))) ||
		((!bSizeOptional || (pSize != NULL)) &&
		 !__xrtRangeValid(pSize, sizeof(size_t))) ||
		((pOutput == NULL) && (iCapacity != 0)) ||
		!__xrtRangeValid(pOutput, iCapacity) ) {
		return false;
	}
	iElements = iCount * sizeof(Element);
	if ( !__xrtRangeValid(pElements, iElements) ) {
		return false;
	}
	for ( i = 0; i < iCount; i++ ) {
		__xrtHttpForwardedValueLoad(pElements, i, &Element);
		if ( !__xrtHttpForwardedValueMemoryValid(&Element) ) {
			return false;
		}
	}
	if ( ((pSize != NULL) &&
		 __xrtHttpForwardedOutputOverlap(
			pElements, iCount, pSize, sizeof(size_t)
		 )) || ((pOutput != NULL) &&
		 (((pSize != NULL) && __xrtRangesOverlap(
			pOutput, iCapacity, pSize, sizeof(size_t)
		 )) || __xrtHttpForwardedOutputOverlap(
			pElements, iCount, pOutput, iCapacity
		 ))) ) {
		return false;
	}
	return true;
}



/* 验证全部生产语义并精确测量规范字段值。 */
static bool __xrtHttpForwardedMeasure(
	const xhttpforwardedvalue* pElements,
	size_t iCount,
	size_t* pSize
)
{
	xhttpforwardedvalue Element;
	size_t iRequired = 0;
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		__xrtHttpForwardedValueLoad(pElements, i, &Element);
		if ( !__xrtHttpForwardedValueValid(&Element) ) {
			__xrtErrorSetValue();
			return false;
		}
		if ( ((i != 0) &&
			 !__xrtHttpSizeAdd(&iRequired, 2u)) ||
			!__xrtHttpForwardedValueMeasure(
				&Element, &iRequired
			) ) {
			return false;
		}
	}
	*pSize = iRequired;
	return true;
}



/* 规范写出 Forwarded 元素数组。 */
XRT_API bool xrtHttpForwardedWrite(
	const xhttpforwardedvalue* pElements,
	size_t iCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	uint8* pWrite = (uint8*)pOutput;
	size_t iRequired;
	size_t iWritten;

	if ( !__xrtHttpForwardedMemoryValid(
		pElements, iCount, pOutput,
		iCapacity, pSize, false
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtHttpForwardedMeasure(
		pElements, iCount, &iRequired
	) ) {
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
	__xrtHttpForwardedWriteUnchecked(
		pElements, iCount, pWrite, &iWritten
	);
	memcpy(pSize, &iWritten, sizeof(iWritten));
	return true;
}



/* 规范写出单个 Forwarded 元素。 */
XRT_API bool xrtHttpForwardedElementWrite(
	const xhttpforwardedvalue* pElement,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	return xrtHttpForwardedWrite(
		pElement, 1u, pOutput, iCapacity, pSize
	);
}



/* 构建零结尾 Forwarded 字段值。 */
XRT_API str xrtHttpForwardedBuild(
	const xhttpforwardedvalue* pElements,
	size_t iCount,
	size_t* pSize
)
{
	str sOutput;
	size_t iRequired;
	size_t iWritten;

	if ( !__xrtHttpForwardedMemoryValid(
		pElements, iCount, NULL, 0, pSize, true
	) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( !__xrtHttpForwardedMeasure(
		pElements, iCount, &iRequired
	) ) {
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
	__xrtHttpForwardedWriteUnchecked(
		pElements, iCount, (uint8*)sOutput, &iWritten
	);
	sOutput[iWritten] = '\0';
	if ( pSize != NULL ) {
		memcpy(pSize, &iWritten, sizeof(iWritten));
	}
	return sOutput;
}

#endif
