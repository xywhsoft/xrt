#include "../internal/xrt_http.h"

#include <xrt/http_via.h>



#if defined(XRT_FEATURE_HTTP_VIA_WRITE)

/* 判断已经解码的注释字节能否被规范转义。 */
static bool __xrtHttpViaCommentValueByte(uint8 iByte)
{
	return (iByte == (uint8)'\t') ||
		(iByte == (uint8)' ') ||
		((iByte >= UINT8_C(0x21)) &&
		 (iByte <= UINT8_C(0x7E))) ||
		(iByte >= UINT8_C(0x80));
}



/* 判断注释字节是否必须使用 quoted-pair。 */
static bool __xrtHttpViaCommentEscape(uint8 iByte)
{
	return (iByte == (uint8)'(') ||
		(iByte == (uint8)')') ||
		(iByte == (uint8)'\\');
}



/* 验证一个 Via 写入值并计算规范线路长度。 */
static bool __xrtHttpViaValueMeasure(
	const xhttpviavalue* pInput,
	xhttpviavalue* pVia,
	size_t* pSize
)
{
	uint32 iKnown = XHTTP_VIA_HAS_PROTOCOL_NAME |
		XHTTP_VIA_HAS_PORT |
		XHTTP_VIA_HAS_COMMENT;
	size_t iRequired = 0;
	size_t i;

	memcpy(pVia, pInput, sizeof(*pVia));
	if ( !__xrtHttpViewValid(pVia->ProtocolName) ||
		!__xrtHttpViewValid(pVia->ProtocolVersion) ||
		!__xrtHttpViewValid(pVia->Pseudonym) ||
		!__xrtHttpViewValid(pVia->Port) ||
		!__xrtHttpViewValid(pVia->Comment) ||
		((pVia->Flags & ~iKnown) != 0) ||
		!xrtHttpTokenValid(pVia->ProtocolVersion) ||
		!xrtHttpTokenValid(pVia->Pseudonym) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( ((pVia->Flags & XHTTP_VIA_HAS_PROTOCOL_NAME) != 0) !=
		(pVia->ProtocolName.Size != 0) ||
		(((pVia->Flags & XHTTP_VIA_HAS_PORT) == 0) &&
		 (pVia->Port.Size != 0)) ||
		(((pVia->Flags & XHTTP_VIA_HAS_COMMENT) == 0) &&
		 (pVia->Comment.Size != 0)) ) {
		__xrtErrorSetValue();
		return false;
	}
	if ( ((pVia->Flags & XHTTP_VIA_HAS_PROTOCOL_NAME) != 0) &&
		!xrtHttpTokenValid(pVia->ProtocolName) ) {
		__xrtErrorSetValue();
		return false;
	}
	if ( (pVia->Flags & XHTTP_VIA_HAS_PORT) != 0 ) {
		for ( i = 0; i < pVia->Port.Size; i++ ) {
			if ( (pVia->Port.Data[i] < '0') ||
				(pVia->Port.Data[i] > '9') ) {
				__xrtErrorSetValue();
				return false;
			}
		}
	}
	if ( (pVia->Flags & XHTTP_VIA_HAS_COMMENT) != 0 ) {
		for ( i = 0; i < pVia->Comment.Size; i++ ) {
			uint8 iByte = (uint8)pVia->Comment.Data[i];

			if ( !__xrtHttpViaCommentValueByte(iByte) ) {
				__xrtErrorSetValue();
				return false;
			}
			if ( !__xrtHttpSizeAdd(
				&iRequired,
				__xrtHttpViaCommentEscape(iByte) ? 2u : 1u
			) ) {
				return false;
			}
		}
		if ( !__xrtHttpSizeAdd(&iRequired, 3u) ) {
			return false;
		}
	}
	if ( !__xrtHttpSizeAdd(
		&iRequired, pVia->ProtocolVersion.Size
	) || !__xrtHttpSizeAdd(
		&iRequired, pVia->Pseudonym.Size
	) || !__xrtHttpSizeAdd(
		&iRequired, 1u
	) ) {
		return false;
	}
	if ( (pVia->Flags & XHTTP_VIA_HAS_PROTOCOL_NAME) != 0 ) {
		if ( !__xrtHttpSizeAdd(
			&iRequired, pVia->ProtocolName.Size
		) || !__xrtHttpSizeAdd(&iRequired, 1u) ) {
			return false;
		}
	}
	if ( (pVia->Flags & XHTTP_VIA_HAS_PORT) != 0 ) {
		if ( !__xrtHttpSizeAdd(
			&iRequired, pVia->Port.Size
		) || !__xrtHttpSizeAdd(&iRequired, 1u) ) {
			return false;
		}
	}
	*pSize = iRequired;
	return true;
}



/* 判断内存是否覆盖写入描述符或任一借用输入。 */
static bool __xrtHttpViaValuesOverlap(
	const xhttpviavalue* pVia,
	size_t iCount,
	const void* pMemory,
	size_t iSize
)
{
	xhttpviavalue Via;
	size_t i;

	if ( __xrtRangesOverlap(
		pVia, iCount * sizeof(*pVia), pMemory, iSize
	) ) {
		return true;
	}
	for ( i = 0; i < iCount; i++ ) {
		memcpy(&Via, pVia + i, sizeof(Via));
		if ( __xrtRangesOverlap(
			Via.ProtocolName.Data, Via.ProtocolName.Size,
			pMemory, iSize
		) || __xrtRangesOverlap(
			Via.ProtocolVersion.Data, Via.ProtocolVersion.Size,
			pMemory, iSize
		) || __xrtRangesOverlap(
			Via.Pseudonym.Data, Via.Pseudonym.Size,
			pMemory, iSize
		) || __xrtRangesOverlap(
			Via.Port.Data, Via.Port.Size,
			pMemory, iSize
		) || __xrtRangesOverlap(
			Via.Comment.Data, Via.Comment.Size,
			pMemory, iSize
		) ) {
			return true;
		}
	}
	return false;
}



/* 在容量已经确认后写出一个 Via 元素。 */
static size_t __xrtHttpViaValueWrite(
	const xhttpviavalue* pVia,
	bytes pOutput,
	size_t iPosition
)
{
	size_t i;

	if ( (pVia->Flags & XHTTP_VIA_HAS_PROTOCOL_NAME) != 0 ) {
		memcpy(
			pOutput + iPosition,
			pVia->ProtocolName.Data,
			pVia->ProtocolName.Size
		);
		iPosition += pVia->ProtocolName.Size;
		pOutput[iPosition++] = (uint8)'/';
	}
	memcpy(
		pOutput + iPosition,
		pVia->ProtocolVersion.Data,
		pVia->ProtocolVersion.Size
	);
	iPosition += pVia->ProtocolVersion.Size;
	pOutput[iPosition++] = (uint8)' ';
	memcpy(
		pOutput + iPosition,
		pVia->Pseudonym.Data,
		pVia->Pseudonym.Size
	);
	iPosition += pVia->Pseudonym.Size;
	if ( (pVia->Flags & XHTTP_VIA_HAS_PORT) != 0 ) {
		pOutput[iPosition++] = (uint8)':';
		memcpy(
			pOutput + iPosition,
			pVia->Port.Data,
			pVia->Port.Size
		);
		iPosition += pVia->Port.Size;
	}
	if ( (pVia->Flags & XHTTP_VIA_HAS_COMMENT) != 0 ) {
		pOutput[iPosition++] = (uint8)' ';
		pOutput[iPosition++] = (uint8)'(';
		for ( i = 0; i < pVia->Comment.Size; i++ ) {
			uint8 iByte = (uint8)pVia->Comment.Data[i];

			if ( __xrtHttpViaCommentEscape(iByte) ) {
				pOutput[iPosition++] = (uint8)'\\';
			}
			pOutput[iPosition++] = iByte;
		}
		pOutput[iPosition++] = (uint8)')';
	}
	return iPosition;
}



/* 规范写出一个或多个 Via 元素。 */
XRT_API bool xrtHttpViaWrite(
	const xhttpviavalue* pVia,
	size_t iCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xhttpviavalue Via;
	size_t iElement;
	size_t iRequired = 0;
	size_t iPosition = 0;
	size_t i;
	bytes pBytes = (bytes)pOutput;

	if ( (iCount == 0) ||
		(iCount > (SIZE_MAX / sizeof(*pVia))) ||
		!__xrtRangeValid(
			pVia, iCount * sizeof(*pVia)
		) || !__xrtRangeValid(pSize, sizeof(iRequired)) ||
		((pOutput == NULL) && (iCapacity != 0)) ||
		((pOutput != NULL) &&
		 !__xrtRangeValid(pOutput, iCapacity)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( __xrtHttpViaValuesOverlap(
		pVia, iCount, pSize, sizeof(iRequired)
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	for ( i = 0; i < iCount; i++ ) {
		if ( !__xrtHttpViaValueMeasure(
			pVia + i, &Via, &iElement
		) || ((i != 0) &&
			!__xrtHttpSizeAdd(&iRequired, 2u)) ||
			!__xrtHttpSizeAdd(&iRequired, iElement) ) {
			return false;
		}
	}
	if ( (pOutput != NULL) &&
		(__xrtHttpViaValuesOverlap(
			pVia, iCount, pOutput, iRequired
		 ) || __xrtRangesOverlap(
			pOutput, iRequired, pSize, sizeof(iRequired)
		 )) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(pSize, &iRequired, sizeof(iRequired));
	if ( pOutput == NULL ) {
		return true;
	}
	if ( iCapacity < iRequired ) {
		__xrtErrorSetRange();
		return false;
	}
	for ( i = 0; i < iCount; i++ ) {
		memcpy(&Via, pVia + i, sizeof(Via));
		if ( i != 0 ) {
			pBytes[iPosition++] = (uint8)',';
			pBytes[iPosition++] = (uint8)' ';
		}
		iPosition = __xrtHttpViaValueWrite(
			&Via, pBytes, iPosition
		);
	}
	return true;
}



/* 规范写出单个 Via 元素。 */
XRT_API bool xrtHttpViaElementWrite(
	const xhttpviavalue* pVia,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	return xrtHttpViaWrite(
		pVia, 1u, pOutput, iCapacity, pSize
	);
}



/* 构建零结尾 Via 字段值。 */
XRT_API str xrtHttpViaBuild(
	const xhttpviavalue* pVia,
	size_t iCount,
	size_t* pSize
)
{
	size_t iRequired;
	str sOutput;

	if ( (pSize != NULL) &&
		(!__xrtRangeValid(pSize, sizeof(*pSize)) ||
		 (iCount <= (SIZE_MAX / sizeof(*pVia)) &&
		  __xrtRangeValid(
			pVia, iCount * sizeof(*pVia)
		  ) && __xrtHttpViaValuesOverlap(
			pVia, iCount, pSize, sizeof(*pSize)
		  ))) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( !xrtHttpViaWrite(
		pVia, iCount, NULL, 0, &iRequired
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
	if ( !xrtHttpViaWrite(
		pVia, iCount, sOutput, iRequired, &iRequired
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
