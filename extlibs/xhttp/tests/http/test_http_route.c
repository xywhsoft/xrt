#include "../test.h"

#include <xrt/http_route.h>



/* 判断两个借用字符串视图是否逐字节相同。 */
static bool testHttpRouteViewEqual(xstrview Left, xstrview Right)
{
	return (Left.Size == Right.Size) &&
		((Left.Size == 0) ||
		 (memcmp(Left.Data, Right.Data, Left.Size) == 0));
}



/* 验证模板语法、参数计数、名称规则和末尾通配约束。 */
static void testHttpRouteValidate(void)
{
	static const xstrview Valid[] = {
		XRT_STR_INIT("/"),
		XRT_STR_INIT("/users"),
		XRT_STR_INIT("/users/{id}"),
		XRT_STR_INIT("/files/{path...}"),
		XRT_STR_INIT("//strict/"),
		XRT_STR_INIT("/v1/{_name}/%7Braw%7D")
	};
	static const size_t Counts[] = { 0, 0, 1, 1, 0, 1 };
	static const xstrview Invalid[] = {
		XRT_STR_INIT(""),
		XRT_STR_INIT("relative"),
		XRT_STR_INIT("/users/{}"),
		XRT_STR_INIT("/users/{1id}"),
		XRT_STR_INIT("/users/{bad-name}"),
		XRT_STR_INIT("/users/{id}/{id}"),
		XRT_STR_INIT("/{tail...}/more"),
		XRT_STR_INIT("/users/{id"),
		XRT_STR_INIT("/users/id}"),
		XRT_STR_INIT("/users/%Q0"),
		XRT_STR_INIT("/users?all"),
		XRT_STR_INIT("/users#part"),
		XRT_STR_INIT("/users\\name")
	};
	size_t i;

	for ( i = 0; i < (sizeof(Valid) / sizeof(Valid[0])); i++ ) {
		size_t iParameters = SIZE_MAX;

		testRequire(
			xrtHttpRouteValidate(Valid[i], &iParameters) &&
			(iParameters == Counts[i]),
			"valid HTTP route pattern was rejected"
		);
	}
	for ( i = 0; i < (sizeof(Invalid) / sizeof(Invalid[0])); i++ ) {
		size_t iParameters = SIZE_MAX;

		testRequire(
			!xrtHttpRouteValidate(Invalid[i], &iParameters) &&
			(iParameters == 0) &&
			(xrtErrorKind(xrtGetError()) == XERR_VALUE),
			"invalid HTTP route pattern was accepted"
		);
		xrtClearError();
	}
}



/* 验证静态路径逐字节匹配，并保留重复斜杠和尾斜杠语义。 */
static void testHttpRouteStatic(void)
{
	size_t iCount = SIZE_MAX;

	testRequire(
		(xrtHttpRouteMatch(
			XRT_STR_LITERAL("/"), XRT_STR_LITERAL("/"),
			NULL, 0, &iCount
		 ) == XHTTP_ROUTE_MATCH) && (iCount == 0),
		"HTTP root route mismatch"
	);
	testRequire(
		(xrtHttpRouteMatch(
			XRT_STR_LITERAL("/users"), XRT_STR_LITERAL("/users"),
			NULL, 0, &iCount
		 ) == XHTTP_ROUTE_MATCH) && (iCount == 0),
		"HTTP static route mismatch"
	);
	testRequire(
		(xrtHttpRouteMatch(
			XRT_STR_LITERAL("/users"), XRT_STR_LITERAL("/Users"),
			NULL, 0, &iCount
		 ) == XHTTP_ROUTE_MISS) && (iCount == 0),
		"HTTP route ignored path case"
	);
	testRequire(
		(xrtHttpRouteMatch(
			XRT_STR_LITERAL("/a/"), XRT_STR_LITERAL("/a"),
			NULL, 0, &iCount
		 ) == XHTTP_ROUTE_MISS) &&
		(xrtHttpRouteMatch(
			XRT_STR_LITERAL("/a//b"), XRT_STR_LITERAL("/a/b"),
			NULL, 0, &iCount
		 ) == XHTTP_ROUTE_MISS) &&
		(xrtHttpRouteMatch(
			XRT_STR_LITERAL("/a//b"), XRT_STR_LITERAL("/a//b"),
			NULL, 0, &iCount
		 ) == XHTTP_ROUTE_MATCH),
		"HTTP route collapsed structural slashes"
	);
}



/* 验证单段参数、尾参数、空尾和编码斜杠都保留原始借用视图。 */
static void testHttpRouteParameters(void)
{
	xhttprouteparam Params[3];
	const xhttprouteparam* pFound;
	size_t iCount = 0;

	memset(Params, 0, sizeof(Params));
	testRequire(
		xrtHttpRouteMatch(
			XRT_STR_LITERAL("/users/{id}/files/{path...}"),
			XRT_STR_LITERAL("/users/alice/files/a/b%2Fc"),
			Params, 3, &iCount
		) == XHTTP_ROUTE_MATCH && (iCount == 2) &&
		testHttpRouteViewEqual(
			Params[0].Name, XRT_STR_LITERAL("id")
		) && testHttpRouteViewEqual(
			Params[0].Value, XRT_STR_LITERAL("alice")
		) && testHttpRouteViewEqual(
			Params[1].Name, XRT_STR_LITERAL("path")
		) && testHttpRouteViewEqual(
			Params[1].Value, XRT_STR_LITERAL("a/b%2Fc")
		),
		"HTTP route parameter capture mismatch"
	);
	pFound = xrtHttpRouteParam(
		Params, iCount, XRT_STR_LITERAL("path")
	);
	testRequire(
		(pFound == &Params[1]) &&
		(xrtHttpRouteParam(
			Params, iCount, XRT_STR_LITERAL("missing")
		 ) == NULL),
		"HTTP route parameter lookup mismatch"
	);
	testRequire(
		xrtHttpRouteMatch(
			XRT_STR_LITERAL("/files/{path...}"),
			XRT_STR_LITERAL("/files/"),
			Params, 3, &iCount
		) == XHTTP_ROUTE_MATCH && (iCount == 1) &&
		(Params[0].Value.Size == 0),
		"HTTP route empty tail mismatch"
	);
	testRequire(
		xrtHttpRouteMatch(
			XRT_STR_LITERAL("/files/{path...}"),
			XRT_STR_LITERAL("/files"),
			Params, 3, &iCount
		) == XHTTP_ROUTE_MISS && (iCount == 0),
		"HTTP route tail ignored its separating slash"
	);
	testRequire(
		xrtHttpRouteMatch(
			XRT_STR_LITERAL("/{path...}"),
			XRT_STR_LITERAL("/"),
			Params, 3, &iCount
		) == XHTTP_ROUTE_MATCH && (iCount == 1) &&
		(Params[0].Value.Size == 0),
		"HTTP route root tail mismatch"
	);
	testRequire(
		xrtHttpRouteMatch(
			XRT_STR_LITERAL("/users/{id}"),
			XRT_STR_LITERAL("/users/"),
			Params, 3, &iCount
		) == XHTTP_ROUTE_MISS && (iCount == 0),
		"HTTP route single parameter accepted an empty segment"
	);
}



/* 验证容量查询和不足结果不会部分覆盖调用方捕获数组。 */
static void testHttpRouteStorage(void)
{
	xhttprouteparam Params[2];
	xhttprouteparam Before[2];
	size_t iCount = 0;

	memset(Params, 0xA5, sizeof(Params));
	memcpy(Before, Params, sizeof(Before));
	testRequire(
		xrtHttpRouteMatch(
			XRT_STR_LITERAL("/{a}/{b}"),
			XRT_STR_LITERAL("/one/two"),
			NULL, 0, &iCount
		) == XHTTP_ROUTE_MORE && (iCount == 2),
		"HTTP route capture count query mismatch"
	);
	testRequire(
		xrtHttpRouteMatch(
			XRT_STR_LITERAL("/{a}/{b}"),
			XRT_STR_LITERAL("/one/two"),
			Params, 1, &iCount
		) == XHTTP_ROUTE_MORE && (iCount == 2) &&
		(memcmp(Params, Before, sizeof(Params)) == 0),
		"HTTP route short storage was not atomic"
	);
}



/* 验证捕获数量不受旧实现的 32 参数内嵌数组限制。 */
static void testHttpRouteManyParameters(void)
{
	char Pattern[2048];
	char Path[2048];
	xhttprouteparam Params[40];
	size_t iPatternSize = 0;
	size_t iPathSize = 0;
	size_t iCount = 0;
	size_t i;

	Pattern[0] = '\0';
	Path[0] = '\0';
	for ( i = 0; i < 40u; i++ ) {
		int iPatternPart = snprintf(
			Pattern + iPatternSize,
			sizeof(Pattern) - iPatternSize,
			"/{p%u}",
			(unsigned int)i
		);
		int iPathPart = snprintf(
			Path + iPathSize,
			sizeof(Path) - iPathSize,
			"/v%u",
			(unsigned int)i
		);

		testRequire((iPatternPart > 0) && (iPathPart > 0) &&
			((size_t)iPatternPart <
			 (sizeof(Pattern) - iPatternSize)) &&
			((size_t)iPathPart < (sizeof(Path) - iPathSize)),
			"HTTP route many-parameter fixture overflowed"
		);
		iPatternSize += (size_t)iPatternPart;
		iPathSize += (size_t)iPathPart;
	}
	testRequire(xrtHttpRouteMatch(
		(xstrview){ Pattern, iPatternSize },
		(xstrview){ Path, iPathSize },
		Params,
		40u,
		&iCount
	) == XHTTP_ROUTE_MATCH && (iCount == 40u) &&
		testHttpRouteViewEqual(
			Params[39].Name, XRT_STR_LITERAL("p39")
		) && testHttpRouteViewEqual(
			Params[39].Value, XRT_STR_LITERAL("v39")
		), "HTTP route did not preserve more than 32 captures");
}



/* 验证非法路径与参数别名等调用错误保持清晰终态。 */
static void testHttpRouteErrors(void)
{
	xhttprouteparam Params[1];
	size_t iCount = SIZE_MAX;

	testRequire(
		xrtHttpRouteMatch(
			XRT_STR_LITERAL("/{id}"), XRT_STR_LITERAL("relative"),
			Params, 1, &iCount
		) == XHTTP_ROUTE_ERROR && (iCount == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_VALUE),
		"HTTP route invalid path error mismatch"
	);
	xrtClearError();
	testRequire(
		xrtHttpRouteMatch(
			XRT_STR_LITERAL("/{id}"), XRT_STR_LITERAL("/%Q0"),
			Params, 1, &iCount
		) == XHTTP_ROUTE_ERROR && (iCount == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_VALUE),
		"HTTP route malformed percent path was accepted"
	);
	xrtClearError();
}



/* 验证路由固定描述符支持未对齐存储并拒绝地址回绕。 */
static void testHttpRouteMemoryContracts(void)
{
	uint8 ParamsStorage[(sizeof(xhttprouteparam) * 2u) + 2u];
	uint8 CountStorage[sizeof(size_t) + 2u];
	xhttprouteparam Params[2];
	const xhttprouteparam* pFound;
	xhttprouteparam Found;
	size_t iCount;

	memset(ParamsStorage, 0xA5, sizeof(ParamsStorage));
	memset(CountStorage, 0xA5, sizeof(CountStorage));
	testRequire(xrtHttpRouteValidate(
		XRT_STR_LITERAL("/{id}/{tail...}"),
		(size_t*)(void*)(CountStorage + 1u)
	), "HTTP route validate rejected an unaligned count output");
	memcpy(&iCount, CountStorage + 1u, sizeof(iCount));
	testRequire(iCount == 2u,
		"HTTP route validate published the wrong count");
	testRequire(xrtHttpRouteMatch(
		XRT_STR_LITERAL("/{id}/{tail...}"),
		XRT_STR_LITERAL("/42/a/b"),
		(xhttprouteparam*)(void*)(ParamsStorage + 1u),
		2u,
		(size_t*)(void*)(CountStorage + 1u)
	) == XHTTP_ROUTE_MATCH,
		"HTTP route match rejected unaligned outputs");
	memcpy(&iCount, CountStorage + 1u, sizeof(iCount));
	memcpy(Params, ParamsStorage + 1u, sizeof(Params));
	testRequire((iCount == 2u) &&
		testHttpRouteViewEqual(
			Params[0].Name, XRT_STR_LITERAL("id")
		) && testHttpRouteViewEqual(
			Params[0].Value, XRT_STR_LITERAL("42")
		) && testHttpRouteViewEqual(
			Params[1].Value, XRT_STR_LITERAL("a/b")
		), "HTTP route match published invalid captures");
	pFound = xrtHttpRouteParam(
		(const xhttprouteparam*)(const void*)(ParamsStorage + 1u),
		2u,
		XRT_STR_LITERAL("tail")
	);
	testRequire(pFound == (const xhttprouteparam*)(const void*)(
		ParamsStorage + 1u + sizeof(xhttprouteparam)
	), "HTTP route lookup returned the wrong unaligned descriptor");
	memcpy(&Found, pFound, sizeof(Found));
	testRequire(testHttpRouteViewEqual(
		Found.Value, XRT_STR_LITERAL("a/b")
	), "HTTP route lookup returned invalid borrowed data");
	testRequire(
		(ParamsStorage[0] == 0xA5) &&
		(ParamsStorage[sizeof(ParamsStorage) - 1u] == 0xA5) &&
		(CountStorage[0] == 0xA5) &&
		(CountStorage[sizeof(CountStorage) - 1u] == 0xA5),
		"HTTP route wrote outside unaligned storage"
	);

	testRequire(!xrtHttpRouteValidate(
		XRT_STR_LITERAL("/{id}"),
		(size_t*)(uintptr_t)(UINTPTR_MAX - 1u)
	), "HTTP route validate accepted a wrapping count output");
	xrtClearError();
	testRequire(xrtHttpRouteMatch(
		XRT_STR_LITERAL("/{id}"),
		XRT_STR_LITERAL("/42"),
		(xhttprouteparam*)(uintptr_t)(UINTPTR_MAX - 1u),
		1u,
		&iCount
	) == XHTTP_ROUTE_ERROR,
		"HTTP route match accepted a wrapping capture array");
	xrtClearError();
	testRequire(xrtHttpRouteMatch(
		XRT_STR_LITERAL("/{id}"),
		XRT_STR_LITERAL("/42"),
		Params,
		1u,
		(size_t*)(uintptr_t)(UINTPTR_MAX - 1u)
	) == XHTTP_ROUTE_ERROR,
		"HTTP route match accepted a wrapping count output");
	xrtClearError();
	testRequire(xrtHttpRouteParam(
		(const xhttprouteparam*)(uintptr_t)(UINTPTR_MAX - 1u),
		1u,
		XRT_STR_LITERAL("id")
	) == NULL, "HTTP route lookup accepted a wrapping array");
	xrtClearError();
}



/* 运行 HTTP 路由协议层的全部确定性边界。 */
int main(void)
{
	testHttpRouteValidate();
	testHttpRouteStatic();
	testHttpRouteParameters();
	testHttpRouteStorage();
	testHttpRouteManyParameters();
	testHttpRouteErrors();
	testHttpRouteMemoryContracts();
	puts("[PASS] HTTP route protocol");
	return 0;
}
