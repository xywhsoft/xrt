#include "../test.h"

#include <xrt/http_router.h>



/* 计数分配器同时使用系统堆完成 Router 构建。 */
typedef struct test_http_router_allocator {
	size_t Allocations;
	size_t Reallocations;
} test_http_router_allocator;



/* 记录一次 Router 构建分配。 */
static ptr testHttpRouterAlloc(ptr pContext, size_t iSize)
{
	test_http_router_allocator* pState =
		(test_http_router_allocator*)pContext;

	pState->Allocations++;
	return malloc(iSize);
}



/* 记录一次 Router 构建重分配。 */
static ptr testHttpRouterRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	test_http_router_allocator* pState =
		(test_http_router_allocator*)pContext;

	pState->Reallocations++;
	return realloc(pMemory, iSize);
}



/* 使用系统堆释放计数分配器内存。 */
static void testHttpRouterFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* 验证冻结 Router 的静态、回溯和尾捕获热路径不分配内存。 */
int main(void)
{
	test_http_router_allocator State;
	xallocator Allocator;
	xhttprouter* pRouter;
	xhttproutermatch Match;
	xhttprouteparam Params[2];
	xstrview Methods[4];
	size_t iAllocations;
	size_t iReallocations;
	size_t iCount;

	memset(&State, 0, sizeof(State));
	Allocator.Context = &State;
	Allocator.Alloc = testHttpRouterAlloc;
	Allocator.Realloc = testHttpRouterRealloc;
	Allocator.Free = testHttpRouterFree;
	testRequire(
		xrtSetAllocator(&Allocator),
		"HTTP router counting allocator install failed"
	);
	pRouter = xrtHttpRouterCreate(NULL);
	testRequire(
		(pRouter != NULL) &&
		xrtHttpRouterAdd(
			pRouter, XRT_STR_LITERAL("POST"),
			XRT_STR_LITERAL("/a/static/end"), (ptr)1
		) && xrtHttpRouterAdd(
			pRouter, XRT_STR_LITERAL("GET"),
			XRT_STR_LITERAL("/a/{id}/{rest...}"), (ptr)2
		) && xrtHttpRouterFreeze(pRouter),
		"HTTP router no-allocation fixture failed"
	);
	iAllocations = State.Allocations;
	iReallocations = State.Reallocations;
	testRequire(
		xrtHttpRouterMatch(
			pRouter, XRT_STR_LITERAL("GET"),
			XRT_STR_LITERAL("/a/static/files/a.txt"),
			Params, 2, &iCount, &Match
		) == XHTTP_ROUTER_MATCH && (iCount == 2) &&
		(Match.Value == (ptr)2) &&
		xrtHttpRouterMethods(
			pRouter,
			XRT_STR_LITERAL("/a/static/files/a.txt"),
			Methods, 4, &iCount
		) == XHTTP_ROUTER_MATCH && (iCount == 2) &&
		(State.Allocations == iAllocations) &&
		(State.Reallocations == iReallocations),
		"HTTP router hot path allocated memory"
	);
	xrtHttpRouterDestroy(pRouter);
	puts("[PASS] http_router_noalloc");
	return 0;
}
