#include "../test.h"

#include <xrt/http_router.h>
#include <xrt/thread.h>



#define TEST_HTTP_ROUTER_THREADS 6u
#define TEST_HTTP_ROUTER_ROUTES 256u
#define TEST_HTTP_ROUTER_ROUNDS 5000u



/* 每个工作线程只读共享 Router，并持有独立匹配输出。 */
typedef struct test_http_router_thread {
	const xhttprouter* Router;
	size_t Index;
} test_http_router_thread;



/* 反复混合静态二分命中和静态死路后的参数分支回溯。 */
static int32 testHttpRouterThreadEntry(ptr pData)
{
	test_http_router_thread* pThread =
		(test_http_router_thread*)pData;
	char Path[96];
	size_t i;

	for ( i = 0; i < TEST_HTTP_ROUTER_ROUNDS; i++ ) {
		xhttproutermatch Match;
		xhttprouteparam Params[2];
		size_t iRoute = (i * 17u + pThread->Index * 31u) %
			TEST_HTTP_ROUTER_ROUTES;
		size_t iPath;
		size_t iCount;

		iPath = (size_t)snprintf(
			Path, sizeof(Path),
			"/items/%03u/detail",
			(unsigned int)iRoute
		);
		if ( xrtHttpRouterMatch(
			pThread->Router,
			XRT_STR_LITERAL("GET"),
			(xstrview){ Path, iPath },
			Params, 2, &iCount, &Match
		) != XHTTP_ROUTER_MATCH ||
			(Match.Value != (ptr)(uintptr_t)(iRoute + 1u)) ||
			(iCount != 0) ) {
			return 1;
		}
		iPath = (size_t)snprintf(
			Path, sizeof(Path),
			"/items/custom-%u/other",
			(unsigned int)pThread->Index
		);
		if ( xrtHttpRouterMatch(
			pThread->Router,
			XRT_STR_LITERAL("GET"),
			(xstrview){ Path, iPath },
			Params, 2, &iCount, &Match
		) != XHTTP_ROUTER_MATCH ||
			(Match.Value != (ptr)(uintptr_t)999u) ||
			(iCount != 1) ) {
			return 2;
		}
	}
	return 0;
}



/* 构建宽静态节点和参数回退分支，再并发执行只读匹配。 */
int main(void)
{
	xhttprouter* pRouter = xrtHttpRouterCreate(NULL);
	test_http_router_thread Contexts[TEST_HTTP_ROUTER_THREADS];
	xthread* Threads[TEST_HTTP_ROUTER_THREADS];
	char Pattern[64];
	size_t i;

	testRequire(pRouter != NULL, "threaded HTTP router create failed");
	for ( i = 0; i < TEST_HTTP_ROUTER_ROUTES; i++ ) {
		size_t iPattern = (size_t)snprintf(
			Pattern, sizeof(Pattern),
			"/items/%03u/detail",
			(unsigned int)i
		);

		testRequire(
			xrtHttpRouterAdd(
				pRouter,
				XRT_STR_LITERAL("GET"),
				(xstrview){ Pattern, iPattern },
				(ptr)(uintptr_t)(i + 1u)
			),
			"threaded HTTP router static registration failed"
		);
	}
	testRequire(
		xrtHttpRouterAdd(
			pRouter, XRT_STR_LITERAL("GET"),
			XRT_STR_LITERAL("/items/{id}/other"),
			(ptr)(uintptr_t)999u
		) && xrtHttpRouterFreeze(pRouter),
		"threaded HTTP router freeze failed"
	);
	for ( i = 0; i < TEST_HTTP_ROUTER_THREADS; i++ ) {
		Contexts[i].Router = pRouter;
		Contexts[i].Index = i;
		Threads[i] = xrtThreadCreate(
			testHttpRouterThreadEntry, &Contexts[i], 0
		);
		testRequire(
			Threads[i] != NULL,
			"HTTP router worker create failed"
		);
	}
	for ( i = 0; i < TEST_HTTP_ROUTER_THREADS; i++ ) {
		testRequire(
			xrtThreadWait(Threads[i]) == XWAIT_OK,
			"HTTP router worker wait failed"
		);
		testRequire(
			xrtThreadExitCode(Threads[i]) == 0,
			"HTTP router concurrent match failed"
		);
		xrtThreadDestroy(Threads[i]);
	}
	xrtHttpRouterDestroy(pRouter);
	printf(
		"[PASS] HTTP router threads (%u matches)\n",
		(unsigned int)(
			TEST_HTTP_ROUTER_THREADS *
			TEST_HTTP_ROUTER_ROUNDS * 2u
		)
	);
	return 0;
}
