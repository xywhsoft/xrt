#include "../internal/xrt_http.h"
#include "../internal/xrt_http_structured_status.h"



#if defined(XRT_FEATURE_HTTP_STRUCTURED)

/* 检查回调并把协议成员错误统一映射为值错误。 */
static bool __xrtHttpStructuredStatusRead(
	xrt_http_structured_status_read pRead,
	const xhttpstructuredmember* pMember,
	void* pOutput
)
{
	if ( pRead(pMember, pOutput) ) {
		return true;
	}
	if ( xrtGetError() == NULL ) {
		__xrtErrorSetValue();
	}
	return false;
}



/* 完整验证 List，并保留首成员及其后继偏移。 */
static bool __xrtHttpStructuredStatusListInspect(
	xstrview Value,
	xrt_http_structured_status_read pRead,
	xhttpstructuredmember* pFirst,
	size_t* pFirstOffset,
	bool* pFound
)
{
	xhttpstructuredmember Member;
	xhttpnext Next;
	size_t iOffset = 0;

	*pFound = false;
	for ( ;; ) {
		Next = xrtHttpStructuredListNext(Value, &iOffset, &Member);
		if ( Next == XHTTP_NEXT_ERROR ) {
			return false;
		}
		if ( Next == XHTTP_NEXT_END ) {
			return true;
		}
		if ( !*pFound ) {
			*pFirst = Member;
			*pFirstOffset = iOffset;
			*pFound = true;
		}
		if ( !__xrtHttpStructuredStatusRead(
			pRead, &Member, NULL
		) ) {
			return false;
		}
	}
}



/* 完整验证重复字段，并保留首成员及其后继游标。 */
static bool __xrtHttpStructuredStatusFieldsInspect(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name,
	xrt_http_structured_status_read pRead,
	xhttpstructuredmember* pFirst,
	xhttpstructuredfieldcursor* pFirstCursor,
	xhttpstructuredfieldcursor* pEndCursor,
	bool* pFound
)
{
	xhttpstructuredfieldcursor Cursor;
	xhttpstructuredmember Member;
	xhttpnext Next;

	*pFound = false;
	xrtHttpStructuredFieldCursorInit(&Cursor);
	for ( ;; ) {
		Next = xrtHttpStructuredListFieldNext(
			pFields, iCount, Name, &Cursor, &Member
		);
		if ( Next == XHTTP_NEXT_ERROR ) {
			return false;
		}
		if ( Next == XHTTP_NEXT_END ) {
			*pEndCursor = Cursor;
			return true;
		}
		if ( !*pFound ) {
			*pFirst = Member;
			*pFirstCursor = Cursor;
			*pFound = true;
		}
		if ( !__xrtHttpStructuredStatusRead(
			pRead, &Member, NULL
		) ) {
			return false;
		}
	}
}



/* 初始化类型化状态单值游标。 */
void __xrtHttpStructuredStatusCursorInit(
	void* pCursor,
	size_t iCursorSize
)
{
	xrt_http_structured_status_cursor Cursor;

	if ( (iCursorSize != sizeof(Cursor)) ||
		!__xrtRangeValid(pCursor, iCursorSize) ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memset(&Cursor, 0, sizeof(Cursor));
	memcpy(pCursor, &Cursor, sizeof(Cursor));
}



/* 初始化类型化状态重复字段游标。 */
void __xrtHttpStructuredStatusFieldCursorInit(
	void* pCursor,
	size_t iCursorSize
)
{
	xrt_http_structured_status_field_cursor Cursor;

	if ( (iCursorSize != sizeof(Cursor)) ||
		!__xrtRangeValid(pCursor, iCursorSize) ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memset(&Cursor, 0, sizeof(Cursor));
	memcpy(pCursor, &Cursor, sizeof(Cursor));
}



/* 公开给类型化模块的完整 List 验证。 */
bool __xrtHttpStructuredStatusValid(
	xstrview Value,
	xrt_http_structured_status_read pRead
)
{
	xhttpstructuredmember First;
	size_t iFirstOffset;
	bool bFound;

	if ( !__xrtHttpViewValid(Value) || (pRead == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtHttpStructuredStatusListInspect(
		Value, pRead, &First, &iFirstOffset, &bFound
	);
}



/* 迭代一个已经完成全部语义预校验的状态 List。 */
xhttpnext __xrtHttpStructuredStatusNext(
	xstrview Value,
	void* pCursor,
	size_t iCursorSize,
	void* pOutput,
	size_t iOutputSize,
	xrt_http_structured_status_read pRead
)
{
	xrt_http_structured_status_cursor Cursor;
	xhttpstructuredmember First;
	xhttpstructuredmember Member;
	xhttpnext Next;
	size_t iFirstOffset;
	bool bFound;

	if ( (iCursorSize != sizeof(Cursor)) ||
		!__xrtHttpViewValid(Value) || (pRead == NULL) ||
		!__xrtRangeValid(pCursor, iCursorSize) ||
		!__xrtRangeValid(pOutput, iOutputSize) ||
		__xrtRangesOverlap(
			Value.Data, Value.Size, pCursor, iCursorSize
		) || __xrtRangesOverlap(
			Value.Data, Value.Size, pOutput, iOutputSize
		) || __xrtRangesOverlap(
			pCursor, iCursorSize, pOutput, iOutputSize
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	memcpy(&Cursor, pCursor, sizeof(Cursor));
	if ( (Cursor.Validated > 1u) ||
		(Cursor.Offset > Value.Size) ||
		((Cursor.Validated == 0) &&
		 ((Cursor.Source != NULL) ||
		  (Cursor.SourceSize != 0) ||
		  (Cursor.Offset != 0))) ||
		((Cursor.Validated != 0) &&
		 ((Cursor.Source != (const void*)Value.Data) ||
		  (Cursor.SourceSize != Value.Size))) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	if ( Cursor.Validated == 0 ) {
		if ( !__xrtHttpStructuredStatusListInspect(
			Value, pRead, &First, &iFirstOffset, &bFound
		) ) {
			return XHTTP_NEXT_ERROR;
		}
		Cursor.Source = Value.Data;
		Cursor.SourceSize = Value.Size;
		Cursor.Validated = 1u;
		if ( !bFound ) {
			Cursor.Offset = Value.Size;
			memset(pOutput, 0, iOutputSize);
			memcpy(pCursor, &Cursor, sizeof(Cursor));
			return XHTTP_NEXT_END;
		}
		if ( !__xrtHttpStructuredStatusRead(
			pRead, &First, pOutput
		) ) {
			return XHTTP_NEXT_ERROR;
		}
		Cursor.Offset = iFirstOffset;
		memcpy(pCursor, &Cursor, sizeof(Cursor));
		return XHTTP_NEXT_ITEM;
	}
	Next = xrtHttpStructuredListNext(
		Value, &Cursor.Offset, &Member
	);
	if ( Next == XHTTP_NEXT_ERROR ) {
		return Next;
	}
	if ( Next == XHTTP_NEXT_END ) {
		memset(pOutput, 0, iOutputSize);
		memcpy(pCursor, &Cursor, sizeof(Cursor));
		return Next;
	}
	if ( !__xrtHttpStructuredStatusRead(
		pRead, &Member, pOutput
	) ) {
		return XHTTP_NEXT_ERROR;
	}
	memcpy(pCursor, &Cursor, sizeof(Cursor));
	return XHTTP_NEXT_ITEM;
}



/* 跨重复字段行迭代一个类型化状态 List。 */
xhttpnext __xrtHttpStructuredStatusFieldNext(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name,
	void* pCursor,
	size_t iCursorSize,
	void* pOutput,
	size_t iOutputSize,
	xrt_http_structured_status_read pRead
)
{
	xrt_http_structured_status_field_cursor Cursor;
	xhttpstructuredfieldcursor EndCursor;
	xhttpstructuredfieldcursor FirstCursor;
	xhttpstructuredmember First;
	xhttpstructuredmember Member;
	xhttpnext Next;
	bool bFound;

	if ( (iCursorSize != sizeof(Cursor)) ||
		!__xrtHttpFieldArrayValid(pFields, iCount) ||
		!__xrtHttpViewValid(Name) || (Name.Size == 0) ||
		!xrtHttpTokenValid(Name) || (pRead == NULL) ||
		!__xrtRangeValid(pCursor, iCursorSize) ||
		!__xrtRangeValid(pOutput, iOutputSize) ||
		__xrtHttpFieldArrayOverlap(
			pFields, iCount, pCursor, iCursorSize
		) || __xrtHttpFieldArrayOverlap(
			pFields, iCount, pOutput, iOutputSize
		) || __xrtRangesOverlap(
			Name.Data, Name.Size, pCursor, iCursorSize
		) || __xrtRangesOverlap(
			Name.Data, Name.Size, pOutput, iOutputSize
		) || __xrtRangesOverlap(
			pCursor, iCursorSize, pOutput, iOutputSize
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	memcpy(&Cursor, pCursor, sizeof(Cursor));
	if ( (Cursor.Validated > 1u) ||
		((Cursor.Validated == 0) &&
		 ((Cursor.Structured.Field != 0) ||
		  (Cursor.Structured.Offset != 0) ||
		  (Cursor.Structured.State != 0))) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	if ( Cursor.Validated == 0 ) {
		if ( !__xrtHttpStructuredStatusFieldsInspect(
			pFields, iCount, Name, pRead,
			&First, &FirstCursor, &EndCursor, &bFound
		) ) {
			return XHTTP_NEXT_ERROR;
		}
		Cursor.Validated = 1u;
		if ( !bFound ) {
			Cursor.Structured = EndCursor;
			memset(pOutput, 0, iOutputSize);
			memcpy(pCursor, &Cursor, sizeof(Cursor));
			return XHTTP_NEXT_END;
		}
		if ( !__xrtHttpStructuredStatusRead(
			pRead, &First, pOutput
		) ) {
			return XHTTP_NEXT_ERROR;
		}
		Cursor.Structured = FirstCursor;
		memcpy(pCursor, &Cursor, sizeof(Cursor));
		return XHTTP_NEXT_ITEM;
	}
	Next = xrtHttpStructuredListFieldNext(
		pFields, iCount, Name, &Cursor.Structured, &Member
	);
	if ( Next == XHTTP_NEXT_ERROR ) {
		return Next;
	}
	if ( Next == XHTTP_NEXT_END ) {
		memset(pOutput, 0, iOutputSize);
		memcpy(pCursor, &Cursor, sizeof(Cursor));
		return Next;
	}
	if ( !__xrtHttpStructuredStatusRead(
		pRead, &Member, pOutput
	) ) {
		return XHTTP_NEXT_ERROR;
	}
	memcpy(pCursor, &Cursor, sizeof(Cursor));
	return XHTTP_NEXT_ITEM;
}

#endif
