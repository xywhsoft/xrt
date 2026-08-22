#include <stdlib.h>
#include <string.h>

#include <xrt/http_router.h>



#define XRT_HTTP_ROUTER_FUZZ_INPUT_MAX ((size_t)1048576u)
#define XRT_HTTP_ROUTER_FUZZ_PARAMS_MAX ((size_t)4u)
#define XRT_HTTP_ROUTER_FUZZ_METHODS_MAX ((size_t)32u)



/* 参考路由保留注册顺序，用于独立计算方法集合和命中结果。 */
typedef struct xrt_http_router_fuzz_route {
	xstrview Method;
	xstrview Pattern;
	uintptr_t Value;
} xrt_http_router_fuzz_route;



/* 同一结构叶允许多个方法，组顺序表达静态、参数、尾参数优先级。 */
typedef struct xrt_http_router_fuzz_group {
	size_t Count;
	size_t Routes[2];
} xrt_http_router_fuzz_group;



static const xrt_http_router_fuzz_route __xrtHttpRouterFuzzRoutes[] = {
	{ XRT_STR_INIT("POST"), XRT_STR_INIT("/users/{name}/detail"), 1u },
	{ XRT_STR_INIT("GET"), XRT_STR_INIT("/files/{file}"), 2u },
	{ XRT_STR_INIT("GET"), XRT_STR_INIT("/users/me/{part}"), 3u },
	{ XRT_STR_INIT("*"), XRT_STR_INIT("/files/static"), 4u },
	{ XRT_STR_INIT("GET"), XRT_STR_INIT("/users/{id}/detail"), 5u },
	{ XRT_STR_INIT("PUT"), XRT_STR_INIT("/users/{id}/{part}"), 6u },
	{ XRT_STR_INIT("GET"), XRT_STR_INIT("/users/{tail...}"), 7u },
	{ XRT_STR_INIT("POST"), XRT_STR_INIT("/users/{rest...}"), 8u },
	{ XRT_STR_INIT("DELETE"), XRT_STR_INIT("/users/me/detail"), 9u },
	{ XRT_STR_INIT("PROPFIND"), XRT_STR_INIT("/files/{tail...}"), 10u },
	{ XRT_STR_INIT("HEAD"), XRT_STR_INIT("/files/{name}"), 11u },
	{ XRT_STR_INIT("GET"), XRT_STR_INIT("/a//b"), 12u },
	{ XRT_STR_INIT("POST"), XRT_STR_INIT("/{first}//{last}"), 13u },
	{ XRT_STR_INIT("GET"), XRT_STR_INIT("/"), 14u },
	{ XRT_STR_INIT("GET"), XRT_STR_INIT("/fallback/{root...}"), 15u },
	{ XRT_STR_INIT("*"), XRT_STR_INIT("/fallback/{all...}"), 16u }
};



static const xrt_http_router_fuzz_group __xrtHttpRouterFuzzGroups[] = {
	{ 1u, { 13u, 0u } },
	{ 1u, { 8u, 0u } },
	{ 1u, { 2u, 0u } },
	{ 2u, { 0u, 4u } },
	{ 1u, { 5u, 0u } },
	{ 2u, { 6u, 7u } },
	{ 1u, { 3u, 0u } },
	{ 2u, { 1u, 10u } },
	{ 1u, { 9u, 0u } },
	{ 1u, { 11u, 0u } },
	{ 2u, { 14u, 15u } },
	{ 1u, { 12u, 0u } }
};



/* 比较两个非拥有字符串视图。 */
static bool __xrtHttpRouterFuzzViewEqual(
	xstrview Left,
	xstrview Right
)
{
	return (Left.Size == Right.Size) &&
		((Left.Size == 0) ||
		 (memcmp(Left.Data, Right.Data, Left.Size) == 0));
}



/* 判断有效模板是否命中路径；容量状态同样表示结构命中。 */
static bool __xrtHttpRouterFuzzPatternMatch(
	xstrview Pattern,
	xstrview Path
)
{
	size_t iCount = 0;
	xhttproutestatus Status = xrtHttpRouteMatch(
		Pattern, Path, NULL, 0, &iCount
	);

	xrtClearError();
	return (Status == XHTTP_ROUTE_MATCH) ||
		(Status == XHTTP_ROUTE_MORE);
}



/* 用公开底层匹配器判断输入是否为合法绝对路径。 */
static bool __xrtHttpRouterFuzzPathValid(xstrview Path)
{
	static const xstrview Root = XRT_STR_INIT("/");
	size_t iCount = 0;
	xhttproutestatus Status = xrtHttpRouteMatch(
		Root, Path, NULL, 0, &iCount
	);

	xrtClearError();
	return Status != XHTTP_ROUTE_ERROR;
}



/* 在一个结构叶中按精确方法、HEAD 回退、任意方法顺序选择路由。 */
static size_t __xrtHttpRouterFuzzSelectGroup(
	const xrt_http_router_fuzz_group* pGroup,
	xstrview Method,
	uint32* pFlags
)
{
	static const xstrview Head = XRT_STR_INIT("HEAD");
	static const xstrview Get = XRT_STR_INIT("GET");
	static const xstrview Any = XRT_STR_INIT("*");
	size_t iGet = SIZE_MAX;
	size_t iAny = SIZE_MAX;
	size_t i;
	bool bHead = __xrtHttpRouterFuzzViewEqual(Method, Head);

	for ( i = 0; i < pGroup->Count; i++ ) {
		size_t iRoute = pGroup->Routes[i];
		xstrview RouteMethod =
			__xrtHttpRouterFuzzRoutes[iRoute].Method;

		if ( __xrtHttpRouterFuzzViewEqual(
			RouteMethod, Method
		) ) {
			*pFlags = 0;
			return iRoute;
		}
		if ( bHead && __xrtHttpRouterFuzzViewEqual(
			RouteMethod, Get
		) ) {
			iGet = iRoute;
		} else if ( __xrtHttpRouterFuzzViewEqual(
			RouteMethod, Any
		) ) {
			iAny = iRoute;
		}
	}
	if ( iGet != SIZE_MAX ) {
		*pFlags = XHTTP_ROUTER_HEAD_FALLBACK;
		return iGet;
	}
	if ( iAny != SIZE_MAX ) {
		*pFlags = XHTTP_ROUTER_ANY_METHOD;
		return iAny;
	}
	return SIZE_MAX;
}



/* 按声明的结构优先级线性寻找参考命中。 */
static size_t __xrtHttpRouterFuzzSelect(
	xstrview Method,
	xstrview Path,
	uint32* pFlags,
	bool* pHasPath
)
{
	size_t i;

	*pHasPath = false;
	for ( i = 0;
		i < (sizeof(__xrtHttpRouterFuzzGroups) /
		 sizeof(__xrtHttpRouterFuzzGroups[0]));
		i++ ) {
		const xrt_http_router_fuzz_group* pGroup =
			&__xrtHttpRouterFuzzGroups[i];
		size_t iRoute;

		if ( !__xrtHttpRouterFuzzPatternMatch(
			__xrtHttpRouterFuzzRoutes[
				pGroup->Routes[0]
			].Pattern,
			Path
		) ) {
			continue;
		}
		*pHasPath = true;
		iRoute = __xrtHttpRouterFuzzSelectGroup(
			pGroup, Method, pFlags
		);
		if ( iRoute != SIZE_MAX ) {
			return iRoute;
		}
	}
	return SIZE_MAX;
}



/* 创建固定但高度重叠的 Router，注册顺序刻意不同于匹配优先级。 */
static xhttprouter* __xrtHttpRouterFuzzCreate(void)
{
	xhttprouter* pRouter = xrtHttpRouterCreate(NULL);
	size_t i;

	if ( pRouter == NULL ) {
		abort();
	}
	for ( i = 0;
		i < (sizeof(__xrtHttpRouterFuzzRoutes) /
		 sizeof(__xrtHttpRouterFuzzRoutes[0]));
		i++ ) {
		const xrt_http_router_fuzz_route* pRoute =
			&__xrtHttpRouterFuzzRoutes[i];

		if ( !xrtHttpRouterAdd(
			pRouter, pRoute->Method, pRoute->Pattern,
			(ptr)pRoute->Value
		) ) {
			xrtHttpRouterDestroy(pRouter);
			abort();
		}
	}
	if ( !xrtHttpRouterFreeze(pRouter) ) {
		xrtHttpRouterDestroy(pRouter);
		abort();
	}
	return pRouter;
}



/* 返回进程内复用的只读 Router，避免模糊执行时间被重复构建主导。 */
static xhttprouter* __xrtHttpRouterFuzzRouter(void)
{
	static xhttprouter* pRouter = NULL;

	if ( pRouter == NULL ) {
		pRouter = __xrtHttpRouterFuzzCreate();
	}
	return pRouter;
}



/* 验证实际捕获与底层线性参考捕获逐项一致。 */
static void __xrtHttpRouterFuzzParams(
	const xhttprouteparam* pActual,
	const xhttprouteparam* pExpected,
	size_t iCount
)
{
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		if ( !__xrtHttpRouterFuzzViewEqual(
			pActual[i].Name, pExpected[i].Name
		) || !__xrtHttpRouterFuzzViewEqual(
			pActual[i].Value, pExpected[i].Value
		) ) {
			abort();
		}
	}
}



/* 对一次方法与路径组合执行匹配状态、回退标志和原子容量检查。 */
static void __xrtHttpRouterFuzzMatch(
	xhttprouter* pRouter,
	xstrview Method,
	xstrview Path,
	bool bPathValid
)
{
	xhttproutermatch Match;
	xhttproutermatch QueryMatch;
	xhttprouteparam Actual[XRT_HTTP_ROUTER_FUZZ_PARAMS_MAX];
	xhttprouteparam Expected[XRT_HTTP_ROUTER_FUZZ_PARAMS_MAX];
	xhttprouteparam Before[XRT_HTTP_ROUTER_FUZZ_PARAMS_MAX];
	xhttprouterstatus Status;
	xhttprouterstatus Query;
	xhttproutestatus RouteStatus;
	size_t iActual = 0;
	size_t iExpected = 0;
	size_t iQuery = 0;
	size_t iRoute;
	uint32 iFlags = 0;
	bool bHasPath = false;
	bool bMethodValid = (Method.Size != 0) &&
		xrtHttpTokenValid(Method);

	xrtClearError();
	Query = xrtHttpRouterMatch(
		pRouter, Method, Path, NULL, 0,
		&iQuery, &QueryMatch
	);
	if ( !bMethodValid || !bPathValid ) {
		if ( Query != XHTTP_ROUTER_ERROR ) {
			abort();
		}
		xrtClearError();
		return;
	}
	iRoute = __xrtHttpRouterFuzzSelect(
		Method, Path, &iFlags, &bHasPath
	);
	if ( iRoute == SIZE_MAX ) {
		xhttprouterstatus ExpectedStatus = bHasPath ?
			XHTTP_ROUTER_METHOD_NOT_ALLOWED :
			XHTTP_ROUTER_NOT_FOUND;

		if ( (Query != ExpectedStatus) || (iQuery != 0) ||
			(QueryMatch.Value != NULL) ) {
			abort();
		}
		return;
	}
	RouteStatus = xrtHttpRouteMatch(
		__xrtHttpRouterFuzzRoutes[iRoute].Pattern,
		Path, Expected, XRT_HTTP_ROUTER_FUZZ_PARAMS_MAX,
		&iExpected
	);
	if ( RouteStatus != XHTTP_ROUTE_MATCH ) {
		abort();
	}
	if ( (Query != (iExpected == 0 ?
		XHTTP_ROUTER_MATCH : XHTTP_ROUTER_MORE)) ||
		(iQuery != iExpected) ||
		(QueryMatch.Flags != iFlags) ||
		(QueryMatch.Value != (ptr)
		 __xrtHttpRouterFuzzRoutes[iRoute].Value) ||
		!__xrtHttpRouterFuzzViewEqual(
			QueryMatch.Method,
			__xrtHttpRouterFuzzRoutes[iRoute].Method
		) || !__xrtHttpRouterFuzzViewEqual(
			QueryMatch.Pattern,
			__xrtHttpRouterFuzzRoutes[iRoute].Pattern
		) ) {
		abort();
	}
	Status = xrtHttpRouterMatch(
		pRouter, Method, Path, Actual,
		XRT_HTTP_ROUTER_FUZZ_PARAMS_MAX,
		&iActual, &Match
	);
	if ( (Status != XHTTP_ROUTER_MATCH) ||
		(iActual != iExpected) ||
		(memcmp(&Match, &QueryMatch, sizeof(Match)) != 0) ) {
		abort();
	}
	__xrtHttpRouterFuzzParams(Actual, Expected, iExpected);
	if ( iExpected != 0 ) {
		memset(Actual, 0xA5, sizeof(Actual));
		memcpy(Before, Actual, sizeof(Before));
		if ( xrtHttpRouterMatch(
			pRouter, Method, Path, Actual, iExpected - 1u,
			&iActual, &Match
		) != XHTTP_ROUTER_MORE ||
			(iActual != iExpected) ||
			(memcmp(Actual, Before, sizeof(Actual)) != 0) ) {
			abort();
		}
	}
	xrtClearError();
}



/* 向参考方法集合追加唯一方法。 */
static void __xrtHttpRouterFuzzMethodAdd(
	xstrview* pMethods,
	size_t* pCount,
	xstrview Method
)
{
	size_t i;

	for ( i = 0; i < *pCount; i++ ) {
		if ( __xrtHttpRouterFuzzViewEqual(
			pMethods[i], Method
		) ) {
			return;
		}
	}
	if ( *pCount == XRT_HTTP_ROUTER_FUZZ_METHODS_MAX ) {
		abort();
	}
	pMethods[(*pCount)++] = Method;
}



/* 按注册顺序建立公开 Methods 接口的独立参考结果。 */
static size_t __xrtHttpRouterFuzzMethodsReference(
	xstrview Path,
	xstrview* pMethods
)
{
	static const xstrview Get = XRT_STR_INIT("GET");
	static const xstrview Head = XRT_STR_INIT("HEAD");
	size_t iCount = 0;
	size_t i;

	for ( i = 0;
		i < (sizeof(__xrtHttpRouterFuzzRoutes) /
		 sizeof(__xrtHttpRouterFuzzRoutes[0]));
		i++ ) {
		const xrt_http_router_fuzz_route* pRoute =
			&__xrtHttpRouterFuzzRoutes[i];

		if ( !__xrtHttpRouterFuzzPatternMatch(
			pRoute->Pattern, Path
		) ) {
			continue;
		}
		__xrtHttpRouterFuzzMethodAdd(
			pMethods, &iCount, pRoute->Method
		);
		if ( __xrtHttpRouterFuzzViewEqual(
			pRoute->Method, Get
		) ) {
			__xrtHttpRouterFuzzMethodAdd(
				pMethods, &iCount, Head
			);
		}
	}
	return iCount;
}



/* 对一次路径执行方法枚举、精确计数和短容量原子性检查。 */
static void __xrtHttpRouterFuzzMethods(
	xhttprouter* pRouter,
	xstrview Path,
	bool bPathValid
)
{
	xstrview Actual[XRT_HTTP_ROUTER_FUZZ_METHODS_MAX];
	xstrview Expected[XRT_HTTP_ROUTER_FUZZ_METHODS_MAX];
	xstrview Before[XRT_HTTP_ROUTER_FUZZ_METHODS_MAX];
	xhttprouterstatus Status;
	size_t iActual = 0;
	size_t iExpected;
	size_t i;

	xrtClearError();
	Status = xrtHttpRouterMethods(
		pRouter, Path, NULL, 0, &iActual
	);
	if ( !bPathValid ) {
		if ( Status != XHTTP_ROUTER_ERROR ) {
			abort();
		}
		xrtClearError();
		return;
	}
	iExpected = __xrtHttpRouterFuzzMethodsReference(
		Path, Expected
	);
	if ( Status != (iExpected == 0 ?
		XHTTP_ROUTER_NOT_FOUND : XHTTP_ROUTER_MORE) ||
		(iActual != iExpected) ) {
		abort();
	}
	if ( iExpected == 0 ) {
		return;
	}
	if ( xrtHttpRouterMethods(
		pRouter, Path, Actual,
		XRT_HTTP_ROUTER_FUZZ_METHODS_MAX, &iActual
	) != XHTTP_ROUTER_MATCH || (iActual != iExpected) ) {
		abort();
	}
	for ( i = 0; i < iExpected; i++ ) {
		if ( !__xrtHttpRouterFuzzViewEqual(
			Actual[i], Expected[i]
		) ) {
			abort();
		}
	}
	memset(Actual, 0x5A, sizeof(Actual));
	memcpy(Before, Actual, sizeof(Before));
	if ( xrtHttpRouterMethods(
		pRouter, Path, Actual, iExpected - 1u,
		&iActual
	) != XHTTP_ROUTER_MORE || (iActual != iExpected) ||
		(memcmp(Actual, Before, sizeof(Actual)) != 0) ) {
		abort();
	}
	xrtClearError();
}



/* 统一公开确定性回归和 libFuzzer 使用的本地 Router 协议入口。 */
int xrtHttpRouterFuzzerTestOneInput(const uint8* pData, size_t iSize)
{
	static const xstrview Methods[] = {
		XRT_STR_INIT("GET"),
		XRT_STR_INIT("POST"),
		XRT_STR_INIT("HEAD"),
		XRT_STR_INIT("DELETE"),
		XRT_STR_INIT("PUT"),
		XRT_STR_INIT("PATCH"),
		XRT_STR_INIT("PROPFIND"),
		XRT_STR_INIT("OPTIONS"),
		XRT_STR_INIT("*"),
		XRT_STR_INIT("BAD METHOD")
	};
	xhttprouter* pRouter;
	xstrview Method;
	xstrview Path;
	bool bPathValid;

	if ( ((pData == NULL) && (iSize != 0)) ||
		(iSize > XRT_HTTP_ROUTER_FUZZ_INPUT_MAX) ||
		(iSize < 2u) ) {
		return 0;
	}
	pRouter = __xrtHttpRouterFuzzRouter();
	Method = Methods[pData[0] %
		(sizeof(Methods) / sizeof(Methods[0]))];
	Path.Data = (cstr)(pData + 1u);
	Path.Size = iSize - 1u;
	bPathValid = __xrtHttpRouterFuzzPathValid(Path);
	__xrtHttpRouterFuzzMatch(
		pRouter, Method, Path, bPathValid
	);
	__xrtHttpRouterFuzzMethods(
		pRouter, Path, bPathValid
	);
	return 0;
}



#if defined(XRT_HTTP_ROUTER_FUZZ_LIBFUZZER)

/* 把独立 Router 入口适配为 Clang/libFuzzer 约定符号。 */
int LLVMFuzzerTestOneInput(const uint8* pData, size_t iSize)
{
	return xrtHttpRouterFuzzerTestOneInput(pData, iSize);
}

#endif
