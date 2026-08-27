#include "../internal/xrt_http_cors_client.h"



#if defined(XRT_FEATURE_HTTP_CORS_CLIENT_WRITE)

/* 安全累加预检字段线缆长度。 */
static bool __xrtHttpCorsClientSizeAdd(
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



/* 按小写 ASCII 字节序比较两个合法字段名。 */
static int __xrtHttpCorsClientNameCompare(
	xstrview Left,
	xstrview Right
)
{
	size_t iSize = Left.Size < Right.Size ?
		Left.Size : Right.Size;
	size_t i;

	for ( i = 0; i < iSize; i++ ) {
		uint8 iLeft = __xrtHttpAsciiLower(
			(uint8)Left.Data[i]
		);
		uint8 iRight = __xrtHttpAsciiLower(
			(uint8)Right.Data[i]
		);

		if ( iLeft < iRight ) {
			return -1;
		}
		if ( iLeft > iRight ) {
			return 1;
		}
	}
	if ( Left.Size < Right.Size ) {
		return -1;
	}
	if ( Left.Size > Right.Size ) {
		return 1;
	}
	return 0;
}



/* 无分配地选择排序集合中的下一个字段名。 */
static bool __xrtHttpCorsClientSortedNameNext(
	const xhttpfield* pFields,
	size_t iCount,
	const xrt_http_cors_request_info* pInfo,
	xstrview Previous,
	bool bHasPrevious,
	xstrview* pSelected
)
{
	xhttpfield Field;
	xstrview Name;
	xstrview Selected = { NULL, 0 };
	size_t i;
	bool bFound = false;

	for ( i = 0; i < iCount; i++ ) {
		if ( !__xrtHttpCorsRequestFieldUnsafe(
			pFields, iCount, pInfo, i
		) ) {
			continue;
		}
		__xrtHttpFieldLoad(pFields, i, &Field);
		Name = Field.Name;
		if ( bHasPrevious &&
			(__xrtHttpCorsClientNameCompare(
				Name, Previous
			) <= 0) ) {
			continue;
		}
		if ( !bFound ||
			(__xrtHttpCorsClientNameCompare(
				Name, Selected
			) < 0) ) {
			Selected = Name;
			bFound = true;
		}
	}
	*pSelected = Selected;
	return bFound;
}



/* 判断输出是否覆盖字段数组、借用视图或长度输出。 */
static bool __xrtHttpCorsClientWriteOverlap(
	const xhttpfield* pFields,
	size_t iCount,
	const size_t* pSize,
	const void* pOutput,
	size_t iOutputSize
)
{
	return __xrtHttpFieldArrayOverlap(
		pFields, iCount, pOutput, iOutputSize
	) || __xrtRangesOverlap(
		pSize, sizeof(*pSize), pOutput, iOutputSize
	);
}



/* 写出 Fetch 要求的排序小写非安全字段名集合。 */
XRT_API bool xrtHttpCorsPreflightHeaderNamesWrite(
	const xhttpfield* pFields,
	size_t iCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xrt_http_cors_request_info Info;
	xstrview Name;
	xstrview Previous = { NULL, 0 };
	uint8* pWrite = (uint8*)pOutput;
	size_t iRequired = 0;
	size_t iOffset = 0;
	size_t i;
	size_t j;

	if ( !__xrtRangeValid(pSize, sizeof(iRequired)) ||
		((pOutput == NULL) && (iCapacity != 0)) ||
		((pOutput != NULL) &&
		 (!__xrtRangeValid(pOutput, iCapacity) ||
		  __xrtRangesOverlap(
			pOutput, iCapacity, pSize, sizeof(iRequired)
		  ))) ||
		!__xrtHttpFieldArrayValid(pFields, iCount) ||
		__xrtHttpFieldArrayOverlap(
			pFields, iCount, pSize, sizeof(iRequired)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(pSize, &iRequired, sizeof(iRequired));
	if ( !__xrtHttpCorsRequestInspect(
		pFields, iCount, &Info
	) ) {
		return false;
	}
	while ( __xrtHttpCorsUnsafeNameNext(
		pFields, iCount, &Info, &iOffset, &Name
	) == XHTTP_NEXT_ITEM ) {
		if ( ((iRequired != 0) &&
			 !__xrtHttpCorsClientSizeAdd(
				&iRequired, 1u
			 )) || !__xrtHttpCorsClientSizeAdd(
			&iRequired, Name.Size
		) ) {
			return false;
		}
	}
	memcpy(pSize, &iRequired, sizeof(iRequired));
	if ( pOutput == NULL ) {
		return true;
	}
	if ( __xrtHttpCorsClientWriteOverlap(
		pFields, iCount, pSize, pOutput, iRequired
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( iCapacity < iRequired ) {
		__xrtErrorSetRange();
		return false;
	}
	iOffset = 0;
	for ( i = 0; i < Info.UnsafeCount; i++ ) {
		if ( !__xrtHttpCorsClientSortedNameNext(
			pFields,
			iCount,
			&Info,
			Previous,
			i != 0,
			&Name
		) ) {
			__xrtErrorSetInternal();
			return false;
		}
		if ( i != 0 ) {
			pWrite[iOffset++] = (uint8)',';
		}
		for ( j = 0; j < Name.Size; j++ ) {
			pWrite[iOffset++] = __xrtHttpAsciiLower(
				(uint8)Name.Data[j]
			);
		}
		Previous = Name;
	}
	return true;
}



/* 直接写出预检请求的 CORS 字段行。 */
XRT_API bool xrtHttpCorsPreflightFieldsWrite(
	const xhttporigin* pOrigin,
	xstrview Method,
	const xhttpfield* pFields,
	size_t iCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	static const char OriginName[] = "Origin: ";
	static const char MethodName[] =
		"Access-Control-Request-Method: ";
	static const char HeaderName[] =
		"Access-Control-Request-Headers: ";
	xhttporigin Origin;
	uint8* pWrite = (uint8*)pOutput;
	size_t iOriginSize;
	size_t iHeaderSize;
	size_t iRequired = 0;
	size_t iWritten;
	size_t iOffset = 0;

	if ( !__xrtRangeValid(pOrigin, sizeof(Origin)) ||
		!__xrtHttpViewValid(Method) ||
		!__xrtRangeValid(pSize, sizeof(iRequired)) ||
		((pOutput == NULL) && (iCapacity != 0)) ||
		((pOutput != NULL) &&
		 (!__xrtRangeValid(pOutput, iCapacity) ||
		  __xrtRangesOverlap(
			pOutput, iCapacity, pSize, sizeof(iRequired)
		  ))) ||
		!__xrtHttpFieldArrayValid(pFields, iCount) ||
		__xrtRangesOverlap(
			pOrigin, sizeof(Origin),
			pSize, sizeof(iRequired)
		) || __xrtRangesOverlap(
			Method.Data, Method.Size,
			pSize, sizeof(iRequired)
		) || __xrtHttpFieldArrayOverlap(
			pFields, iCount, pSize, sizeof(iRequired)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(&Origin, pOrigin, sizeof(Origin));
	memcpy(pSize, &iRequired, sizeof(iRequired));
	if ( !__xrtHttpOriginValueValid(&Origin) ||
		__xrtHttpOriginOverlap(
			&Origin, pSize, sizeof(iRequired)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtHttpTokenValid(Method) ) {
		__xrtErrorSetValue();
		return false;
	}
	if ( !xrtHttpOriginWrite(
		&Origin, NULL, 0, &iOriginSize
	) || !xrtHttpCorsPreflightHeaderNamesWrite(
		pFields, iCount, NULL, 0, &iHeaderSize
	) || !__xrtHttpCorsClientSizeAdd(
		&iRequired, sizeof(OriginName) - 1u
	) || !__xrtHttpCorsClientSizeAdd(
		&iRequired, iOriginSize
	) || !__xrtHttpCorsClientSizeAdd(
		&iRequired, 2u
	) || !__xrtHttpCorsClientSizeAdd(
		&iRequired, sizeof(MethodName) - 1u
	) || !__xrtHttpCorsClientSizeAdd(
		&iRequired, Method.Size
	) || !__xrtHttpCorsClientSizeAdd(
		&iRequired, 2u
	) || ((iHeaderSize != 0) &&
		(!__xrtHttpCorsClientSizeAdd(
			&iRequired, sizeof(HeaderName) - 1u
		) || !__xrtHttpCorsClientSizeAdd(
			&iRequired, iHeaderSize
		) || !__xrtHttpCorsClientSizeAdd(
			&iRequired, 2u
		))) ) {
		return false;
	}
	memcpy(pSize, &iRequired, sizeof(iRequired));
	if ( pOutput == NULL ) {
		return true;
	}
	if ( __xrtRangesOverlap(
		pOrigin, sizeof(Origin), pOutput, iRequired
	) || __xrtHttpOriginOverlap(
		&Origin, pOutput, iRequired
	) || __xrtRangesOverlap(
		Method.Data, Method.Size, pOutput, iRequired
	) || __xrtHttpCorsClientWriteOverlap(
		pFields, iCount, pSize, pOutput, iRequired
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( iCapacity < iRequired ) {
		__xrtErrorSetRange();
		return false;
	}
	memcpy(pWrite + iOffset, OriginName, sizeof(OriginName) - 1u);
	iOffset += sizeof(OriginName) - 1u;
	if ( !xrtHttpOriginWrite(
		&Origin,
		pWrite + iOffset,
		iOriginSize,
		&iWritten
	) ) {
		return false;
	}
	iOffset += iWritten;
	memcpy(pWrite + iOffset, "\r\n", 2u);
	iOffset += 2u;
	memcpy(pWrite + iOffset, MethodName, sizeof(MethodName) - 1u);
	iOffset += sizeof(MethodName) - 1u;
	memcpy(pWrite + iOffset, Method.Data, Method.Size);
	iOffset += Method.Size;
	memcpy(pWrite + iOffset, "\r\n", 2u);
	iOffset += 2u;
	if ( iHeaderSize != 0 ) {
		memcpy(
			pWrite + iOffset,
			HeaderName,
			sizeof(HeaderName) - 1u
		);
		iOffset += sizeof(HeaderName) - 1u;
		if ( !xrtHttpCorsPreflightHeaderNamesWrite(
			pFields,
			iCount,
			pWrite + iOffset,
			iHeaderSize,
			&iWritten
		) ) {
			return false;
		}
		iOffset += iWritten;
		memcpy(pWrite + iOffset, "\r\n", 2u);
		iOffset += 2u;
	}
	return true;
}

#endif
