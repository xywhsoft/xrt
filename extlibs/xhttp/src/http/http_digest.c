#include "../internal/xrt_http.h"

#include <xrt/http_digest.h>



#if defined(XHTTP_FEATURE_HTTP_DIGEST)

/* 两种类型化结果共用同一个失败原子发布缓冲。 */
typedef union xrt_http_digest_output {
	xhttpdigest Digest;
	xhttpdigestpreference Preference;
} xrt_http_digest_output;



/* 游标状态同时绑定单值/字段、摘要/偏好和摘要目标。 */
enum {
	XRT_HTTP_DIGEST_STATE_INITIAL = 0,
	XRT_HTTP_DIGEST_STATE_VALUE,
	XRT_HTTP_DIGEST_STATE_PREFERENCE_VALUE,
	XRT_HTTP_DIGEST_STATE_CONTENT_FIELDS,
	XRT_HTTP_DIGEST_STATE_REPRESENTATION_FIELDS,
	XRT_HTTP_DIGEST_STATE_CONTENT_PREFERENCE_FIELDS,
	XRT_HTTP_DIGEST_STATE_REPRESENTATION_PREFERENCE_FIELDS
};



/* 把摘要目标映射到四个 RFC 9530 字段名称之一。 */
static bool __xrtHttpDigestFieldName(
	xhttpdigesttarget Target,
	bool bPreference,
	xstrview* pName,
	uint8* pState
)
{
	if ( Target == XHTTP_DIGEST_CONTENT ) {
		*pName = bPreference ?
			XRT_STR_LITERAL("Want-Content-Digest") :
			XRT_STR_LITERAL("Content-Digest");
		*pState = (uint8)(bPreference ?
			XRT_HTTP_DIGEST_STATE_CONTENT_PREFERENCE_FIELDS :
			XRT_HTTP_DIGEST_STATE_CONTENT_FIELDS);
		return true;
	}
	if ( Target == XHTTP_DIGEST_REPRESENTATION ) {
		*pName = bPreference ?
			XRT_STR_LITERAL("Want-Repr-Digest") :
			XRT_STR_LITERAL("Repr-Digest");
		*pState = (uint8)(bPreference ?
			XRT_HTTP_DIGEST_STATE_REPRESENTATION_PREFERENCE_FIELDS :
			XRT_HTTP_DIGEST_STATE_REPRESENTATION_FIELDS);
		return true;
	}
	return false;
}



/* 将一个 Structured Dictionary 成员转换为摘要成员。 */
static bool __xrtHttpDigestMember(
	const xhttpstructureddictionarymember* pMember,
	void* pOutput
)
{
	xhttpdigest Digest;

	if ( (pMember->Member.Kind !=
		XHTTP_STRUCTURED_MEMBER_ITEM) ||
		(pMember->Member.Bare.Type !=
		 XHTTP_STRUCTURED_BYTES) ) {
		return false;
	}
	if ( pOutput != NULL ) {
		memset(&Digest, 0, sizeof(Digest));
		Digest.Algorithm = pMember->Key;
		Digest.Value = pMember->Member.Bare;
		Digest.Parameters = pMember->Member.Parameters;
		memcpy(pOutput, &Digest, sizeof(Digest));
	}
	return true;
}



/* 将一个 Structured Dictionary 成员转换为摘要偏好。 */
static bool __xrtHttpDigestPreferenceMember(
	const xhttpstructureddictionarymember* pMember,
	void* pOutput
)
{
	xhttpdigestpreference Preference;

	if ( (pMember->Member.Kind !=
		XHTTP_STRUCTURED_MEMBER_ITEM) ||
		(pMember->Member.Bare.Type !=
		 XHTTP_STRUCTURED_INTEGER) ||
		(pMember->Member.Bare.Number < 0) ||
		(pMember->Member.Bare.Number > 10) ) {
		return false;
	}
	if ( pOutput != NULL ) {
		memset(&Preference, 0, sizeof(Preference));
		Preference.Algorithm = pMember->Key;
		Preference.Weight =
			(uint8)pMember->Member.Bare.Number;
		Preference.Parameters = pMember->Member.Parameters;
		memcpy(pOutput, &Preference, sizeof(Preference));
	}
	return true;
}



/* 判断摘要游标是否是初始化后的全零状态。 */
static bool __xrtHttpDigestCursorInitial(
	const xhttpdigestcursor* pCursor
)
{
	return (pCursor->Structured.Source == NULL) &&
		(pCursor->Structured.Name == NULL) &&
		(pCursor->Structured.SourceSize == 0) &&
		(pCursor->Structured.NameSize == 0) &&
		(pCursor->Structured.Field == 0) &&
		(pCursor->Structured.Offset == 0) &&
		(pCursor->Structured.Order == 0) &&
		(pCursor->Structured.State == 0) &&
		(pCursor->State ==
		 (uint8)XRT_HTTP_DIGEST_STATE_INITIAL);
}



/* 验证摘要游标仍用于首次选择的类型化操作。 */
static bool __xrtHttpDigestCursorValid(
	const xhttpdigestcursor* pCursor,
	uint8 iState
)
{
	if ( pCursor->State ==
		(uint8)XRT_HTTP_DIGEST_STATE_INITIAL ) {
		return __xrtHttpDigestCursorInitial(pCursor);
	}
	return (pCursor->State == iState) &&
		(pCursor->Structured.State != 0);
}



/* 完整验证单值 Dictionary 的全部抽象成员。 */
static bool __xrtHttpDigestValueValidate(
	xstrview Value,
	bool (*Convert)(
		const xhttpstructureddictionarymember*, void*
	)
)
{
	xhttpstructuredmapcursor Cursor;
	xhttpstructureddictionarymember Member;
	xhttpnext Next;

	xrtHttpStructuredMapCursorInit(&Cursor);
	while ( (Next = xrtHttpStructuredDictionaryMapNext(
		Value, &Cursor, &Member
	)) == XHTTP_NEXT_ITEM ) {
		if ( !Convert(&Member, NULL) ) {
			__xrtErrorSetValue();
			return false;
		}
	}
	return Next == XHTTP_NEXT_END;
}



/* 完整验证重复字段组合的全部抽象成员。 */
static bool __xrtHttpDigestFieldsValidate(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name,
	bool (*Convert)(
		const xhttpstructureddictionarymember*, void*
	)
)
{
	xhttpstructuredmapcursor Cursor;
	xhttpstructureddictionarymember Member;
	xhttpnext Next;

	xrtHttpStructuredMapCursorInit(&Cursor);
	while ( (Next = xrtHttpStructuredDictionaryMapFieldNext(
		pFields, iCount, Name, &Cursor, &Member
	)) == XHTTP_NEXT_ITEM ) {
		if ( !Convert(&Member, NULL) ) {
			__xrtErrorSetValue();
			return false;
		}
	}
	return Next == XHTTP_NEXT_END;
}



/* 共享单值摘要与偏好的来源绑定、预校验和类型化迭代。 */
static xhttpnext __xrtHttpDigestValueNext(
	xstrview Value,
	xhttpdigestcursor* pCursor,
	void* pOutput,
	size_t iOutputSize,
	uint8 iState,
	bool (*Convert)(
		const xhttpstructureddictionarymember*, void*
	)
)
{
	xhttpstructureddictionarymember Member;
	xrt_http_digest_output Output;
	xhttpdigestcursor Cursor;
	xhttpnext Next;

	memset(&Output, 0, sizeof(Output));
	if ( !__xrtHttpViewValid(Value) ||
		!__xrtRangeValid(pCursor, sizeof(Cursor)) ||
		!__xrtRangeValid(pOutput, iOutputSize) ||
		__xrtRangesOverlap(
			pCursor, sizeof(Cursor), pOutput, iOutputSize
		) || __xrtRangesOverlap(
			pCursor, sizeof(Cursor), Value.Data, Value.Size
		) || __xrtRangesOverlap(
			pOutput, iOutputSize, Value.Data, Value.Size
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	memcpy(&Cursor, pCursor, sizeof(Cursor));
	if ( !__xrtHttpDigestCursorValid(&Cursor, iState) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	if ( Cursor.State ==
		(uint8)XRT_HTTP_DIGEST_STATE_INITIAL ) {
		if ( !__xrtHttpDigestValueValidate(
			Value, Convert
		) ) {
			return XHTTP_NEXT_ERROR;
		}
		Cursor.State = iState;
	}
	Next = xrtHttpStructuredDictionaryMapNext(
		Value, &Cursor.Structured, &Member
	);
	if ( Next == XHTTP_NEXT_ERROR ) {
		return Next;
	}
	if ( (Next == XHTTP_NEXT_ITEM) &&
		!Convert(&Member, &Output) ) {
		__xrtErrorSetValue();
		return XHTTP_NEXT_ERROR;
	}
	memcpy(pOutput, &Output, iOutputSize);
	memcpy(pCursor, &Cursor, sizeof(Cursor));
	return Next;
}



/* 共享重复字段摘要与偏好的来源绑定、预校验和类型化迭代。 */
static xhttpnext __xrtHttpDigestFieldsNext(
	const xhttpfield* pFields,
	size_t iFieldCount,
	xstrview Name,
	uint8 iState,
	xhttpdigestcursor* pCursor,
	void* pOutput,
	size_t iOutputSize,
	bool (*Convert)(
		const xhttpstructureddictionarymember*, void*
	)
)
{
	xhttpstructureddictionarymember Member;
	xrt_http_digest_output Output;
	xhttpdigestcursor Cursor;
	xhttpnext Next;

	memset(&Output, 0, sizeof(Output));
	if ( !__xrtHttpFieldArrayValid(pFields, iFieldCount) ||
		!__xrtRangeValid(pCursor, sizeof(Cursor)) ||
		!__xrtRangeValid(pOutput, iOutputSize) ||
		__xrtHttpFieldArrayOverlap(
			pFields, iFieldCount, pCursor, sizeof(Cursor)
		) || __xrtHttpFieldArrayOverlap(
			pFields, iFieldCount, pOutput, iOutputSize
		) || __xrtRangesOverlap(
			pCursor, sizeof(Cursor), pOutput, iOutputSize
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	memcpy(&Cursor, pCursor, sizeof(Cursor));
	if ( !__xrtHttpDigestCursorValid(&Cursor, iState) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	if ( Cursor.State ==
		(uint8)XRT_HTTP_DIGEST_STATE_INITIAL ) {
		if ( !__xrtHttpDigestFieldsValidate(
			pFields, iFieldCount, Name, Convert
		) ) {
			return XHTTP_NEXT_ERROR;
		}
		Cursor.State = iState;
	}
	Next = xrtHttpStructuredDictionaryMapFieldNext(
		pFields, iFieldCount, Name,
		&Cursor.Structured, &Member
	);
	if ( Next == XHTTP_NEXT_ERROR ) {
		return Next;
	}
	if ( (Next == XHTTP_NEXT_ITEM) &&
		!Convert(&Member, &Output) ) {
		__xrtErrorSetValue();
		return XHTTP_NEXT_ERROR;
	}
	memcpy(pOutput, &Output, iOutputSize);
	memcpy(pCursor, &Cursor, sizeof(Cursor));
	return Next;
}



/* 初始化摘要游标。 */
XRT_API void xrtHttpDigestCursorInit(xhttpdigestcursor* pCursor)
{
	xhttpdigestcursor Cursor;

	if ( !__xrtRangeValid(pCursor, sizeof(Cursor)) ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memset(&Cursor, 0, sizeof(Cursor));
	memcpy(pCursor, &Cursor, sizeof(Cursor));
}



/* 验证摘要 Dictionary。 */
XRT_API bool xrtHttpDigestValid(xstrview Value)
{
	if ( !__xrtHttpViewValid(Value) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtHttpDigestValueValidate(
		Value, __xrtHttpDigestMember
	);
}



/* 迭代摘要 Dictionary。 */
XRT_API xhttpnext xrtHttpDigestNext(
	xstrview Value,
	xhttpdigestcursor* pCursor,
	xhttpdigest* pDigest
)
{
	return __xrtHttpDigestValueNext(
		Value, pCursor, pDigest, sizeof(*pDigest),
		(uint8)XRT_HTTP_DIGEST_STATE_VALUE,
		__xrtHttpDigestMember
	);
}



/* 跨重复字段行迭代摘要 Dictionary。 */
XRT_API xhttpnext xrtHttpDigestFieldNext(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpdigesttarget Target,
	xhttpdigestcursor* pCursor,
	xhttpdigest* pDigest
)
{
	xstrview Name;
	uint8 iState;

	if ( !__xrtHttpDigestFieldName(
		Target, false, &Name, &iState
	) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	return __xrtHttpDigestFieldsNext(
		pFields, iCount, Name, iState, pCursor,
		pDigest, sizeof(*pDigest),
		__xrtHttpDigestMember
	);
}



/* 验证摘要参数区可以作为完整 Structured Parameters 迭代。 */
static bool __xrtHttpDigestParametersValid(xstrview Parameters)
{
	xhttpstructuredparameter Parameter;
	xhttpnext Next;
	size_t iOffset = 0;

	while ( (Next = xrtHttpStructuredParameterNext(
		Parameters, &iOffset, &Parameter
	)) == XHTTP_NEXT_ITEM ) {
	}
	return Next == XHTTP_NEXT_END;
}



/* 验证摘要解码的描述符、借用范围和输出互不破坏。 */
static bool __xrtHttpDigestReadArguments(
	const xhttpdigest* pDigest,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpdigest* pLoaded
)
{
	if ( !__xrtRangeValid(pDigest, sizeof(*pDigest)) ||
		!__xrtRangeValid(pSize, sizeof(*pSize)) ||
		((pOutput == NULL) && (iCapacity != 0)) ||
		((pOutput != NULL) &&
		 !__xrtRangeValid(pOutput, iCapacity)) ||
		__xrtRangesOverlap(
			pDigest, sizeof(*pDigest), pSize, sizeof(*pSize)
		) || ((pOutput != NULL) && __xrtRangesOverlap(
			pDigest, sizeof(*pDigest), pOutput, iCapacity
		)) || ((pOutput != NULL) && __xrtRangesOverlap(
			pSize, sizeof(*pSize), pOutput, iCapacity
		)) ) {
		return false;
	}
	memcpy(pLoaded, pDigest, sizeof(*pLoaded));
	if ( !__xrtHttpViewValid(pLoaded->Algorithm) ||
		!__xrtHttpViewValid(pLoaded->Value.Encoded) ||
		!__xrtHttpViewValid(pLoaded->Parameters) ||
		__xrtRangesOverlap(
			pLoaded->Algorithm.Data, pLoaded->Algorithm.Size,
			pSize, sizeof(*pSize)
		) || __xrtRangesOverlap(
			pLoaded->Value.Encoded.Data,
			pLoaded->Value.Encoded.Size,
			pSize, sizeof(*pSize)
		) || __xrtRangesOverlap(
			pLoaded->Parameters.Data, pLoaded->Parameters.Size,
			pSize, sizeof(*pSize)
		) || ((pOutput != NULL) && __xrtRangesOverlap(
			pLoaded->Algorithm.Data, pLoaded->Algorithm.Size,
			pOutput, iCapacity
		)) || ((pOutput != NULL) && __xrtRangesOverlap(
			pLoaded->Parameters.Data, pLoaded->Parameters.Size,
			pOutput, iCapacity
		)) ) {
		return false;
	}
	return true;
}



/* 解码一个摘要 Byte Sequence。 */
XRT_API bool xrtHttpDigestRead(
	const xhttpdigest* pDigest,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xhttpdigest Digest;

	if ( !__xrtHttpDigestReadArguments(
		pDigest, pOutput, iCapacity, pSize, &Digest
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtHttpStructuredKeyValid(Digest.Algorithm) ||
		(Digest.Value.Type != XHTTP_STRUCTURED_BYTES) ||
		(Digest.Value.Number != 0) ||
		!__xrtHttpDigestParametersValid(Digest.Parameters) ) {
		if ( xrtGetError() == NULL ) {
			__xrtErrorSetValue();
		}
		return false;
	}
	return xrtHttpStructuredBytesDecode(
		&Digest.Value, pOutput, iCapacity, pSize
	);
}



/* 验证摘要偏好 Dictionary。 */
XRT_API bool xrtHttpDigestPreferenceValid(xstrview Value)
{
	if ( !__xrtHttpViewValid(Value) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtHttpDigestValueValidate(
		Value, __xrtHttpDigestPreferenceMember
	);
}



/* 迭代摘要偏好 Dictionary。 */
XRT_API xhttpnext xrtHttpDigestPreferenceNext(
	xstrview Value,
	xhttpdigestcursor* pCursor,
	xhttpdigestpreference* pPreference
)
{
	return __xrtHttpDigestValueNext(
		Value, pCursor, pPreference, sizeof(*pPreference),
		(uint8)XRT_HTTP_DIGEST_STATE_PREFERENCE_VALUE,
		__xrtHttpDigestPreferenceMember
	);
}



/* 跨重复字段行迭代摘要偏好 Dictionary。 */
XRT_API xhttpnext xrtHttpDigestPreferenceFieldNext(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpdigesttarget Target,
	xhttpdigestcursor* pCursor,
	xhttpdigestpreference* pPreference
)
{
	xstrview Name;
	uint8 iState;

	if ( !__xrtHttpDigestFieldName(
		Target, true, &Name, &iState
	) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	return __xrtHttpDigestFieldsNext(
		pFields, iCount, Name, iState, pCursor,
		pPreference, sizeof(*pPreference),
		__xrtHttpDigestPreferenceMember
	);
}

#endif
