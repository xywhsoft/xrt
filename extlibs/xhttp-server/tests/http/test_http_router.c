#include "../test.h"

#include <xrt/http_router.h>



/* 判断借用视图与字面量逐字节相同。 */
static bool testHttpRouterView(xstrview View, cstr sText)
{
	size_t iSize = strlen(sText);

	return (View.Size == iSize) &&
		((iSize == 0) || (memcmp(View.Data, sText, iSize) == 0));
}



/* 创建包含静态、参数、尾参数、任意方法和自定义方法的冻结 Router。 */
static xhttprouter* testHttpRouterCreate(void)
{
	xhttprouter* pRouter = xrtHttpRouterCreate(NULL);

	testRequire(pRouter != NULL, "HTTP router create failed");
	testRequire(
		xrtHttpRouterAdd(
			pRouter, XRT_STR_LITERAL("GET"),
			XRT_STR_LITERAL("/users/{id}"), (ptr)1
		) && xrtHttpRouterAdd(
			pRouter, XRT_STR_LITERAL("POST"),
			XRT_STR_LITERAL("/users/{name}"), (ptr)2
		) && xrtHttpRouterAdd(
			pRouter, XRT_STR_LITERAL("POST"),
			XRT_STR_LITERAL("/users/me/detail"), (ptr)3
		) && xrtHttpRouterAdd(
			pRouter, XRT_STR_LITERAL("GET"),
			XRT_STR_LITERAL("/users/{id}/other"), (ptr)4
		) && xrtHttpRouterAdd(
			pRouter, XRT_STR_LITERAL("GET"),
			XRT_STR_LITERAL("/files/{path...}"), (ptr)5
		) && xrtHttpRouterAdd(
			pRouter, XRT_STR_LITERAL("*"),
			XRT_STR_LITERAL("/health"), (ptr)6
		) && xrtHttpRouterAdd(
			pRouter, XRT_STR_LITERAL("PROPFIND"),
			XRT_STR_LITERAL("/dav/{path...}"), (ptr)7
		) && xrtHttpRouterAdd(
			pRouter, XRT_STR_LITERAL("GET"),
			XRT_STR_LITERAL("/{root...}"), (ptr)8
		),
		"HTTP router registration failed"
	);
	testRequire(
		(xrtHttpRouterCount(pRouter) == 8u) &&
		(xrtHttpRouterNodes(pRouter) > 1u) &&
		(xrtHttpRouterBytes(pRouter) != 0) &&
		xrtHttpRouterFreeze(pRouter) &&
		xrtHttpRouterFreeze(pRouter) &&
		xrtHttpRouterFrozen(pRouter),
		"HTTP router freeze or statistics mismatch"
	);
	return pRouter;
}



/* 验证方法选择、HEAD 回退、任意方法和 404/405 终态。 */
static void testHttpRouterMethods(xhttprouter* pRouter)
{
	xhttproutermatch Match;
	xhttprouteparam Params[2];
	xstrview Methods[4];
	xstrview Before[4];
	size_t iCount;

	testRequire(
		xrtHttpRouterMatch(
			pRouter, XRT_STR_LITERAL("GET"),
			XRT_STR_LITERAL("/users/42"),
			Params, 2, &iCount, &Match
		) == XHTTP_ROUTER_MATCH && (iCount == 1) &&
		(Match.Value == (ptr)1) && (Match.Flags == 0) &&
		testHttpRouterView(Match.Method, "GET") &&
		testHttpRouterView(Match.Pattern, "/users/{id}") &&
		testHttpRouterView(Params[0].Name, "id") &&
		testHttpRouterView(Params[0].Value, "42"),
		"HTTP router GET match mismatch"
	);
	testRequire(
		xrtHttpRouterMatch(
			pRouter, XRT_STR_LITERAL("POST"),
			XRT_STR_LITERAL("/users/alice"),
			Params, 2, &iCount, &Match
		) == XHTTP_ROUTER_MATCH &&
		(Match.Value == (ptr)2) &&
		testHttpRouterView(Params[0].Name, "name"),
		"HTTP router method-specific parameter name mismatch"
	);
	testRequire(
		xrtHttpRouterMatch(
			pRouter, XRT_STR_LITERAL("HEAD"),
			XRT_STR_LITERAL("/users/42"),
			Params, 2, &iCount, &Match
		) == XHTTP_ROUTER_MATCH &&
		((Match.Flags & XHTTP_ROUTER_HEAD_FALLBACK) != 0) &&
		(Match.Value == (ptr)1),
		"HTTP router HEAD fallback mismatch"
	);
	testRequire(
		xrtHttpRouterMatch(
			pRouter, XRT_STR_LITERAL("PATCH"),
			XRT_STR_LITERAL("/health"),
			Params, 2, &iCount, &Match
		) == XHTTP_ROUTER_MATCH &&
		((Match.Flags & XHTTP_ROUTER_ANY_METHOD) != 0) &&
		(Match.Value == (ptr)6),
		"HTTP router any-method match mismatch"
	);
	testRequire(
		xrtHttpRouterMatch(
			pRouter, XRT_STR_LITERAL("DELETE"),
			XRT_STR_LITERAL("/users/42"),
			Params, 2, &iCount, &Match
		) == XHTTP_ROUTER_METHOD_NOT_ALLOWED &&
		(iCount == 0) && (Match.Value == NULL),
		"HTTP router 405 mismatch"
	);
	testRequire(
		xrtHttpRouterMatch(
			pRouter, XRT_STR_LITERAL("GET"),
			XRT_STR_LITERAL("/missing"),
			Params, 2, &iCount, &Match
		) == XHTTP_ROUTER_MATCH &&
		(Match.Value == (ptr)8),
		"HTTP router root tail fallback mismatch"
	);
	memset(Methods, 0xA5, sizeof(Methods));
	memcpy(Before, Methods, sizeof(Before));
	testRequire(
		xrtHttpRouterMethods(
			pRouter, XRT_STR_LITERAL("/users/42"),
			Methods, 2, &iCount
		) == XHTTP_ROUTER_MORE && (iCount == 3) &&
		(memcmp(Methods, Before, sizeof(Methods)) == 0),
		"HTTP router method list short storage was not atomic"
	);
	testRequire(
		xrtHttpRouterMethods(
			pRouter, XRT_STR_LITERAL("/users/42"),
			Methods, 4, &iCount
		) == XHTTP_ROUTER_MATCH && (iCount == 3) &&
		testHttpRouterView(Methods[0], "GET") &&
		testHttpRouterView(Methods[1], "HEAD") &&
		testHttpRouterView(Methods[2], "POST"),
		"HTTP router allowed method list mismatch"
	);
	testRequire(
		xrtHttpRouterMethods(
			pRouter, XRT_STR_LITERAL("/health"),
			Methods, 4, &iCount
		) == XHTTP_ROUTER_MATCH && (iCount == 3) &&
		testHttpRouterView(Methods[0], "*") &&
		testHttpRouterView(Methods[1], "GET") &&
		testHttpRouterView(Methods[2], "HEAD"),
		"HTTP router any and fallback method list mismatch"
	);
}



/* 验证静态死路可以迭代回溯到参数分支，尾参数仍是最后选择。 */
static void testHttpRouterPrecedence(xhttprouter* pRouter)
{
	xhttproutermatch Match;
	xhttprouteparam Params[2];
	size_t iCount;

	testRequire(
		xrtHttpRouterMatch(
			pRouter, XRT_STR_LITERAL("GET"),
			XRT_STR_LITERAL("/users/me/other"),
			Params, 2, &iCount, &Match
		) == XHTTP_ROUTER_MATCH &&
		(Match.Value == (ptr)4) && (iCount == 1) &&
		testHttpRouterView(Params[0].Value, "me"),
		"HTTP router static dead-end backtracking failed"
	);
	testRequire(
		xrtHttpRouterMatch(
			pRouter, XRT_STR_LITERAL("GET"),
			XRT_STR_LITERAL("/files/a/b%2Fc"),
			Params, 2, &iCount, &Match
		) == XHTTP_ROUTER_MATCH &&
		(Match.Value == (ptr)5) &&
		testHttpRouterView(Params[0].Value, "a/b%2Fc"),
		"HTTP router tail parameter mismatch"
	);
	testRequire(
		xrtHttpRouterMatch(
			pRouter, XRT_STR_LITERAL("PROPFIND"),
			XRT_STR_LITERAL("/dav/docs"),
			Params, 2, &iCount, &Match
		) == XHTTP_ROUTER_MATCH &&
		(Match.Value == (ptr)7),
		"HTTP router extension method mismatch"
	);
}



/* 验证容量查询不写捕获，并且冻结后注册被稳定拒绝。 */
static void testHttpRouterStorage(xhttprouter* pRouter)
{
	xhttproutermatch Match;
	xhttprouteparam Params[2];
	xhttprouteparam Before[2];
	size_t iCount;

	memset(Params, 0xA5, sizeof(Params));
	memcpy(Before, Params, sizeof(Before));
	testRequire(
		xrtHttpRouterMatch(
			pRouter, XRT_STR_LITERAL("GET"),
			XRT_STR_LITERAL("/users/42"),
			Params, 0, &iCount, &Match
		) == XHTTP_ROUTER_MORE && (iCount == 1) &&
		(Match.Value == (ptr)1) &&
		(memcmp(Params, Before, sizeof(Params)) == 0),
		"HTTP router short capture storage was not atomic"
	);
	testRequire(
		!xrtHttpRouterAdd(
			pRouter, XRT_STR_LITERAL("GET"),
			XRT_STR_LITERAL("/late"), NULL
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP router accepted registration after freeze"
	);
	xrtClearError();
}



/* 验证冲突注册与显式资源限额不会留下可见半提交路由。 */
static void testHttpRouterRegistration(void)
{
	xhttprouterconfig Config;
	xhttprouter* pRouter;

	xrtHttpRouterConfigInit(&Config);
	Config.MaxRoutes = 1u;
	Config.MaxNodes = 4u;
	Config.MaxBytes = 64u;
	pRouter = xrtHttpRouterCreate(&Config);
	testRequire(pRouter != NULL, "limited HTTP router create failed");
	testRequire(
		xrtHttpRouterAdd(
			pRouter, XRT_STR_LITERAL("GET"),
			XRT_STR_LITERAL("/{id}"), NULL
		) && !xrtHttpRouterAdd(
			pRouter, XRT_STR_LITERAL("GET"),
			XRT_STR_LITERAL("/{name}"), NULL
		) && (xrtHttpRouterCount(pRouter) == 1u) &&
		(xrtErrorKind(xrtGetError()) == XERR_VALUE),
		"HTTP router structural method conflict mismatch"
	);
	xrtClearError();
	testRequire(
		!xrtHttpRouterAdd(
			pRouter, XRT_STR_LITERAL("POST"),
			XRT_STR_LITERAL("/{name}"), NULL
		) && (xrtHttpRouterCount(pRouter) == 1u) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"HTTP router route limit mismatch"
	);
	xrtClearError();
	xrtHttpRouterDestroy(pRouter);
}



/* 验证 Router 配置支持未对齐存储、立即快照并拒绝回绕地址。 */
static void testHttpRouterMemoryContracts(void)
{
	uint8 Storage[sizeof(xhttprouterconfig) + 2u];
	union {
		uint64 Align;
		uint8 Bytes[sizeof(xhttproutermatch) + 1u];
	} MatchStorage;
	union {
		uint64 Align;
		uint8 Bytes[sizeof(xhttprouteparam) + 1u];
	} ParamStorage;
	union {
		uint64 Align;
		uint8 Bytes[sizeof(xstrview) * 2u + 1u];
	} MethodStorage;
	union {
		uint64 Align;
		uint8 Bytes[sizeof(size_t) + 1u];
	} CountStorage;
	xhttproutermatch* pMatch = (xhttproutermatch*)(void*)(
		MatchStorage.Bytes + 1u
	);
	xhttprouteparam* pParam = (xhttprouteparam*)(void*)(
		ParamStorage.Bytes + 1u
	);
	xstrview* pMethods = (xstrview*)(void*)(
		MethodStorage.Bytes + 1u
	);
	size_t* pCount = (size_t*)(void*)(CountStorage.Bytes + 1u);
	xhttprouterconfig Config;
	xhttproutermatch Match;
	xhttproutermatch MatchBefore;
	xhttprouteparam Param;
	xstrview Methods[2];
	xhttprouter* pRouter;
	size_t iCount;

	memset(Storage, 0xA5, sizeof(Storage));
	xrtHttpRouterConfigInit(
		(xhttprouterconfig*)(void*)(Storage + 1u)
	);
	memcpy(&Config, Storage + 1u, sizeof(Config));
	testRequire((Storage[0] == 0xA5) &&
		(Storage[sizeof(Storage) - 1u] == 0xA5) &&
		(Config.MaxRoutes == 4096u) &&
		(Config.MaxNodes == 16384u) &&
		(Config.MaxBytes == 4194304u),
		"HTTP Router config init did not support unaligned storage");
	Config.MaxRoutes = 1u;
	Config.MaxNodes = 2u;
	Config.MaxBytes = 16u;
	memcpy(Storage + 1u, &Config, sizeof(Config));
	pRouter = xrtHttpRouterCreate(
		(const xhttprouterconfig*)(const void*)(Storage + 1u)
	);
	testRequire(pRouter != NULL,
		"HTTP Router create did not support unaligned config");
	memset(Storage + 1u, 0, sizeof(Config));
	testRequire(xrtHttpRouterAdd(
		pRouter,
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("/"),
		NULL
	), "HTTP Router retained caller config storage");
	xrtHttpRouterDestroy(pRouter);

	xrtHttpRouterConfigInit((xhttprouterconfig*)(uintptr_t)(
		UINTPTR_MAX - 1u
	));
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"HTTP Router config init accepted wrapping output");
	xrtClearError();
	testRequire((xrtHttpRouterCreate(
		(const xhttprouterconfig*)(uintptr_t)(UINTPTR_MAX - 1u)
	) == NULL) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP Router create accepted wrapping config");
	xrtClearError();

	/* 匹配和方法枚举的全部固定输出都支持打包存储。 */
	pRouter = xrtHttpRouterCreate(NULL);
	testRequire(
		(pRouter != NULL) && xrtHttpRouterAdd(
			pRouter,
			XRT_STR_LITERAL("GET"),
			XRT_STR_LITERAL("/packed/{id}"),
			(ptr)1
		) && xrtHttpRouterFreeze(pRouter),
		"HTTP Router packed-output fixture failed"
	);
	iCount = SIZE_MAX;
	memcpy(pCount, &iCount, sizeof(iCount));
	testRequire(
		xrtHttpRouterMatch(
			pRouter,
			XRT_STR_LITERAL("GET"),
			XRT_STR_LITERAL("/packed/42"),
			pParam,
			1,
			pCount,
			pMatch
		) == XHTTP_ROUTER_MATCH,
		"HTTP Router rejected unaligned match outputs"
	);
	memcpy(&iCount, pCount, sizeof(iCount));
	memcpy(&Match, pMatch, sizeof(Match));
	memcpy(&Param, pParam, sizeof(Param));
	testRequire(
		(iCount == 1u) && (Match.Value == (ptr)1) &&
		testHttpRouterView(Param.Name, "id") &&
		testHttpRouterView(Param.Value, "42"),
		"HTTP Router unaligned match output mismatch"
	);
	iCount = SIZE_MAX;
	memcpy(pCount, &iCount, sizeof(iCount));
	testRequire(
		xrtHttpRouterMethods(
			pRouter,
			XRT_STR_LITERAL("/packed/42"),
			pMethods,
			2,
			pCount
		) == XHTTP_ROUTER_MATCH,
		"HTTP Router rejected unaligned method outputs"
	);
	memcpy(&iCount, pCount, sizeof(iCount));
	memcpy(Methods, pMethods, sizeof(Methods));
	testRequire(
		(iCount == 2u) &&
		testHttpRouterView(Methods[0], "GET") &&
		testHttpRouterView(Methods[1], "HEAD"),
		"HTTP Router unaligned method output mismatch"
	);

	/* 参数错误必须在发布任何输出之前被拒绝。 */
	iCount = 73u;
	memset(&Match, 0xA5, sizeof(Match));
	memcpy(&MatchBefore, &Match, sizeof(MatchBefore));
	testRequire(
		xrtHttpRouterMatch(
			pRouter,
			XRT_STR_LITERAL("GET"),
			XRT_STR_LITERAL("/packed/42"),
			(xhttprouteparam*)(uintptr_t)UINTPTR_MAX,
			1,
			&iCount,
			&Match
		) == XHTTP_ROUTER_ERROR,
		"HTTP Router accepted a wrapped capture range"
	);
	testRequire(
		(iCount == 73u) &&
		(memcmp(&Match, &MatchBefore, sizeof(Match)) == 0),
		"HTTP Router argument error changed match outputs"
	);
	xrtClearError();
	iCount = 91u;
	testRequire(
		xrtHttpRouterMethods(
			pRouter,
			XRT_STR_LITERAL("/packed/42"),
			(xstrview*)(uintptr_t)UINTPTR_MAX,
			1,
			&iCount
		) == XHTTP_ROUTER_ERROR,
		"HTTP Router accepted a wrapped method range"
	);
	testRequire(
		iCount == 91u,
		"HTTP Router method argument error changed Count"
	);
	xrtClearError();
	xrtHttpRouterDestroy(pRouter);
}



/* 验证宽静态节点排序和无分支深路径都能稳定命中。 */
static void testHttpRouterScaleShape(void)
{
	xhttprouter* pRouter = xrtHttpRouterCreate(NULL);
	xhttproutermatch Match;
	char Pattern[1024];
	char Path[64];
	size_t iCount;
	size_t iSize;
	size_t i;

	testRequire(pRouter != NULL, "scaled HTTP Router create failed");
	for ( i = 512u; i != 0; i-- ) {
		iSize = (size_t)snprintf(
			Pattern,
			sizeof(Pattern),
			"/wide/%04u",
			(unsigned int)(i - 1u)
		);
		testRequire(
			xrtHttpRouterAdd(
				pRouter,
				XRT_STR_LITERAL("GET"),
				(xstrview){ Pattern, iSize },
				(ptr)(uintptr_t)i
			),
			"wide HTTP Router registration failed"
		);
	}
	iSize = 0;
	for ( i = 0; i < 256u; i++ ) {
		Pattern[iSize++] = '/';
		Pattern[iSize++] = 'a';
	}
	testRequire(
		xrtHttpRouterAdd(
			pRouter,
			XRT_STR_LITERAL("GET"),
			(xstrview){ Pattern, iSize },
			(ptr)(uintptr_t)999u
		) && xrtHttpRouterFreeze(pRouter),
		"scaled HTTP Router freeze failed"
	);
	for ( i = 0; i < 3u; i++ ) {
		static const unsigned int Index[] = { 0u, 257u, 511u };

		iSize = (size_t)snprintf(
			Path,
			sizeof(Path),
			"/wide/%04u",
			Index[i]
		);
		testRequire(
			xrtHttpRouterMatch(
				pRouter,
				XRT_STR_LITERAL("GET"),
				(xstrview){ Path, iSize },
				NULL,
				0,
				&iCount,
				&Match
			) == XHTTP_ROUTER_MATCH &&
			(Match.Value == (ptr)(uintptr_t)(Index[i] + 1u)),
			"wide HTTP Router binary lookup mismatch"
		);
	}
	testRequire(
		xrtHttpRouterMatch(
			pRouter,
			XRT_STR_LITERAL("GET"),
			(xstrview){ Pattern, 512u },
			NULL,
			0,
			&iCount,
			&Match
		) == XHTTP_ROUTER_MATCH &&
		(Match.Value == (ptr)(uintptr_t)999u),
		"deep HTTP Router linear path mismatch"
	);
	xrtHttpRouterDestroy(pRouter);
}



/* 验证超过本地快速集合的扩展方法仍能唯一、完整且无隐藏上限地列出。 */
static void testHttpRouterManyMethods(void)
{
	xhttprouter* pRouter = xrtHttpRouterCreate(NULL);
	union {
		uint64 Align;
		uint8 Bytes[sizeof(xstrview) * 24u + 1u];
	} MethodStorage;
	xstrview* pMethods = (xstrview*)(void*)(
		MethodStorage.Bytes + 1u
	);
	xstrview Methods[24];
	char Method[16];
	size_t iCount;
	size_t i;

	testRequire(pRouter != NULL, "many-method HTTP router create failed");
	for ( i = 0; i < 20u; i++ ) {
		size_t iMethod = (size_t)snprintf(
			Method, sizeof(Method), "M%02u", (unsigned int)i
		);

		testRequire(
			xrtHttpRouterAdd(
				pRouter, (xstrview){ Method, iMethod },
				XRT_STR_LITERAL("/many"), (ptr)(uintptr_t)i
			),
			"many-method HTTP router registration failed"
		);
	}
	testRequire(
		xrtHttpRouterFreeze(pRouter) &&
		xrtHttpRouterMethods(
			pRouter, XRT_STR_LITERAL("/many"),
			pMethods, 24, &iCount
		) == XHTTP_ROUTER_MATCH && (iCount == 20u),
		"HTTP router large method enumeration failed"
	);
	memcpy(Methods, pMethods, sizeof(Methods));
	testRequire(
		testHttpRouterView(Methods[0], "M00") &&
		testHttpRouterView(Methods[19], "M19"),
		"HTTP router large method list mismatch"
	);
	xrtHttpRouterDestroy(pRouter);
}



/* 运行不可变 HTTP Router 的注册、编译和匹配契约。 */
int main(void)
{
	xhttprouter* pRouter = testHttpRouterCreate();

	testHttpRouterMethods(pRouter);
	testHttpRouterPrecedence(pRouter);
	testHttpRouterStorage(pRouter);
	xrtHttpRouterDestroy(pRouter);
	testHttpRouterRegistration();
	testHttpRouterMemoryContracts();
	testHttpRouterManyMethods();
	testHttpRouterScaleShape();
	puts("[PASS] HTTP router");
	return 0;
}
