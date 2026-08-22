#include "../internal/xrt_http_semantics.h"



#if defined(XHTTP_FEATURE_HTTP_RANGE)

/* 无错误副作用地解析一个 uint64 十进制数。 */
static bool __xrtHttpRangeNumber(
	xstrview Text,
	size_t* pOffset,
	uint64* pValue
)
{
	uint64 iValue = 0;
	uint64 iDigit;
	size_t iOffset = *pOffset;
	bool bAny = false;

	while ( (iOffset < Text.Size) &&
		(Text.Data[iOffset] >= '0') &&
		(Text.Data[iOffset] <= '9') ) {
		iDigit = (uint64)(Text.Data[iOffset] - '0');
		if ( iValue > ((UINT64_MAX - iDigit) / UINT64_C(10)) ) {
			return false;
		}
		iValue = (iValue * UINT64_C(10)) + iDigit;
		iOffset++;
		bAny = true;
	}
	if ( !bAny ) {
		return false;
	}
	*pOffset = iOffset;
	*pValue = iValue;
	return true;
}



/* 安全累加一个输出长度。 */
static bool __xrtHttpRangeSizeAdd(
	size_t* pSize,
	size_t iAdd
)
{
	if ( *pSize > (SIZE_MAX - iAdd) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	*pSize += iAdd;
	return true;
}



/* 无错误副作用地拆分一个完整 Range 字段值。 */
bool __xrtHttpRangeParseValue(
	xstrview Value,
	xstrview* pUnit,
	xstrview* pSet
)
{
	xstrview Unit;
	xstrview Set;
	size_t iEqual = 0;

	if ( (pUnit == NULL) || (pSet == NULL) ||
		!__xrtHttpViewValid(Value) ) {
		return false;
	}
	Value = xrtHttpOwsTrim(Value);
	while ( (iEqual < Value.Size) &&
		(Value.Data[iEqual] != '=') ) {
		iEqual++;
	}
	if ( (iEqual == Value.Size) || (iEqual == 0) ) {
		return false;
	}
	Unit.Data = Value.Data;
	Unit.Size = iEqual;
	Set.Data = Value.Data + iEqual + 1u;
	Set.Size = Value.Size - iEqual - 1u;
	if ( !xrtHttpTokenValid(Unit) || (Set.Size == 0) ) {
		return false;
	}
	*pUnit = Unit;
	*pSet = Set;
	return true;
}



/* 把 Range 字段拆成范围单位和集合。 */
XRT_API bool xrtHttpRangeParse(
	xstrview Value,
	xstrview* pUnit,
	xstrview* pSet
)
{
	xstrview Unit;
	xstrview Set;
	xstrview Empty = { NULL, 0 };

	if ( !__xrtRangeValid(pUnit, sizeof(Unit)) ||
		!__xrtRangeValid(pSet, sizeof(Set)) ||
		!__xrtHttpViewValid(Value) ||
		__xrtRangesOverlap(
			pUnit, sizeof(*pUnit), Value.Data, Value.Size
		) || __xrtRangesOverlap(
			pSet, sizeof(*pSet), Value.Data, Value.Size
		) || __xrtRangesOverlap(
			pUnit, sizeof(*pUnit), pSet, sizeof(*pSet)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(pUnit, &Empty, sizeof(Empty));
	memcpy(pSet, &Empty, sizeof(Empty));
	if ( !__xrtHttpRangeParseValue(
		Value, &Unit, &Set
	) ) {
		__xrtErrorSetValue();
		return false;
	}
	memcpy(pUnit, &Unit, sizeof(Unit));
	memcpy(pSet, &Set, sizeof(Set));
	return true;
}



/* 解析一个去掉 OWS 的 byte-range-spec。 */
static bool __xrtHttpByteRangeSpecParse(
	xstrview Text,
	xhttprangespec* pSpec
)
{
	xhttprangespec Spec;
	size_t iOffset = 0;

	memset(&Spec, 0, sizeof(Spec));
	if ( Text.Size == 0 ) {
		return false;
	}
	if ( Text.Data[0] == '-' ) {
		iOffset = 1;
		if ( !__xrtHttpRangeNumber(
			Text, &iOffset, &Spec.First
		) || (iOffset != Text.Size) ) {
			return false;
		}
		Spec.Form = XHTTP_RANGE_SPEC_SUFFIX;
		*pSpec = Spec;
		return true;
	}
	if ( !__xrtHttpRangeNumber(Text, &iOffset, &Spec.First) ||
		(iOffset >= Text.Size) ||
		(Text.Data[iOffset++] != '-') ) {
		return false;
	}
	if ( iOffset == Text.Size ) {
		Spec.Form = XHTTP_RANGE_SPEC_OPEN;
		*pSpec = Spec;
		return true;
	}
	if ( !__xrtHttpRangeNumber(
		Text, &iOffset, &Spec.Last
	) || (iOffset != Text.Size) ||
		(Spec.First > Spec.Last) ) {
		return false;
	}
	Spec.Form = XHTTP_RANGE_SPEC_CLOSED;
	*pSpec = Spec;
	return true;
}



/* 无错误副作用地迭代已经验证视图的 byte-range-set。 */
xhttpnext __xrtHttpByteRangeNextValue(
	xstrview Set,
	size_t* pOffset,
	xhttprangespec* pSpec
)
{
	xstrview Text;
	size_t iStart;
	size_t iEnd;

	memset(pSpec, 0, sizeof(*pSpec));
	while ( *pOffset < Set.Size ) {
		iStart = *pOffset;
		iEnd = iStart;
		while ( (iEnd < Set.Size) && (Set.Data[iEnd] != ',') ) {
			iEnd++;
		}
		*pOffset = (iEnd < Set.Size) ? (iEnd + 1u) : iEnd;
		Text.Data = Set.Data + iStart;
		Text.Size = iEnd - iStart;
		Text = xrtHttpOwsTrim(Text);
		if ( Text.Size == 0 ) {
			continue;
		}
		if ( !__xrtHttpByteRangeSpecParse(Text, pSpec) ) {
			return XHTTP_NEXT_ERROR;
		}
		return XHTTP_NEXT_ITEM;
	}
	return XHTTP_NEXT_END;
}



/* 迭代 byte-range-set。 */
XRT_API xhttpnext xrtHttpByteRangeNext(
	xstrview Set,
	size_t* pOffset,
	xhttprangespec* pSpec
)
{
	xhttpnext Next;
	xhttprangespec Spec;
	size_t iOffset;

	if ( !__xrtRangeValid(pOffset, sizeof(iOffset)) ||
		!__xrtRangeValid(pSpec, sizeof(Spec)) ||
		!__xrtHttpViewValid(Set) ||
		__xrtRangesOverlap(
			pOffset, sizeof(*pOffset), Set.Data, Set.Size
		) || __xrtRangesOverlap(
			pSpec, sizeof(*pSpec), Set.Data, Set.Size
		) || __xrtRangesOverlap(
			pOffset, sizeof(*pOffset), pSpec, sizeof(*pSpec)
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	memcpy(&iOffset, pOffset, sizeof(iOffset));
	if ( iOffset > Set.Size ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	Next = __xrtHttpByteRangeNextValue(
		Set, &iOffset, &Spec
	);
	memcpy(pOffset, &iOffset, sizeof(iOffset));
	memcpy(pSpec, &Spec, sizeof(Spec));
	if ( Next == XHTTP_NEXT_ERROR ) {
		__xrtErrorSetValue();
	}
	return Next;
}



/* 验证并统计完整 byte-range-set。 */
XRT_API bool xrtHttpByteRangeCount(
	xstrview Set,
	size_t* pCount
)
{
	xhttprangespec Spec;
	xhttpnext Next;
	size_t iOffset = 0;
	size_t iCount = 0;

	if ( !__xrtRangeValid(pCount, sizeof(iCount)) ||
		!__xrtHttpViewValid(Set) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( __xrtRangesOverlap(
		pCount, sizeof(*pCount), Set.Data, Set.Size
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	do {
		Next = xrtHttpByteRangeNext(Set, &iOffset, &Spec);
		if ( Next == XHTTP_NEXT_ERROR ) {
			return false;
		}
		if ( Next == XHTTP_NEXT_ITEM ) {
			if ( iCount == SIZE_MAX ) {
				__xrtErrorSetSizeOverflow();
				return false;
			}
			iCount++;
		}
	} while ( Next == XHTTP_NEXT_ITEM );
	if ( iCount == 0 ) {
		__xrtErrorSetValue();
		return false;
	}
	memcpy(pCount, &iCount, sizeof(iCount));
	return true;
}



/* 把一个范围项解析到表示长度内。 */
XRT_API xhttprangeresult xrtHttpByteRangeResolve(
	const xhttprangespec* pSpec,
	uint64 iLength,
	xhttpbyterange* pRange
)
{
	xhttprangespec Spec;
	xhttpbyterange Range = { 0, 0 };

	if ( !__xrtRangeValid(pSpec, sizeof(Spec)) ||
		!__xrtRangeValid(pRange, sizeof(Range)) ||
		__xrtRangesOverlap(
			pSpec, sizeof(*pSpec), pRange, sizeof(*pRange)
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_RANGE_ERROR;
	}
	memcpy(&Spec, pSpec, sizeof(Spec));
	if ( (Spec.Form < XHTTP_RANGE_SPEC_CLOSED) ||
		(Spec.Form > XHTTP_RANGE_SPEC_SUFFIX) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_RANGE_ERROR;
	}
	memcpy(pRange, &Range, sizeof(Range));
	if ( Spec.Form == XHTTP_RANGE_SPEC_SUFFIX ) {
		if ( Spec.First == 0 ) {
			return XHTTP_RANGE_UNSATISFIED;
		}
		if ( iLength == 0 ) {
			return XHTTP_RANGE_EMPTY;
		}
		Range.First = (Spec.First >= iLength) ?
			UINT64_C(0) :
			(iLength - Spec.First);
		Range.Last = iLength - UINT64_C(1);
		memcpy(pRange, &Range, sizeof(Range));
		return XHTTP_RANGE_SATISFIED;
	}
	if ( (Spec.Form == XHTTP_RANGE_SPEC_CLOSED) &&
		(Spec.First > Spec.Last) ) {
		__xrtErrorSetValue();
		return XHTTP_RANGE_ERROR;
	}
	if ( (iLength == 0) || (Spec.First >= iLength) ) {
		return XHTTP_RANGE_UNSATISFIED;
	}
	Range.First = Spec.First;
	if ( Spec.Form == XHTTP_RANGE_SPEC_OPEN ) {
		Range.Last = iLength - UINT64_C(1);
	} else {
		Range.Last = (Spec.Last >= iLength) ?
			(iLength - UINT64_C(1)) :
			Spec.Last;
	}
	memcpy(pRange, &Range, sizeof(Range));
	return XHTTP_RANGE_SATISFIED;
}



/* 按起点和终点比较两个已经解析的闭区间。 */
static int __xrtHttpByteRangeCompare(
	const void* pLeft,
	const void* pRight
)
{
	xhttpbyterange Left;
	xhttpbyterange Right;

	memcpy(&Left, pLeft, sizeof(Left));
	memcpy(&Right, pRight, sizeof(Right));
	if ( Left.First < Right.First ) {
		return -1;
	}
	if ( Left.First > Right.First ) {
		return 1;
	}
	if ( Left.Last < Right.Last ) {
		return -1;
	}
	return Left.Last > Right.Last ? 1 : 0;
}



/* 判断右侧有序区间是否应按允许间隔并入左侧。 */
static bool __xrtHttpByteRangeJoins(
	const xhttpbyterange* pLeft,
	const xhttpbyterange* pRight,
	uint64 iMergeGap
)
{
	if ( pRight->First <= pLeft->Last ) {
		return true;
	}
	return (pRight->First - pLeft->Last - UINT64_C(1)) <=
		iMergeGap;
}



/* 原地合并有序范围并计算有效负载总长度。 */
static bool __xrtHttpByteRangesMerge(
	xhttpbyterange* pRanges,
	size_t* pCount,
	uint64 iMergeGap,
	uint64* pSelectedLength
)
{
	xhttpbyterange Current;
	xhttpbyterange Previous;
	size_t iRead;
	size_t iWrite = 0;
	uint64 iSelectedLength = 0;

	for ( iRead = 0; iRead < *pCount; iRead++ ) {
		memcpy(
			&Current,
			((const uint8*)pRanges) +
				(iRead * sizeof(Current)),
			sizeof(Current)
		);
		if ( iWrite != 0 ) {
			memcpy(
				&Previous,
				((const uint8*)pRanges) +
					((iWrite - 1u) * sizeof(Previous)),
				sizeof(Previous)
			);
		}
		if ( (iWrite != 0) &&
			__xrtHttpByteRangeJoins(
				&Previous,
				&Current,
				iMergeGap
			) ) {
			if ( Current.Last > Previous.Last ) {
				Previous.Last = Current.Last;
				memcpy(
					((uint8*)pRanges) +
						((iWrite - 1u) * sizeof(Previous)),
					&Previous,
					sizeof(Previous)
				);
			}
			continue;
		}
		memcpy(
			((uint8*)pRanges) + (iWrite * sizeof(Current)),
			&Current,
			sizeof(Current)
		);
		iWrite++;
	}

	for ( iRead = 0; iRead < iWrite; iRead++ ) {
		memcpy(
			&Current,
			((const uint8*)pRanges) +
				(iRead * sizeof(Current)),
			sizeof(Current)
		);
		uint64 iRangeLength =
			(Current.Last - Current.First) +
			UINT64_C(1);

		if ( iSelectedLength >
			(UINT64_MAX - iRangeLength) ) {
			__xrtErrorSetSizeOverflow();
			return false;
		}
		iSelectedLength += iRangeLength;
	}
	*pCount = iWrite;
	*pSelectedLength = iSelectedLength;
	return true;
}



/* 验证范围集合输出与借用输入完全分离。 */
static bool __xrtHttpByteRangesOutputValid(
	xstrview Set,
	xhttpbyterange* pRanges,
	size_t iCapacity,
	size_t* pCount,
	uint64* pSelectedLength
)
{
	size_t iRangeBytes;

	if ( !__xrtHttpViewValid(Set) ||
		((pRanges == NULL) && (iCapacity != 0)) ||
		(iCapacity > (SIZE_MAX / sizeof(*pRanges))) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	iRangeBytes = iCapacity * sizeof(*pRanges);
	if ( !__xrtRangeValid(pRanges, iRangeBytes) ||
		!__xrtRangeValid(pCount, sizeof(*pCount)) ||
		!__xrtRangeValid(
			pSelectedLength,
			sizeof(*pSelectedLength)
		) || __xrtRangesOverlap(
			pRanges, iRangeBytes, Set.Data, Set.Size
		) || __xrtRangesOverlap(
			pCount, sizeof(*pCount), Set.Data, Set.Size
		) || __xrtRangesOverlap(
			pSelectedLength,
			sizeof(*pSelectedLength),
			Set.Data,
			Set.Size
		) || __xrtRangesOverlap(
			pCount,
			sizeof(*pCount),
			pSelectedLength,
			sizeof(*pSelectedLength)
		) || __xrtRangesOverlap(
			pRanges,
			iRangeBytes,
			pCount,
			sizeof(*pCount)
		) || __xrtRangesOverlap(
			pRanges,
			iRangeBytes,
			pSelectedLength,
			sizeof(*pSelectedLength)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return true;
}



/* 解析、裁剪、排序并合并完整 byte-range-set。 */
XRT_API xhttprangeresult xrtHttpByteRangesResolve(
	xstrview Set,
	uint64 iLength,
	xhttpbyterange* pRanges,
	size_t iCapacity,
	uint64 iMergeGap,
	size_t* pCount,
	uint64* pSelectedLength
)
{
	xhttprangespec Spec;
	xhttpbyterange Range;
	xhttprangeresult Result;
	xhttpnext Next;
	size_t iInputCount;
	size_t iOffset = 0;
	size_t iResolved = 0;
	uint64 iSelectedLength = 0;
	bool bEmpty = false;

	if ( !__xrtHttpByteRangesOutputValid(
			Set,
			pRanges,
			iCapacity,
			pCount,
			pSelectedLength
		) || !xrtHttpByteRangeCount(
			Set,
			&iInputCount
		) ) {
		return XHTTP_RANGE_ERROR;
	}
	if ( iInputCount > iCapacity ) {
		__xrtErrorSetRange();
		return XHTTP_RANGE_ERROR;
	}

	do {
		Next = __xrtHttpByteRangeNextValue(
			Set,
			&iOffset,
			&Spec
		);
		if ( Next == XHTTP_NEXT_ERROR ) {
			__xrtErrorSetInternal();
			return XHTTP_RANGE_ERROR;
		}
		if ( Next != XHTTP_NEXT_ITEM ) {
			continue;
		}
		Result = xrtHttpByteRangeResolve(
			&Spec,
			iLength,
			&Range
		);
		if ( Result == XHTTP_RANGE_ERROR ) {
			return XHTTP_RANGE_ERROR;
		}
		if ( Result == XHTTP_RANGE_SATISFIED ) {
			memcpy(
				((uint8*)pRanges) +
					(iResolved * sizeof(Range)),
				&Range,
				sizeof(Range)
			);
			iResolved++;
		} else if ( Result == XHTTP_RANGE_EMPTY ) {
			bEmpty = true;
		}
	} while ( Next == XHTTP_NEXT_ITEM );

	if ( iResolved == 0 ) {
		memcpy(pCount, &iResolved, sizeof(iResolved));
		memcpy(
			pSelectedLength,
			&iSelectedLength,
			sizeof(iSelectedLength)
		);
		return bEmpty ?
			XHTTP_RANGE_EMPTY :
			XHTTP_RANGE_UNSATISFIED;
	}
	qsort(
		pRanges,
		iResolved,
		sizeof(*pRanges),
		__xrtHttpByteRangeCompare
	);
	if ( !__xrtHttpByteRangesMerge(
			pRanges,
			&iResolved,
			iMergeGap,
			&iSelectedLength
		) ) {
		return XHTTP_RANGE_ERROR;
	}
	memcpy(pCount, &iResolved, sizeof(iResolved));
	memcpy(
		pSelectedLength,
		&iSelectedLength,
		sizeof(iSelectedLength)
	);
	return XHTTP_RANGE_SATISFIED;
}



/* 验证范围写出项并计算精确长度。 */
static bool __xrtHttpRangeWriteMeasure(
	const xhttprangespec* pSpecs,
	size_t iCount,
	size_t* pRequired
)
{
	size_t iRequired = 6;
	xhttprangespec Spec;
	size_t i;

	if ( (iCount == 0) ||
		(iCount > (SIZE_MAX / sizeof(Spec))) ||
		!__xrtRangeValid(pSpecs, iCount * sizeof(Spec)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	for ( i = 0; i < iCount; i++ ) {
		memcpy(
			&Spec,
			(const uint8*)pSpecs + (i * sizeof(Spec)),
			sizeof(Spec)
		);
		if ( (Spec.Form < XHTTP_RANGE_SPEC_CLOSED) ||
			(Spec.Form > XHTTP_RANGE_SPEC_SUFFIX) ||
			((Spec.Form == XHTTP_RANGE_SPEC_CLOSED) &&
			 (Spec.First > Spec.Last)) ) {
			__xrtErrorSetValue();
			return false;
		}
		if ( (i != 0) &&
			!__xrtHttpRangeSizeAdd(&iRequired, 2) ) {
			return false;
		}
		if ( Spec.Form == XHTTP_RANGE_SPEC_SUFFIX ) {
			if ( !__xrtHttpRangeSizeAdd(
				&iRequired,
				1u + __xrtHttpUInt64Size(Spec.First)
			) ) {
				return false;
			}
		} else {
			if ( !__xrtHttpRangeSizeAdd(
				&iRequired,
				__xrtHttpUInt64Size(Spec.First) + 1u
			) ) {
				return false;
			}
			if ( (Spec.Form == XHTTP_RANGE_SPEC_CLOSED) &&
				!__xrtHttpRangeSizeAdd(
					&iRequired,
					__xrtHttpUInt64Size(Spec.Last)
				) ) {
				return false;
			}
		}
	}
	*pRequired = iRequired;
	return true;
}



/* 写出一个 uint64 并推进输出位置。 */
static void __xrtHttpRangeAppendNumber(
	uint8* pOutput,
	size_t* pOffset,
	uint64 iValue
)
{
	*pOffset += __xrtHttpUInt64Write(
		(char*)pOutput + *pOffset,
		iValue
	);
}



/* 写出 bytes Range 字段值。 */
XRT_API bool xrtHttpRangeWrite(
	const xhttprangespec* pSpecs,
	size_t iCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	uint8* pWrite = (uint8*)pOutput;
	xhttprangespec Spec;
	size_t iRequired;
	size_t iOffset = 0;
	size_t i;

	if ( !__xrtRangeValid(pSize, sizeof(iRequired)) ||
		((pOutput == NULL) && (iCapacity != 0)) ||
		(iCount > (SIZE_MAX / sizeof(*pSpecs))) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtHttpRangeWriteMeasure(
		pSpecs, iCount, &iRequired
	) ) {
		return false;
	}
	if ( __xrtRangesOverlap(
		pSize, sizeof(iRequired), pSpecs, iCount * sizeof(*pSpecs)
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pOutput == NULL ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		return true;
	}
	if ( (iCapacity >= iRequired) &&
		!__xrtRangeValid(pOutput, iRequired) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( __xrtRangesOverlap(
		pSize, sizeof(iRequired), pOutput, iRequired
	) || __xrtRangesOverlap(
		pSpecs,
		iCount * sizeof(*pSpecs),
		pOutput,
		iRequired
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( iCapacity < iRequired ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		__xrtErrorSetRange();
		return false;
	}
	memcpy(pWrite, "bytes=", 6);
	iOffset = 6;
	for ( i = 0; i < iCount; i++ ) {
		memcpy(
			&Spec,
			(const uint8*)pSpecs + (i * sizeof(Spec)),
			sizeof(Spec)
		);
		if ( i != 0 ) {
			pWrite[iOffset++] = (uint8)',';
			pWrite[iOffset++] = (uint8)' ';
		}
		if ( Spec.Form == XHTTP_RANGE_SPEC_SUFFIX ) {
			pWrite[iOffset++] = (uint8)'-';
			__xrtHttpRangeAppendNumber(
				pWrite, &iOffset, Spec.First
			);
		} else {
			__xrtHttpRangeAppendNumber(
				pWrite, &iOffset, Spec.First
			);
			pWrite[iOffset++] = (uint8)'-';
			if ( Spec.Form == XHTTP_RANGE_SPEC_CLOSED ) {
				__xrtHttpRangeAppendNumber(
					pWrite, &iOffset, Spec.Last
				);
			}
		}
	}
	memcpy(pSize, &iOffset, sizeof(iOffset));
	return true;
}



/* 构建零结尾 bytes Range 字段值。 */
XRT_API str xrtHttpRangeBuild(
	const xhttprangespec* pSpecs,
	size_t iCount,
	size_t* pSize
)
{
	str sOutput;
	size_t iRequired;

	if ( (pSize != NULL) &&
		!__xrtRangeValid(pSize, sizeof(iRequired)) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( (pSize != NULL) && (pSpecs != NULL) &&
		(iCount <= (SIZE_MAX / sizeof(*pSpecs))) &&
		__xrtRangesOverlap(
			pSize,
			sizeof(iRequired),
			pSpecs,
			iCount * sizeof(*pSpecs)
		) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( !xrtHttpRangeWrite(
		pSpecs, iCount, NULL, 0, &iRequired
	) || (iRequired == SIZE_MAX) ) {
		return NULL;
	}
	sOutput = (str)xrtMalloc(iRequired + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !xrtHttpRangeWrite(
		pSpecs, iCount, sOutput, iRequired, &iRequired
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



/* 无错误副作用地解析一个完整 bytes Content-Range 字段值。 */
bool __xrtHttpContentRangeParseValue(
	xstrview Value,
	xhttpcontentrange* pRange
)
{
	xhttpcontentrange Range;
	size_t iOffset;

	if ( (pRange == NULL) || !__xrtHttpViewValid(Value) ) {
		return false;
	}
	memset(&Range, 0, sizeof(Range));
	Value = xrtHttpOwsTrim(Value);
	if ( (Value.Size < 8) ||
		!xrtHttpTokenEqual(
			(xstrview){ Value.Data, 5 },
			XRT_STR_LITERAL("bytes")
		) || (Value.Data[5] != ' ') ) {
		return false;
	}
	iOffset = 6;
	if ( Value.Data[iOffset] == '*' ) {
		iOffset++;
		if ( (iOffset >= Value.Size) ||
			(Value.Data[iOffset++] != '/') ||
			!__xrtHttpRangeNumber(
				Value, &iOffset, &Range.Length
			) || (iOffset != Value.Size) ) {
			return false;
		}
		Range.Satisfied = false;
		Range.HasLength = true;
		memcpy(pRange, &Range, sizeof(Range));
		return true;
	}
	if ( !__xrtHttpRangeNumber(
		Value, &iOffset, &Range.First
	) || (iOffset >= Value.Size) ||
		(Value.Data[iOffset++] != '-') ||
		!__xrtHttpRangeNumber(
			Value, &iOffset, &Range.Last
		) || (iOffset >= Value.Size) ||
		(Value.Data[iOffset++] != '/') ||
		(Range.First > Range.Last) ) {
		return false;
	}
	if ( Value.Data[iOffset] == '*' ) {
		iOffset++;
		Range.HasLength = false;
	} else {
		if ( !__xrtHttpRangeNumber(
			Value, &iOffset, &Range.Length
		) ) {
			return false;
		}
		Range.HasLength = true;
	}
	if ( (iOffset != Value.Size) ||
		(Range.HasLength && (Range.Last >= Range.Length)) ) {
		return false;
	}
	Range.Satisfied = true;
	memcpy(pRange, &Range, sizeof(Range));
	return true;
}



/* 严格解析 bytes Content-Range 字段值。 */
XRT_API bool xrtHttpContentRangeParse(
	xstrview Value,
	xhttpcontentrange* pRange
)
{
	xhttpcontentrange Range;

	if ( !__xrtHttpViewValid(Value) ||
		!__xrtRangeValid(pRange, sizeof(Range)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( __xrtRangesOverlap(
		pRange, sizeof(*pRange), Value.Data, Value.Size
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(&Range, 0, sizeof(Range));
	memcpy(pRange, &Range, sizeof(Range));
	if ( !__xrtHttpContentRangeParseValue(
		Value, &Range
	) ) {
		__xrtErrorSetValue();
		return false;
	}
	memcpy(pRange, &Range, sizeof(Range));
	return true;
}



/* 验证 Content-Range 并计算精确线路长度。 */
static bool __xrtHttpContentRangeMeasure(
	const xhttpcontentrange* pRange,
	size_t* pRequired
)
{
	size_t iRequired = 6;

	if ( pRange == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pRange->Satisfied ) {
		if ( (pRange->First > pRange->Last) ||
			(pRange->HasLength &&
			 (pRange->Last >= pRange->Length)) ) {
			__xrtErrorSetValue();
			return false;
		}
		iRequired += __xrtHttpUInt64Size(pRange->First);
		iRequired += 1u;
		iRequired += __xrtHttpUInt64Size(pRange->Last);
		iRequired += 1u;
		iRequired += pRange->HasLength ?
			__xrtHttpUInt64Size(pRange->Length) :
			1u;
	} else {
		if ( !pRange->HasLength ) {
			__xrtErrorSetValue();
			return false;
		}
		iRequired += 2u;
		iRequired += __xrtHttpUInt64Size(pRange->Length);
	}
	*pRequired = iRequired;
	return true;
}



/* 写出 bytes Content-Range 字段值。 */
XRT_API bool xrtHttpContentRangeWrite(
	const xhttpcontentrange* pRange,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xhttpcontentrange Range;
	uint8* pWrite = (uint8*)pOutput;
	size_t iRequired;
	size_t iOffset = 6;

	if ( !__xrtRangeValid(pRange, sizeof(Range)) ||
		!__xrtRangeValid(pSize, sizeof(iRequired)) ||
		((pOutput == NULL) && (iCapacity != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(&Range, pRange, sizeof(Range));
	if ( !__xrtHttpContentRangeMeasure(
		&Range, &iRequired
	) ) {
		return false;
	}
	if ( __xrtRangesOverlap(
		pSize, sizeof(iRequired), pRange, sizeof(Range)
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pOutput == NULL ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		return true;
	}
	if ( (iCapacity >= iRequired) &&
		!__xrtRangeValid(pOutput, iRequired) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( __xrtRangesOverlap(
		pSize, sizeof(iRequired), pOutput, iRequired
	) || __xrtRangesOverlap(
		pRange, sizeof(Range), pOutput, iRequired
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( iCapacity < iRequired ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		__xrtErrorSetRange();
		return false;
	}
	memcpy(pWrite, "bytes ", 6);
	if ( Range.Satisfied ) {
		__xrtHttpRangeAppendNumber(
			pWrite, &iOffset, Range.First
		);
		pWrite[iOffset++] = (uint8)'-';
		__xrtHttpRangeAppendNumber(
			pWrite, &iOffset, Range.Last
		);
		pWrite[iOffset++] = (uint8)'/';
		if ( Range.HasLength ) {
			__xrtHttpRangeAppendNumber(
				pWrite, &iOffset, Range.Length
			);
		} else {
			pWrite[iOffset++] = (uint8)'*';
		}
	} else {
		pWrite[iOffset++] = (uint8)'*';
		pWrite[iOffset++] = (uint8)'/';
		__xrtHttpRangeAppendNumber(
			pWrite, &iOffset, Range.Length
		);
	}
	memcpy(pSize, &iOffset, sizeof(iOffset));
	return true;
}



/* 构建零结尾 bytes Content-Range 字段值。 */
XRT_API str xrtHttpContentRangeBuild(
	const xhttpcontentrange* pRange,
	size_t* pSize
)
{
	xhttpcontentrange Range;
	str sOutput;
	size_t iRequired;

	if ( !__xrtRangeValid(pRange, sizeof(Range)) ||
		((pSize != NULL) &&
		 !__xrtRangeValid(pSize, sizeof(iRequired))) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	memcpy(&Range, pRange, sizeof(Range));
	if ( (pSize != NULL) &&
		__xrtRangesOverlap(
			pSize, sizeof(iRequired), pRange, sizeof(Range)
		) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( !xrtHttpContentRangeWrite(
		&Range, NULL, 0, &iRequired
	) || (iRequired == SIZE_MAX) ) {
		return NULL;
	}
	sOutput = (str)xrtMalloc(iRequired + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !xrtHttpContentRangeWrite(
		&Range, sOutput, iRequired, &iRequired
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
