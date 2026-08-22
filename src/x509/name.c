#include "../internal/xrt_x509.h"
#include "../internal/xrt_charset.h"
#include "../internal/xrt_x509_name_data.h"



#if defined(XRT_FEATURE_X509_NAME)

#define XRT_X509_NAME_LOCAL_SCALARS 64u
#define XRT_X509_NAME_LOCAL_ATTRIBUTES 16u



typedef struct xrt_x509_name_buffer {
	uint32* Data;
	size_t Size;
	size_t Capacity;
	uint32 Local[XRT_X509_NAME_LOCAL_SCALARS];
} xrt_x509_name_buffer;



typedef struct xrt_x509_name_entry {
	xx509nameattr Attribute;
	bool Matched;
} xrt_x509_name_entry;



typedef struct xrt_x509_name_entries {
	xrt_x509_name_entry* Data;
	size_t Size;
	size_t Capacity;
	size_t RdnCount;
	xrt_x509_name_entry Local[XRT_X509_NAME_LOCAL_ATTRIBUTES];
} xrt_x509_name_entries;



typedef struct xrt_x509_name_cursor {
	const xrt_x509_name_buffer* Buffer;
	size_t First;
	size_t Last;
	size_t Index;
	uint8 State;
	bool PendingSpace;
} xrt_x509_name_cursor;



typedef struct xrt_x509_name_map {
	uint16 Offset;
	uint8 Size;
} xrt_x509_name_map;



static const uint8 __xrtX509NameDomainComponentOid[] = {
	0x09, 0x92, 0x26, 0x89, 0x93, 0xF2, 0x2C, 0x64, 0x01, 0x19
};

static const uint8 __xrtX509NameEmailOid[] = {
	0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x09, 0x01
};



/* 设置名称比较错误并保留可选底层原因。 */
static bool __xrtX509NameError(
	xerrkind Kind,
	cstr sMessage,
	const xerror* pCause
)
{
	__xrtX509Error(
		Kind, X509_ERROR_NAME, "x509-name-compare",
		sMessage, SIZE_MAX, pCause
	);
	return false;
}



/* 比较两个借用字节视图。 */
static bool __xrtX509NameBytesEqual(xbytesview Left, xbytesview Right)
{
	return (Left.Size == Right.Size) &&
		((Left.Size == 0) || (memcmp(Left.Data, Right.Data, Left.Size) == 0));
}



/* 比较 OID 内容和一项内建常量。 */
static bool __xrtX509NameOid(
	xbytesview Oid,
	const uint8* pValue,
	size_t iSize
)
{
	return (Oid.Size == iSize) &&
		((iSize == 0) || (memcmp(Oid.Data, pValue, iSize) == 0));
}



/* 从生成的定宽位流读取一个不超过 32 位的字段。 */
static uint32 __xrtX509NameBits(
	const uint32* pWords,
	size_t iBit,
	uint8 iWidth
)
{
	size_t iWord = iBit / 32u;
	uint8 iShift = (uint8)(iBit % 32u);
	uint64 iValue = (uint64)pWords[iWord] >> iShift;

	if ( (uint8)(iShift + iWidth) > 32u ) {
		iValue |= (uint64)pWords[iWord + 1u] << (32u - iShift);
	}
	return (uint32)(iValue & ((UINT64_C(1) << iWidth) - 1u));
}



/* 从连续 Unicode 标量位流读取一个码点。 */
static uint32 __xrtX509NameScalar(
	const uint32* pScalars,
	size_t iIndex
)
{
	return __xrtX509NameBits(pScalars, iIndex * 21u, 21u);
}



/* 在有序紧凑映射表中查找一个 Unicode 码点。 */
static bool __xrtX509NameMapFind(
	const uint32* pMaps,
	size_t iCount,
	uint32 iCode,
	xrt_x509_name_map* pMap
)
{
	size_t iFirst = 0;

	while ( iFirst < iCount ) {
		size_t iMiddle = iFirst + ((iCount - iFirst) / 2u);
		size_t iBit = iMiddle * 40u;
		uint32 iCurrent = __xrtX509NameBits(pMaps, iBit, 21u);

		if ( iCurrent < iCode ) {
			iFirst = iMiddle + 1u;
		} else if ( iCurrent > iCode ) {
			iCount = iMiddle;
		} else {
			pMap->Offset = (uint16)__xrtX509NameBits(
				pMaps, iBit + 21u, 14u
			);
			pMap->Size = (uint8)__xrtX509NameBits(
				pMaps, iBit + 35u, 5u
			);
			return true;
		}
	}
	return false;
}



/* 返回 Unicode 3.2 规范组合类。 */
static uint8 __xrtX509NameCombining(uint32 iCode)
{
	size_t iFirst = 0;
	size_t iCount = XRT_X509_NAME_CLASS_COUNT;

	while ( iFirst < iCount ) {
		size_t iMiddle = iFirst + ((iCount - iFirst) / 2u);
		size_t iBit = iMiddle * 29u;
		uint32 iCurrent = __xrtX509NameBits(
			__xrtX509NameClasses, iBit, 21u
		);

		if ( iCurrent < iCode ) {
			iFirst = iMiddle + 1u;
		} else if ( iCurrent > iCode ) {
			iCount = iMiddle;
		} else {
			return (uint8)__xrtX509NameBits(
				__xrtX509NameClasses, iBit + 21u, 8u
			);
		}
	}
	return 0;
}



/* 在有序紧凑闭区间表中判断一个 Unicode 码点。 */
static bool __xrtX509NameRangeContains(
	const uint32* pRanges,
	size_t iCount,
	uint32 iCode
)
{
	size_t iFirst = 0;

	while ( iFirst < iCount ) {
		size_t iMiddle = iFirst + ((iCount - iFirst) / 2u);
		size_t iBit = iMiddle * 42u;
		uint32 iRangeFirst = __xrtX509NameBits(
			pRanges, iBit, 21u
		);
		uint32 iRangeLast = __xrtX509NameBits(
			pRanges, iBit + 21u, 21u
		);

		if ( iCode < iRangeFirst ) {
			iCount = iMiddle;
		} else if ( iCode > iRangeLast ) {
			iFirst = iMiddle + 1u;
		} else {
			return true;
		}
	}
	return false;
}



/* 判断码点是否是 RFC 4518 定义的 Unicode 3.2 标记。 */
static bool __xrtX509NameIsMark(uint32 iCode)
{
	return __xrtX509NameRangeContains(
		__xrtX509NameMarks,
		XRT_X509_NAME_MARK_COUNT,
		iCode
	);
}



/* 判断码点是否被 RFC 4518 禁止。 */
static bool __xrtX509NameIsProhibited(uint32 iCode)
{
	return __xrtX509NameRangeContains(
		__xrtX509NameProhibited,
		XRT_X509_NAME_PROHIBITED_COUNT,
		iCode
	);
}



/* 初始化短值保持栈内的 Unicode 标量缓冲。 */
static void __xrtX509NameBufferInit(xrt_x509_name_buffer* pBuffer)
{
	pBuffer->Data = pBuffer->Local;
	pBuffer->Size = 0;
	pBuffer->Capacity = XRT_X509_NAME_LOCAL_SCALARS;
}



/* 释放名称标量缓冲的可选堆存储。 */
static void __xrtX509NameBufferUnit(xrt_x509_name_buffer* pBuffer)
{
	if ( pBuffer->Data != pBuffer->Local ) {
		xrtFree(pBuffer->Data);
	}
}



/* 为异常长名称扩展标量缓冲。 */
static bool __xrtX509NameBufferReserve(
	xrt_x509_name_buffer* pBuffer,
	size_t iNeed
)
{
	size_t iCapacity = pBuffer->Capacity;
	uint32* pData;

	if ( iNeed <= iCapacity ) {
		return true;
	}
	while ( iCapacity < iNeed ) {
		if ( iCapacity > (SIZE_MAX / 2u) ) {
			return __xrtX509NameError(
				XERR_RANGE, "prepared X.509 Name is too large", NULL
			);
		}
		iCapacity *= 2u;
	}
	if ( iCapacity > (SIZE_MAX / sizeof(uint32)) ) {
		return __xrtX509NameError(
			XERR_RANGE, "prepared X.509 Name is too large", NULL
		);
	}
	if ( pBuffer->Data == pBuffer->Local ) {
		pData = (uint32*)xrtMalloc(iCapacity * sizeof(uint32));
		if ( pData != NULL ) {
			memcpy(pData, pBuffer->Data, pBuffer->Size * sizeof(uint32));
		}
	} else {
		pData = (uint32*)xrtRealloc(
			pBuffer->Data, iCapacity * sizeof(uint32)
		);
	}
	if ( pData == NULL ) {
		const xerror* pCause = xrtGetError();

		return __xrtX509NameError(
			XERR_MEMORY, "X.509 Name preparation allocation failed", pCause
		);
	}
	pBuffer->Data = pData;
	pBuffer->Capacity = iCapacity;
	return true;
}



/* 向名称缓冲追加一个 Unicode 标量。 */
static bool __xrtX509NameBufferAppend(
	xrt_x509_name_buffer* pBuffer,
	uint32 iCode
)
{
	if ( !__xrtX509NameBufferReserve(pBuffer, pBuffer->Size + 1u) ) {
		return false;
	}
	pBuffer->Data[pBuffer->Size++] = iCode;
	return true;
}



/* 递归执行 Unicode 3.2 兼容分解。 */
static bool __xrtX509NameDecompose(
	xrt_x509_name_buffer* pBuffer,
	uint32 iCode
)
{
	const uint32 iSBase = UINT32_C(0x00AC00);
	const uint32 iLBase = UINT32_C(0x001100);
	const uint32 iVBase = UINT32_C(0x001161);
	const uint32 iTBase = UINT32_C(0x0011A7);
	const uint32 iLCount = 19u;
	const uint32 iVCount = 21u;
	const uint32 iTCount = 28u;
	const uint32 iNCount = iVCount * iTCount;
	const uint32 iSCount = iLCount * iNCount;
	xrt_x509_name_map Map;

	/* Hangul 音节使用 Unicode 规定的算法分解。 */
	if ( (iCode >= iSBase) && (iCode < (iSBase + iSCount)) ) {
		uint32 iIndex = iCode - iSBase;
		uint32 iTrail = iIndex % iTCount;

		if ( !__xrtX509NameBufferAppend(
			pBuffer, iLBase + (iIndex / iNCount)
		) || !__xrtX509NameBufferAppend(
			pBuffer, iVBase + ((iIndex % iNCount) / iTCount)
		) ) {
			return false;
		}
		return (iTrail == 0) || __xrtX509NameBufferAppend(
			pBuffer, iTBase + iTrail
		);
	}
	if ( !__xrtX509NameMapFind(
		__xrtX509NameDecompositions,
		XRT_X509_NAME_DECOMPOSITION_COUNT,
		iCode,
		&Map
	) ) {
		return __xrtX509NameBufferAppend(pBuffer, iCode);
	}
	for ( size_t i = 0; i < Map.Size; i++ ) {
		if ( !__xrtX509NameDecompose(
			pBuffer,
			__xrtX509NameScalar(
				__xrtX509NameDecompositionData,
				(size_t)Map.Offset + i
			)
		) ) {
			return false;
		}
	}
	return true;
}



/* 执行 RFC 4518 显式控制字符和空白映射。 */
static size_t __xrtX509NameSpecialMap(uint32 iCode, uint32* pMapped)
{
	if ( (iCode == UINT32_C(0x0000AD)) ||
		(iCode == UINT32_C(0x001806)) ||
		(iCode == UINT32_C(0x00034F)) ||
		((iCode >= UINT32_C(0x00180B)) &&
		 (iCode <= UINT32_C(0x00180D))) ||
		((iCode >= UINT32_C(0x00FE00)) &&
		 (iCode <= UINT32_C(0x00FE0F))) ||
		(iCode == UINT32_C(0x00FFFC)) ||
		(iCode == UINT32_C(0x00200B)) ||
		(iCode <= UINT32_C(0x000008)) ||
		((iCode >= UINT32_C(0x00000E)) &&
		 (iCode <= UINT32_C(0x00001F))) ||
		((iCode >= UINT32_C(0x00007F)) &&
		 (iCode <= UINT32_C(0x000084))) ||
		((iCode >= UINT32_C(0x000086)) &&
		 (iCode <= UINT32_C(0x00009F))) ||
		(iCode == UINT32_C(0x0006DD)) ||
		(iCode == UINT32_C(0x00070F)) ||
		(iCode == UINT32_C(0x00180E)) ||
		((iCode >= UINT32_C(0x00200C)) &&
		 (iCode <= UINT32_C(0x00200F))) ||
		((iCode >= UINT32_C(0x00202A)) &&
		 (iCode <= UINT32_C(0x00202E))) ||
		((iCode >= UINT32_C(0x002060)) &&
		 (iCode <= UINT32_C(0x002063))) ||
		((iCode >= UINT32_C(0x00206A)) &&
		 (iCode <= UINT32_C(0x00206F))) ||
		(iCode == UINT32_C(0x00FEFF)) ||
		((iCode >= UINT32_C(0x00FFF9)) &&
		 (iCode <= UINT32_C(0x00FFFB))) ||
		((iCode >= UINT32_C(0x01D173)) &&
		 (iCode <= UINT32_C(0x01D17A))) ||
		(iCode == UINT32_C(0x0E0001)) ||
		((iCode >= UINT32_C(0x0E0020)) &&
		 (iCode <= UINT32_C(0x0E007F))) ) {
		return 0;
	}
	if ( ((iCode >= UINT32_C(0x000009)) &&
		 (iCode <= UINT32_C(0x00000D))) ||
		(iCode == UINT32_C(0x000085)) ||
		(iCode == UINT32_C(0x0000A0)) ||
		(iCode == UINT32_C(0x001680)) ||
		((iCode >= UINT32_C(0x002000)) &&
		 (iCode <= UINT32_C(0x00200A))) ||
		((iCode >= UINT32_C(0x002028)) &&
		 (iCode <= UINT32_C(0x002029))) ||
		(iCode == UINT32_C(0x00202F)) ||
		(iCode == UINT32_C(0x00205F)) ||
		(iCode == UINT32_C(0x003000)) ) {
		*pMapped = UINT32_C(0x000020);
		return 1;
	}
	*pMapped = iCode;
	return 1;
}



/* 对单个输入标量执行映射、case fold 和兼容分解。 */
static bool __xrtX509NameMapScalar(
	xrt_x509_name_buffer* pBuffer,
	uint32 iCode
)
{
	xrt_x509_name_map Map;
	uint32 iMapped;

	if ( __xrtX509NameSpecialMap(iCode, &iMapped) == 0 ) {
		return true;
	}
	if ( !__xrtX509NameMapFind(
		__xrtX509NameMaps,
		XRT_X509_NAME_MAP_COUNT,
		iMapped,
		&Map
	) ) {
		return __xrtX509NameDecompose(pBuffer, iMapped);
	}
	for ( size_t i = 0; i < Map.Size; i++ ) {
		if ( !__xrtX509NameDecompose(
			pBuffer,
			__xrtX509NameScalar(
				__xrtX509NameMapData,
				(size_t)Map.Offset + i
			)
		) ) {
			return false;
		}
	}
	return true;
}



/* 按规范组合类稳定重排兼容分解结果。 */
static void __xrtX509NameReorder(xrt_x509_name_buffer* pBuffer)
{
	for ( size_t i = 1; i < pBuffer->Size; i++ ) {
		uint32 iCode = pBuffer->Data[i];
		uint8 iClass = __xrtX509NameCombining(iCode);
		size_t j = i;

		if ( iClass == 0 ) {
			continue;
		}
		while ( j > 0 ) {
			uint8 iPrevious = __xrtX509NameCombining(
				pBuffer->Data[j - 1u]
			);

			if ( (iPrevious == 0) || (iPrevious <= iClass) ) {
				break;
			}
			pBuffer->Data[j] = pBuffer->Data[j - 1u];
			j--;
		}
		pBuffer->Data[j] = iCode;
	}
}



/* 查找规范组合，并处理 Hangul 算法组合。 */
static bool __xrtX509NameComposePair(
	uint32 iFirst,
	uint32 iSecond,
	uint32* pResult
)
{
	const uint32 iSBase = UINT32_C(0x00AC00);
	const uint32 iLBase = UINT32_C(0x001100);
	const uint32 iVBase = UINT32_C(0x001161);
	const uint32 iTBase = UINT32_C(0x0011A7);
	const uint32 iLCount = 19u;
	const uint32 iVCount = 21u;
	const uint32 iTCount = 28u;
	const uint32 iNCount = iVCount * iTCount;
	const uint32 iSCount = iLCount * iNCount;
	size_t iLow = 0;
	size_t iHigh = XRT_X509_NAME_COMPOSITION_COUNT;

	if ( (iFirst >= iLBase) && (iFirst < (iLBase + iLCount)) &&
		(iSecond >= iVBase) && (iSecond < (iVBase + iVCount)) ) {
		*pResult = iSBase + ((iFirst - iLBase) * iNCount) +
			((iSecond - iVBase) * iTCount);
		return true;
	}
	if ( (iFirst >= iSBase) && (iFirst < (iSBase + iSCount)) &&
		(((iFirst - iSBase) % iTCount) == 0) &&
		(iSecond > iTBase) && (iSecond < (iTBase + iTCount)) ) {
		*pResult = iFirst + (iSecond - iTBase);
		return true;
	}
	while ( iLow < iHigh ) {
		size_t iMiddle = iLow + ((iHigh - iLow) / 2u);
		size_t iBit = iMiddle * 63u;
		uint32 iComposeFirst = __xrtX509NameBits(
			__xrtX509NameCompositions, iBit, 21u
		);
		uint32 iComposeSecond = __xrtX509NameBits(
			__xrtX509NameCompositions, iBit + 21u, 21u
		);

		if ( (iComposeFirst < iFirst) ||
			((iComposeFirst == iFirst) &&
			 (iComposeSecond < iSecond)) ) {
			iLow = iMiddle + 1u;
		} else if ( (iComposeFirst > iFirst) ||
			((iComposeFirst == iFirst) &&
			 (iComposeSecond > iSecond)) ) {
			iHigh = iMiddle;
		} else {
			*pResult = __xrtX509NameBits(
				__xrtX509NameCompositions, iBit + 42u, 21u
			);
			return true;
		}
	}
	return false;
}



/* 把已重排的兼容分解序列组合为 NFKC。 */
static void __xrtX509NameCompose(xrt_x509_name_buffer* pBuffer)
{
	size_t iOutput = 0;
	size_t iStarter = SIZE_MAX;
	uint8 iLastClass = 0;

	for ( size_t i = 0; i < pBuffer->Size; i++ ) {
		uint32 iCode = pBuffer->Data[i];
		uint8 iClass = __xrtX509NameCombining(iCode);
		uint32 iComposed;

		if ( (iStarter != SIZE_MAX) &&
			((iLastClass == 0) || (iLastClass < iClass)) &&
			__xrtX509NameComposePair(
				pBuffer->Data[iStarter], iCode, &iComposed
			) ) {
			pBuffer->Data[iStarter] = iComposed;
			continue;
		}
		if ( iClass == 0 ) {
			iStarter = iOutput;
		}
		pBuffer->Data[iOutput++] = iCode;
		iLastClass = iClass;
	}
	pBuffer->Size = iOutput;
}



/* 判断字符是否属于 ASN.1 PrintableString 字符集。 */
static bool __xrtX509NamePrintable(uint8 iChar)
{
	return ((iChar >= 'A') && (iChar <= 'Z')) ||
		((iChar >= 'a') && (iChar <= 'z')) ||
		((iChar >= '0') && (iChar <= '9')) ||
		(iChar == ' ') || (iChar == '\'') || (iChar == '(') ||
		(iChar == ')') || (iChar == '+') || (iChar == ',') ||
		(iChar == '-') || (iChar == '.') || (iChar == '/') ||
		(iChar == ':') || (iChar == '=') || (iChar == '?');
}



/* 解码并映射一个 DirectoryString 属性值。 */
static bool __xrtX509NameDecode(
	const xx509nameattr* pAttribute,
	xrt_x509_name_buffer* pBuffer
)
{
	const uint8* pData = pAttribute->Value.Data;
	size_t iSize = pAttribute->Value.Size;
	uint32 iTag = pAttribute->ValueTag.Number;

	if ( (pAttribute->ValueTag.Class != XASN1_UNIVERSAL) ||
		pAttribute->ValueTag.Constructed ) {
		return __xrtX509NameError(
			XERR_PROTOCOL, "DirectoryString must use a primitive universal tag",
			NULL
		);
	}
	if ( iSize == 0 ) {
		return __xrtX509NameError(
			XERR_PROTOCOL, "DirectoryString must not be empty", NULL
		);
	}
	if ( iTag == (uint32)XASN1_UTF8_STRING ) {
		size_t iOffset = 0;

		while ( iOffset < iSize ) {
			xrt_utf_decode Decode = __xrtUtf8Decode(
				pData + iOffset, iSize - iOffset
			);

			if ( Decode.Status != XUTF_OK ) {
				return __xrtX509NameError(
					XERR_PROTOCOL, "DirectoryString contains invalid UTF-8", NULL
				);
			}
			if ( !__xrtX509NameMapScalar(pBuffer, Decode.Scalar) ) {
				return false;
			}
			iOffset += Decode.Read;
		}
		return true;
	}
	if ( (iTag == (uint32)XASN1_PRINTABLE_STRING) ||
		(iTag == (uint32)XASN1_TELETEX_STRING) ) {
		for ( size_t i = 0; i < iSize; i++ ) {
			if ( (iTag == (uint32)XASN1_PRINTABLE_STRING) &&
				!__xrtX509NamePrintable(pData[i]) ) {
				return __xrtX509NameError(
					XERR_PROTOCOL,
					"DirectoryString contains an invalid PrintableString character",
					NULL
				);
			}
			if ( (iTag == (uint32)XASN1_TELETEX_STRING) &&
				(pData[i] > UINT8_C(0x7F)) ) {
				return __xrtX509NameError(
					XERR_UNSUPPORTED,
					"non-ASCII TeletexString mapping is implementation-defined",
					NULL
				);
			}
			if ( !__xrtX509NameMapScalar(pBuffer, pData[i]) ) {
				return false;
			}
		}
		return true;
	}
	if ( iTag == (uint32)XASN1_BMP_STRING ) {
		if ( (iSize % 2u) != 0 ) {
			return __xrtX509NameError(
				XERR_PROTOCOL, "BMPString has an incomplete code unit", NULL
			);
		}
		for ( size_t i = 0; i < iSize; i += 2u ) {
			uint32 iCode = ((uint32)pData[i] << 8u) | pData[i + 1u];

			if ( (iCode >= UINT32_C(0xD800)) &&
				(iCode <= UINT32_C(0xDFFF)) ) {
				return __xrtX509NameError(
					XERR_PROTOCOL, "BMPString contains a surrogate", NULL
				);
			}
			if ( !__xrtX509NameMapScalar(pBuffer, iCode) ) {
				return false;
			}
		}
		return true;
	}
	if ( iTag == (uint32)XASN1_UNIVERSAL_STRING ) {
		if ( (iSize % 4u) != 0 ) {
			return __xrtX509NameError(
				XERR_PROTOCOL, "UniversalString has an incomplete code unit", NULL
			);
		}
		for ( size_t i = 0; i < iSize; i += 4u ) {
			uint32 iCode = ((uint32)pData[i] << 24u) |
				((uint32)pData[i + 1u] << 16u) |
				((uint32)pData[i + 2u] << 8u) | pData[i + 3u];

			if ( !xrtUnicodeScalar(iCode) ) {
				return __xrtX509NameError(
					XERR_PROTOCOL,
					"UniversalString contains an invalid Unicode scalar", NULL
				);
			}
			if ( !__xrtX509NameMapScalar(pBuffer, iCode) ) {
				return false;
			}
		}
		return true;
	}
	return __xrtX509NameError(
		XERR_PROTOCOL, "unsupported DirectoryString encoding", NULL
	);
}



/* 完成 NFKC 并检查 RFC 4518 禁止字符。 */
static bool __xrtX509NamePrepare(
	const xx509nameattr* pAttribute,
	xrt_x509_name_buffer* pBuffer
)
{
	__xrtX509NameBufferInit(pBuffer);
	if ( !__xrtX509NameDecode(pAttribute, pBuffer) ) {
		return false;
	}
	__xrtX509NameReorder(pBuffer);
	__xrtX509NameCompose(pBuffer);
	for ( size_t i = 0; i < pBuffer->Size; i++ ) {
		if ( __xrtX509NameIsProhibited(pBuffer->Data[i]) ) {
			return __xrtX509NameError(
				XERR_PROTOCOL,
				"DirectoryString contains a prohibited Unicode code point", NULL
			);
		}
	}
	return true;
}



/* 判断 NFKC 序列中的 SPACE 是否没有跟随组合标记。 */
static bool __xrtX509NameInsignificantSpace(
	const xrt_x509_name_buffer* pBuffer,
	size_t iIndex
)
{
	return (pBuffer->Data[iIndex] == UINT32_C(0x20)) &&
		((iIndex + 1u == pBuffer->Size) ||
		 !__xrtX509NameIsMark(pBuffer->Data[iIndex + 1u]));
}



/* 初始化 RFC 4518 非子串值的空白压缩游标。 */
static void __xrtX509NameCursorInit(
	const xrt_x509_name_buffer* pBuffer,
	xrt_x509_name_cursor* pCursor
)
{
	size_t iFirst = 0;
	size_t iLast = pBuffer->Size;

	while ( (iFirst < iLast) &&
		__xrtX509NameInsignificantSpace(pBuffer, iFirst) ) {
		iFirst++;
	}
	while ( (iLast > iFirst) &&
		__xrtX509NameInsignificantSpace(pBuffer, iLast - 1u) ) {
		iLast--;
	}
	pCursor->Buffer = pBuffer;
	pCursor->First = iFirst;
	pCursor->Last = iLast;
	pCursor->Index = iFirst;
	pCursor->State = 0;
	pCursor->PendingSpace = false;
}



/* 读取空白处理后的下一个比较标量。 */
static bool __xrtX509NameCursorRead(
	xrt_x509_name_cursor* pCursor,
	uint32* pCode
)
{
	if ( pCursor->State == 0 ) {
		pCursor->State = 1;
		*pCode = UINT32_C(0x20);
		return true;
	}
	if ( pCursor->State == 1 ) {
		if ( pCursor->PendingSpace ) {
			pCursor->PendingSpace = false;
			*pCode = UINT32_C(0x20);
			return true;
		}
		if ( pCursor->Index < pCursor->Last ) {
			if ( __xrtX509NameInsignificantSpace(
				pCursor->Buffer, pCursor->Index
			) ) {
				do {
					pCursor->Index++;
				} while ( (pCursor->Index < pCursor->Last) &&
					__xrtX509NameInsignificantSpace(
						pCursor->Buffer, pCursor->Index
					) );
				pCursor->PendingSpace = true;
				*pCode = UINT32_C(0x20);
				return true;
			}
			*pCode = pCursor->Buffer->Data[pCursor->Index++];
			return true;
		}
		pCursor->State = 2;
	}
	if ( pCursor->State == 2 ) {
		pCursor->State = 3;
		*pCode = UINT32_C(0x20);
		return true;
	}
	return false;
}



/* 比较两个完成 StringPrep 的 DirectoryString 值。 */
static xx509result __xrtX509NamePreparedEqual(
	const xx509nameattr* pLeft,
	const xx509nameattr* pRight
)
{
	xrt_x509_name_buffer Left;
	xrt_x509_name_buffer Right;
	xrt_x509_name_cursor LeftCursor;
	xrt_x509_name_cursor RightCursor;
	xx509result Result = X509_ERROR;

	if ( !__xrtX509NamePrepare(pLeft, &Left) ) {
		__xrtX509NameBufferUnit(&Left);
		return X509_ERROR;
	}
	if ( !__xrtX509NamePrepare(pRight, &Right) ) {
		__xrtX509NameBufferUnit(&Left);
		__xrtX509NameBufferUnit(&Right);
		return X509_ERROR;
	}
	__xrtX509NameCursorInit(&Left, &LeftCursor);
	__xrtX509NameCursorInit(&Right, &RightCursor);
	while ( true ) {
		uint32 iLeft;
		uint32 iRight;
		bool bLeft = __xrtX509NameCursorRead(&LeftCursor, &iLeft);
		bool bRight = __xrtX509NameCursorRead(&RightCursor, &iRight);

		if ( bLeft != bRight ) {
			Result = X509_DONE;
			break;
		}
		if ( !bLeft ) {
			Result = X509_VALUE;
			break;
		}
		if ( iLeft != iRight ) {
			Result = X509_DONE;
			break;
		}
	}
	__xrtX509NameBufferUnit(&Left);
	__xrtX509NameBufferUnit(&Right);
	return Result;
}



/* 判断标签是否是 RFC 5280 DirectoryString 编码。 */
static bool __xrtX509NameDirectoryTag(const xasn1tag* pTag)
{
	if ( (pTag->Class != XASN1_UNIVERSAL) || pTag->Constructed ) {
		return false;
	}
	return (pTag->Number == (uint32)XASN1_UTF8_STRING) ||
		(pTag->Number == (uint32)XASN1_PRINTABLE_STRING) ||
		(pTag->Number == (uint32)XASN1_TELETEX_STRING) ||
		(pTag->Number == (uint32)XASN1_UNIVERSAL_STRING) ||
		(pTag->Number == (uint32)XASN1_BMP_STRING);
}



/* 验证采用 caseIgnoreIA5Match 的已知名称属性。 */
static bool __xrtX509NameIa5Valid(
	const xx509nameattr* pAttribute,
	bool bDomain
)
{
	if ( (pAttribute->ValueTag.Class != XASN1_UNIVERSAL) ||
		pAttribute->ValueTag.Constructed ||
		(pAttribute->ValueTag.Number != (uint32)XASN1_IA5_STRING) ) {
		return __xrtX509NameError(
			XERR_PROTOCOL, "caseIgnoreIA5Match attribute is not IA5String", NULL
		);
	}
	if ( (pAttribute->Value.Size == 0) ||
		pAttribute->Value.Size > (bDomain ? 63u : 255u) ) {
		return __xrtX509NameError(
			XERR_PROTOCOL,
			bDomain ? "domainComponent label length is invalid" :
				"emailAddress length is invalid",
			NULL
		);
	}
	for ( size_t i = 0; i < pAttribute->Value.Size; i++ ) {
		uint8 iChar = pAttribute->Value.Data[i];
		bool bAlpha = ((iChar >= 'A') && (iChar <= 'Z')) ||
			((iChar >= 'a') && (iChar <= 'z'));
		bool bDigit = (iChar >= '0') && (iChar <= '9');

		if ( iChar > UINT8_C(0x7F) ) {
			return __xrtX509NameError(
				XERR_PROTOCOL, "IA5String contains a non-ASCII octet", NULL
			);
		}
		if ( bDomain && !bAlpha && !bDigit &&
			!((iChar == '-') && (i > 0) &&
			  (i + 1u < pAttribute->Value.Size)) ) {
			return __xrtX509NameError(
				XERR_PROTOCOL,
				"domainComponent is not one RFC 4519 label", NULL
			);
		}
	}
	return true;
}



/* 比较采用 caseIgnoreIA5Match 的已知名称属性。 */
static xx509result __xrtX509NameIa5Equal(
	const xx509nameattr* pLeft,
	const xx509nameattr* pRight,
	bool bDomain
)
{
	if ( !__xrtX509NameIa5Valid(pLeft, bDomain) ||
		!__xrtX509NameIa5Valid(pRight, bDomain) ) {
		return X509_ERROR;
	}
	if ( pLeft->Value.Size != pRight->Value.Size ) {
		return X509_DONE;
	}
	for ( size_t i = 0; i < pLeft->Value.Size; i++ ) {
		uint8 iLeft = pLeft->Value.Data[i];
		uint8 iRight = pRight->Value.Data[i];

		if ( (iLeft >= 'A') && (iLeft <= 'Z') ) {
			iLeft = (uint8)(iLeft + ('a' - 'A'));
		}
		if ( (iRight >= 'A') && (iRight <= 'Z') ) {
			iRight = (uint8)(iRight + ('a' - 'A'));
		}
		if ( iLeft != iRight ) {
			return X509_DONE;
		}
	}
	return X509_VALUE;
}



/* 按属性类型选择 RFC 5280 要求的匹配规则。 */
static xx509result __xrtX509NameAttributeEqual(
	const xx509nameattr* pLeft,
	const xx509nameattr* pRight
)
{
	bool bLeftDirectory;
	bool bRightDirectory;

	if ( !__xrtX509NameBytesEqual(pLeft->Oid, pRight->Oid) ) {
		return X509_DONE;
	}
	if ( __xrtX509NameOid(
		pLeft->Oid, __xrtX509NameDomainComponentOid,
		sizeof(__xrtX509NameDomainComponentOid)
	) ) {
		return __xrtX509NameIa5Equal(pLeft, pRight, true);
	}
	if ( __xrtX509NameOid(
		pLeft->Oid, __xrtX509NameEmailOid,
		sizeof(__xrtX509NameEmailOid)
	) ) {
		return __xrtX509NameIa5Equal(pLeft, pRight, false);
	}
	bLeftDirectory = __xrtX509NameDirectoryTag(&pLeft->ValueTag);
	bRightDirectory = __xrtX509NameDirectoryTag(&pRight->ValueTag);
	if ( bLeftDirectory && bRightDirectory ) {
		return __xrtX509NamePreparedEqual(pLeft, pRight);
	}
	if ( bLeftDirectory != bRightDirectory ) {
		return X509_DONE;
	}
	if ( (pLeft->ValueTag.Class != pRight->ValueTag.Class) ||
		(pLeft->ValueTag.Number != pRight->ValueTag.Number) ||
		(pLeft->ValueTag.Constructed != pRight->ValueTag.Constructed) ) {
		return X509_DONE;
	}
	return __xrtX509NameBytesEqual(pLeft->Value, pRight->Value) ?
		X509_VALUE : X509_DONE;
}



/* 验证一个比较器能够理解的属性值。 */
static bool __xrtX509NameAttributeValid(
	const xx509nameattr* pAttribute
)
{
	if ( __xrtX509NameOid(
		pAttribute->Oid, __xrtX509NameDomainComponentOid,
		sizeof(__xrtX509NameDomainComponentOid)
	) ) {
		return __xrtX509NameIa5Valid(pAttribute, true);
	}
	if ( __xrtX509NameOid(
		pAttribute->Oid, __xrtX509NameEmailOid,
		sizeof(__xrtX509NameEmailOid)
	) ) {
		return __xrtX509NameIa5Valid(pAttribute, false);
	}
	if ( __xrtX509NameDirectoryTag(&pAttribute->ValueTag) ) {
		xrt_x509_name_buffer Buffer;
		bool bResult = __xrtX509NamePrepare(pAttribute, &Buffer);

		__xrtX509NameBufferUnit(&Buffer);
		return bResult;
	}
	return true;
}



/* 初始化短名称保持栈内的属性集合。 */
static void __xrtX509NameEntriesInit(xrt_x509_name_entries* pEntries)
{
	pEntries->Data = pEntries->Local;
	pEntries->Size = 0;
	pEntries->Capacity = XRT_X509_NAME_LOCAL_ATTRIBUTES;
	pEntries->RdnCount = 0;
}



/* 释放名称属性集合的可选堆存储。 */
static void __xrtX509NameEntriesUnit(xrt_x509_name_entries* pEntries)
{
	if ( pEntries->Data != pEntries->Local ) {
		xrtFree(pEntries->Data);
	}
}



/* 追加一个 Name 属性并按需扩展集合。 */
static bool __xrtX509NameEntriesAppend(
	xrt_x509_name_entries* pEntries,
	const xx509nameattr* pAttribute
)
{
	if ( pEntries->Size == pEntries->Capacity ) {
		size_t iCapacity;
		xrt_x509_name_entry* pData;

		if ( pEntries->Capacity >
			(SIZE_MAX / 2u / sizeof(xrt_x509_name_entry)) ) {
			return __xrtX509NameError(
				XERR_RANGE, "X.509 Name has too many attributes", NULL
			);
		}
		iCapacity = pEntries->Capacity * 2u;
		if ( pEntries->Data == pEntries->Local ) {
			pData = (xrt_x509_name_entry*)xrtMalloc(
				iCapacity * sizeof(xrt_x509_name_entry)
			);
			if ( pData != NULL ) {
				memcpy(
					pData, pEntries->Data,
					pEntries->Size * sizeof(xrt_x509_name_entry)
				);
			}
		} else {
			pData = (xrt_x509_name_entry*)xrtRealloc(
				pEntries->Data,
				iCapacity * sizeof(xrt_x509_name_entry)
			);
		}
		if ( pData == NULL ) {
			const xerror* pCause = xrtGetError();

			return __xrtX509NameError(
				XERR_MEMORY, "X.509 Name attribute allocation failed", pCause
			);
		}
		pEntries->Data = pData;
		pEntries->Capacity = iCapacity;
	}
	pEntries->Data[pEntries->Size].Attribute = *pAttribute;
	pEntries->Data[pEntries->Size].Matched = false;
	pEntries->Size++;
	if ( pEntries->RdnCount <= pAttribute->Rdn ) {
		pEntries->RdnCount = pAttribute->Rdn + 1u;
	}
	return true;
}



/* 解析完整 Name 并收集借用属性。 */
static bool __xrtX509NameCollect(
	xbytesview Name,
	xrt_x509_name_entries* pEntries
)
{
	xx509namecursor Cursor;

	__xrtX509NameEntriesInit(pEntries);
	if ( !xrtX509NameInit(Name, &Cursor) ) {
		return false;
	}
	while ( true ) {
		xx509nameattr Attribute;
		xx509result Result = xrtX509NameRead(&Cursor, &Attribute);

		if ( Result == X509_DONE ) {
			return true;
		}
		if ( (Result == X509_ERROR) ||
			!__xrtX509NameEntriesAppend(pEntries, &Attribute) ) {
			return false;
		}
	}
}



/* 计算指定 RDN 中的属性数量。 */
static size_t __xrtX509NameRdnSize(
	const xrt_x509_name_entries* pEntries,
	size_t iRdn
)
{
	size_t iCount = 0;

	for ( size_t i = 0; i < pEntries->Size; i++ ) {
		if ( pEntries->Data[i].Attribute.Rdn == iRdn ) {
			iCount++;
		}
	}
	return iCount;
}



/* 验证名称集合中全部可识别的字符串属性。 */
static bool __xrtX509NameEntriesValid(
	const xrt_x509_name_entries* pEntries
)
{
	for ( size_t i = 0; i < pEntries->Size; i++ ) {
		if ( !__xrtX509NameAttributeValid(
			&pEntries->Data[i].Attribute
		) ) {
			return false;
		}
	}
	return true;
}



/* 按无序属性集合比较前 iRdnCount 个同序 RDN。 */
static xx509result __xrtX509NameEntriesEqual(
	const xrt_x509_name_entries* pLeft,
	xrt_x509_name_entries* pRight,
	size_t iRdnCount
)
{
	for ( size_t i = 0; i < pRight->Size; i++ ) {
		pRight->Data[i].Matched = false;
	}
	for ( size_t iRdn = 0; iRdn < iRdnCount; iRdn++ ) {
		if ( __xrtX509NameRdnSize(pLeft, iRdn) !=
			__xrtX509NameRdnSize(pRight, iRdn) ) {
			return X509_DONE;
		}
		for ( size_t i = 0; i < pLeft->Size; i++ ) {
			bool bFound = false;

			if ( pLeft->Data[i].Attribute.Rdn != iRdn ) {
				continue;
			}
			for ( size_t j = 0; j < pRight->Size; j++ ) {
				xx509result Result;

				if ( pRight->Data[j].Matched ||
					(pRight->Data[j].Attribute.Rdn != iRdn) ) {
					continue;
				}
				Result = __xrtX509NameAttributeEqual(
					&pLeft->Data[i].Attribute,
					&pRight->Data[j].Attribute
				);
				if ( Result == X509_ERROR ) {
					return X509_ERROR;
				}
				if ( Result == X509_VALUE ) {
					pRight->Data[j].Matched = true;
					bFound = true;
					break;
				}
			}
			if ( !bFound ) {
				return X509_DONE;
			}
		}
	}
	return X509_VALUE;
}



/* 按 RFC 5280 7.1 比较两个完整 Distinguished Name。 */
XRT_API xx509result xrtX509NameEqual(xbytesview Left, xbytesview Right)
{
	xrt_x509_name_entries LeftEntries;
	xrt_x509_name_entries RightEntries;
	xx509result Result;

	if ( ((Left.Data == NULL) && (Left.Size != 0)) ||
		((Right.Data == NULL) && (Right.Size != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return X509_ERROR;
	}
	if ( !__xrtX509NameCollect(Left, &LeftEntries) ) {
		__xrtX509NameEntriesUnit(&LeftEntries);
		return X509_ERROR;
	}
	if ( !__xrtX509NameCollect(Right, &RightEntries) ) {
		__xrtX509NameEntriesUnit(&LeftEntries);
		__xrtX509NameEntriesUnit(&RightEntries);
		return X509_ERROR;
	}
	if ( LeftEntries.RdnCount != RightEntries.RdnCount ) {
		Result = X509_DONE;
	} else {
		Result = __xrtX509NameEntriesEqual(
			&LeftEntries, &RightEntries, LeftEntries.RdnCount
		);
	}
	if ( (Result == X509_DONE) &&
		(!__xrtX509NameEntriesValid(&LeftEntries) ||
		 !__xrtX509NameEntriesValid(&RightEntries)) ) {
		Result = X509_ERROR;
	}
	__xrtX509NameEntriesUnit(&LeftEntries);
	__xrtX509NameEntriesUnit(&RightEntries);
	return Result;
}



/* 判断完整名称是否位于指定 Distinguished Name 子树内。 */
XRT_API xx509result xrtX509NameWithin(xbytesview Name, xbytesview Base)
{
	xrt_x509_name_entries NameEntries;
	xrt_x509_name_entries BaseEntries;
	xx509result Result;

	if ( ((Name.Data == NULL) && (Name.Size != 0)) ||
		((Base.Data == NULL) && (Base.Size != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return X509_ERROR;
	}
	if ( !__xrtX509NameCollect(Name, &NameEntries) ) {
		__xrtX509NameEntriesUnit(&NameEntries);
		return X509_ERROR;
	}
	if ( !__xrtX509NameCollect(Base, &BaseEntries) ) {
		__xrtX509NameEntriesUnit(&NameEntries);
		__xrtX509NameEntriesUnit(&BaseEntries);
		return X509_ERROR;
	}
	if ( NameEntries.RdnCount < BaseEntries.RdnCount ) {
		Result = X509_DONE;
	} else {
		Result = __xrtX509NameEntriesEqual(
			&BaseEntries, &NameEntries, BaseEntries.RdnCount
		);
	}
	if ( (Result == X509_DONE) &&
		(!__xrtX509NameEntriesValid(&NameEntries) ||
		 !__xrtX509NameEntriesValid(&BaseEntries)) ) {
		Result = X509_ERROR;
	}
	__xrtX509NameEntriesUnit(&NameEntries);
	__xrtX509NameEntriesUnit(&BaseEntries);
	return Result;
}

#endif
