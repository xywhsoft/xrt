#include "../internal/xrt_http_server_router.h"



#if defined(XHTTP_FEATURE_HTTP_SERVER_ROUTER)

/* 判断借用文本是否与固定方法逐字节相同。 */
static bool __xrtHttpServerRouterTextEqual(
	xstrview Text,
	cstr sValue
)
{
	size_t iSize = strlen(sValue);

	return (Text.Size == iSize) &&
		(memcmp(Text.Data, sValue, iSize) == 0);
}



/* 释放匹配阶段为极端参数数量分配的描述符。 */
void __xrtHttpServerRouterMatchClear(
	xrt_http_server_route_match* pMatch
)
{
	if ( pMatch == NULL ) {
		return;
	}
	if ( (pMatch->Params != NULL) &&
		(pMatch->Params != pMatch->Local) ) {
		xrtFree(pMatch->Params);
	}
	pMatch->Params = NULL;
}



/* 把匹配结果的参数所有权转移到按需分配的请求期缓存。 */
xrt_http_server_route_match* __xrtHttpServerRouterMatchTake(
	xrt_http_server_route_match* pMatch
)
{
	xrt_http_server_route_match* pStored;

	if ( pMatch == NULL ) {
		__xrtHttpServerRouterSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ROUTER_ERROR_ARGUMENT,
			"cache-http-server-route",
			"HTTP server route match is null",
			NULL
		);
		return NULL;
	}
	pStored = (xrt_http_server_route_match*)xrtMalloc(
		sizeof(*pStored)
	);
	if ( pStored == NULL ) {
		__xrtHttpServerRouterWrapError(
			XERR_MEMORY,
			XHTTP_SERVER_ROUTER_ERROR_MEMORY,
			"cache-http-server-route",
			"HTTP server route cache allocation failed"
		);
		return NULL;
	}
	memcpy(pStored, pMatch, sizeof(*pStored));
	if ( pMatch->Params == pMatch->Local ) {
		pStored->Params = pStored->Local;
		if ( pMatch->Count != 0 ) {
			memcpy(
				pStored->Local,
				pMatch->Local,
				pMatch->Count * sizeof(*pMatch->Local)
			);
		}
	} else {
		pStored->Params = pMatch->Params;
	}
	pMatch->Params = NULL;
	return pStored;
}



/* 释放请求期路由缓存和极端参数模板的拥有存储。 */
void __xrtHttpServerRouterMatchDestroy(ptr pData)
{
	xrt_http_server_route_match* pMatch =
		(xrt_http_server_route_match*)pData;

	if ( pMatch == NULL ) {
		return;
	}
	__xrtHttpServerRouterMatchClear(pMatch);
	memset(pMatch, 0, sizeof(*pMatch));
	xrtFree(pMatch);
}



/* 把通用 Router 的非空整数 Value 解码为高层回调记录。 */
static bool __xrtHttpServerRouterEntry(
	const xhttpserverrouter* pRouter,
	ptr pValue,
	const xrt_http_server_route_entry** ppEntry
)
{
	uintptr_t iValue = (uintptr_t)pValue;

	if ( (iValue == 0) || (iValue > pRouter->Count) ) {
		__xrtHttpServerRouterSetError(
			XERR_INTERNAL,
			XHTTP_SERVER_ROUTER_ERROR_INTERNAL,
			"match-http-server-route",
			"HTTP server route index is outside the callback table",
			NULL
		);
		return false;
	}
	*ppEntry = &pRouter->Entries[iValue - 1u];
	return true;
}



/* 解析 origin/absolute 请求目标并执行容量完整的结构匹配。 */
bool __xrtHttpServerRouterMatch(
	const xhttpserverrouter* pRouter,
	const xhttpserverrequest* pRequest,
	xrt_http_server_route_match* pMatch
)
{
	static const xstrview Root = XRT_STR_INIT("/");
	xhttptarget Target;
	xhttproutermatch Route;
	size_t iCount = 0;
	xhttprouterstatus Status;

	if ( (pRouter == NULL) || (pRequest == NULL) ||
		(pMatch == NULL) ||
		!xrtHttpServerRouterFrozen(pRouter) ) {
		__xrtHttpServerRouterSetError(
			(pRouter != NULL) && (pRequest != NULL) ?
				XERR_STATE : XERR_ARGUMENT,
			(pRouter != NULL) && (pRequest != NULL) ?
				XHTTP_SERVER_ROUTER_ERROR_STATE :
				XHTTP_SERVER_ROUTER_ERROR_ARGUMENT,
			"match-http-server-route",
			"HTTP server router must be frozen and request must be valid",
			NULL
		);
		return false;
	}
	memset(pMatch, 0, sizeof(*pMatch));
	pMatch->Params = pMatch->Local;
	pMatch->Method = xrtHttpServerRequestMethod(pRequest);
	if ( !xrtHttpServerRequestParseTarget(
		pRequest, &Target
	) ) {
		const xerror* pCause = xrtGetError();

		__xrtHttpServerRouterSetError(
			__xrtHttpServerRouterCauseKind(
				pCause, XERR_INTERNAL
			),
			XHTTP_SERVER_ROUTER_ERROR_TARGET,
			"match-http-server-route",
			"HTTP server request target could not be parsed",
			pCause
		);
		return false;
	}
	if ( (Target.Form != XHTTP_TARGET_ORIGIN) &&
		(Target.Form != XHTTP_TARGET_ABSOLUTE) ) {
		pMatch->Status = XHTTP_ROUTER_NOT_FOUND;
		return true;
	}
	pMatch->Path = Target.Path.Size != 0 ?
		Target.Path : Root;
	Status = xrtHttpRouterMatch(
		pRouter->Index,
		pMatch->Method,
		pMatch->Path,
		pMatch->Local,
		XRT_HTTP_SERVER_ROUTER_LOCAL_PARAMS,
		&iCount,
		&Route
	);
	if ( Status == XHTTP_ROUTER_ERROR ) {
		const xerror* pCause = xrtGetError();

		__xrtHttpServerRouterSetError(
			__xrtHttpServerRouterCauseKind(
				pCause, XERR_PROTOCOL
			),
			XHTTP_SERVER_ROUTER_ERROR_TARGET,
			"match-http-server-route",
			"HTTP server request path could not be routed",
			pCause
		);
		return false;
	}
	if ( Status == XHTTP_ROUTER_MORE ) {
		pMatch->Params = (xhttprouteparam*)xrtMalloc(
			iCount * sizeof(*pMatch->Params)
		);
		if ( pMatch->Params == NULL ) {
			__xrtHttpServerRouterWrapError(
				XERR_MEMORY,
				XHTTP_SERVER_ROUTER_ERROR_MEMORY,
				"match-http-server-route",
				"HTTP server route parameter allocation failed"
			);
			return false;
		}
		Status = xrtHttpRouterMatch(
			pRouter->Index,
			pMatch->Method,
			pMatch->Path,
			pMatch->Params,
			iCount,
			&pMatch->Count,
			&Route
		);
		if ( Status != XHTTP_ROUTER_MATCH ) {
			const xerror* pCause = xrtGetError();

			__xrtHttpServerRouterMatchClear(pMatch);
			__xrtHttpServerRouterSetError(
				XERR_INTERNAL,
				XHTTP_SERVER_ROUTER_ERROR_INTERNAL,
				"match-http-server-route",
				"HTTP server route changed during frozen rematch",
				pCause
			);
			return false;
		}
	} else {
		pMatch->Count = iCount;
	}
	pMatch->Status = Status;
	if ( Status == XHTTP_ROUTER_MATCH ) {
		if ( !__xrtHttpServerRouterEntry(
			pRouter, Route.Value, &pMatch->Entry
		) ) {
			__xrtHttpServerRouterMatchClear(pMatch);
			return false;
		}
	}
	return true;
}



/* 收集一个路径的允许方法，常见集合完全使用栈空间。 */
static bool __xrtHttpServerRouterMethods(
	const xhttpserverrouter* pRouter,
	xstrview Path,
	xstrview* pLocal,
	xstrview** ppMethods,
	size_t* pCount
)
{
	xhttprouterstatus Status = xrtHttpRouterMethods(
		pRouter->Index,
		Path,
		pLocal,
		16u,
		pCount
	);

	*ppMethods = pLocal;
	if ( Status == XHTTP_ROUTER_MORE ) {
		if ( *pCount >=
			(SIZE_MAX / sizeof(**ppMethods)) ) {
			__xrtHttpServerRouterSetError(
				XERR_RANGE,
				XHTTP_SERVER_ROUTER_ERROR_RESPONSE,
				"list-http-server-route-methods",
				"HTTP server route method list is too large",
				NULL
			);
			return false;
		}
		*ppMethods = (xstrview*)xrtMalloc(
			(*pCount + 1u) * sizeof(**ppMethods)
		);
		if ( *ppMethods == NULL ) {
			__xrtHttpServerRouterWrapError(
				XERR_MEMORY,
				XHTTP_SERVER_ROUTER_ERROR_MEMORY,
				"list-http-server-route-methods",
				"HTTP server route method list allocation failed"
			);
			return false;
		}
		Status = xrtHttpRouterMethods(
			pRouter->Index,
			Path,
			*ppMethods,
			*pCount,
			pCount
		);
	}
	if ( Status != XHTTP_ROUTER_MATCH ) {
		const xerror* pCause = xrtGetError();

		if ( *ppMethods != pLocal ) {
			xrtFree(*ppMethods);
			*ppMethods = pLocal;
		}
		__xrtHttpServerRouterSetError(
			XERR_INTERNAL,
			XHTTP_SERVER_ROUTER_ERROR_INTERNAL,
			"list-http-server-route-methods",
			"HTTP server 405 path has no stable method set",
			pCause
		);
		return false;
	}
	return true;
}



/* 判断方法集合是否已经包含自动 OPTIONS。 */
static bool __xrtHttpServerRouterHasOptions(
	const xstrview* pMethods,
	size_t iCount
)
{
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		if ( __xrtHttpServerRouterTextEqual(
			pMethods[i], "OPTIONS"
		) ) {
			return true;
		}
	}
	return false;
}



/* 提交带 Allow 的 405 或自动 OPTIONS 响应。 */
static bool __xrtHttpServerRouterAllowReply(
	const xhttpserverrouter* pRouter,
	xhttpconn* pConnection,
	xstrview Path,
	bool bOptions
)
{
	xstrview LocalMethods[17];
	xstrview* pMethods;
	char LocalAllow[256];
	char* pAllow = LocalAllow;
	xhttpreply* pReply = NULL;
	size_t iCount = 0;
	size_t iAllow;
	bool bAddOptions;
	bool bSuccess = false;

	if ( !__xrtHttpServerRouterMethods(
		pRouter, Path, LocalMethods, &pMethods, &iCount
	) ) {
		return false;
	}
	bAddOptions = !__xrtHttpServerRouterHasOptions(
		pMethods, iCount
	);
	if ( bAddOptions ) {
		pMethods[iCount++] = XRT_STR_LITERAL("OPTIONS");
	}
	if ( !xrtHttpTokenListWrite(
		pMethods, iCount, NULL, 0, &iAllow
	) ) {
		const xerror* pCause = xrtGetError();

		__xrtHttpServerRouterSetError(
			__xrtHttpServerRouterCauseKind(
				pCause, XERR_INTERNAL
			),
			XHTTP_SERVER_ROUTER_ERROR_RESPONSE,
			"format-http-allow",
			"HTTP Allow field could not be measured",
			pCause
		);
		goto Cleanup;
	}
	if ( iAllow > sizeof(LocalAllow) ) {
		pAllow = (char*)xrtMalloc(iAllow);
		if ( pAllow == NULL ) {
			__xrtHttpServerRouterWrapError(
				XERR_MEMORY,
				XHTTP_SERVER_ROUTER_ERROR_MEMORY,
				"format-http-allow",
				"HTTP Allow field allocation failed"
			);
			goto Cleanup;
		}
	}
	if ( !xrtHttpTokenListWrite(
		pMethods, iCount, pAllow, iAllow, &iAllow
	) ) {
		const xerror* pCause = xrtGetError();

		__xrtHttpServerRouterSetError(
			XERR_PROTOCOL,
			XHTTP_SERVER_ROUTER_ERROR_RESPONSE,
			"format-http-allow",
			"HTTP Allow field could not be written",
			pCause
		);
		goto Cleanup;
	}
	pReply = xrtHttpReplyCreate(
		bOptions ? XHTTP_STATUS_NO_CONTENT :
		XHTTP_STATUS_METHOD_NOT_ALLOWED
	);
	if ( (pReply == NULL) || !xrtHttpReplyAddHeader(
		pReply,
		XRT_STR_LITERAL("Allow"),
		(xstrview){ pAllow, iAllow }
	) || (!bOptions && !xrtHttpReplySetBytes(
		pReply,
		XRT_BYTES_LITERAL("Method Not Allowed"),
		XRT_STR_LITERAL("text/plain; charset=utf-8")
	)) || (xrtHttpConnRespond(
		pConnection, pReply
	) != XNET_RESULT_OK) ) {
		const xerror* pCause = xrtGetError();

		__xrtHttpServerRouterSetError(
			__xrtHttpServerRouterCauseKind(
				pCause, XERR_IO
			),
			XHTTP_SERVER_ROUTER_ERROR_RESPONSE,
			bOptions ? "respond-http-options" :
				"respond-http-method-not-allowed",
			"HTTP server route default response failed",
			pCause
		);
		goto Cleanup;
	}
	bSuccess = true;

Cleanup:
	xrtHttpReplyDestroy(pReply);
	if ( pAllow != LocalAllow ) {
		xrtFree(pAllow);
	}
	if ( pMethods != LocalMethods ) {
		xrtFree(pMethods);
	}
	return bSuccess;
}



/* 提交 Router 未命中、方法拒绝或自动 OPTIONS 的默认响应。 */
bool __xrtHttpServerRouterDefault(
	const xhttpserverrouter* pRouter,
	xhttpconn* pConnection,
	const xrt_http_server_route_match* pMatch
)
{
	if ( pMatch->Status == XHTTP_ROUTER_NOT_FOUND ) {
		if ( xrtHttpConnReply(
			pConnection,
			XHTTP_STATUS_NOT_FOUND,
			XRT_STR_LITERAL("text/plain; charset=utf-8"),
			XRT_BYTES_LITERAL("Not Found")
		) == XNET_RESULT_OK ) {
			return true;
		}
		{
			const xerror* pCause = xrtGetError();

			__xrtHttpServerRouterSetError(
				__xrtHttpServerRouterCauseKind(
					pCause, XERR_IO
				),
				XHTTP_SERVER_ROUTER_ERROR_RESPONSE,
				"respond-http-not-found",
				"HTTP server route 404 response failed",
				pCause
			);
		}
		return false;
	}
	if ( pMatch->Status == XHTTP_ROUTER_METHOD_NOT_ALLOWED ) {
		return __xrtHttpServerRouterAllowReply(
			pRouter,
			pConnection,
			pMatch->Path,
			__xrtHttpServerRouterTextEqual(
				pMatch->Method, "OPTIONS"
			)
		);
	}
	__xrtHttpServerRouterSetError(
		XERR_INTERNAL,
		XHTTP_SERVER_ROUTER_ERROR_INTERNAL,
		"respond-http-route-default",
		"HTTP server route default received an invalid status",
		NULL
	);
	return false;
}



/* 对完整请求执行高层回调或标准默认响应。 */
XRT_API xhttprouterstatus xrtHttpServerRouterDispatch(
	const xhttpserverrouter* pRouter,
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest
)
{
	xrt_http_server_route_match Match;
	xhttprouterstatus Status;

	if ( (pServer == NULL) || (pConnection == NULL) ) {
		__xrtHttpServerRouterSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ROUTER_ERROR_ARGUMENT,
			"dispatch-http-server-route",
			"HTTP server or connection is null",
			NULL
		);
		return XHTTP_ROUTER_ERROR;
	}
	if ( !__xrtHttpServerRouterMatch(
		pRouter, pRequest, &Match
	) ) {
		return XHTTP_ROUTER_ERROR;
	}
	Status = Match.Status;
	#if defined(XHTTP_FEATURE_HTTP_SERVER_MIDDLEWARE)
		if ( !__xrtHttpServerMiddlewareDispatch(
			pRouter,
			pServer,
			pConnection,
			pRequest,
			&Match,
			NULL
		) ) {
			__xrtHttpServerRouterDispatchFail(
				pServer, pConnection, NULL
			);
			Status = XHTTP_ROUTER_ERROR;
		}
	#else
		if ( !__xrtHttpServerRouterDispatchTerminal(
			pRouter,
			pServer,
			pConnection,
			pRequest,
			&Match,
			NULL
		) ) {
			Status = XHTTP_ROUTER_ERROR;
		}
	#endif
	if ( (Status != XHTTP_ROUTER_ERROR) &&
		(Status == XHTTP_ROUTER_METHOD_NOT_ALLOWED) &&
		__xrtHttpServerRouterTextEqual(
			Match.Method, "OPTIONS"
		) ) {
		Status = XHTTP_ROUTER_MATCH;
	}
	__xrtHttpServerRouterMatchClear(&Match);
	return Status;
}

#endif
