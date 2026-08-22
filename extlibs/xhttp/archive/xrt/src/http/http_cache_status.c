#include "../internal/xrt_http.h"
#include "../internal/xrt_http_structured_status.h"

#include <xrt/http_cache_status.h>



#if defined(XRT_FEATURE_HTTP_CACHE_STATUS)

_Static_assert(
	sizeof(xhttpcachestatuscursor) ==
	sizeof(xrt_http_structured_status_cursor),
	"Cache-Status cursor layout mismatch"
);
_Static_assert(
	offsetof(xhttpcachestatuscursor, Offset) ==
	offsetof(xrt_http_structured_status_cursor, Offset) &&
	offsetof(xhttpcachestatuscursor, Source) ==
	offsetof(xrt_http_structured_status_cursor, Source) &&
	offsetof(xhttpcachestatuscursor, SourceSize) ==
	offsetof(xrt_http_structured_status_cursor, SourceSize) &&
	offsetof(xhttpcachestatuscursor, Validated) ==
	offsetof(xrt_http_structured_status_cursor, Validated),
	"Cache-Status cursor member layout mismatch"
);
_Static_assert(
	sizeof(xhttpcachestatusfieldcursor) ==
	sizeof(xrt_http_structured_status_field_cursor),
	"Cache-Status field cursor layout mismatch"
);
_Static_assert(
	offsetof(xhttpcachestatusfieldcursor, Structured) ==
	offsetof(xrt_http_structured_status_field_cursor, Structured) &&
	offsetof(xhttpcachestatusfieldcursor, Validated) ==
	offsetof(xrt_http_structured_status_field_cursor, Validated),
	"Cache-Status field cursor member layout mismatch"
);

/* 判断参数 key 是否与 ASCII 常量完全相同。 */
static bool __xrtHttpCacheStatusKey(
	xstrview Key,
	const char* sExpected,
	size_t iSize
)
{
	return (Key.Size == iSize) &&
		(memcmp(Key.Data, sExpected, iSize) == 0);
}



/* 清除某个已知参数之前发布的值和有效性。 */
static void __xrtHttpCacheStatusReset(
	xhttpcachestatus* pStatus,
	uint16 iFlag
)
{
	pStatus->Flags &= (uint16)~iFlag;
	pStatus->InvalidFlags &= (uint16)~iFlag;
	if ( iFlag == XHTTP_CACHE_STATUS_HAS_FORWARD ) {
		memset(&pStatus->Forward, 0, sizeof(pStatus->Forward));
	} else if ( iFlag == XHTTP_CACHE_STATUS_HAS_FORWARD_STATUS ) {
		pStatus->ForwardStatus = 0;
	} else if ( iFlag == XHTTP_CACHE_STATUS_HAS_TTL ) {
		pStatus->Ttl = 0;
	} else if ( iFlag == XHTTP_CACHE_STATUS_HAS_KEY ) {
		memset(&pStatus->Key, 0, sizeof(pStatus->Key));
	} else if ( iFlag == XHTTP_CACHE_STATUS_HAS_DETAIL ) {
		memset(&pStatus->Detail, 0, sizeof(pStatus->Detail));
	} else if ( iFlag == XHTTP_CACHE_STATUS_HAS_HIT ) {
		pStatus->Hit = 0;
	} else if ( iFlag == XHTTP_CACHE_STATUS_HAS_STORED ) {
		pStatus->Stored = 0;
	} else if ( iFlag == XHTTP_CACHE_STATUS_HAS_COLLAPSED ) {
		pStatus->Collapsed = 0;
	}
}



/* 提交一个已通过类型检查的参数。 */
static void __xrtHttpCacheStatusAccept(
	xhttpcachestatus* pStatus,
	uint16 iFlag
)
{
	pStatus->Flags |= iFlag;
}



/* 标记最后一次出现的已知参数类型或范围无效。 */
static void __xrtHttpCacheStatusReject(
	xhttpcachestatus* pStatus,
	uint16 iFlag
)
{
	pStatus->InvalidFlags |= iFlag;
}



/* 应用一个参数，并让重复参数的最后一次出现决定结果。 */
static void __xrtHttpCacheStatusParameterApply(
	xhttpcachestatus* pStatus,
	const xhttpstructuredparameter* pParameter
)
{
	uint16 iFlag;

	if ( __xrtHttpCacheStatusKey(
		pParameter->Key, "hit", 3u
	) ) {
		iFlag = XHTTP_CACHE_STATUS_HAS_HIT;
		__xrtHttpCacheStatusReset(pStatus, iFlag);
		if ( pParameter->Value.Type != XHTTP_STRUCTURED_BOOLEAN ) {
			__xrtHttpCacheStatusReject(pStatus, iFlag);
			return;
		}
		pStatus->Hit = (uint8)pParameter->Value.Number;
		__xrtHttpCacheStatusAccept(pStatus, iFlag);
		return;
	}
	if ( __xrtHttpCacheStatusKey(
		pParameter->Key, "fwd", 3u
	) ) {
		iFlag = XHTTP_CACHE_STATUS_HAS_FORWARD;
		__xrtHttpCacheStatusReset(pStatus, iFlag);
		if ( pParameter->Value.Type != XHTTP_STRUCTURED_TOKEN ) {
			__xrtHttpCacheStatusReject(pStatus, iFlag);
			return;
		}
		pStatus->Forward = pParameter->Value;
		__xrtHttpCacheStatusAccept(pStatus, iFlag);
		return;
	}
	if ( __xrtHttpCacheStatusKey(
		pParameter->Key, "fwd-status", 10u
	) ) {
		iFlag = XHTTP_CACHE_STATUS_HAS_FORWARD_STATUS;
		__xrtHttpCacheStatusReset(pStatus, iFlag);
		if ( (pParameter->Value.Type !=
			XHTTP_STRUCTURED_INTEGER) ||
			(pParameter->Value.Number < 100) ||
			(pParameter->Value.Number > 599) ) {
			__xrtHttpCacheStatusReject(pStatus, iFlag);
			return;
		}
		pStatus->ForwardStatus = pParameter->Value.Number;
		__xrtHttpCacheStatusAccept(pStatus, iFlag);
		return;
	}
	if ( __xrtHttpCacheStatusKey(
		pParameter->Key, "ttl", 3u
	) ) {
		iFlag = XHTTP_CACHE_STATUS_HAS_TTL;
		__xrtHttpCacheStatusReset(pStatus, iFlag);
		if ( pParameter->Value.Type != XHTTP_STRUCTURED_INTEGER ) {
			__xrtHttpCacheStatusReject(pStatus, iFlag);
			return;
		}
		pStatus->Ttl = pParameter->Value.Number;
		__xrtHttpCacheStatusAccept(pStatus, iFlag);
		return;
	}
	if ( __xrtHttpCacheStatusKey(
		pParameter->Key, "stored", 6u
	) ) {
		iFlag = XHTTP_CACHE_STATUS_HAS_STORED;
		__xrtHttpCacheStatusReset(pStatus, iFlag);
		if ( pParameter->Value.Type != XHTTP_STRUCTURED_BOOLEAN ) {
			__xrtHttpCacheStatusReject(pStatus, iFlag);
			return;
		}
		pStatus->Stored = (uint8)pParameter->Value.Number;
		__xrtHttpCacheStatusAccept(pStatus, iFlag);
		return;
	}
	if ( __xrtHttpCacheStatusKey(
		pParameter->Key, "collapsed", 9u
	) ) {
		iFlag = XHTTP_CACHE_STATUS_HAS_COLLAPSED;
		__xrtHttpCacheStatusReset(pStatus, iFlag);
		if ( pParameter->Value.Type != XHTTP_STRUCTURED_BOOLEAN ) {
			__xrtHttpCacheStatusReject(pStatus, iFlag);
			return;
		}
		pStatus->Collapsed = (uint8)pParameter->Value.Number;
		__xrtHttpCacheStatusAccept(pStatus, iFlag);
		return;
	}
	if ( __xrtHttpCacheStatusKey(
		pParameter->Key, "key", 3u
	) ) {
		iFlag = XHTTP_CACHE_STATUS_HAS_KEY;
		__xrtHttpCacheStatusReset(pStatus, iFlag);
		if ( pParameter->Value.Type != XHTTP_STRUCTURED_STRING ) {
			__xrtHttpCacheStatusReject(pStatus, iFlag);
			return;
		}
		pStatus->Key = pParameter->Value;
		__xrtHttpCacheStatusAccept(pStatus, iFlag);
		return;
	}
	if ( __xrtHttpCacheStatusKey(
		pParameter->Key, "detail", 6u
	) ) {
		iFlag = XHTTP_CACHE_STATUS_HAS_DETAIL;
		__xrtHttpCacheStatusReset(pStatus, iFlag);
		if ( (pParameter->Value.Type != XHTTP_STRUCTURED_STRING) &&
			(pParameter->Value.Type != XHTTP_STRUCTURED_TOKEN) ) {
			__xrtHttpCacheStatusReject(pStatus, iFlag);
			return;
		}
		pStatus->Detail = pParameter->Value;
		__xrtHttpCacheStatusAccept(pStatus, iFlag);
	}
}



/* 计算不会使字段失效的参数组合诊断。 */
static void __xrtHttpCacheStatusIssues(
	xhttpcachestatus* pStatus
)
{
	uint16 iForwardDependent =
		XHTTP_CACHE_STATUS_HAS_FORWARD_STATUS |
		XHTTP_CACHE_STATUS_HAS_STORED |
		XHTTP_CACHE_STATUS_HAS_COLLAPSED;

	if ( ((pStatus->Flags & XHTTP_CACHE_STATUS_HAS_HIT) != 0) &&
		((pStatus->Flags & XHTTP_CACHE_STATUS_HAS_FORWARD) != 0) ) {
		pStatus->Issues |= XHTTP_CACHE_STATUS_ISSUE_HIT_AND_FORWARD;
	}
	if ( ((pStatus->Flags & iForwardDependent) != 0) &&
		((pStatus->Flags & XHTTP_CACHE_STATUS_HAS_FORWARD) == 0) ) {
		pStatus->Issues |= XHTTP_CACHE_STATUS_ISSUE_FORWARD_REQUIRED;
	}
}



/* 将已经完成 Structured Fields 解析的成员转换为 Cache-Status。 */
static bool __xrtHttpCacheStatusMember(
	const xhttpstructuredmember* pMember,
	void* pOutput
)
{
	xhttpstructuredparameter Parameter;
	xhttpcachestatus Status;
	xhttpnext Next;
	size_t iOffset = 0;

	memset(&Status, 0, sizeof(Status));
	if ( (pMember->Kind != XHTTP_STRUCTURED_MEMBER_ITEM) ||
		((pMember->Bare.Type != XHTTP_STRUCTURED_STRING) &&
		 (pMember->Bare.Type != XHTTP_STRUCTURED_TOKEN)) ) {
		return false;
	}
	Status.Cache = pMember->Bare;
	Status.Parameters = pMember->Parameters;
	for ( ;; ) {
		Next = xrtHttpStructuredParameterNext(
			pMember->Parameters, &iOffset, &Parameter
		);
		if ( Next == XHTTP_NEXT_ERROR ) {
			return false;
		}
		if ( Next == XHTTP_NEXT_END ) {
			break;
		}
		__xrtHttpCacheStatusParameterApply(&Status, &Parameter);
	}
	__xrtHttpCacheStatusIssues(&Status);
	if ( pOutput != NULL ) {
		memcpy(pOutput, &Status, sizeof(Status));
	}
	return true;
}



/* 初始化单字段游标。 */
XRT_API void xrtHttpCacheStatusCursorInit(
	xhttpcachestatuscursor* pCursor
)
{
	__xrtHttpStructuredStatusCursorInit(
		pCursor, sizeof(*pCursor)
	);
}



/* 初始化重复字段游标。 */
XRT_API void xrtHttpCacheStatusFieldCursorInit(
	xhttpcachestatusfieldcursor* pCursor
)
{
	__xrtHttpStructuredStatusFieldCursorInit(
		pCursor, sizeof(*pCursor)
	);
}



/* 验证完整 Cache-Status 字段值。 */
XRT_API bool xrtHttpCacheStatusValid(xstrview Value)
{
	return __xrtHttpStructuredStatusValid(
		Value, __xrtHttpCacheStatusMember
	);
}



/* 迭代单个 Cache-Status 字段值。 */
XRT_API xhttpnext xrtHttpCacheStatusNext(
	xstrview Value,
	xhttpcachestatuscursor* pCursor,
	xhttpcachestatus* pStatus
)
{
	return __xrtHttpStructuredStatusNext(
		Value, pCursor, sizeof(*pCursor),
		pStatus, sizeof(*pStatus),
		__xrtHttpCacheStatusMember
	);
}



/* 跨重复字段行迭代 Cache-Status。 */
XRT_API xhttpnext xrtHttpCacheStatusFieldNext(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpcachestatusfieldcursor* pCursor,
	xhttpcachestatus* pStatus
)
{
	return __xrtHttpStructuredStatusFieldNext(
		pFields, iCount, XRT_STR_LITERAL("Cache-Status"),
		pCursor, sizeof(*pCursor),
		pStatus, sizeof(*pStatus),
		__xrtHttpCacheStatusMember
	);
}

#endif
