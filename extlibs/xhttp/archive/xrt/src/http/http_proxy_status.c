#include "../internal/xrt_http.h"
#include "../internal/xrt_http_structured_status.h"

#include <xrt/http_proxy_status.h>



#if defined(XRT_FEATURE_HTTP_PROXY_STATUS)

_Static_assert(
	sizeof(xhttpproxystatuscursor) ==
	sizeof(xrt_http_structured_status_cursor),
	"Proxy-Status cursor layout mismatch"
);
_Static_assert(
	offsetof(xhttpproxystatuscursor, Offset) ==
	offsetof(xrt_http_structured_status_cursor, Offset) &&
	offsetof(xhttpproxystatuscursor, Source) ==
	offsetof(xrt_http_structured_status_cursor, Source) &&
	offsetof(xhttpproxystatuscursor, SourceSize) ==
	offsetof(xrt_http_structured_status_cursor, SourceSize) &&
	offsetof(xhttpproxystatuscursor, Validated) ==
	offsetof(xrt_http_structured_status_cursor, Validated),
	"Proxy-Status cursor member layout mismatch"
);
_Static_assert(
	sizeof(xhttpproxystatusfieldcursor) ==
	sizeof(xrt_http_structured_status_field_cursor),
	"Proxy-Status field cursor layout mismatch"
);
_Static_assert(
	offsetof(xhttpproxystatusfieldcursor, Structured) ==
	offsetof(xrt_http_structured_status_field_cursor, Structured) &&
	offsetof(xhttpproxystatusfieldcursor, Validated) ==
	offsetof(xrt_http_structured_status_field_cursor, Validated),
	"Proxy-Status field cursor member layout mismatch"
);



/* 判断参数 key 是否与 ASCII 常量完全相同。 */
static bool __xrtHttpProxyStatusKey(
	xstrview Key,
	const char* sExpected,
	size_t iSize
)
{
	return (Key.Size == iSize) &&
		(memcmp(Key.Data, sExpected, iSize) == 0);
}



/* 清除某个已知参数之前发布的值和有效性。 */
static void __xrtHttpProxyStatusReset(
	xhttpproxystatus* pStatus,
	uint16 iFlag
)
{
	pStatus->Flags &= (uint16)~iFlag;
	pStatus->InvalidFlags &= (uint16)~iFlag;
	if ( iFlag == XHTTP_PROXY_STATUS_HAS_ERROR ) {
		memset(&pStatus->Error, 0, sizeof(pStatus->Error));
	} else if ( iFlag == XHTTP_PROXY_STATUS_HAS_NEXT_HOP ) {
		memset(&pStatus->NextHop, 0, sizeof(pStatus->NextHop));
	} else if ( iFlag == XHTTP_PROXY_STATUS_HAS_NEXT_PROTOCOL ) {
		memset(&pStatus->NextProtocol, 0, sizeof(pStatus->NextProtocol));
	} else if ( iFlag == XHTTP_PROXY_STATUS_HAS_RECEIVED_STATUS ) {
		pStatus->ReceivedStatus = 0;
	} else if ( iFlag == XHTTP_PROXY_STATUS_HAS_DETAILS ) {
		memset(&pStatus->Details, 0, sizeof(pStatus->Details));
	} else if ( iFlag == XHTTP_PROXY_STATUS_HAS_NEXT_HOP_ALIASES ) {
		memset(
			&pStatus->NextHopAliases, 0,
			sizeof(pStatus->NextHopAliases)
		);
	}
}



/* 设置一个类型正确的已知参数。 */
static void __xrtHttpProxyStatusAccept(
	xhttpproxystatus* pStatus,
	uint16 iFlag
)
{
	pStatus->Flags |= iFlag;
}



/* 标记最后一次出现的已知参数类型或范围无效。 */
static void __xrtHttpProxyStatusReject(
	xhttpproxystatus* pStatus,
	uint16 iFlag
)
{
	pStatus->InvalidFlags |= iFlag;
}



/* 验证 ALPN 标识长度，并拒绝可用 Token 表达的 Byte Sequence。 */
static bool __xrtHttpProxyStatusProtocolValid(
	const xhttpstructuredbare* pProtocol
)
{
	char arrProtocol[XHTTP_PROXY_ALPN_MAX];
	size_t iSize;

	if ( pProtocol->Type == XHTTP_STRUCTURED_TOKEN ) {
		return (pProtocol->Encoded.Size != 0) &&
			(pProtocol->Encoded.Size <= XHTTP_PROXY_ALPN_MAX);
	}
	if ( pProtocol->Type != XHTTP_STRUCTURED_BYTES ) {
		return false;
	}
	if ( !xrtHttpStructuredBytesDecode(
		pProtocol, NULL, 0, &iSize
	) || (iSize == 0) ||
		(iSize > XHTTP_PROXY_ALPN_MAX) ) {
		return false;
	}
	if ( !xrtHttpStructuredBytesDecode(
		pProtocol, arrProtocol, sizeof(arrProtocol), &iSize
	) ) {
		return false;
	}
	return !xrtHttpStructuredTokenValid(
		(xstrview){ arrProtocol, iSize }
	);
}



/* 应用一个参数，并让重复参数的最后一次出现决定结果。 */
static void __xrtHttpProxyStatusParameterApply(
	xhttpproxystatus* pStatus,
	const xhttpstructuredparameter* pParameter
)
{
	uint16 iFlag;

	if ( __xrtHttpProxyStatusKey(
		pParameter->Key, "error", 5u
	) ) {
		iFlag = XHTTP_PROXY_STATUS_HAS_ERROR;
		__xrtHttpProxyStatusReset(pStatus, iFlag);
		if ( pParameter->Value.Type != XHTTP_STRUCTURED_TOKEN ) {
			__xrtHttpProxyStatusReject(pStatus, iFlag);
			return;
		}
		pStatus->Error = pParameter->Value;
		__xrtHttpProxyStatusAccept(pStatus, iFlag);
		return;
	}
	if ( __xrtHttpProxyStatusKey(
		pParameter->Key, "next-hop", 8u
	) ) {
		iFlag = XHTTP_PROXY_STATUS_HAS_NEXT_HOP;
		__xrtHttpProxyStatusReset(pStatus, iFlag);
		if ( (pParameter->Value.Type != XHTTP_STRUCTURED_STRING) &&
			(pParameter->Value.Type != XHTTP_STRUCTURED_TOKEN) ) {
			__xrtHttpProxyStatusReject(pStatus, iFlag);
			return;
		}
		pStatus->NextHop = pParameter->Value;
		__xrtHttpProxyStatusAccept(pStatus, iFlag);
		return;
	}
	if ( __xrtHttpProxyStatusKey(
		pParameter->Key, "next-protocol", 13u
	) ) {
		iFlag = XHTTP_PROXY_STATUS_HAS_NEXT_PROTOCOL;
		__xrtHttpProxyStatusReset(pStatus, iFlag);
		if ( !__xrtHttpProxyStatusProtocolValid(
			&pParameter->Value
		) ) {
			__xrtHttpProxyStatusReject(pStatus, iFlag);
			return;
		}
		pStatus->NextProtocol = pParameter->Value;
		__xrtHttpProxyStatusAccept(pStatus, iFlag);
		return;
	}
	if ( __xrtHttpProxyStatusKey(
		pParameter->Key, "received-status", 15u
	) ) {
		iFlag = XHTTP_PROXY_STATUS_HAS_RECEIVED_STATUS;
		__xrtHttpProxyStatusReset(pStatus, iFlag);
		if ( (pParameter->Value.Type !=
			XHTTP_STRUCTURED_INTEGER) ||
			(pParameter->Value.Number < 100) ||
			(pParameter->Value.Number > 599) ) {
			__xrtHttpProxyStatusReject(pStatus, iFlag);
			return;
		}
		pStatus->ReceivedStatus = pParameter->Value.Number;
		__xrtHttpProxyStatusAccept(pStatus, iFlag);
		return;
	}
	if ( __xrtHttpProxyStatusKey(
		pParameter->Key, "details", 7u
	) ) {
		iFlag = XHTTP_PROXY_STATUS_HAS_DETAILS;
		__xrtHttpProxyStatusReset(pStatus, iFlag);
		if ( pParameter->Value.Type != XHTTP_STRUCTURED_STRING ) {
			__xrtHttpProxyStatusReject(pStatus, iFlag);
			return;
		}
		pStatus->Details = pParameter->Value;
		__xrtHttpProxyStatusAccept(pStatus, iFlag);
		return;
	}
	if ( __xrtHttpProxyStatusKey(
		pParameter->Key, "next-hop-aliases", 16u
	) ) {
		iFlag = XHTTP_PROXY_STATUS_HAS_NEXT_HOP_ALIASES;
		__xrtHttpProxyStatusReset(pStatus, iFlag);
		if ( pParameter->Value.Type != XHTTP_STRUCTURED_STRING ) {
			__xrtHttpProxyStatusReject(pStatus, iFlag);
			return;
		}
		pStatus->NextHopAliases = pParameter->Value;
		__xrtHttpProxyStatusAccept(pStatus, iFlag);
	}
}



/* 将已经完成 Structured Fields 解析的成员转换为 Proxy-Status。 */
static bool __xrtHttpProxyStatusMember(
	const xhttpstructuredmember* pMember,
	void* pOutput
)
{
	xhttpstructuredparameter Parameter;
	xhttpproxystatus Status;
	xhttpnext Next;
	size_t iOffset = 0;

	memset(&Status, 0, sizeof(Status));
	if ( (pMember->Kind != XHTTP_STRUCTURED_MEMBER_ITEM) ||
		((pMember->Bare.Type != XHTTP_STRUCTURED_STRING) &&
		 (pMember->Bare.Type != XHTTP_STRUCTURED_TOKEN)) ) {
		return false;
	}
	Status.Proxy = pMember->Bare;
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
		__xrtHttpProxyStatusParameterApply(&Status, &Parameter);
	}
	if ( pOutput != NULL ) {
		memcpy(pOutput, &Status, sizeof(Status));
	}
	return true;
}



/* 初始化单字段游标。 */
XRT_API void xrtHttpProxyStatusCursorInit(
	xhttpproxystatuscursor* pCursor
)
{
	__xrtHttpStructuredStatusCursorInit(
		pCursor, sizeof(*pCursor)
	);
}



/* 初始化重复字段游标。 */
XRT_API void xrtHttpProxyStatusFieldCursorInit(
	xhttpproxystatusfieldcursor* pCursor
)
{
	__xrtHttpStructuredStatusFieldCursorInit(
		pCursor, sizeof(*pCursor)
	);
}



/* 验证完整 Proxy-Status 字段值。 */
XRT_API bool xrtHttpProxyStatusValid(xstrview Value)
{
	return __xrtHttpStructuredStatusValid(
		Value, __xrtHttpProxyStatusMember
	);
}



/* 迭代单个 Proxy-Status 字段值。 */
XRT_API xhttpnext xrtHttpProxyStatusNext(
	xstrview Value,
	xhttpproxystatuscursor* pCursor,
	xhttpproxystatus* pStatus
)
{
	return __xrtHttpStructuredStatusNext(
		Value, pCursor, sizeof(*pCursor),
		pStatus, sizeof(*pStatus),
		__xrtHttpProxyStatusMember
	);
}



/* 跨重复字段行迭代 Proxy-Status。 */
XRT_API xhttpnext xrtHttpProxyStatusFieldNext(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpproxystatusfieldcursor* pCursor,
	xhttpproxystatus* pStatus
)
{
	return __xrtHttpStructuredStatusFieldNext(
		pFields, iCount, XRT_STR_LITERAL("Proxy-Status"),
		pCursor, sizeof(*pCursor),
		pStatus, sizeof(*pStatus),
		__xrtHttpProxyStatusMember
	);
}

#endif
