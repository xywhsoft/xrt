#include "../internal/xrt_http.h"

#include <xrt/http_structured.h>



#if defined(XHTTP_FEATURE_HTTP_STRUCTURED_WRITE)

#define XRT_HTTP_STRUCTURED_INTEGER_MAX INT64_C(999999999999999)



/* 三次遍历分别负责测量、重叠校验和最终写出。 */
typedef struct xrt_http_structured_writer {
	char* Output;
	size_t Position;
	const void* Target;
	size_t TargetSize;
	const void* Protected;
	size_t ProtectedSize;
} xrt_http_structured_writer;



/* 公共写出入口类型。 */
typedef enum xrt_http_structured_root {
	XRT_HTTP_STRUCTURED_ROOT_BARE = 1,
	XRT_HTTP_STRUCTURED_ROOT_ITEM,
	XRT_HTTP_STRUCTURED_ROOT_LIST,
	XRT_HTTP_STRUCTURED_ROOT_DICTIONARY
} xrt_http_structured_root;



/* 验证借用范围不会被长度输出或最终目标覆盖。 */
static bool __xrtHttpStructuredWriteBorrow(
	const xrt_http_structured_writer* pWriter,
	const void* pMemory,
	size_t iSize
)
{
	if ( !__xrtRangeValid(pMemory, iSize) ||
		__xrtRangesOverlap(
			pMemory, iSize,
			pWriter->Protected, pWriter->ProtectedSize
		) || __xrtRangesOverlap(
			pMemory, iSize,
			pWriter->Target, pWriter->TargetSize
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return true;
}



/* 验证借用文本并应用相同的重叠约束。 */
static bool __xrtHttpStructuredWriteView(
	const xrt_http_structured_writer* pWriter,
	xstrview Text
)
{
	if ( !__xrtHttpViewValid(Text) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtHttpStructuredWriteBorrow(
		pWriter, Text.Data, Text.Size
	);
}



/* 安全计算描述符数组范围。 */
static bool __xrtHttpStructuredWriteArray(
	const xrt_http_structured_writer* pWriter,
	const void* pArray,
	size_t iCount,
	size_t iElementSize,
	size_t* pBytes
)
{
	if ( iCount > (SIZE_MAX / iElementSize) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	*pBytes = iCount * iElementSize;
	return __xrtHttpStructuredWriteBorrow(
		pWriter, pArray, *pBytes
	);
}



/* 追加字节并在测量阶段只推进长度。 */
static bool __xrtHttpStructuredWriteBytes(
	xrt_http_structured_writer* pWriter,
	const void* pData,
	size_t iSize
)
{
	if ( iSize > (SIZE_MAX - pWriter->Position) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	if ( pWriter->Output != NULL ) {
		if ( (pData == NULL) && (iSize != 0) ) {
			__xrtErrorSetInvalidArgument();
			return false;
		}
		if ( iSize != 0 ) {
			memcpy(pWriter->Output + pWriter->Position, pData, iSize);
		}
	}
	pWriter->Position += iSize;
	return true;
}



/* 追加一个 ASCII 字节。 */
static bool __xrtHttpStructuredWriteByte(
	xrt_http_structured_writer* pWriter,
	char iByte
)
{
	return __xrtHttpStructuredWriteBytes(pWriter, &iByte, 1u);
}



/* 写出带可选负号的规范十进制整数。 */
static bool __xrtHttpStructuredWriteInteger(
	xrt_http_structured_writer* pWriter,
	int64 iValue
)
{
	char arrDigits[20];
	uint64 iMagnitude;
	size_t iSize;

	if ( (iValue < -XRT_HTTP_STRUCTURED_INTEGER_MAX) ||
		(iValue > XRT_HTTP_STRUCTURED_INTEGER_MAX) ) {
		return false;
	}
	iMagnitude = iValue < 0 ? (uint64)(-iValue) : (uint64)iValue;
	if ( (iValue < 0) &&
		!__xrtHttpStructuredWriteByte(pWriter, '-') ) {
		return false;
	}
	iSize = __xrtHttpUInt64Write(arrDigits, iMagnitude);
	return __xrtHttpStructuredWriteBytes(
		pWriter, arrDigits, iSize
	);
}



/* 写出以千分之一保存的规范 Decimal。 */
static bool __xrtHttpStructuredWriteDecimal(
	xrt_http_structured_writer* pWriter,
	int64 iValue
)
{
	char arrDigits[20];
	char arrFraction[3];
	uint64 iMagnitude;
	uint64 iInteger;
	uint32 iFraction;
	size_t iIntegerSize;
	size_t iFractionSize = 3u;

	if ( (iValue < -XRT_HTTP_STRUCTURED_INTEGER_MAX) ||
		(iValue > XRT_HTTP_STRUCTURED_INTEGER_MAX) ) {
		return false;
	}
	iMagnitude = iValue < 0 ? (uint64)(-iValue) : (uint64)iValue;
	iInteger = iMagnitude / 1000u;
	iFraction = (uint32)(iMagnitude % 1000u);
	if ( (iValue < 0) &&
		!__xrtHttpStructuredWriteByte(pWriter, '-') ) {
		return false;
	}
	iIntegerSize = __xrtHttpUInt64Write(arrDigits, iInteger);
	arrFraction[0] = (char)('0' + (iFraction / 100u));
	arrFraction[1] = (char)('0' + ((iFraction / 10u) % 10u));
	arrFraction[2] = (char)('0' + (iFraction % 10u));
	while ( (iFractionSize > 1u) &&
		(arrFraction[iFractionSize - 1u] == '0') ) {
		iFractionSize--;
	}
	return __xrtHttpStructuredWriteBytes(
		pWriter, arrDigits, iIntegerSize
	) && __xrtHttpStructuredWriteByte(
		pWriter, '.'
	) && __xrtHttpStructuredWriteBytes(
		pWriter, arrFraction, iFractionSize
	);
}



/* 写出并转义 ASCII String。 */
static bool __xrtHttpStructuredWriteString(
	xrt_http_structured_writer* pWriter,
	xstrview Text
)
{
	size_t i;

	if ( !__xrtHttpStructuredWriteView(pWriter, Text) ||
		!__xrtHttpStructuredWriteByte(pWriter, '"') ) {
		return false;
	}
	for ( i = 0; i < Text.Size; i++ ) {
		unsigned char iByte = (unsigned char)Text.Data[i];

		if ( (iByte < 0x20u) || (iByte > 0x7Eu) ) {
			return false;
		}
		if ( ((iByte == (unsigned char)'"') ||
			(iByte == (unsigned char)'\\')) &&
			!__xrtHttpStructuredWriteByte(pWriter, '\\') ) {
			return false;
		}
		if ( !__xrtHttpStructuredWriteByte(
			pWriter, (char)iByte
		) ) {
			return false;
		}
	}
	return __xrtHttpStructuredWriteByte(pWriter, '"');
}



/* 写出已经验证的 token。 */
static bool __xrtHttpStructuredWriteToken(
	xrt_http_structured_writer* pWriter,
	xstrview Token
)
{
	if ( !__xrtHttpStructuredWriteView(pWriter, Token) ||
		!xrtHttpStructuredTokenValid(Token) ) {
		return false;
	}
	return __xrtHttpStructuredWriteBytes(
		pWriter, Token.Data, Token.Size
	);
}



/* 写出带规范填充的 Base64 Byte Sequence。 */
static bool __xrtHttpStructuredWriteByteSequence(
	xrt_http_structured_writer* pWriter,
	xstrview Data
)
{
	size_t iEncoded;
	size_t iWritten;

	if ( !__xrtHttpStructuredWriteView(pWriter, Data) ||
		!xrtBase64Encode(
			Data.Data, Data.Size, NULL, 0, &iEncoded, NULL
		) || !__xrtHttpStructuredWriteByte(pWriter, ':') ) {
		return false;
	}
	if ( pWriter->Output == NULL ) {
		if ( !__xrtHttpStructuredWriteBytes(
			pWriter, NULL, iEncoded
		) ) {
			return false;
		}
	} else {
		if ( !xrtBase64Encode(
			Data.Data, Data.Size,
			pWriter->Output + pWriter->Position,
			iEncoded + 1u, &iWritten, NULL
		) || (iWritten != iEncoded) ) {
			return false;
		}
		pWriter->Position += iEncoded;
	}
	return __xrtHttpStructuredWriteByte(pWriter, ':');
}



/* 返回小写十六进制字符。 */
static char __xrtHttpStructuredWriteHex(uint8 iValue)
{
	return iValue < 10u ?
		(char)((uint8)'0' + iValue) :
		(char)((uint8)'a' + iValue - UINT8_C(10));
}



/* 写出 UTF-8 Display String，并百分号转义非可直接显示字节。 */
static bool __xrtHttpStructuredWriteDisplay(
	xrt_http_structured_writer* pWriter,
	xstrview Text
)
{
	size_t i;

	if ( !__xrtHttpStructuredWriteView(pWriter, Text) ||
		!xrtUtf8Valid(Text, NULL) ||
		!__xrtHttpStructuredWriteBytes(pWriter, "%\"", 2u) ) {
		return false;
	}
	for ( i = 0; i < Text.Size; i++ ) {
		uint8 iByte = (uint8)Text.Data[i];

		if ( (iByte < 0x20u) || (iByte > 0x7Eu) ||
			(iByte == (uint8)'%') || (iByte == (uint8)'"') ) {
			char arrEncoded[3];

			arrEncoded[0] = '%';
			arrEncoded[1] = __xrtHttpStructuredWriteHex(
				(uint8)(iByte >> 4u)
			);
			arrEncoded[2] = __xrtHttpStructuredWriteHex(
				(uint8)(iByte & 0x0Fu)
			);
			if ( !__xrtHttpStructuredWriteBytes(
				pWriter, arrEncoded, sizeof(arrEncoded)
			) ) {
				return false;
			}
		} else if ( !__xrtHttpStructuredWriteByte(
			pWriter, (char)iByte
		) ) {
			return false;
		}
	}
	return __xrtHttpStructuredWriteByte(pWriter, '"');
}



/* 写出一个抽象裸值。 */
static bool __xrtHttpStructuredWriteBareValue(
	xrt_http_structured_writer* pWriter,
	const xhttpstructuredvalue* pValue
)
{
	if ( !__xrtHttpStructuredWriteView(pWriter, pValue->Data) ) {
		return false;
	}
	switch ( pValue->Type ) {
		case XHTTP_STRUCTURED_INTEGER:
			return (pValue->Data.Size == 0) &&
				__xrtHttpStructuredWriteInteger(
					pWriter, pValue->Number
				);

		case XHTTP_STRUCTURED_DECIMAL:
			return (pValue->Data.Size == 0) &&
				__xrtHttpStructuredWriteDecimal(
					pWriter, pValue->Number
				);

		case XHTTP_STRUCTURED_STRING:
			return __xrtHttpStructuredWriteString(
				pWriter, pValue->Data
			);

		case XHTTP_STRUCTURED_TOKEN:
			return __xrtHttpStructuredWriteToken(
				pWriter, pValue->Data
			);

		case XHTTP_STRUCTURED_BYTES:
			return __xrtHttpStructuredWriteByteSequence(
				pWriter, pValue->Data
			);

		case XHTTP_STRUCTURED_BOOLEAN:
			return (pValue->Data.Size == 0) &&
				((pValue->Number == 0) ||
				 (pValue->Number == 1)) &&
				__xrtHttpStructuredWriteBytes(
					pWriter,
					pValue->Number == 0 ? "?0" : "?1",
					2u
				);

		case XHTTP_STRUCTURED_DATE:
			return (pValue->Data.Size == 0) &&
				__xrtHttpStructuredWriteByte(
					pWriter, '@'
				) && __xrtHttpStructuredWriteInteger(
					pWriter, pValue->Number
				);

		case XHTTP_STRUCTURED_DISPLAY:
			return __xrtHttpStructuredWriteDisplay(
				pWriter, pValue->Data
			);

		default:
			return false;
	}
}



/* 从允许未对齐的参数数组读取一个描述符。 */
static void __xrtHttpStructuredWriteParameterLoad(
	const xhttpstructuredparameterentry* pParameters,
	size_t iIndex,
	xhttpstructuredparameterentry* pOutput
)
{
	memcpy(
		pOutput, (const unsigned char*)pParameters +
		(iIndex * sizeof(*pParameters)), sizeof(*pOutput)
	);
}



/* 判断参数 key 是否在前缀中重复。 */
static bool __xrtHttpStructuredWriteParameterSeen(
	const xhttpstructuredparameterentry* pParameters,
	size_t iBefore,
	xstrview Key
)
{
	xhttpstructuredparameterentry Parameter;
	size_t i;

	for ( i = 0; i < iBefore; i++ ) {
		__xrtHttpStructuredWriteParameterLoad(
			pParameters, i, &Parameter
		);
		if ( __xrtHttpViewEqual(Parameter.Key, Key) ) {
			return true;
		}
	}
	return false;
}



/* 写出唯一 key 的规范参数序列。 */
static bool __xrtHttpStructuredWriteParameters(
	xrt_http_structured_writer* pWriter,
	const xhttpstructuredparameterentry* pParameters,
	size_t iCount
)
{
	xhttpstructuredparameterentry Parameter;
	size_t iBytes;
	size_t i;

	if ( !__xrtHttpStructuredWriteArray(
		pWriter, pParameters, iCount,
		sizeof(*pParameters), &iBytes
	) ) {
		return false;
	}
	for ( i = 0; i < iCount; i++ ) {
		__xrtHttpStructuredWriteParameterLoad(
			pParameters, i, &Parameter
		);
		if ( !__xrtHttpStructuredWriteView(
			pWriter, Parameter.Key
		) || !xrtHttpStructuredKeyValid(Parameter.Key) ||
			__xrtHttpStructuredWriteParameterSeen(
				pParameters, i, Parameter.Key
			) || !__xrtHttpStructuredWriteByte(
				pWriter, ';'
			) || !__xrtHttpStructuredWriteBytes(
				pWriter, Parameter.Key.Data, Parameter.Key.Size
			) ) {
			return false;
		}
		if ( (Parameter.Value.Type == XHTTP_STRUCTURED_BOOLEAN) &&
			(Parameter.Value.Number == 1) &&
			(Parameter.Value.Data.Size == 0) ) {
			continue;
		}
		if ( !__xrtHttpStructuredWriteByte(
			pWriter, '='
		) || !__xrtHttpStructuredWriteBareValue(
			pWriter, &Parameter.Value
		) ) {
			return false;
		}
	}
	return true;
}



/* 写出一个 Item。 */
static bool __xrtHttpStructuredWriteItemValue(
	xrt_http_structured_writer* pWriter,
	const xhttpstructureditemvalue* pItem
)
{
	return __xrtHttpStructuredWriteBareValue(
		pWriter, &pItem->Bare
	) && __xrtHttpStructuredWriteParameters(
		pWriter, pItem->Parameters, pItem->ParameterCount
	);
}



/* 从允许未对齐的 Item 数组读取一个描述符。 */
static void __xrtHttpStructuredWriteItemLoad(
	const xhttpstructureditemvalue* pItems,
	size_t iIndex,
	xhttpstructureditemvalue* pOutput
)
{
	memcpy(
		pOutput, (const unsigned char*)pItems +
		(iIndex * sizeof(*pItems)), sizeof(*pOutput)
	);
}



/* 写出 Item 或 Inner List 成员。 */
static bool __xrtHttpStructuredWriteMemberValue(
	xrt_http_structured_writer* pWriter,
	const xhttpstructuredmembervalue* pMember
)
{
	xhttpstructureditemvalue Item;
	size_t iBytes;
	size_t i;

	if ( pMember->Kind == XHTTP_STRUCTURED_MEMBER_ITEM ) {
		if ( (pMember->Inner != NULL) ||
			(pMember->InnerCount != 0) ||
			(pMember->Parameters != NULL) ||
			(pMember->ParameterCount != 0) ) {
			return false;
		}
		return __xrtHttpStructuredWriteItemValue(
			pWriter, &pMember->Item
		);
	}
	if ( pMember->Kind != XHTTP_STRUCTURED_MEMBER_INNER_LIST ) {
		return false;
	}
	if ( (pMember->Item.Bare.Type != 0) ||
		(pMember->Item.Bare.Number != 0) ||
		(pMember->Item.Bare.Data.Data != NULL) ||
		(pMember->Item.Bare.Data.Size != 0) ||
		(pMember->Item.Parameters != NULL) ||
		(pMember->Item.ParameterCount != 0) ||
		!__xrtHttpStructuredWriteArray(
			pWriter, pMember->Inner, pMember->InnerCount,
			sizeof(*pMember->Inner), &iBytes
		) || !__xrtHttpStructuredWriteByte(pWriter, '(') ) {
		return false;
	}
	for ( i = 0; i < pMember->InnerCount; i++ ) {
		__xrtHttpStructuredWriteItemLoad(
			pMember->Inner, i, &Item
		);
		if ( ((i != 0) && !__xrtHttpStructuredWriteByte(
			pWriter, ' '
		)) || !__xrtHttpStructuredWriteItemValue(
			pWriter, &Item
		) ) {
			return false;
		}
	}
	return __xrtHttpStructuredWriteByte(
		pWriter, ')'
	) && __xrtHttpStructuredWriteParameters(
		pWriter, pMember->Parameters, pMember->ParameterCount
	);
}



/* 从允许未对齐的成员数组读取一个描述符。 */
static void __xrtHttpStructuredWriteMemberLoad(
	const xhttpstructuredmembervalue* pMembers,
	size_t iIndex,
	xhttpstructuredmembervalue* pOutput
)
{
	memcpy(
		pOutput, (const unsigned char*)pMembers +
		(iIndex * sizeof(*pMembers)), sizeof(*pOutput)
	);
}



/* 写出 List 数组。 */
static bool __xrtHttpStructuredWriteListValue(
	xrt_http_structured_writer* pWriter,
	const xhttpstructuredmembervalue* pMembers,
	size_t iCount
)
{
	xhttpstructuredmembervalue Member;
	size_t iBytes;
	size_t i;

	if ( !__xrtHttpStructuredWriteArray(
		pWriter, pMembers, iCount,
		sizeof(*pMembers), &iBytes
	) ) {
		return false;
	}
	for ( i = 0; i < iCount; i++ ) {
		__xrtHttpStructuredWriteMemberLoad(
			pMembers, i, &Member
		);
		if ( ((i != 0) && !__xrtHttpStructuredWriteBytes(
			pWriter, ", ", 2u
		)) || !__xrtHttpStructuredWriteMemberValue(
			pWriter, &Member
		) ) {
			return false;
		}
	}
	return true;
}



/* 从允许未对齐的 Dictionary 数组读取一个描述符。 */
static void __xrtHttpStructuredWriteDictionaryLoad(
	const xhttpstructureddictionaryentry* pEntries,
	size_t iIndex,
	xhttpstructureddictionaryentry* pOutput
)
{
	memcpy(
		pOutput, (const unsigned char*)pEntries +
		(iIndex * sizeof(*pEntries)), sizeof(*pOutput)
	);
}



/* 判断 Dictionary key 是否在前缀中重复。 */
static bool __xrtHttpStructuredWriteDictionarySeen(
	const xhttpstructureddictionaryentry* pEntries,
	size_t iBefore,
	xstrview Key
)
{
	xhttpstructureddictionaryentry Entry;
	size_t i;

	for ( i = 0; i < iBefore; i++ ) {
		__xrtHttpStructuredWriteDictionaryLoad(
			pEntries, i, &Entry
		);
		if ( __xrtHttpViewEqual(Entry.Key, Key) ) {
			return true;
		}
	}
	return false;
}



/* 写出唯一 key 的规范 Dictionary。 */
static bool __xrtHttpStructuredWriteDictionaryValue(
	xrt_http_structured_writer* pWriter,
	const xhttpstructureddictionaryentry* pEntries,
	size_t iCount
)
{
	xhttpstructureddictionaryentry Entry;
	size_t iBytes;
	size_t i;

	if ( !__xrtHttpStructuredWriteArray(
		pWriter, pEntries, iCount,
		sizeof(*pEntries), &iBytes
	) ) {
		return false;
	}
	for ( i = 0; i < iCount; i++ ) {
		__xrtHttpStructuredWriteDictionaryLoad(
			pEntries, i, &Entry
		);
		if ( !__xrtHttpStructuredWriteView(
			pWriter, Entry.Key
		) || !xrtHttpStructuredKeyValid(Entry.Key) ||
			__xrtHttpStructuredWriteDictionarySeen(
				pEntries, i, Entry.Key
			) || ((i != 0) &&
			 !__xrtHttpStructuredWriteBytes(
				pWriter, ", ", 2u
			 )) || !__xrtHttpStructuredWriteBytes(
				pWriter, Entry.Key.Data, Entry.Key.Size
			) ) {
			return false;
		}
		if ( (Entry.Member.Kind == XHTTP_STRUCTURED_MEMBER_ITEM) &&
			(Entry.Member.Item.Bare.Type == XHTTP_STRUCTURED_BOOLEAN) &&
			(Entry.Member.Item.Bare.Number == 1) &&
			(Entry.Member.Item.Bare.Data.Size == 0) ) {
			if ( (Entry.Member.Inner != NULL) ||
				(Entry.Member.InnerCount != 0) ||
				(Entry.Member.Parameters != NULL) ||
				(Entry.Member.ParameterCount != 0) ||
				!__xrtHttpStructuredWriteParameters(
					pWriter,
					Entry.Member.Item.Parameters,
					Entry.Member.Item.ParameterCount
				) ) {
				return false;
			}
			continue;
		}
		if ( !__xrtHttpStructuredWriteByte(
			pWriter, '='
		) || !__xrtHttpStructuredWriteMemberValue(
			pWriter, &Entry.Member
		) ) {
			return false;
		}
	}
	return true;
}



/* 按根类型加载未对齐描述符并执行一次遍历。 */
static bool __xrtHttpStructuredWriteRoot(
	xrt_http_structured_writer* pWriter,
	xrt_http_structured_root Root,
	const void* pData,
	size_t iCount
)
{
	xhttpstructureddictionaryentry Dictionary;
	xhttpstructureditemvalue Item;
	xhttpstructuredmembervalue Member;
	xhttpstructuredvalue Value;
	size_t iBytes;

	switch ( Root ) {
		case XRT_HTTP_STRUCTURED_ROOT_BARE:
			if ( !__xrtHttpStructuredWriteBorrow(
				pWriter, pData, sizeof(Value)
			) ) {
				return false;
			}
			memcpy(&Value, pData, sizeof(Value));
			return __xrtHttpStructuredWriteBareValue(
				pWriter, &Value
			);

		case XRT_HTTP_STRUCTURED_ROOT_ITEM:
			if ( !__xrtHttpStructuredWriteBorrow(
				pWriter, pData, sizeof(Item)
			) ) {
				return false;
			}
			memcpy(&Item, pData, sizeof(Item));
			return __xrtHttpStructuredWriteItemValue(
				pWriter, &Item
			);

		case XRT_HTTP_STRUCTURED_ROOT_LIST:
			if ( !__xrtHttpStructuredWriteArray(
				pWriter, pData, iCount, sizeof(Member), &iBytes
			) ) {
				return false;
			}
			return __xrtHttpStructuredWriteListValue(
				pWriter,
				(const xhttpstructuredmembervalue*)pData,
				iCount
			);

		case XRT_HTTP_STRUCTURED_ROOT_DICTIONARY:
			if ( !__xrtHttpStructuredWriteArray(
				pWriter, pData, iCount,
				sizeof(Dictionary), &iBytes
			) ) {
				return false;
			}
			return __xrtHttpStructuredWriteDictionaryValue(
				pWriter,
				(const xhttpstructureddictionaryentry*)pData,
				iCount
			);

		default:
			return false;
	}
}



/* 执行测量、容量判断、完整重叠校验和最终写出。 */
static bool __xrtHttpStructuredWrite(
	xrt_http_structured_root Root,
	const void* pData,
	size_t iCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xrt_http_structured_writer Writer;
	size_t iRequired;

	if ( !__xrtRangeValid(pSize, sizeof(*pSize)) ||
		((pOutput == NULL) && (iCapacity != 0)) ||
		((pOutput != NULL) &&
		 !__xrtRangeValid(pOutput, iCapacity)) ||
		((pOutput != NULL) && __xrtRangesOverlap(
			pOutput, iCapacity, pSize, sizeof(*pSize)
		)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(&Writer, 0, sizeof(Writer));
	Writer.Protected = pSize;
	Writer.ProtectedSize = sizeof(*pSize);
	if ( !__xrtHttpStructuredWriteRoot(
		&Writer, Root, pData, iCount
	) ) {
		if ( xrtGetError() == NULL ) {
			__xrtErrorSetValue();
		}
		return false;
	}
	iRequired = Writer.Position;
	if ( pOutput == NULL ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		return true;
	}
	if ( iCapacity < iRequired ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		__xrtErrorSetRange();
		return false;
	}
	memset(&Writer, 0, sizeof(Writer));
	Writer.Protected = pSize;
	Writer.ProtectedSize = sizeof(*pSize);
	Writer.Target = pOutput;
	Writer.TargetSize = iRequired;
	if ( !__xrtHttpStructuredWriteRoot(
		&Writer, Root, pData, iCount
	) ) {
		if ( xrtGetError() == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return false;
	}
	memset(&Writer, 0, sizeof(Writer));
	Writer.Output = (char*)pOutput;
	if ( !__xrtHttpStructuredWriteRoot(
		&Writer, Root, pData, iCount
	) || (Writer.Position != iRequired) ) {
		if ( xrtGetError() == NULL ) {
			__xrtErrorSetValue();
		}
		return false;
	}
	memcpy(pSize, &iRequired, sizeof(iRequired));
	return true;
}



/* 规范序列化一个抽象裸值。 */
XRT_API bool xrtHttpStructuredBareWrite(
	const xhttpstructuredvalue* pValue,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	return __xrtHttpStructuredWrite(
		XRT_HTTP_STRUCTURED_ROOT_BARE,
		pValue, 1u, pOutput, iCapacity, pSize
	);
}



/* 规范序列化一个 Item 及其参数。 */
XRT_API bool xrtHttpStructuredItemWrite(
	const xhttpstructureditemvalue* pItem,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	return __xrtHttpStructuredWrite(
		XRT_HTTP_STRUCTURED_ROOT_ITEM,
		pItem, 1u, pOutput, iCapacity, pSize
	);
}



/* 规范序列化完整 List。 */
XRT_API bool xrtHttpStructuredListWrite(
	const xhttpstructuredmembervalue* pMembers,
	size_t iCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	return __xrtHttpStructuredWrite(
		XRT_HTTP_STRUCTURED_ROOT_LIST,
		pMembers, iCount, pOutput, iCapacity, pSize
	);
}



/* 规范序列化完整 Dictionary。 */
XRT_API bool xrtHttpStructuredDictionaryWrite(
	const xhttpstructureddictionaryentry* pEntries,
	size_t iCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	return __xrtHttpStructuredWrite(
		XRT_HTTP_STRUCTURED_ROOT_DICTIONARY,
		pEntries, iCount, pOutput, iCapacity, pSize
	);
}

#endif
