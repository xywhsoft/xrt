#include "../internal/xrt_asn1.h"

#include <stdio.h>



#if defined(XRT_FEATURE_ASN1_DER)

#define XRT_DER_MAX_DEPTH 64u



/* 设置带输入偏移的 ASN.1 结构化错误。 */
void __xrtAsn1Error(
	xerrkind Kind,
	xasn1error Code,
	cstr sOperation,
	cstr sMessage,
	size_t iOffset
)
{
	char Data[64];
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.asn1";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	if ( iOffset != SIZE_MAX ) {
		(void)snprintf(
			Data, sizeof(Data), "offset=%llu", (unsigned long long)iOffset
		);
		Desc.Data = Data;
	}
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 验证 OBJECT IDENTIFIER 或 RELATIVE-OID 的规范 base-128 子标识符。 */
static bool __xrtDerOidValid(const uint8* pData, size_t iSize)
{
	bool bFirst = true;
	bool bOpen = false;

	if ( iSize == 0 ) {
		return false;
	}
	for ( size_t i = 0; i < iSize; i++ ) {
		uint8 iByte = pData[i];

		if ( bFirst && (iByte == UINT8_C(0x80)) ) {
			return false;
		}
		bOpen = (iByte & UINT8_C(0x80)) != 0;
		bFirst = !bOpen;
	}
	return !bOpen;
}



/* 验证 DER 原始值中可由局部内容判定的规范约束。 */
static bool __xrtDerValueValid(
	const xdervalue* pValue,
	cstr sOperation,
	size_t iOffset
)
{
	const uint8* pData = pValue->Value.Data;
	size_t iSize = pValue->Value.Size;
	uint32 iTag;
	bool bPrimitive;

	if ( pValue->Tag.Class != XASN1_UNIVERSAL ) {
		return true;
	}
	iTag = pValue->Tag.Number;
	bPrimitive = !pValue->Tag.Constructed;
	if ( (iTag == 0u) || (iTag == 15u) ) {
		__xrtAsn1Error(
			XERR_PROTOCOL, XASN1_ERROR_TAG, sOperation,
			"reserved ASN.1 universal tag is not valid DER", iOffset
		);
		return false;
	}
	if ( ((iTag == (uint32)XASN1_SEQUENCE) ||
		 (iTag == (uint32)XASN1_SET)) && bPrimitive ) {
		__xrtAsn1Error(
			XERR_PROTOCOL, XASN1_ERROR_TAG, sOperation,
			"DER sequence and set values must be constructed", iOffset
		);
		return false;
	}
	if ( ((iTag >= 1u) && (iTag <= 6u)) ||
		(iTag == (uint32)XASN1_ENUMERATED) ||
		(iTag == (uint32)XASN1_UTF8_STRING) ||
		(iTag == (uint32)XASN1_RELATIVE_OID) ||
		((iTag >= (uint32)XASN1_NUMERIC_STRING) &&
		 (iTag <= (uint32)XASN1_BMP_STRING)) ) {
		if ( !bPrimitive ) {
			__xrtAsn1Error(
				XERR_PROTOCOL, XASN1_ERROR_TAG, sOperation,
				"DER primitive value uses a constructed encoding", iOffset
			);
			return false;
		}
	}

	if ( iTag == (uint32)XASN1_BOOLEAN ) {
		if ( (iSize != 1u) ||
			((pData[0] != 0) && (pData[0] != UINT8_C(0xFF))) ) {
			__xrtAsn1Error(
				XERR_PROTOCOL, XASN1_ERROR_VALUE, sOperation,
				"DER boolean must contain exactly 00 or FF", iOffset
			);
			return false;
		}
	} else if ( (iTag == (uint32)XASN1_INTEGER) ||
		(iTag == (uint32)XASN1_ENUMERATED) ) {
		if ( (iSize == 0) || ((iSize > 1u) &&
			(((pData[0] == 0) && ((pData[1] & UINT8_C(0x80)) == 0)) ||
			 ((pData[0] == UINT8_C(0xFF)) &&
			  ((pData[1] & UINT8_C(0x80)) != 0)))) ) {
			__xrtAsn1Error(
				XERR_PROTOCOL, XASN1_ERROR_VALUE, sOperation,
				"DER integer is empty or not minimally encoded", iOffset
			);
			return false;
		}
	} else if ( iTag == (uint32)XASN1_BIT_STRING ) {
		uint8 iUnused;

		if ( iSize == 0 ) {
			__xrtAsn1Error(
				XERR_PROTOCOL, XASN1_ERROR_VALUE, sOperation,
				"DER bit string is missing the unused-bit count", iOffset
			);
			return false;
		}
		iUnused = pData[0];
		if ( (iUnused > 7u) || ((iSize == 1u) && (iUnused != 0)) ||
			((iUnused != 0) &&
			 ((pData[iSize - 1u] & ((UINT8_C(1) << iUnused) - 1u)) != 0)) ) {
			__xrtAsn1Error(
				XERR_PROTOCOL, XASN1_ERROR_VALUE, sOperation,
				"DER bit string has invalid or nonzero unused bits", iOffset
			);
			return false;
		}
	} else if ( iTag == (uint32)XASN1_NULL ) {
		if ( iSize != 0 ) {
			__xrtAsn1Error(
				XERR_PROTOCOL, XASN1_ERROR_VALUE, sOperation,
				"DER null value must be empty", iOffset
			);
			return false;
		}
	} else if ( (iTag == (uint32)XASN1_OBJECT_IDENTIFIER) ||
		(iTag == (uint32)XASN1_RELATIVE_OID) ) {
		if ( !__xrtDerOidValid(pData, iSize) ) {
			__xrtAsn1Error(
				XERR_PROTOCOL, XASN1_ERROR_VALUE, sOperation,
				"DER object identifier is empty, truncated or nonminimal", iOffset
			);
			return false;
		}
	}
	return true;
}



/* 在指定绝对偏移解析一个规范 DER TLV。 */
static bool __xrtDerParse(
	const uint8* pData,
	size_t iSize,
	size_t iBaseOffset,
	xdervalue* pValue,
	size_t* pConsumed,
	cstr sOperation
)
{
	xdervalue Value;
	size_t iOffset = 0;
	size_t iLength;
	uint8 iFirst;
	uint32 iTag;

	if ( iSize == 0 ) {
		__xrtAsn1Error(
			XERR_PROTOCOL, XASN1_ERROR_END, sOperation,
			"DER value is truncated before its tag", iBaseOffset
		);
		return false;
	}
	memset(&Value, 0, sizeof(Value));
	iFirst = pData[iOffset++];
	Value.Tag.Class = (xasn1class)(iFirst >> 6u);
	Value.Tag.Constructed = (iFirst & UINT8_C(0x20)) != 0;
	iTag = iFirst & UINT8_C(0x1F);
	if ( iTag == UINT8_C(0x1F) ) {
		bool bFirst = true;

		iTag = 0;
		while ( true ) {
			uint8 iByte;

			if ( iOffset >= iSize ) {
				__xrtAsn1Error(
					XERR_PROTOCOL, XASN1_ERROR_TAG, sOperation,
					"high ASN.1 tag number is truncated", iBaseOffset + iOffset
				);
				return false;
			}
			iByte = pData[iOffset++];
			if ( bFirst && ((iByte & UINT8_C(0x7F)) == 0) ) {
				__xrtAsn1Error(
					XERR_PROTOCOL, XASN1_ERROR_TAG, sOperation,
					"high ASN.1 tag number is not minimally encoded",
					iBaseOffset + iOffset - 1u
				);
				return false;
			}
			bFirst = false;
			if ( (iTag > (UINT32_MAX >> 7u)) ||
				((iTag == (UINT32_MAX >> 7u)) &&
				 ((uint32)(iByte & UINT8_C(0x7F)) >
				  (UINT32_MAX & UINT32_C(0x7F)))) ) {
				__xrtAsn1Error(
					XERR_RANGE, XASN1_ERROR_TAG, sOperation,
					"ASN.1 tag number exceeds 32 bits", iBaseOffset + iOffset - 1u
				);
				return false;
			}
			iTag = (iTag << 7u) | (uint32)(iByte & UINT8_C(0x7F));
			if ( (iByte & UINT8_C(0x80)) == 0 ) {
				break;
			}
		}
		if ( iTag < 31u ) {
			__xrtAsn1Error(
				XERR_PROTOCOL, XASN1_ERROR_TAG, sOperation,
				"high ASN.1 tag form encodes a low tag number", iBaseOffset
			);
			return false;
		}
	}
	Value.Tag.Number = iTag;

	if ( iOffset >= iSize ) {
		__xrtAsn1Error(
			XERR_PROTOCOL, XASN1_ERROR_LENGTH, sOperation,
			"DER value is truncated before its length", iBaseOffset + iOffset
		);
		return false;
	}
	iFirst = pData[iOffset++];
	if ( iFirst < UINT8_C(0x80) ) {
		iLength = iFirst;
	} else {
		size_t iBytes = iFirst & UINT8_C(0x7F);

		if ( (iBytes == 0) || (iBytes > sizeof(size_t)) ||
			(iBytes > iSize - iOffset) ) {
			__xrtAsn1Error(
				XERR_PROTOCOL, XASN1_ERROR_LENGTH, sOperation,
				"DER length is indefinite, oversized or truncated",
				iBaseOffset + iOffset - 1u
			);
			return false;
		}
		if ( pData[iOffset] == 0 ) {
			__xrtAsn1Error(
				XERR_PROTOCOL, XASN1_ERROR_LENGTH, sOperation,
				"DER length contains a redundant leading zero",
				iBaseOffset + iOffset
			);
			return false;
		}
		iLength = 0;
		for ( size_t i = 0; i < iBytes; i++ ) {
			if ( iLength > (SIZE_MAX >> 8u) ) {
				__xrtAsn1Error(
					XERR_RANGE, XASN1_ERROR_LENGTH, sOperation,
					"DER content length overflows size_t", iBaseOffset + iOffset + i
				);
				return false;
			}
			iLength = (iLength << 8u) | pData[iOffset + i];
		}
		iOffset += iBytes;
		if ( iLength < 128u ) {
			__xrtAsn1Error(
				XERR_PROTOCOL, XASN1_ERROR_LENGTH, sOperation,
				"DER long-form length is not minimal", iBaseOffset + iOffset - iBytes - 1u
			);
			return false;
		}
	}
	if ( iLength > iSize - iOffset ) {
		__xrtAsn1Error(
			XERR_PROTOCOL, XASN1_ERROR_LENGTH, sOperation,
			"DER content extends beyond the input", iBaseOffset + iOffset
		);
		return false;
	}
	Value.Raw.Data = pData;
	Value.Raw.Size = iOffset + iLength;
	Value.Value.Data = pData + iOffset;
	Value.Value.Size = iLength;
	Value.HeaderSize = iOffset;
	if ( !__xrtDerValueValid(&Value, sOperation, iBaseOffset) ) {
		return false;
	}
	*pValue = Value;
	*pConsumed = Value.Raw.Size;
	return true;
}



/* 初始化借用输入的 DER 游标。 */
XRT_API bool xrtDerInit(xdercursor* pCursor, const void* pData, size_t iSize)
{
	xdercursor Cursor;

	if ( (pCursor == NULL) || ((pData == NULL) && (iSize != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	Cursor.Data = (const uint8*)pData;
	Cursor.Size = iSize;
	Cursor.Offset = 0;
	*pCursor = Cursor;
	return true;
}



/* 查看下一项但不推进游标。 */
XRT_API xderresult xrtDerPeek(const xdercursor* pCursor, xdervalue* pValue)
{
	xdervalue Value;
	size_t iConsumed;

	if ( (pCursor == NULL) || (pValue == NULL) ||
		((pCursor->Data == NULL) && (pCursor->Size != 0)) ||
		(pCursor->Offset > pCursor->Size) ) {
		__xrtErrorSetInvalidArgument();
		return XDER_ERROR;
	}
	if ( pCursor->Offset == pCursor->Size ) {
		return XDER_DONE;
	}
	if ( !__xrtDerParse(
		pCursor->Data + pCursor->Offset, pCursor->Size - pCursor->Offset,
		pCursor->Offset, &Value, &iConsumed, "der-peek"
	) ) {
		return XDER_ERROR;
	}
	(void)iConsumed;
	*pValue = Value;
	return XDER_VALUE;
}



/* 读取下一项并只在成功后推进游标。 */
XRT_API xderresult xrtDerRead(xdercursor* pCursor, xdervalue* pValue)
{
	xdervalue Value;
	size_t iConsumed;

	if ( (pCursor == NULL) || (pValue == NULL) ||
		((pCursor->Data == NULL) && (pCursor->Size != 0)) ||
		(pCursor->Offset > pCursor->Size) ) {
		__xrtErrorSetInvalidArgument();
		return XDER_ERROR;
	}
	if ( pCursor->Offset == pCursor->Size ) {
		return XDER_DONE;
	}
	if ( !__xrtDerParse(
		pCursor->Data + pCursor->Offset, pCursor->Size - pCursor->Offset,
		pCursor->Offset, &Value, &iConsumed, "der-read"
	) ) {
		return XDER_ERROR;
	}
	pCursor->Offset += iConsumed;
	*pValue = Value;
	return XDER_VALUE;
}



/* 读取并检查指定标签，失败时保持游标不变。 */
XRT_API bool xrtDerExpect(
	xdercursor* pCursor,
	xasn1class Class,
	uint32 iNumber,
	bool bConstructed,
	xdervalue* pValue
)
{
	xdervalue Value;
	xderresult Result;

	if ( (pCursor == NULL) || (pValue == NULL) ||
		(Class < XASN1_UNIVERSAL) || (Class > XASN1_PRIVATE) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	Result = xrtDerPeek(pCursor, &Value);
	if ( Result == XDER_DONE ) {
		__xrtAsn1Error(
			XERR_PROTOCOL, XASN1_ERROR_END, "der-expect",
			"required DER value is missing", pCursor->Offset
		);
		return false;
	}
	if ( Result == XDER_ERROR ) {
		return false;
	}
	if ( !xrtDerIs(&Value, Class, iNumber, bConstructed) ) {
		__xrtAsn1Error(
			XERR_TYPE, XASN1_ERROR_TYPE, "der-expect",
			"DER value has an unexpected tag", pCursor->Offset
		);
		return false;
	}
	pCursor->Offset += Value.Raw.Size;
	*pValue = Value;
	return true;
}



/* 从构造值的内容初始化子游标。 */
XRT_API bool xrtDerEnter(const xdervalue* pValue, xdercursor* pCursor)
{
	if ( (pValue == NULL) || (pCursor == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !pValue->Tag.Constructed ) {
		__xrtAsn1Error(
			XERR_TYPE, XASN1_ERROR_TYPE, "der-enter",
			"cannot enter a primitive DER value", SIZE_MAX
		);
		return false;
	}
	return xrtDerInit(pCursor, pValue->Value.Data, pValue->Value.Size);
}



/* 返回游标是否已经完整消费。 */
XRT_API bool xrtDerDone(const xdercursor* pCursor)
{
	if ( (pCursor == NULL) ||
		((pCursor->Data == NULL) && (pCursor->Size != 0)) ||
		(pCursor->Offset > pCursor->Size) ) {
		return false;
	}
	return pCursor->Offset == pCursor->Size;
}



/* 返回游标剩余字节数。 */
XRT_API size_t xrtDerRemaining(const xdercursor* pCursor)
{
	if ( (pCursor == NULL) ||
		((pCursor->Data == NULL) && (pCursor->Size != 0)) ||
		(pCursor->Offset > pCursor->Size) ) {
		__xrtErrorSetInvalidArgument();
		return 0;
	}
	return pCursor->Size - pCursor->Offset;
}



/* 仅比较 DER 标签。 */
XRT_API bool xrtDerIs(
	const xdervalue* pValue,
	xasn1class Class,
	uint32 iNumber,
	bool bConstructed
)
{
	return (pValue != NULL) && (pValue->Tag.Class == Class) &&
		(pValue->Tag.Number == iNumber) &&
		(pValue->Tag.Constructed == bConstructed);
}



/* 按 DER 字节字典序比较两个完整编码。 */
static int __xrtDerCompare(const xbytesview* pLeft, const xbytesview* pRight)
{
	size_t iCommon = pLeft->Size < pRight->Size ? pLeft->Size : pRight->Size;
	int iResult = memcmp(pLeft->Data, pRight->Data, iCommon);

	if ( iResult != 0 ) {
		return iResult;
	}
	if ( pLeft->Size < pRight->Size ) {
		return -1;
	}
	return pLeft->Size > pRight->Size ? 1 : 0;
}



/* 递归验证一个游标范围中的全部值和 DER SET 排序。 */
static bool __xrtDerValidateCursor(
	const uint8* pData,
	size_t iSize,
	size_t iBaseOffset,
	size_t iDepth,
	bool bSet,
	bool bSingle
)
{
	size_t iOffset = 0;
	size_t iCount = 0;
	xbytesview Previous = { NULL, 0 };

	while ( iOffset < iSize ) {
		xdervalue Value;
		size_t iConsumed;

		if ( !__xrtDerParse(
			pData + iOffset, iSize - iOffset, iBaseOffset + iOffset,
			&Value, &iConsumed, "der-validate"
		) ) {
			return false;
		}
		if ( bSet && (iCount != 0) &&
			(__xrtDerCompare(&Previous, &Value.Raw) > 0) ) {
			__xrtAsn1Error(
				XERR_PROTOCOL, XASN1_ERROR_ORDER, "der-validate",
				"DER set elements are not in canonical order",
				iBaseOffset + iOffset
			);
			return false;
		}
		if ( Value.Tag.Constructed ) {
			if ( iDepth >= XRT_DER_MAX_DEPTH ) {
				__xrtAsn1Error(
					XERR_RANGE, XASN1_ERROR_DEPTH, "der-validate",
					"DER nesting exceeds 64 levels", iBaseOffset + iOffset
				);
				return false;
			}
			if ( !__xrtDerValidateCursor(
				Value.Value.Data, Value.Value.Size,
				iBaseOffset + iOffset + Value.HeaderSize, iDepth + 1u,
				(Value.Tag.Class == XASN1_UNIVERSAL) &&
				(Value.Tag.Number == (uint32)XASN1_SET), false
			) ) {
				return false;
			}
		}
		Previous = Value.Raw;
		iOffset += iConsumed;
		iCount++;
	}
	if ( bSingle && (iCount != 1u) ) {
		__xrtAsn1Error(
			XERR_PROTOCOL, XASN1_ERROR_TRAILING, "der-validate",
			"input must contain exactly one DER value", iBaseOffset + iOffset
		);
		return false;
	}
	return true;
}



/* 验证一个完整 DER 树。 */
XRT_API bool xrtDerValidate(const void* pData, size_t iSize)
{
	if ( (pData == NULL) || (iSize == 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtDerValidateCursor(
		(const uint8*)pData, iSize, 0, 0, false, true
	);
}



/* 检查一个值是否是指定的 primitive Universal 类型。 */
static bool __xrtDerRequirePrimitive(
	const xdervalue* pValue,
	uint32 iTag,
	cstr sOperation
)
{
	if ( pValue == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtDerIs(pValue, XASN1_UNIVERSAL, iTag, false) ) {
		__xrtAsn1Error(
			XERR_TYPE, XASN1_ERROR_TYPE, sOperation,
			"DER value has the wrong universal type", SIZE_MAX
		);
		return false;
	}
	return __xrtDerValueValid(pValue, sOperation, SIZE_MAX);
}



/* 读取规范 BOOLEAN。 */
XRT_API bool xrtDerBoolean(const xdervalue* pValue, bool* pResult)
{
	bool bResult;

	if ( pResult == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtDerRequirePrimitive(
		pValue, (uint32)XASN1_BOOLEAN, "der-boolean"
	) ) {
		return false;
	}
	bResult = pValue->Value.Data[0] != 0;
	*pResult = bResult;
	return true;
}



/* 读取非负且最短编码的 INTEGER 字节。 */
XRT_API bool xrtDerUnsigned(const xdervalue* pValue, xbytesview* pResult)
{
	xbytesview Result;

	if ( pResult == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtDerRequirePrimitive(
		pValue, (uint32)XASN1_INTEGER, "der-unsigned"
	) ) {
		return false;
	}
	if ( (pValue->Value.Data[0] & UINT8_C(0x80)) != 0 ) {
		__xrtAsn1Error(
			XERR_VALUE, XASN1_ERROR_VALUE, "der-unsigned",
			"DER integer is negative", SIZE_MAX
		);
		return false;
	}
	Result = pValue->Value;
	if ( (Result.Size > 1u) && (Result.Data[0] == 0) ) {
		Result.Data++;
		Result.Size--;
	}
	*pResult = Result;
	return true;
}



/* 读取 64 位非负 INTEGER。 */
XRT_API bool xrtDerUInt64(const xdervalue* pValue, uint64* pResult)
{
	xbytesview Bytes;
	uint64 iResult = 0;

	if ( pResult == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtDerUnsigned(pValue, &Bytes) ) {
		return false;
	}
	if ( Bytes.Size > 8u ) {
		__xrtAsn1Error(
			XERR_RANGE, XASN1_ERROR_RANGE, "der-uint64",
			"DER integer does not fit in uint64", SIZE_MAX
		);
		return false;
	}
	for ( size_t i = 0; i < Bytes.Size; i++ ) {
		iResult = (iResult << 8u) | Bytes.Data[i];
	}
	*pResult = iResult;
	return true;
}



/* 读取不超过 64 位的有符号 DER INTEGER。 */
XRT_API bool xrtDerInt64(const xdervalue* pValue, int64* pResult)
{
	uint64 iBits = 0;
	uint64 iMagnitude;
	bool bNegative;

	if ( pResult == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtDerRequirePrimitive(
		pValue, (uint32)XASN1_INTEGER, "der-int64"
	) ) {
		return false;
	}
	if ( pValue->Value.Size > 8u ) {
		__xrtAsn1Error(
			XERR_RANGE, XASN1_ERROR_RANGE, "der-int64",
			"DER integer does not fit in int64", SIZE_MAX
		);
		return false;
	}
	bNegative = (pValue->Value.Data[0] & UINT8_C(0x80)) != 0;
	for ( size_t i = 0; i < pValue->Value.Size; i++ ) {
		iBits = (iBits << 8u) | pValue->Value.Data[i];
	}
	if ( !bNegative ) {
		*pResult = (int64)iBits;
		return true;
	}

	/* 从补码恢复绝对值，单独处理 INT64_MIN以避免求负溢出。 */
	if ( pValue->Value.Size < 8u ) {
		iBits |= UINT64_MAX << (pValue->Value.Size * 8u);
	}
	iMagnitude = (~iBits) + 1u;
	if ( iMagnitude == (UINT64_C(1) << 63u) ) {
		*pResult = INT64_MIN;
	} else {
		*pResult = -(int64)iMagnitude;
	}
	return true;
}



/* 读取 BIT STRING 数据和未使用位数。 */
XRT_API bool xrtDerBitString(
	const xdervalue* pValue,
	xbytesview* pResult,
	uint8* pUnusedBits
)
{
	xbytesview Result;
	uint8 iUnused;

	if ( (pResult == NULL) || (pUnusedBits == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtDerRequirePrimitive(
		pValue, (uint32)XASN1_BIT_STRING, "der-bit-string"
	) ) {
		return false;
	}
	iUnused = pValue->Value.Data[0];
	Result.Data = pValue->Value.Data + 1u;
	Result.Size = pValue->Value.Size - 1u;
	*pResult = Result;
	*pUnusedBits = iUnused;
	return true;
}



/* 读取 OCTET STRING 借用内容。 */
XRT_API bool xrtDerOctets(const xdervalue* pValue, xbytesview* pResult)
{
	if ( pResult == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtDerRequirePrimitive(
		pValue, (uint32)XASN1_OCTET_STRING, "der-octets"
	) ) {
		return false;
	}
	*pResult = pValue->Value;
	return true;
}



/* 读取 OBJECT IDENTIFIER 借用内容。 */
XRT_API bool xrtDerOid(const xdervalue* pValue, xbytesview* pResult)
{
	if ( pResult == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtDerRequirePrimitive(
		pValue, (uint32)XASN1_OBJECT_IDENTIFIER, "der-oid"
	) ) {
		return false;
	}
	*pResult = pValue->Value;
	return true;
}



/* 比较 OBJECT IDENTIFIER 内容。 */
XRT_API bool xrtDerOidEqual(
	const xdervalue* pValue,
	const void* pOid,
	size_t iOidSize
)
{
	return (pValue != NULL) &&
		xrtDerIs(
			pValue, XASN1_UNIVERSAL,
			(uint32)XASN1_OBJECT_IDENTIFIER, false
		) && ((pOid != NULL) || (iOidSize == 0)) &&
		(pValue->Value.Size == iOidSize) &&
		((iOidSize == 0) ||
		 (memcmp(pValue->Value.Data, pOid, iOidSize) == 0));
}



/* 追加一个不依赖 PKCS 语义的 DER 标签字节。 */
static bool __xrtDerAppendTag(
	xbuffer* pOutput,
	xasn1class Class,
	uint32 iNumber,
	bool bConstructed
)
{
	uint8 Data[6];
	size_t iSize = 0;

	if ( (Class < XASN1_UNIVERSAL) || (Class > XASN1_PRIVATE) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	Data[iSize++] = (uint8)((uint32)Class << 6u) |
		(bConstructed ? UINT8_C(0x20) : 0u);
	if ( iNumber < 31u ) {
		Data[0] |= (uint8)iNumber;
		return xrtBufferAppend(pOutput, (xbytesview){ Data, iSize });
	}
	Data[0] |= UINT8_C(0x1F);
	{
		uint8 Reverse[5];
		size_t iReverse = 0;

		do {
			Reverse[iReverse++] = (uint8)(iNumber & UINT32_C(0x7F));
			iNumber >>= 7u;
		} while ( iNumber != 0 );
		while ( iReverse != 0 ) {
			uint8 iByte = Reverse[--iReverse];

			if ( iReverse != 0 ) {
				iByte |= UINT8_C(0x80);
			}
			Data[iSize++] = iByte;
		}
	}
	return xrtBufferAppend(pOutput, (xbytesview){ Data, iSize });
}



/* 追加规范 DER definite length。 */
static bool __xrtDerAppendLength(xbuffer* pOutput, size_t iLength)
{
	uint8 Data[sizeof(size_t) + 1u];
	size_t iSize = 0;

	if ( iLength < 128u ) {
		Data[0] = (uint8)iLength;
		return xrtBufferAppend(pOutput, (xbytesview){ Data, 1u });
	}
	while ( iLength != 0 ) {
		Data[sizeof(Data) - ++iSize] = (uint8)(iLength & UINT8_C(0xFF));
		iLength >>= 8u;
	}
	iSize++;
	Data[sizeof(Data) - iSize] = UINT8_C(0x80) | (uint8)(iSize - 1u);
	return xrtBufferAppend(
		pOutput,
		(xbytesview){ Data + sizeof(Data) - iSize, iSize }
	);
}



/* 追加 TLV，在扩容成功前不修改对外可见长度。 */
XRT_API bool xrtDerAppend(
	xbuffer* pOutput,
	xasn1class Class,
	uint32 iNumber,
	bool bConstructed,
	xbytesview Content
)
{
	xbuffer Encoded;
	size_t iOriginalSize;

	if ( (pOutput == NULL) || ((Content.Data == NULL) && (Content.Size != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtBufferInit(&Encoded) ) {
		return false;
	}
	if ( !__xrtDerAppendTag(&Encoded, Class, iNumber, bConstructed) ||
		 !__xrtDerAppendLength(&Encoded, Content.Size) ||
		 !xrtBufferAppend(&Encoded, Content) ) {
		xrtBufferUnit(&Encoded);
		return false;
	}
	/* 不把“可写入”误当成“已是 DER”；构造值也必须递归规范。 */
	if ( !xrtDerValidate(Encoded.Data, Encoded.Size) ) {
		xrtBufferUnit(&Encoded);
		return false;
	}
	iOriginalSize = pOutput->Size;
	if ( !xrtBufferAppend(pOutput, xrtBufferView(&Encoded)) ) {
		pOutput->Size = iOriginalSize;
		xrtBufferUnit(&Encoded);
		return false;
	}
	xrtBufferUnit(&Encoded);
	return true;
}



/* 追加规范 DER BOOLEAN。 */
XRT_API bool xrtDerAppendBoolean(xbuffer* pOutput, bool bValue)
{
	uint8 iValue = bValue ? UINT8_C(0xFF) : 0;

	return xrtDerAppend(
		pOutput, XASN1_UNIVERSAL, (uint32)XASN1_BOOLEAN, false,
		(xbytesview){ &iValue, 1u }
	);
}



/* 追加规范非负 DER INTEGER。 */
XRT_API bool xrtDerAppendUInt64(xbuffer* pOutput, uint64 iValue)
{
	uint8 Data[9];
	size_t iOffset = sizeof(Data);

	do {
		Data[--iOffset] = (uint8)(iValue & UINT8_C(0xFF));
		iValue >>= 8u;
	} while ( iValue != 0 );
	if ( (Data[iOffset] & UINT8_C(0x80)) != 0 ) {
		Data[--iOffset] = 0;
	}
	return xrtDerAppend(
		pOutput, XASN1_UNIVERSAL, (uint32)XASN1_INTEGER, false,
		(xbytesview){ Data + iOffset, sizeof(Data) - iOffset }
	);
}



/* 追加规范有符号 DER INTEGER。 */
XRT_API bool xrtDerAppendInt64(xbuffer* pOutput, int64 iValue)
{
	uint8 Data[8];
	uint64 iBits = (uint64)iValue;
	size_t iOffset = sizeof(Data);

	do {
		Data[--iOffset] = (uint8)(iBits & UINT8_C(0xFF));
		iBits >>= 8u;
	} while ( iOffset != 0 );
	while ( (iOffset < sizeof(Data) - 1u) &&
		((Data[iOffset] == 0 && (Data[iOffset + 1u] & UINT8_C(0x80)) == 0) ||
		 (Data[iOffset] == UINT8_C(0xFF) &&
		  (Data[iOffset + 1u] & UINT8_C(0x80)) != 0)) ) {
		iOffset++;
	}
	return xrtDerAppend(
		pOutput, XASN1_UNIVERSAL, (uint32)XASN1_INTEGER, false,
		(xbytesview){ Data + iOffset, sizeof(Data) - iOffset }
	);
}



/* 追加常用的 DER 原始类型。 */
XRT_API bool xrtDerAppendOctets(xbuffer* pOutput, xbytesview Content)
{
	return xrtDerAppend(
		pOutput, XASN1_UNIVERSAL, (uint32)XASN1_OCTET_STRING, false, Content
	);
}



XRT_API bool xrtDerAppendNull(xbuffer* pOutput)
{
	return xrtDerAppend(
		pOutput, XASN1_UNIVERSAL, (uint32)XASN1_NULL, false,
		(xbytesview){ NULL, 0 }
	);
}



XRT_API bool xrtDerAppendBitString(
	xbuffer* pOutput,
	xbytesview Content,
	uint8 iUnusedBits
)
{
	xbuffer Value;
	bool bResult;

	if ( (iUnusedBits > 7u) || ((Content.Size == 0) && (iUnusedBits != 0)) ||
		 ((Content.Data == NULL) && (Content.Size != 0)) ||
		 ((iUnusedBits != 0) &&
		  ((Content.Data[Content.Size - 1u] & ((UINT8_C(1) << iUnusedBits) - 1u)) != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtBufferInit(&Value) || !xrtBufferAppendByte(&Value, iUnusedBits) ||
		 !xrtBufferAppend(&Value, Content) ) {
		xrtBufferUnit(&Value);
		return false;
	}
	bResult = xrtDerAppend(
		pOutput, XASN1_UNIVERSAL, (uint32)XASN1_BIT_STRING, false,
		xrtBufferView(&Value)
	);
	xrtBufferUnit(&Value);
	return bResult;
}



/* 解析一个不允许前导零的 OID 组件。 */
static bool __xrtDerOidPart(xstrview Text, size_t* pOffset, uint64* pValue)
{
	uint64 iValue = 0;
	size_t iOffset;

	if ( (pOffset == NULL) || (pValue == NULL) || (*pOffset >= Text.Size) ||
		 (Text.Data[*pOffset] < '0') || (Text.Data[*pOffset] > '9') ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	iOffset = *pOffset;
	if ( (Text.Data[iOffset] == '0') && (iOffset + 1u < Text.Size) &&
		 (Text.Data[iOffset + 1u] != '.') ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	while ( (iOffset < Text.Size) && (Text.Data[iOffset] >= '0') &&
		(Text.Data[iOffset] <= '9') ) {
		uint64 iDigit = (uint64)(Text.Data[iOffset] - '0');

		if ( iValue > (UINT64_MAX - iDigit) / 10u ) {
			__xrtErrorSetRange();
			return false;
		}
		iValue = iValue * 10u + iDigit;
		iOffset++;
	}
	*pOffset = iOffset;
	*pValue = iValue;
	return true;
}



/* 把无符号组件追加为 base-128 编码。 */
static bool __xrtDerOidAppendPart(xbuffer* pOutput, uint64 iValue)
{
	uint8 Data[10];
	size_t iSize = 0;

	do {
		Data[iSize++] = (uint8)(iValue & UINT8_C(0x7F));
		iValue >>= 7u;
	} while ( iValue != 0 );
	while ( iSize != 0 ) {
		uint8 iByte = Data[--iSize];

		if ( iSize != 0 ) {
			iByte |= UINT8_C(0x80);
		}
		if ( !xrtBufferAppendByte(pOutput, iByte) ) {
			return false;
		}
	}
	return true;
}



/* 把点分 OID 文本追加为 DER OBJECT IDENTIFIER 内容。 */
XRT_API bool xrtDerOidEncode(xstrview Text, xbuffer* pOutput)
{
	xbuffer Encoded;
	uint64 iFirst;
	uint64 iSecond;
	size_t iOffset = 0;
	size_t iOriginalSize;

	if ( (pOutput == NULL) || (Text.Data == NULL) || (Text.Size == 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtBufferInit(&Encoded) || !__xrtDerOidPart(Text, &iOffset, &iFirst) ||
		 (iOffset >= Text.Size) || (Text.Data[iOffset++] != '.') ||
		 !__xrtDerOidPart(Text, &iOffset, &iSecond) || (iFirst > 2u) ||
		 ((iFirst < 2u) && (iSecond > 39u)) ||
		 ((iFirst == 2u) && (iSecond > UINT64_MAX - 80u)) ) {
		xrtBufferUnit(&Encoded);
		return false;
	}
	if ( !__xrtDerOidAppendPart(&Encoded, iFirst * 40u + iSecond) ) {
		xrtBufferUnit(&Encoded);
		return false;
	}
	while ( iOffset < Text.Size ) {
		uint64 iPart;

		if ( (Text.Data[iOffset++] != '.') ||
			 !__xrtDerOidPart(Text, &iOffset, &iPart) ||
			 !__xrtDerOidAppendPart(&Encoded, iPart) ) {
			xrtBufferUnit(&Encoded);
			return false;
		}
	}
	iOriginalSize = pOutput->Size;
	if ( !xrtBufferAppend(pOutput, xrtBufferView(&Encoded)) ) {
		pOutput->Size = iOriginalSize;
		xrtBufferUnit(&Encoded);
		return false;
	}
	xrtBufferUnit(&Encoded);
	return true;
}



/* 把一个 base-128 OID 组件读为 uint64。 */
static bool __xrtDerOidReadPart(
	xbytesview Oid,
	size_t* pOffset,
	uint64* pValue
)
{
	uint64 iValue = 0;
	bool bFirst = true;

	if ( (pOffset == NULL) || (pValue == NULL) || (*pOffset >= Oid.Size) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	while ( true ) {
		uint8 iByte;

		if ( *pOffset >= Oid.Size ) {
			__xrtErrorSetInvalidArgument();
			return false;
		}
		iByte = Oid.Data[(*pOffset)++];
		if ( bFirst && (iByte == UINT8_C(0x80)) ) {
			__xrtErrorSetInvalidArgument();
			return false;
		}
		if ( (iValue > (UINT64_MAX >> 7u)) ||
			((iValue == (UINT64_MAX >> 7u)) &&
			 ((uint64)(iByte & UINT8_C(0x7F)) >
			  (UINT64_MAX & UINT64_C(0x7F)))) ) {
			__xrtErrorSetRange();
			return false;
		}
		iValue = (iValue << 7u) | (uint64)(iByte & UINT8_C(0x7F));
		bFirst = false;
		if ( (iByte & UINT8_C(0x80)) == 0 ) {
			break;
		}
	}
	*pValue = iValue;
	return true;
}



/* 把 DER OBJECT IDENTIFIER 内容追加为点分文本。 */
XRT_API bool xrtDerOidDecode(xbytesview Oid, xbuffer* pOutput)
{
	xbuffer Text;
	uint64 iCombined;
	uint64 iFirst;
	uint64 iSecond;
	size_t iOffset = 0;
	size_t iOriginalSize;
	char Number[32];

	if ( (pOutput == NULL) || (Oid.Data == NULL) || (Oid.Size == 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtBufferInit(&Text) ||
		 !__xrtDerOidReadPart(Oid, &iOffset, &iCombined) ) {
		xrtBufferUnit(&Text);
		return false;
	}
	if ( iCombined < 40u ) {
		iFirst = 0;
		iSecond = iCombined;
	} else if ( iCombined < 80u ) {
		iFirst = 1;
		iSecond = iCombined - 40u;
	} else {
		iFirst = 2;
		iSecond = iCombined - 80u;
	}
	for ( size_t i = 0; i < 2u; i++ ) {
		uint64 iPart = i == 0 ? iFirst : iSecond;
		int iSize = snprintf(Number, sizeof(Number), "%llu",
			(unsigned long long)iPart);

		if ( (iSize <= 0) || ((size_t)iSize >= sizeof(Number)) ||
			((i != 0) && !xrtBufferAppendByte(&Text, '.')) ||
			!xrtBufferAppend(&Text, (xbytesview){ (const uint8*)Number, (size_t)iSize }) ) {
			xrtBufferUnit(&Text);
			return false;
		}
	}
	while ( iOffset < Oid.Size ) {
		uint64 iPart;
		int iSize;

		if ( !__xrtDerOidReadPart(Oid, &iOffset, &iPart) ) {
			xrtBufferUnit(&Text);
			return false;
		}
		iSize = snprintf(Number, sizeof(Number), ".%llu",
			(unsigned long long)iPart);
		if ( (iSize <= 0) || ((size_t)iSize >= sizeof(Number)) ||
			!xrtBufferAppend(&Text, (xbytesview){ (const uint8*)Number, (size_t)iSize }) ) {
			xrtBufferUnit(&Text);
			return false;
		}
	}
	iOriginalSize = pOutput->Size;
	if ( !xrtBufferAppend(pOutput, xrtBufferView(&Text)) ) {
		pOutput->Size = iOriginalSize;
		xrtBufferUnit(&Text);
		return false;
	}
	xrtBufferUnit(&Text);
	return true;
}



/* 把点分 OID 直接追加为规范 DER TLV。 */
XRT_API bool xrtDerAppendOid(xbuffer* pOutput, xstrview Text)
{
	xbuffer Oid;
	bool bResult;

	if ( pOutput == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtBufferInit(&Oid) || !xrtDerOidEncode(Text, &Oid) ) {
		xrtBufferUnit(&Oid);
		return false;
	}
	bResult = xrtDerAppend(
		pOutput, XASN1_UNIVERSAL, (uint32)XASN1_OBJECT_IDENTIFIER, false,
		xrtBufferView(&Oid)
	);
	xrtBufferUnit(&Oid);
	return bResult;
}

#endif
